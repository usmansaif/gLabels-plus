//  LabelEditor.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//


#include "LabelEditor.hpp"

#include "Cursors.hpp"
#include "UndoRedoModel.hpp"
#include "VariableHighlighter.hpp"

#include "model/FrameCd.hpp"
#include "model/FrameEllipse.hpp"
#include "model/FrameRect.hpp"
#include "model/FrameRound.hpp"
#include "model/Model.hpp"
#include "model/ModelObject.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Markup.hpp"
#include "model/Settings.hpp"
#include "model/TextNode.hpp"
#include "model/Size.hpp"

#include "merge/DataSourceUtil.hpp"

#include <QAbstractTextDocumentLayout>
#include <QDebug>
#include <QKeyEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QTextBlock>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPalette>
#include <QtMath>


//
// Private
//
namespace
{
        const int     nZoomLevels = 11;
        const double  zoomLevels[nZoomLevels] = { 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.67, 0.50, 0.33 };

        const double  ZOOM_TO_FIT_PAD = 16.0;

        const QColor  shadowColor( 0, 0, 0, 64 );
        const double  shadowOffsetPixels = 4;

        const QColor  labelColor( 255, 255, 255 );
        const QColor  labelOutlineColor( 0, 0, 0 );
        const double  labelOutlineWidthPixels = 1;

        const QColor  gridLineColor( 192, 192, 192 );
        const double  gridLineWidthPixels = 1;

        const QColor  markupLineColor( 240, 99, 99 );
        const double  markupLineWidthPixels = 1;

        const QColor  selectRegionFillColor( 192, 192, 255, 128 );
        const QColor  selectRegionOutlineColor( 0, 0, 255, 128 );

        const QColor  guideColor( 242, 72, 34 );          // smart guides and measure tape
        const QColor  guideTextColor( 255, 255, 255 );
        const double  snapTolerancePixels = 5;
        const double  selectRegionOutlineWidthPixels = 3;

        // Multi-object selection box: resized from its corners, aspect ratio kept
        const QColor  groupBoxColor( 0, 0, 255, 160 );
        const QColor  groupHandleFillColor( 0, 192, 0, 96 );   // same look as object handles
        const QColor  groupHandleOutlineColor( 0, 0, 0, 192 );
        const double  groupHandlePixels     = 7;
        const double  groupHandleGrabPixels = 11;
}


namespace
{
        const char* VARIABLE_MIME_TYPE = "application/x-glabels-variable";
}


namespace
{
        ///
        /// Resize cursor pointing the way a handle pulls, allowing for rotation
        ///
        QCursor resizeCursor( const glabels::model::Handle& handle )
        {
                QPointF d = handle.direction();
                const QTransform m = handle.owner()->matrix();
                d = QPointF( m.m11()*d.x() + m.m21()*d.y(), m.m12()*d.x() + m.m22()*d.y() );   // rotate, no shift
                if ( d.isNull() )
                {
                        return QCursor( Qt::SizeAllCursor );
                }

                // Fold to 0..180 degrees, then pick the nearest of the four resize cursors
                double angle = std::fmod( qRadiansToDegrees( std::atan2( d.y(), d.x() ) ) + 360.0, 180.0 );
                if ( angle < 22.5 || angle >= 157.5 )
                {
                        return QCursor( Qt::SizeHorCursor );
                }
                if ( angle < 67.5 )
                {
                        return QCursor( Qt::SizeFDiagCursor );       // down-right / up-left
                }
                if ( angle < 112.5 )
                {
                        return QCursor( Qt::SizeVerCursor );
                }
                return QCursor( Qt::SizeBDiagCursor );               // down-left / up-right
        }


        ///
        /// Rotate zone: a ring just outside each corner of the selected object
        ///
        const double rotateZonePixels = 22;


        ///
        /// Photoshop-style rotate cursor: a curved double-headed arrow wrapping the
        /// corner, turned to face outwardDeg (screen degrees, y down)
        ///
        QCursor rotateCursor( double outwardDeg, qreal dpr )
        {
                const int S = 32;
                QPixmap pm( QSize( S, S ) * dpr );
                pm.setDevicePixelRatio( dpr );
                pm.fill( Qt::transparent );

                QPainter p( &pm );
                p.setRenderHint( QPainter::Antialiasing );

                // Drawn facing up-right (-45 degrees), then turned to face the corner
                p.translate( S/2.0, S/2.0 );
                p.rotate( outwardDeg + 45.0 );
                p.translate( -S/2.0, -S/2.0 );

                const QPointF c( 8, 24 );
                const double  r = 13;
                const QRectF  arcRect( c.x() - r, c.y() - r, 2*r, 2*r );
                QPainterPath arc;
                arc.arcMoveTo( arcRect, 0 );
                arc.arcTo( arcRect, 0, 90 );

                auto head = []( const QPointF& base, const QPointF& d ) {
                        const QPointF n( -d.y(), d.x() );
                        QPainterPath h;
                        h.moveTo( base + d*5.5 );
                        h.lineTo( base + n*4.5 );
                        h.lineTo( base - n*4.5 );
                        h.closeSubpath();
                        return h;
                };
                QPainterPath heads = head( QPointF( c.x() + r, c.y() ), QPointF( 0, 1 ) )
                                   + head( QPointF( c.x(), c.y() - r ), QPointF( -1, 0 ) );

                // White halo so the cursor reads on any background, then black
                p.setBrush( Qt::NoBrush );
                p.setPen( QPen( Qt::white, 4.5, Qt::SolidLine, Qt::RoundCap ) );
                p.drawPath( arc );
                p.setPen( QPen( Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
                p.setBrush( Qt::white );
                p.drawPath( heads );

                p.setPen( QPen( Qt::black, 1.6, Qt::SolidLine, Qt::RoundCap ) );
                p.setBrush( Qt::NoBrush );
                p.drawPath( arc );
                p.setPen( Qt::NoPen );
                p.setBrush( Qt::black );
                p.drawPath( heads );
                p.end();

                return QCursor( pm, S/2, S/2 );
        }
}


namespace glabels
{

        ///
        /// Constructor
        ///
        LabelEditor::LabelEditor( QScrollArea* scrollArea, QWidget* parent )
                : QWidget(parent), mScrollArea(scrollArea)
        {
                mModel              = nullptr;
                mUndoRedoModel      = nullptr;

                mZoom               = 1;
                mZoomToFitFlag      = false;
                mScale              = 1;
                mMarkupVisible      = true;
                mGridVisible        = true;

                mState = IdleState;

                mSelectRegionVisible  = false;
                mResizeObject         = nullptr;
                mResizeHandleLocation = model::Handle::NULL_HANDLE;
                mResizeHonorAspect    = false;
                mCreateObjectType     = Box;
                mCreateObject         = nullptr;

                setMouseTracking( true );
                setFocusPolicy(Qt::StrongFocus);
                setAcceptDrops( true );

                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                onSettingsChanged();
        }


        ///
        /// Zoom property
        ///
        double
        LabelEditor::zoom() const
        {
                return mZoom;
        }


        ///
        /// Scale property (pixels per point)
        ///
        double
        LabelEditor::scale() const
        {
                return mScale;
        }


        ///
        /// Label origin in widget pixels
        ///
        QPointF
        LabelEditor::labelOriginPixels() const
        {
                return QPointF( mX0.pt()*mScale, mY0.pt()*mScale );
        }


        ///
        /// Label size (as displayed, i.e. after any rotation)
        ///
        model::Distance
        LabelEditor::labelWidth() const
        {
                return mModel ? mModel->w() : model::Distance(0);
        }

        model::Distance
        LabelEditor::labelHeight() const
        {
                return mModel ? mModel->h() : model::Distance(0);
        }


        ///
        /// Standard zoom levels (descending)
        ///
        QList<double>
        LabelEditor::standardZoomLevels()
        {
                QList<double> list;
                for ( int i = 0; i < nZoomLevels; i++ )
                {
                        list << zoomLevels[i];
                }
                return list;
        }


        ///
        /// Set zoom to arbitrary value (clamped to supported range)
        ///
        void
        LabelEditor::setZoom( double zoom )
        {
                if ( !mModel )
                {
                        return;
                }
                zoom = qMin( zoom, zoomLevels[0] );
                zoom = qMax( zoom, zoomLevels[nZoomLevels-1] );
                setZoomReal( zoom, false );
        }


        ///
        /// Markup visible? property
        ///
        bool
        LabelEditor::markupVisible() const
        {
                return mMarkupVisible;
        }


        ///
        /// Grid visible? property
        ///
        bool
        LabelEditor::qridVisible() const
        {
                return mGridVisible;
        }


        ///
        /// Model Parameter Setter
        ///
        void
        LabelEditor::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                endTextEdit( false );

                mModel = model;
                mUndoRedoModel = undoRedoModel;
                mPreviewRecord = -1;

                if ( model )
                {
                        zoomToFit();

                        connect( model, SIGNAL(changed()), this, SLOT(update()) );
                        connect( model, SIGNAL(selectionChanged()), this, SLOT(update()) );
                        connect( model, SIGNAL(sizeChanged()), this, SLOT(onModelSizeChanged()) );
                        connect( model, SIGNAL(mergeChanged()), this, SLOT(onMergeChanged()) );
                        connect( model, SIGNAL(mergeSourceChanged()), this, SLOT(onMergeChanged()) );
                        connect( model, SIGNAL(mergeSelectionChanged()), this, SLOT(onMergeChanged()) );

                        update();
                }
        }


        ///
        /// Grid Visibility Parameter Setter
        ///
        void
        LabelEditor::setGridVisible( bool visibleFlag )
        {
                mGridVisible = visibleFlag;
                update();
        }


        ///
        /// Markup Visibility Parameter Setter
        ///
        void
        LabelEditor::setMarkupVisible( bool visibleFlag )
        {
                mMarkupVisible = visibleFlag;
                update();
        }


        ///
        /// Zoom In "One Notch"
        ///
        void
        LabelEditor::zoomIn()
        {
                // Find closest standard zoom level to our current zoom
                // Start with 2nd largest scale
                int i_min = 1;
                double dist_min = qFabs( zoomLevels[1] - mZoom );

                for ( int i = 2; i < nZoomLevels; i++ )
                {
                        double dist = qFabs( zoomLevels[i] - mZoom );
                        if ( dist < dist_min )
                        {
                                i_min = i;
                                dist_min = dist;
                        }
                }

                // Zoom in one notch
                setZoomReal( zoomLevels[i_min-1], false );
        }


        ///
        /// Zoom Out "One Notch"
        ///
        void
        LabelEditor::zoomOut()
        {
                // Find closest standard zoom level to our current zoom
                // Start with largest scale, end on 2nd smallest
                int i_min = 0;
                double dist_min = qFabs( zoomLevels[0] - mZoom );

                for ( int i = 1; i < (nZoomLevels-1); i++ )
                {
                        double dist = qFabs( zoomLevels[i] - mZoom );
                        if ( dist < dist_min )
                        {
                                i_min = i;
                                dist_min = dist;
                        }
                }

                // Zoom out one notch
                setZoomReal( zoomLevels[i_min+1], false );
        }


        ///
        /// Zoom To 1:1 Scale
        ///
        void
        LabelEditor::zoom1To1()
        {
                setZoomReal( 1.0, false );
        }


        ///
        /// Zoom To Fit
        ///
        void
        LabelEditor::zoomToFit()
        {
                double wPixels = mScrollArea->maximumViewportSize().width();
                double hPixels = mScrollArea->maximumViewportSize().height();

                double x_scale = ( wPixels - ZOOM_TO_FIT_PAD ) / mModel->w().pt();
                double y_scale = ( hPixels - ZOOM_TO_FIT_PAD ) / mModel->h().pt();
                double newZoom = qMin( x_scale, y_scale ) * model::PTS_PER_INCH / physicalDpiX();

                // Limits
                newZoom = qMin( newZoom, zoomLevels[0] );
                newZoom = qMax( newZoom, zoomLevels[nZoomLevels-1] );

                setZoomReal( newZoom, true );
        }


        ///
        /// Is Zoom at Maximum?
        ///
        bool
        LabelEditor::isZoomMax() const
        {
                return mZoom >= zoomLevels[0];
        }


        ///
        /// Is Zoom at Minimum?
        ///
        bool
        LabelEditor::isZoomMin() const
        {
                return mZoom <= zoomLevels[nZoomLevels-1];
        }


        ///
        /// Set Zoom to Value
        ///
        void
        LabelEditor::setZoomReal( double zoom, bool zoomToFitFlag )
        {
                mZoom          = zoom;
                mZoomToFitFlag = zoomToFitFlag;

                /* Actual scale depends on DPI of display (assume DpiX == DpiY). */
                mScale = zoom * physicalDpiX() / model::PTS_PER_INCH;

                setMinimumSize( mScale*mModel->w().pt() + ZOOM_TO_FIT_PAD,
                                mScale*mModel->h().pt() + ZOOM_TO_FIT_PAD );

                /* Adjust origin to center label in widget. */
                mX0 = (width()/mScale - mModel->w()) / 2;
                mY0 = (height()/mScale - mModel->h()) / 2;

                positionTextEditor();
                update();

                emit zoomChanged();
                emit viewChanged();
        }


        ///
        /// Arrow mode (normal mode)
        ///
        void
        LabelEditor::arrowMode()
        {
                setCursor( Qt::ArrowCursor );

                mState = IdleState;
                mMeasureVisible = false;
                update();
        }


        ///
        /// Measure mode: drag anywhere to measure a distance
        ///
        void
        LabelEditor::measureMode()
        {
                if ( isTextEditActive() )
                {
                        endTextEdit( true );
                }
                setCursor( Qt::CrossCursor );

                mState = MeasureIdle;
                mMeasureVisible = false;
                update();
        }


        ///
        /// Create box mode
        ///
        void
        LabelEditor::createBoxMode()
        {
                setCursor( Cursors::Box() );

                mCreateObjectType = Box;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Create ellipse mode
        ///
        void
        LabelEditor::createEllipseMode()
        {
                setCursor( Cursors::Ellipse() );

                mCreateObjectType = Ellipse;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Create image mode
        ///
        void
        LabelEditor::createImageMode()
        {
                setCursor( Cursors::Image() );

                mCreateObjectType = Image;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Create line mode
        ///
        void
        LabelEditor::createLineMode()
        {
                setCursor( Cursors::Line() );

                mCreateObjectType = Line;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Create text mode
        ///
        void
        LabelEditor::createTextMode()
        {
                setCursor( Cursors::Text() );

                mCreateObjectType = Text;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Create barcode mode
        ///
        void
        LabelEditor::createBarcodeMode()
        {
                setCursor( Cursors::Barcode() );

                mCreateObjectType = Barcode;
                mState = CreateIdle;
                mMeasureVisible = false;
        }


        ///
        /// Resize Event Handler
        ///
        void
        LabelEditor::resizeEvent( QResizeEvent *event )
        {
                if ( mModel )
                {
                        if ( mZoomToFitFlag )
                        {
                                zoomToFit();
                        }
                        else
                        {
                                /* Re-adjust origin to center label in widget. */
                                mX0 = (width()/mScale - mModel->w()) / 2;
                                mY0 = (height()/mScale - mModel->h()) / 2;

                                positionTextEditor();
                                update();
                                emit viewChanged();
                        }
                }
        }


        ///
        /// Mouse Wheel Event Handler
        ///
        /// Wheel scrolls the page (vertically; touchpads may also scroll sideways).
        /// Shift+wheel scrolls horizontally.
        /// Ctrl+wheel zooms in/out, keeping the point under the cursor fixed.
        ///
        void
        LabelEditor::wheelEvent( QWheelEvent* event )
        {
                if ( !mModel )
                {
                        QWidget::wheelEvent( event );
                        return;
                }

                auto mods = event->modifiers();

                if ( !(mods & Qt::ControlModifier) )
                {
                        QPoint d = event->pixelDelta().isNull() ? event->angleDelta() / 120 * 40
                                                                : event->pixelDelta();
                        auto* h = mScrollArea->horizontalScrollBar();
                        auto* v = mScrollArea->verticalScrollBar();
                        if ( mods & Qt::ShiftModifier )
                        {
                                // Qt may report Shift+wheel on either axis
                                int delta = d.x() != 0 ? d.x() : d.y();
                                h->setValue( h->value() - delta );
                        }
                        else
                        {
                                h->setValue( h->value() - d.x() );
                                v->setValue( v->value() - d.y() );
                        }
                        event->accept();
                        return;
                }

                // Accumulate high-resolution wheel deltas into whole notches
                QPoint angle = event->angleDelta();
                mWheelAccum += angle.y() != 0 ? angle.y() : angle.x();
                int steps = mWheelAccum / 120;
                if ( steps == 0 )
                {
                        event->accept();
                        return;
                }
                mWheelAccum -= steps * 120;

                // Model point under the cursor before zooming
                QPointF cursorPx = event->position();
                double xModel = cursorPx.x() / mScale - mX0.pt();
                double yModel = cursorPx.y() / mScale - mY0.pt();
                QPoint  cursorInViewport = mScrollArea->viewport()->mapFromGlobal( event->globalPosition().toPoint() );

                double newZoom = mZoom * qPow( 1.2, steps );
                setZoom( newZoom );

                // Resize now (rather than waiting for the scroll area's layout pass)
                // so that the scroll bars can be positioned immediately.
                QSize viewport = mScrollArea->viewport()->size();
                resize( qMax( viewport.width(),  minimumWidth() ),
                        qMax( viewport.height(), minimumHeight() ) );

                double newPxX = (xModel + mX0.pt()) * mScale;
                double newPxY = (yModel + mY0.pt()) * mScale;
                mScrollArea->horizontalScrollBar()->setValue( qRound( newPxX - cursorInViewport.x() ) );
                mScrollArea->verticalScrollBar()->setValue( qRound( newPxY - cursorInViewport.y() ) );

                event->accept();
        }


        ///
        /// Mouse Button Press Event Handler
        ///
        void
        LabelEditor::mousePressEvent( QMouseEvent* event )
        {
                if ( isTextEditActive() )
                {
                        endTextEdit( true );
                }

                if ( mModel )
                {
                        /*
                         * Transform to label coordinates
                         */
                        QTransform transform;

                        transform.scale( mScale, mScale );
                        transform.translate( mX0.pt(), mY0.pt() );

                        QPointF pWorld = transform.inverted().map( event->pos() );
                        model::Distance xWorld = model::Distance::pt( pWorld.x() );
                        model::Distance yWorld = model::Distance::pt( pWorld.y() );


                        if ( event->button() & Qt::LeftButton )
                        {
                                //
                                // LEFT BUTTON
                                //
                                switch (mState)
                                {

                                case IdleState:
                                {
                                        model::ModelObject* object = nullptr;
                                        auto& handle = mModel->handleAt( mScale, xWorld, yWorld );
                                        const int groupCorner = groupHandleAt( pWorld );

                                        if ( groupCorner >= 0 )
                                        {
                                                //
                                                // Start a resize of the whole selection, keeping its
                                                // aspect ratio: the opposite corner stays where it is
                                                //
                                                mGroupResizeStartBox = selectionExtent();
                                                mGroupResizeBox = mGroupResizeStartBox;
                                                const QRectF& b = mGroupResizeStartBox;
                                                const QPointF corners[4] = { b.topLeft(), b.topRight(),
                                                                             b.bottomRight(), b.bottomLeft() };
                                                mGroupResizeAnchor = corners[ (groupCorner + 2) % 4 ];

                                                mGroupResizeItems.clear();
                                                for ( auto* o : mModel->getSelection() )
                                                {
                                                        mGroupResizeItems << GroupResizeItem{
                                                                o,
                                                                QPointF( o->x0().pt(), o->y0().pt() ),
                                                                QSizeF( o->w().pt(), o->h().pt() ),
                                                                o->canText() ? o->fontSize() : 0.0,
                                                                o->canLineWidth() ? o->lineWidth().pt() : 0.0 };
                                                }

                                                mState = ArrowGroupResize;
                                        }
                                        else if ( mModel->isSelectionAtomic() && !handle.isNull() )
                                        {
                                                //
                                                // Start an object resize
                                                //
                                                mResizeObject = handle.owner();
                                                mResizeHandleLocation = handle.location();
                                                mResizeHonorAspect = event->modifiers() & Qt::ControlModifier;
                                                if ( mResizeObject->lockAspectRatio() )
                                                {
                                                        mResizeHonorAspect = !mResizeHonorAspect;
                                                }

                                                mState = ArrowResize;
                                        }
                                        else if ( (object = rotateTargetAt( pWorld, &mRotateCursorAngle )) != nullptr )
                                        {
                                                //
                                                // Start a rotation about the object's center
                                                //
                                                mRotateObject = object;
                                                const QTransform m = object->matrix();
                                                mRotateCenter = QPointF( object->x0().pt(), object->y0().pt() )
                                                              + m.map( QPointF( object->w().pt()/2, object->h().pt()/2 ) );
                                                const QPointF d = pWorld - mRotateCenter;
                                                mRotateStartAngle    = qRadiansToDegrees( std::atan2( d.y(), d.x() ) );
                                                mRotateStartRotation = object->rotation();

                                                mState = ArrowRotate;
                                        }
                                        else if ( (object = mModel->objectAt( mScale, xWorld, yWorld )) != nullptr )
                                        {
                                                //
                                                // Start a Move Selection (adjusting selection if necessary)
                                                //
                                                if ( event->modifiers() & Qt::ControlModifier )
                                                {
                                                        if ( object->isSelected() )
                                                        {
                                                                // Un-selecting a selected item
                                                                mModel->unselectObject( object );
                                                        }
                                                        else
                                                        {
                                                                // Add to current selection
                                                                mModel->selectObject( object );
                                                        }
                                                }
                                                else
                                                {
                                                        if ( !object->isSelected() )
                                                        {
                                                                // Replace current selection with this object
                                                                mModel->unselectAll();
                                                                mModel->selectObject( object );
                                                        }
                                                }

                                                mMoveLastX = xWorld;
                                                mMoveLastY = yWorld;
                                                mMoveStart = QPointF( xWorld.pt(), yWorld.pt() );
                                                mMoveStartBox = selectionExtent();
                                                mAlignGuides.clear();

                                                mState = ArrowMove;
                                        }
                                        else
                                        {
                                                //
                                                // Start a Select Region
                                                //
                                                if ( !(event->modifiers() & Qt::ControlModifier) )
                                                {
                                                        mModel->unselectAll();
                                                }

                                                mSelectRegionVisible = true;
                                                mSelectRegion.setX1( xWorld );
                                                mSelectRegion.setY1( yWorld );
                                                mSelectRegion.setX2( xWorld );
                                                mSelectRegion.setY2( yWorld );

                                                mState = ArrowSelectRegion;
                                                update();
                                        }
                                }
                                break;


                                case CreateIdle:
                                {
                                        switch ( mCreateObjectType )
                                        {
                                        case Box:
                                                mCreateObject = new model::ModelBoxObject();
                                                break;
                                        case Ellipse:
                                                mCreateObject = new model::ModelEllipseObject();
                                                break;
                                        case Line:
                                                mCreateObject = new model::ModelLineObject();
                                                break;
                                        case Image:
                                                mCreateObject = new model::ModelImageObject();
                                                break;
                                        case Text:
                                                mCreateObject = new model::ModelTextObject();
                                                break;
                                        case Barcode:
                                                mCreateObject = new model::ModelBarcodeObject();
                                                break;
                                        default:
                                                qDebug() << "LabelEditor::mousePressEvent: Invalid creation type. Should not happen!";
                                                break;
                                        }

                                        mCreateObject->setPosition( xWorld, yWorld );
                                        mCreateObject->setSize( 0, 0 );
                                        mModel->addObject( mCreateObject );

                                        mModel->unselectAll();
                                        mModel->selectObject( mCreateObject );

                                        mCreateX0 = xWorld;
                                        mCreateY0 = yWorld;

                                        mState = CreateDrag;
                                }
                                break;


                                case MeasureIdle:
                                case MeasureDrag:
                                {
                                        QPointF p = snapMeasurePoint( QPointF( xWorld.pt(), yWorld.pt() ) );
                                        mMeasureP1 = p;
                                        mMeasureP2 = p;
                                        mMeasureVisible = true;
                                        mState = MeasureDrag;
                                        update();
                                }
                                break;


                                default:
                                {
                                        qDebug() << "LabelEditor::mousePressEvent: Invalid state. Should not happen!";
                                }
                                break;

                                }

                        }
                        else if ( event->button() & Qt::RightButton )
                        {
                                //
                                // RIGHT BUTTON
                                //
                                if ( mState == IdleState )
                                {
                                        emit contextMenuActivate( model::Point( xWorld, yWorld ) );
                                }
                        }
                }
        }


        ///
        /// Mouse Movement Event Handler
        ///
        void
        LabelEditor::mouseMoveEvent( QMouseEvent* event )
        {
                if ( mModel )
                {
                        /*
                         * Transform to label coordinates
                         */
                        QTransform transform;

                        transform.scale( mScale, mScale );
                        transform.translate( mX0.pt(), mY0.pt() );

                        QPointF pWorld = transform.inverted().map( event->pos() );
                        model::Distance xWorld = model::Distance::pt( pWorld.x() );
                        model::Distance yWorld = model::Distance::pt( pWorld.y() );


                        /*
                         * Emit signal regardless of mode
                         */
                        emit pointerMoved( model::Point( xWorld, yWorld ) );


                        /*
                         * Handle event as appropriate for state
                         */
                        switch (mState)
                        {

                        case IdleState:
                                if ( int c = groupHandleAt( pWorld ); c >= 0 )
                                {
                                        setCursor( (c % 2 == 0) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor );
                                }
                                else if ( mModel->isSelectionAtomic() &&
                                     !mModel->handleAt( mScale, xWorld, yWorld ).isNull() )
                                {
                                        setCursor( resizeCursor( mModel->handleAt( mScale, xWorld, yWorld ) ) );
                                }
                                else if ( double deg = 0; rotateTargetAt( pWorld, &deg ) )
                                {
                                        setCursor( rotateCursor( deg, devicePixelRatioF() ) );
                                }
                                else if ( mModel->objectAt( mScale, xWorld, yWorld ) )
                                {
                                        setCursor( Cursors::Move() );
                                }
                                else
                                {
                                        setCursor( Qt::ArrowCursor );
                                }
                                break;

                        case ArrowSelectRegion:
                                mSelectRegion.setX2( xWorld );
                                mSelectRegion.setY2( yWorld );
                                update();
                                break;

                        case ArrowMove:
                                mUndoRedoModel->checkpoint( tr("Move") );
                                if ( mMoveStartBox.isValid() )
                                {
                                        // Move to where the pointer says, then snap to
                                        // other objects and the label (Ctrl = no snapping)
                                        QRectF box = mMoveStartBox.translated( xWorld.pt() - mMoveStart.x(),
                                                                               yWorld.pt() - mMoveStart.y() );
                                        mAlignGuides.clear();
                                        if ( !(event->modifiers() & Qt::ControlModifier) )
                                        {
                                                box = snapMoveBox( box );
                                        }
                                        QRectF current = selectionExtent();
                                        double dx = box.left() - current.left();
                                        double dy = box.top()  - current.top();
                                        if ( dx != 0 || dy != 0 )
                                        {
                                                mModel->moveSelection( model::Distance::pt( dx ), model::Distance::pt( dy ) );
                                        }
                                        update();
                                }
                                else
                                {
                                        mModel->moveSelection( (xWorld - mMoveLastX),
                                                               (yWorld - mMoveLastY) );
                                }
                                mMoveLastX = xWorld;
                                mMoveLastY = yWorld;
                                break;

                        case ArrowResize:
                                mUndoRedoModel->checkpoint( tr("Resize") );
                                handleResizeMotion( xWorld, yWorld );
                                break;

                        case ArrowGroupResize:
                                mUndoRedoModel->checkpoint( tr("Resize") );
                                handleGroupResizeMotion( pWorld );
                                update();
                                break;

                        case ArrowRotate:
                        {
                                mUndoRedoModel->checkpoint( tr("Rotate") );
                                const QPointF d = pWorld - mRotateCenter;
                                const double delta = qRadiansToDegrees( std::atan2( d.y(), d.x() ) ) - mRotateStartAngle;
                                double theta = mRotateStartRotation + delta;
                                if ( event->modifiers() & Qt::ShiftModifier )
                                {
                                        theta = std::round( theta / 15.0 ) * 15.0;   // snap to 15 degrees
                                }
                                theta = std::fmod( theta, 360.0 );
                                if ( theta < 0 )
                                {
                                        theta += 360.0;
                                }
                                mRotateObject->setRotation( theta );
                                setCursor( rotateCursor( mRotateCursorAngle + delta, devicePixelRatioF() ) );
                        }
                        break;

                        case CreateIdle:
                        case MeasureIdle:
                                break;

                        case MeasureDrag:
                        {
                                QPointF p = snapMeasurePoint( QPointF( xWorld.pt(), yWorld.pt() ) );
                                if ( event->modifiers() & Qt::ShiftModifier )
                                {
                                        // Constrain to horizontal or vertical
                                        if ( qAbs( p.x() - mMeasureP1.x() ) >= qAbs( p.y() - mMeasureP1.y() ) )
                                        {
                                                p.setY( mMeasureP1.y() );
                                        }
                                        else
                                        {
                                                p.setX( mMeasureP1.x() );
                                        }
                                }
                                mMeasureP2 = p;
                                update();
                        }
                        break;

                        case CreateDrag:
                                switch (mCreateObjectType)
                                {
                                case Box:
                                case Ellipse:
                                case Image:
                                case Text:
                                case Barcode:
                                        mCreateObject->setPosition( min( xWorld, mCreateX0 ),
                                                                    min( yWorld, mCreateY0 ) );
                                        mCreateObject->setSize( max(xWorld,mCreateX0) - min(xWorld,mCreateX0),
                                                                max(yWorld,mCreateY0) - min(yWorld,mCreateY0) );

                                        break;
                                case Line:
                                        mCreateObject->setSize( xWorld - mCreateX0, yWorld - mCreateY0 );
                                        break;
                                default:
                                        qDebug() << "LabelEditor::mouseMoveEvent: Invalid creation mode. Should not happen!";
                                        break;
                                }
                                break;

                        default:
                                qDebug() << "LabelEditor::mouseMoveEvent: Invalid state. Should not happen!";
                                break;

                        }

                        if ( mState == ArrowMove || mState == ArrowResize || mState == ArrowGroupResize ||
                             mState == ArrowRotate || mState == CreateDrag )
                        {
                                showDragInfo( event->pos() );
                        }
                }
        }


        ///
        /// Mouse Button Release Event Handler
        ///
        void
        LabelEditor::mouseReleaseEvent( QMouseEvent* event )
        {
                if ( mModel )
                {
                        /*
                         * Transform to label coordinates
                         */
                        QTransform transform;

                        transform.scale( mScale, mScale );
                        transform.translate( mX0.pt(), mY0.pt() );

                        QPointF pWorld = transform.inverted().map( event->pos() );
                        model::Distance xWorld = model::Distance::pt( pWorld.x() );
                        model::Distance yWorld = model::Distance::pt( pWorld.y() );


                        if ( event->button() & Qt::LeftButton )
                        {
                                hideDragInfo();

                                //
                                // LEFT BUTTON Release
                                //
                                switch (mState)
                                {

                                case ArrowResize:
                                        mState = IdleState;
                                        update();
                                        break;

                                case ArrowGroupResize:
                                        mGroupResizeItems.clear();
                                        mState = IdleState;
                                        update();
                                        break;

                                case ArrowRotate:
                                        mRotateObject = nullptr;
                                        mState = IdleState;
                                        update();
                                        break;

                                case ArrowMove:
                                        mAlignGuides.clear();
                                        mState = IdleState;
                                        update();
                                        break;

                                case MeasureDrag:
                                        mState = MeasureIdle;   // keep the measurement on screen
                                        update();
                                        break;

                                case MeasureIdle:
                                        break;

                                case ArrowSelectRegion:
                                        mSelectRegionVisible = false;
                                        mSelectRegion.setX2( xWorld );
                                        mSelectRegion.setY2( yWorld );

                                        mModel->selectRegion( mSelectRegion );

                                        mState = IdleState;
                                        update();
                                        break;

                                case CreateDrag:
                                        if ( (fabs(mCreateObject->w()) < 4) && (fabs(mCreateObject->h()) < 4) )
                                        {
                                                switch (mCreateObjectType)
                                                {
                                                case Text:
                                                        mCreateObject->setSize( 72, 36 );
                                                        break;
                                                case Barcode:
                                                        mCreateObject->setSize( 72, 36 );
                                                        break;
                                                case Line:
                                                        mCreateObject->setSize( 72, 0 );
                                                        break;
                                                default:
                                                        mCreateObject->setSize( 72, 72 );
                                                        break;
                                                }
                                        }

                                        setCursor( Qt::ArrowCursor );
                                        mState = IdleState;
                                        break;

                                default:
                                        mState = IdleState;
                                        break;

                                }

                        }
                }
        }


        ///
        /// Leave Event Handler
        ///
        void
        LabelEditor::leaveEvent( QEvent* event )
        {
                if ( mModel )
                {
                        emit pointerExited();
                }
        }


        ///
        /// Handle resize motion
        ///
        void
        LabelEditor::handleResizeMotion( model::Distance xWorld,
                                         model::Distance yWorld )
        {
                QPointF p( xWorld.pt(), yWorld.pt() );

                /*
                 * Change point to object relative coordinates
                 */
                p -= QPointF( mResizeObject->x0().pt(), mResizeObject->y0().pt() );
                p = mResizeObject->matrix().inverted().map( p );

                /*
                 * Initialize origin and 2 corners in object relative coordinates.
                 */
                double x0 = 0.0;
                double y0 = 0.0;

                double x1 = 0.0;
                double y1 = 0.0;

                double x2 = mResizeObject->w().pt();
                double y2 = mResizeObject->h().pt();

                /*
                 * Calculate new size
                 */
                double w = 0.0;
                double h = 0.0;
                switch ( mResizeHandleLocation )
                {
                case model::Handle::NW:
                        w = std::max( x2 - p.x(), 0.0 );
                        h = std::max( y2 - p.y(), 0.0 );
                        break;
                case model::Handle::N:
                        w = x2 - x1;
                        h = std::max( y2 - p.y(), 0.0 );
                        break;
                case model::Handle::NE:
                        w = std::max( p.x() - x1, 0.0 );
                        h = std::max( y2 - p.y(), 0.0 );
                        break;
                case model::Handle::E:
                        w = std::max( p.x() - x1, 0.0 );
                        h = y2 - y1;
                        break;
                case model::Handle::SE:
                        w = std::max( p.x() - x1, 0.0 );
                        h = std::max( p.y() - y1, 0.0 );
                        break;
                case model::Handle::S:
                        w = x2 - x1;
                        h = std::max( p.y() - y1, 0.0 );
                        break;
                case model::Handle::SW:
                        w = std::max( x2 - p.x(), 0.0 );
                        h = std::max( p.y() - y1, 0.0 );
                        break;
                case model::Handle::W:
                        w = std::max( x2 - p.x(), 0.0 );
                        h = y2 - y1;
                        break;
                case model::Handle::P1:
                        x1 = p.x();
                        y1 = p.y();
                        w  = x2 - p.x();
                        h  = y2 - p.y();
                        x0 = x0 + x1;
                        y0 = y0 + y1;
                        break;
                case model::Handle::P2:
                        w  = p.x() - x1;
                        h  = p.y() - y1;
                        x0 = x0 + x1;
                        y0 = y0 + y1;
                        break;
                default:
                        qDebug() << "LabelEditor::handleResizeMotion: Invalid Handle Location. Should not happen!";
                }

                /*
                 * Set size
                 */
                if ( !(mResizeHandleLocation == model::Handle::P1) && !(mResizeHandleLocation == model::Handle::P2) )
                {
                        if ( mResizeHonorAspect )
                        {
                                switch ( mResizeHandleLocation )
                                {
                                case model::Handle::E:
                                case model::Handle::W:
                                        mResizeObject->setWHonorAspect( model::Distance::pt(w) );
                                        break;
                                case model::Handle::N:
                                case model::Handle::S:
                                        mResizeObject->setHHonorAspect( model::Distance::pt(h) );
                                        break;
                                default:
                                        mResizeObject->setSizeHonorAspect( model::Distance::pt(w),
                                                                           model::Distance::pt(h) );
                                        break;
                                }
                        }
                        else
                        {
                                mResizeObject->setSize( model::Distance::pt(w),
                                                        model::Distance::pt(h) );
                        }

                        /*
                         * Adjust origin, if needed.
                         */
                        switch ( mResizeHandleLocation )
                        {
                        case model::Handle::NW:
                                x0 += x2 - mResizeObject->w().pt();
                                y0 += y2 - mResizeObject->h().pt();
                                break;
                        case model::Handle::N:
                        case model::Handle::NE:
                                y0 += y2 - mResizeObject->h().pt();
                                break;
                        case model::Handle::W:
                        case model::Handle::SW:
                                x0 += x2 - mResizeObject->w().pt();
                                break;
                        default:
                                break;
                        }
                }
                else
                {
                        mResizeObject->setSize( model::Distance::pt(w),
                                                model::Distance::pt(h) );
                }

                /*
                 * Put new origin back into world coordinates and set.
                 */
                QPointF p0( x0, y0 );
                p0 = mResizeObject->matrix().map( p0 );
                p0 += QPointF( mResizeObject->x0().pt(), mResizeObject->y0().pt() );
                mResizeObject->setPosition( model::Distance::pt(p0.x()),
                                            model::Distance::pt(p0.y()) );
        }


        ///
        /// Show position (move) or size (resize/create) next to the cursor while dragging
        ///
        model::ModelObject*
        LabelEditor::rotateTargetAt( const QPointF& pWorld, double* outwardDeg )
        {
                if ( !mModel || !model::Settings::mouseRotate() || !mModel->isSelectionAtomic() )
                {
                        return nullptr;
                }
                model::ModelObject* object = mModel->getFirstSelectedObject();
                if ( !object || object->isLocked() )
                {
                        return nullptr;
                }

                const double    w = object->w().pt();
                const double    h = object->h().pt();
                const QTransform m = object->matrix();
                const QPointF   local = m.inverted().map( pWorld - QPointF( object->x0().pt(), object->y0().pt() ) );
                const QPointF   center( w/2, h/2 );

                // Only outside the object: inside belongs to moving it
                if ( QRectF( 0, 0, w, h ).normalized().contains( local ) )
                {
                        return nullptr;
                }

                const double reach = rotateZonePixels / mScale;
                const QPointF corners[] = { { 0, 0 }, { w, 0 }, { 0, h }, { w, h } };
                for ( const auto& corner : corners )
                {
                        const QPointF d   = local - corner;
                        const QPointF out = corner - center;
                        if ( std::hypot( d.x(), d.y() ) <= reach &&
                             d.x()*out.x() + d.y()*out.y() > 0 )
                        {
                                if ( outwardDeg )
                                {
                                        // Corner direction on screen (rotate, no shift)
                                        const QPointF s( m.m11()*out.x() + m.m21()*out.y(),
                                                         m.m12()*out.x() + m.m22()*out.y() );
                                        *outwardDeg = qRadiansToDegrees( std::atan2( s.y(), s.x() ) );
                                }
                                return object;
                        }
                }
                return nullptr;
        }


        void
        LabelEditor::showDragInfo( const QPoint& pos )
        {
                QString text;
                model::Units units = model::Settings::units();
                int digits = units.resolutionDigits();
                auto fmt = [&]( model::Distance d ) {
                        return QString::number( d.inUnits( units ), 'f', digits );
                };

                model::ModelObject* object = nullptr;
                if ( mState == ArrowMove )
                {
                        // Top-left corner of the whole selection
                        bool first = true;
                        model::Distance x, y;
                        for ( auto* o : mModel->getSelection() )
                        {
                                model::Region r = o->getExtent();
                                model::Distance rx = min( r.x1(), r.x2() );
                                model::Distance ry = min( r.y1(), r.y2() );
                                if ( first || rx < x ) x = rx;
                                if ( first || ry < y ) y = ry;
                                first = false;
                        }
                        if ( first )
                        {
                                return;
                        }
                        text = tr("X: %1  Y: %2 %3").arg( fmt(x), fmt(y), units.toIdString() );
                }
                else if ( mState == ArrowRotate )
                {
                        if ( !mRotateObject )
                        {
                                return;
                        }
                        text = tr("Angle: %1°").arg( QString::number( mRotateObject->rotation(), 'f', 1 ) );
                }
                else if ( mState == ArrowGroupResize )
                {
                        text = tr("W: %1  H: %2 %3")
                                .arg( fmt( model::Distance::pt( mGroupResizeBox.width() ) ),
                                      fmt( model::Distance::pt( mGroupResizeBox.height() ) ),
                                      units.toIdString() );
                }
                else
                {
                        object = (mState == ArrowResize) ? mResizeObject : mCreateObject;
                        if ( !object )
                        {
                                return;
                        }
                        if ( dynamic_cast<model::ModelLineObject*>( object ) )
                        {
                                double w = object->w().pt();
                                double h = object->h().pt();
                                double angle = qRadiansToDegrees( std::atan2( -h, w ) );
                                text = tr("Length: %1 %2  Angle: %3°")
                                        .arg( fmt( model::Distance::pt( std::hypot( w, h ) ) ),
                                              units.toIdString(),
                                              QString::number( angle, 'f', 1 ) );
                        }
                        else
                        {
                                text = tr("W: %1  H: %2 %3")
                                        .arg( fmt( object->w() ), fmt( object->h() ), units.toIdString() );
                        }
                }

                if ( !mDragInfo )
                {
                        mDragInfo = new QLabel( this );
                        mDragInfo->setAttribute( Qt::WA_TransparentForMouseEvents );
                        mDragInfo->setFocusPolicy( Qt::NoFocus );
                }
                if ( !mDragInfo->isVisible() )
                {
                        // Refresh colors each drag so they follow light/dark theme changes
                        mDragInfo->setStyleSheet( QString( "QLabel { background: %1; color: %2; border: 1px solid %3;"
                                                           " border-radius: 3px; padding: 2px 6px; }" )
                                                  .arg( palette().color( QPalette::ToolTipBase ).name(),
                                                        palette().color( QPalette::ToolTipText ).name(),
                                                        palette().color( QPalette::Mid ).name() ) );
                }

                mDragInfo->setText( text );
                mDragInfo->adjustSize();

                // Below-right of the cursor; flip to the other side near the visible edges
                QRect visible = visibleRegion().boundingRect();
                QSize size = mDragInfo->size();
                int x = pos.x() + 16;
                int y = pos.y() + 20;
                if ( x + size.width() > visible.right() )
                {
                        x = pos.x() - size.width() - 8;
                }
                if ( y + size.height() > visible.bottom() )
                {
                        y = pos.y() - size.height() - 8;
                }
                x = std::max( x, visible.left() );
                y = std::max( y, visible.top() );

                mDragInfo->move( x, y );
                mDragInfo->raise();
                mDragInfo->show();
        }


        void
        LabelEditor::hideDragInfo()
        {
                if ( mDragInfo )
                {
                        mDragInfo->hide();
                }
        }


        ///
        /// Key Press Event Handler
        void
        LabelEditor::keyPressEvent( QKeyEvent* event )
        {
                if ( mState == MeasureIdle || mState == MeasureDrag )
                {
                        if ( event->key() == Qt::Key_Escape )
                        {
                                if ( mMeasureVisible )
                                {
                                        mMeasureVisible = false;   // first Esc clears the measurement
                                        mState = MeasureIdle;
                                        update();
                                }
                                else
                                {
                                        arrowMode();               // second Esc leaves the tool
                                        emit modeChanged();
                                }
                                return;
                        }
                        QWidget::keyPressEvent( event );
                        return;
                }

                if ( mState == IdleState && mModel )
                {
                        // Shift+arrow moves in larger steps
                        model::Distance step = (event->modifiers() & Qt::ShiftModifier) ? 10*mStepSize : mStepSize;

                        switch (event->key())
                        {

                        case Qt::Key_Left:
                                mUndoRedoModel->checkpoint( tr("Move") );
                                mModel->moveSelection( -step, model::Distance(0) );
                                break;

                        case Qt::Key_Up:
                                mUndoRedoModel->checkpoint( tr("Move") );
                                mModel->moveSelection( model::Distance(0), -step );
                                break;

                        case Qt::Key_Right:
                                mUndoRedoModel->checkpoint( tr("Move") );
                                mModel->moveSelection( step, model::Distance(0) );
                                break;

                        case Qt::Key_Down:
                                mUndoRedoModel->checkpoint( tr("Move") );
                                mModel->moveSelection( model::Distance(0), step );
                                break;

                        case Qt::Key_F2:
                        case Qt::Key_Return:
                        case Qt::Key_Enter:
                                if ( mModel->isSelectionAtomic() )
                                {
                                        beginTextEdit( mModel->getFirstSelectedObject() );
                                }
                                break;

                        case Qt::Key_Escape:
                                mModel->unselectAll();
                                break;

                        case Qt::Key_Delete:
                                mUndoRedoModel->checkpoint( tr("Delete") );
                                mModel->deleteSelection();
                                setCursor( Qt::ArrowCursor );
                                break;

                        default:
                                QWidget::keyPressEvent( event );
                                break;

                        }
                }
                else
                {
                        QWidget::keyPressEvent( event );
                }
        }


        ///
        /// Paint Event Handler
        ///
        void
        LabelEditor::paintEvent( QPaintEvent* event )
        {
                if ( mModel )
                {
                        QPainter painter( this );

                        painter.setRenderHint( QPainter::Antialiasing, true );
                        painter.setRenderHint( QPainter::TextAntialiasing, true );
                        painter.setRenderHint( QPainter::SmoothPixmapTransform, true );

                        /* Fill background before any transformations */
                        painter.setBrush( QBrush( palette().color(QPalette::AlternateBase).darker(110) ) );
                        painter.setPen( Qt::NoPen );
                        painter.drawRect( rect() );

                        /* Transform. */
                        painter.scale( mScale, mScale );
                        painter.translate( mX0.pt(), mY0.pt() );

                        /* Now draw from the bottom layer up. */
                        drawBgLayer( &painter );
                        drawGridLayer( &painter );
                        drawMarkupLayer( &painter );
                        drawObjectsLayer( &painter );
                        drawFgLayer( &painter );
                        drawHighlightLayer( &painter );
                        drawSelectRegionLayer( &painter );
                        drawGuidesLayer( &painter );
                        drawMeasureLayer( &painter );
                }
        }


        //
        // Handle drag enter event
        //
        void LabelEditor::dragEnterEvent( QDragEnterEvent *event )
        {
                if ( event->mimeData()->hasFormat( VARIABLE_MIME_TYPE ) ||
                     event->mimeData()->hasUrls()   ||
                     event->mimeData()->hasImage()  ||
                     event->mimeData()->hasText() )
                {
                        event->acceptProposedAction();
                }
                else
                {
                        event->ignore();
                }
        }


        //
        // Handle drag move event
        //
        void LabelEditor::dragMoveEvent( QDragMoveEvent *event )
        {
                if ( event->mimeData()->hasFormat( VARIABLE_MIME_TYPE ) ||
                     event->mimeData()->hasUrls()   ||
                     event->mimeData()->hasImage()  ||
                     event->mimeData()->hasText() )
                {
                        event->acceptProposedAction();
                }
                else
                {
                        event->ignore();
                }
        }


        //
        // Handle drop event
        //
        void LabelEditor::dropEvent( QDropEvent *event )
        {
                /*
                 * Transform to label coordinates
                 */
                QTransform transform;

                transform.scale( mScale, mScale );
                transform.translate( mX0.pt(), mY0.pt() );

                QPointF pWorld = transform.inverted().map( event->position() );
                auto xWorld = model::Distance::pt( pWorld.x() );
                auto yWorld = model::Distance::pt( pWorld.y() );
                auto p = model::Point( xWorld, yWorld );

                if ( event->mimeData()->hasFormat( VARIABLE_MIME_TYPE ) )
                {
                        QStringList parts = QString::fromUtf8( event->mimeData()->data( VARIABLE_MIME_TYPE ) ).split( "\n" );
                        if ( !parts.isEmpty() && !parts[0].isEmpty() )
                        {
                                addVariableObject( parts[0], parts.value( 1, "text" ), p );
                                event->acceptProposedAction();
                                setFocus();
                        }
                        else
                        {
                                event->ignore();
                        }
                }
                else if ( event->mimeData()->hasUrls() )
                {
                        mUndoRedoModel->checkpoint( tr("Drop") );
                        mModel->pasteAsUrls( event->mimeData(), p );
                        event->acceptProposedAction();
                }
                else if ( event->mimeData()->hasImage() )
                {
                        mUndoRedoModel->checkpoint( tr("Drop") );
                        mModel->pasteAsImage( event->mimeData(), p );
                        event->acceptProposedAction();
                }
                else if ( event->mimeData()->hasText() )
                {
                        mUndoRedoModel->checkpoint( tr("Drop") );
                        mModel->pasteAsText( event->mimeData(), p );
                        event->acceptProposedAction();
                }
                else
                {
                        event->ignore();
                }
        }


        ///
        /// Add object bound to a variable, centered on the label
        ///
        void LabelEditor::addVariableObject( const QString& name, const QString& type )
        {
                if ( !mModel )
                {
                        return;
                }
                // Center of the label; cascade so repeated adds don't stack exactly
                double step = 10.0;
                double cx = mModel->w().pt()/2;
                double cy = mModel->h().pt()/2;
                int n = 0;
                bool occupied = true;
                while ( occupied && n < 12 )
                {
                        occupied = false;
                        for ( auto* object : mModel->objectList() )
                        {
                                double ox = object->x0().pt() + object->w().pt()/2;
                                double oy = object->y0().pt() + object->h().pt()/2;
                                if ( std::abs( ox - ( cx + n*step ) ) < 4 && std::abs( oy - ( cy + n*step ) ) < 4 )
                                {
                                        occupied = true;
                                        n++;
                                        break;
                                }
                        }
                }
                model::Point center( model::Distance::pt( cx + n*step ), model::Distance::pt( cy + n*step ) );
                addVariableObject( name, type, center );
        }


        ///
        /// Add object bound to a variable, centered on p
        ///
        void LabelEditor::addVariableObject( const QString& name, const QString& type, const model::Point& p )
        {
                if ( !mModel || !mUndoRedoModel || name.isEmpty() )
                {
                        return;
                }

                double labelW = mModel->w().pt();
                double labelH = mModel->h().pt();

                model::ModelObject* object = nullptr;
                if ( type == merge::DataSourceUtil::TYPE_BARCODE )
                {
                        auto* barcode = new model::ModelBarcodeObject();
                        barcode->setBcData( "${" + name + "}" );
                        barcode->setSize( model::Distance::pt( std::min( 144.0, labelW*0.8 ) ),
                                          model::Distance::pt( std::min( 54.0, labelH*0.45 ) ) );
                        object = barcode;
                }
                else if ( type == merge::DataSourceUtil::TYPE_IMAGE )
                {
                        auto* image = new model::ModelImageObject();
                        image->setFilenameNode( model::TextNode( true, name ) );
                        double side = std::min( 72.0, std::min( labelW, labelH )*0.6 );
                        image->setSize( model::Distance::pt( side ), model::Distance::pt( side ) );
                        object = image;
                }
                else
                {
                        auto* text = new model::ModelTextObject();
                        text->setText( "${" + name + "}" );
                        text->setSize( text->naturalSize() );
                        object = text;
                }

                // Center on p, but keep the object on the label
                double w = object->w().pt();
                double h = object->h().pt();
                double x = qBound( 0.0, p.x().pt() - w/2, std::max( 0.0, labelW - w ) );
                double y = qBound( 0.0, p.y().pt() - h/2, std::max( 0.0, labelH - h ) );
                object->setPosition( model::Distance::pt( x ), model::Distance::pt( y ) );

                mUndoRedoModel->checkpoint( tr("Add Variable") );
                mModel->addObject( object );
                mModel->unselectAll();
                mModel->selectObject( object );
        }


        ///
        /// Draw Background Layer
        ///
        void
        LabelEditor::drawBgLayer( QPainter* painter )
        {
                /*
                 * Draw shadow
                 */
                painter->save();

                painter->setBrush( QBrush( shadowColor ) );
                painter->setPen( Qt::NoPen );

                painter->translate( shadowOffsetPixels/mScale, shadowOffsetPixels/mScale );

                if ( mModel->rotate() )
                {
                        painter->rotate( -90 );
                        painter->translate( -mModel->frame()->w().pt(), 0 );
                }
                painter->drawPath( mModel->frame()->path() );

                painter->restore();


                /*
                 * Draw label
                 */
                painter->save();

                painter->setBrush( QBrush( labelColor ) );
                painter->setPen( Qt::NoPen );

                if ( mModel->rotate() )
                {
                        painter->rotate( -90 );
                        painter->translate( -mModel->frame()->w().pt(), 0 );
                }
                painter->drawPath( mModel->frame()->path() );

                painter->restore();
        }


        ///
        /// Draw Grid Layer
        ///
        void
        LabelEditor::drawGridLayer( QPainter* painter )
        {
                if ( mGridVisible )
                {
                        auto gridSpacing = model::Settings::gridSpacing();
                        auto gridOrigin  = model::Settings::gridOrigin();

                        bool isRectangular = dynamic_cast<const model::FrameRect*>( mModel->frame() );

                        model::Distance w = mModel->frame()->w();
                        model::Distance h = mModel->frame()->h();

                        // Set origin of grid.  For non-rectangular labels (e.g. round, cd, etc.),
                        // ignore the gridOrigin setting and always use the center of the label.
                        auto x0 = gridSpacing;
                        auto y0 = gridSpacing;
                        if ( gridOrigin == model::Settings::ORIGIN_CENTER || !isRectangular )
                        {
                                x0 = fmod( w/2, gridSpacing );
                                y0 = fmod( h/2, gridSpacing );
                        }

                        painter->save();
                        if ( mModel->rotate() )
                        {
                                painter->rotate( -90 );
                                painter->translate( -mModel->frame()->w().pt(), 0 );
                        }

                        painter->setClipPath( mModel->frame()->path() );

                        QPen pen( gridLineColor, gridLineWidthPixels );
                        pen.setCosmetic( true );
                        painter->setPen( pen );

                        for ( model::Distance x = x0; x < w; x += gridSpacing )
                        {
                                painter->drawLine( x.pt(), 0, x.pt(), h.pt() );
                        }

                        for ( model::Distance y = y0; y < h; y += gridSpacing )
                        {
                                painter->drawLine( 0, y.pt(), w.pt(), y.pt() );
                        }

                        painter->restore();
                }
        }


        ///
        /// Draw Markup Layer
        ///
        void
        LabelEditor::drawMarkupLayer( QPainter* painter )
        {
                if ( mMarkupVisible )
                {
                        painter->save();

                        QPen pen( markupLineColor, markupLineWidthPixels );
                        pen.setCosmetic( true );

                        painter->setBrush( Qt::NoBrush );
                        painter->setPen( pen );

                        if ( mModel->rotate() )
                        {
                                painter->rotate( -90 );
                                painter->translate( -mModel->frame()->w().pt(), 0 );
                        }

                        for( auto& markup : mModel->frame()->markups() )
                        {
                                painter->drawPath( markup->path( *mModel->frame() ) );
                        }

                        painter->restore();
                }
        }


        ///
        /// Draw Objects Layer
        ///
        void
        LabelEditor::drawObjectsLayer( QPainter* painter )
        {
                auto* merge = mModel->merge();
                if ( mPreviewRecord >= 0 && merge )
                {
                        auto records = merge->selectedRecords();
                        if ( mPreviewRecord < records.size() )
                        {
                                model::Variables variables( mModel->constVariables() );
                                mModel->draw( painter, false, records[mPreviewRecord], variables );
                                return;
                        }
                }

                mModel->draw( painter, true, merge::NullRecord(), model::Variables() );
        }


        ///
        /// Draw Foreground Layer
        ///
        void
        LabelEditor::drawFgLayer( QPainter* painter )
        {
                /*
                 * Draw label outline
                 */
                painter->save();

                QPen pen( labelOutlineColor, labelOutlineWidthPixels );
                pen.setCosmetic( true );
                painter->setBrush( QBrush( Qt::NoBrush ) );
                painter->setPen( pen );

                if ( mModel->rotate() )
                {
                        painter->rotate( -90 );
                        painter->translate( -mModel->frame()->w().pt(), 0 );
                }
                painter->drawPath( mModel->frame()->path() );

                painter->restore();
        }


        ///
        /// Draw Highlight Layer
        ///
        void
        LabelEditor::drawHighlightLayer( QPainter* painter )
        {
                painter->save();

                for ( model::ModelObject* object : mModel->objectList() )
                {
                        if ( object->isSelected() )
                        {
                                object->drawSelectionHighlight( painter, mScale );
                        }
                }

                painter->restore();

                drawGroupHandles( painter );
        }


        ///
        /// Whether the selection can be resized as one: two or more objects, none locked
        ///
        bool
        LabelEditor::canGroupResize() const
        {
                if ( !mModel || mModel->isSelectionEmpty() || mModel->isSelectionAtomic() )
                {
                        return false;
                }
                for ( auto* object : mModel->getSelection() )
                {
                        if ( object->isLocked() )
                        {
                                return false;
                        }
                }
                return selectionExtent().isValid();
        }


        ///
        /// Corner handle of the selection's bounding box under pWorld, or -1
        ///
        int
        LabelEditor::groupHandleAt( const QPointF& pWorld ) const
        {
                if ( !canGroupResize() )
                {
                        return -1;
                }
                const QRectF b = selectionExtent();
                const QPointF corners[4] = { b.topLeft(), b.topRight(), b.bottomRight(), b.bottomLeft() };
                const double c = groupHandleGrabPixels / (2*mScale);
                for ( int i = 0; i < 4; i++ )
                {
                        if ( qAbs( pWorld.x() - corners[i].x() ) <= c && qAbs( pWorld.y() - corners[i].y() ) <= c )
                        {
                                return i;
                        }
                }
                return -1;
        }


        ///
        /// Scale every selected object about the anchor corner by the same factor,
        /// so the group keeps its aspect ratio (and so does every object in it)
        ///
        void
        LabelEditor::handleGroupResizeMotion( const QPointF& pWorld )
        {
                const QRectF& b0 = mGroupResizeStartBox;
                if ( b0.width() <= 0 && b0.height() <= 0 )
                {
                        return;
                }

                // Use whichever direction the pointer has pulled further
                const double sx = b0.width()  > 0 ? qAbs( pWorld.x() - mGroupResizeAnchor.x() ) / b0.width()  : 0;
                const double sy = b0.height() > 0 ? qAbs( pWorld.y() - mGroupResizeAnchor.y() ) / b0.height() : 0;
                const double minSize = 1.0;   // pt: never shrink the group to nothing
                const double sMin = minSize / std::max( b0.width(), b0.height() );
                const double s = std::max( std::max( sx, sy ), sMin );

                for ( const auto& item : mGroupResizeItems )
                {
                        auto* o = item.object;
                        const QPointF origin = mGroupResizeAnchor + s * (item.origin - mGroupResizeAnchor);
                        o->setSize( model::Distance::pt( item.size.width()  * s ),
                                    model::Distance::pt( item.size.height() * s ) );
                        o->setPosition( model::Distance::pt( origin.x() ),
                                        model::Distance::pt( origin.y() ) );
                        if ( item.fontSize > 0 )
                        {
                                o->setFontSize( item.fontSize * s );
                        }
                        if ( item.lineWidth > 0 )
                        {
                                o->setLineWidth( model::Distance::pt( item.lineWidth * s ) );
                        }
                }

                mGroupResizeBox = QRectF( mGroupResizeAnchor + s * (b0.topLeft()     - mGroupResizeAnchor),
                                          mGroupResizeAnchor + s * (b0.bottomRight() - mGroupResizeAnchor) ).normalized();
        }


        ///
        /// Bounding box and corner handles of a multi-object selection
        ///
        void
        LabelEditor::drawGroupHandles( QPainter* painter )
        {
                if ( !canGroupResize() )
                {
                        return;
                }

                const QRectF b = selectionExtent();
                const double s = 1.0 / mScale;

                painter->save();

                QPen dash( groupBoxColor, 1, Qt::DashLine );
                dash.setCosmetic( true );
                painter->setPen( dash );
                painter->setBrush( Qt::NoBrush );
                painter->drawRect( b );

                QPen pen( groupHandleOutlineColor );
                pen.setCosmetic( true );
                painter->setPen( pen );
                painter->setBrush( groupHandleFillColor );
                for ( const QPointF& p : { b.topLeft(), b.topRight(), b.bottomRight(), b.bottomLeft() } )
                {
                        painter->drawRect( QRectF( p.x() - s*groupHandlePixels/2, p.y() - s*groupHandlePixels/2,
                                                   s*groupHandlePixels, s*groupHandlePixels ) );
                }

                painter->restore();
        }


        ///
        /// Draw Select Region Layer
        ///
        void
        LabelEditor::drawSelectRegionLayer( QPainter* painter )
        {
                if ( mSelectRegionVisible )
                {
                        painter->save();

                        QPen pen( selectRegionOutlineColor, selectRegionOutlineWidthPixels );
                        pen.setCosmetic( true );
                        painter->setBrush( selectRegionFillColor );
                        painter->setPen( pen );

                        painter->drawRect( mSelectRegion.rect() );

                        painter->restore();
                }

        }


        ///
        /// Settings changed handler
        ///
        void LabelEditor::onSettingsChanged()
        {
                model::Units units = model::Settings::units();

                mStepSize = model::Distance( units.resolution(), units );

                update();
        }


        ///
        /// Model size changed handler
        ///
        void LabelEditor::onModelSizeChanged()
        {
                if (mZoomToFitFlag)
                {
                        double wPixels = mScrollArea->maximumViewportSize().width();
                        double hPixels = mScrollArea->maximumViewportSize().height();

                        double x_scale = ( wPixels - ZOOM_TO_FIT_PAD ) / mModel->w().pt();
                        double y_scale = ( hPixels - ZOOM_TO_FIT_PAD ) / mModel->h().pt();
                        double newZoom = qMin( x_scale, y_scale ) * model::PTS_PER_INCH / physicalDpiX();

                        // Limits
                        newZoom = qMin( newZoom, zoomLevels[0] );
                        newZoom = qMax( newZoom, zoomLevels[nZoomLevels-1] );

                        mZoom = newZoom;
                }

                /* Actual scale depends on DPI of display (assume DpiX == DpiY). */
                mScale = mZoom * physicalDpiX() / model::PTS_PER_INCH;

                setMinimumSize( mScale*mModel->w().pt() + ZOOM_TO_FIT_PAD,
                                mScale*mModel->h().pt() + ZOOM_TO_FIT_PAD );

                /* Adjust origin to center label in widget. */
                mX0 = (width()/mScale - mModel->w()) / 2;
                mY0 = (height()/mScale - mModel->h()) / 2;

                positionTextEditor();
                update();

                emit zoomChanged();
                emit viewChanged();
        }


        ///
        /// Merge changed handler
        ///
        void LabelEditor::onMergeChanged()
        {
                int n = nPreviewRecords();
                if ( mPreviewRecord >= n )
                {
                        mPreviewRecord = (n > 0) ? n-1 : -1;
                }
                update();
                emit previewRecordChanged();
        }


        ///
        /// Current preview record (-1 = none)
        ///
        int LabelEditor::previewRecord() const
        {
                return mPreviewRecord;
        }


        ///
        /// Number of records available for preview
        ///
        int LabelEditor::nPreviewRecords() const
        {
                if ( mModel && mModel->merge() && !mModel->merge()->keys().isEmpty() )
                {
                        return mModel->merge()->selectedRecords().size();
                }
                return 0;
        }


        ///
        /// Set preview record
        ///
        void LabelEditor::setPreviewRecord( int index )
        {
                int n = nPreviewRecords();
                if ( index >= n )
                {
                        index = n - 1;
                }
                if ( index < -1 )
                {
                        index = -1;
                }

                if ( index != mPreviewRecord )
                {
                        mPreviewRecord = index;
                        update();
                        emit previewRecordChanged();
                }
        }


        ///
        /// Mouse double-click: edit text/barcode data in place
        ///
        void
        LabelEditor::mouseDoubleClickEvent( QMouseEvent* event )
        {
                if ( !mModel || mState != IdleState || !(event->button() & Qt::LeftButton) )
                {
                        QWidget::mouseDoubleClickEvent( event );
                        return;
                }

                QTransform transform;
                transform.scale( mScale, mScale );
                transform.translate( mX0.pt(), mY0.pt() );
                QPointF pWorld = transform.inverted().map( event->pos() );

                auto* object = mModel->objectAt( mScale,
                                                 model::Distance::pt( pWorld.x() ),
                                                 model::Distance::pt( pWorld.y() ) );
                if ( object )
                {
                        beginTextEdit( object );
                }
        }


        ///
        /// Is an in-place text edit currently active?
        ///
        bool LabelEditor::isTextEditActive() const
        {
                return !mTextEditor.isNull();
        }


        ///
        /// Begin in-place editing of object's text (text objects) or data (barcodes)
        ///
        void LabelEditor::beginTextEdit( model::ModelObject* object )
        {
                if ( !object || !mModel )
                {
                        return;
                }

                bool isText    = dynamic_cast<model::ModelTextObject*>( object ) != nullptr;
                bool isBarcode = dynamic_cast<model::ModelBarcodeObject*>( object ) != nullptr;
                if ( !isText && !isBarcode )
                {
                        return;
                }

                endTextEdit( true );

                mModel->unselectAll();
                mModel->selectObject( object );

                mTextEditObject    = object;
                mTextEditIsBarcode = isBarcode;

                mTextEditor = new QTextEdit( this );
                mTextEditor->setAcceptRichText( false );
                mTextEditor->setFrameShape( QFrame::Box );
                mTextEditor->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mTextEditor->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mTextEditor->document()->setDocumentMargin( 0 );
                mTextEditor->setPlainText( merge::DataSourceUtil::toDisplay( isBarcode ? object->bcData() : object->text() ) );
                new VariableHighlighter( mTextEditor->document() );
                mTextEditor->setToolTip( isBarcode ? tr("Enter to apply, Esc to cancel")
                                                   : tr("Ctrl+Enter or click outside to apply, Esc to cancel") );

                // Use a light background so the text stays readable on the white label in any theme
                QPalette pal = mTextEditor->palette();
                pal.setColor( QPalette::Base, QColor( 255, 255, 240 ) );
                pal.setColor( QPalette::Text, isText ? object->textColorNode().color() : QColor( Qt::black ) );
                mTextEditor->setPalette( pal );
                mTextEditor->setStyleSheet( "QTextEdit { border: 1px dashed #3465a4; }" );

                if ( isText )
                {
                        mTextEditor->setLineWrapMode( object->textWrapMode() == QTextOption::NoWrap
                                                      ? QTextEdit::NoWrap : QTextEdit::WidgetWidth );
                }
                else
                {
                        mTextEditor->setLineWrapMode( QTextEdit::NoWrap );
                }

                mTextEditor->installEventFilter( this );

                positionTextEditor();

                mTextEditor->show();
                mTextEditor->setFocus();
                mTextEditor->selectAll();
        }


        ///
        /// Place the in-place editor over its object (respecting zoom and rotation)
        ///
        void LabelEditor::positionTextEditor()
        {
                if ( !mTextEditor )
                {
                        return;
                }
                if ( !mTextEditObject )
                {
                        endTextEdit( false );
                        return;
                }

                auto* object = mTextEditObject.data();

                // Object center in label coordinates
                QPointF c = object->matrix().map( QPointF( object->w().pt()/2, object->h().pt()/2 ) );
                c += QPointF( object->x0().pt(), object->y0().pt() );

                // Object center in widget pixels
                QPointF cPix( (c.x() + mX0.pt()) * mScale, (c.y() + mY0.pt()) * mScale );

                // Editor size: object's own (unrotated) size at current zoom, with sane minimums
                double wPix = qMax( object->w().pt() * mScale, 60.0 );
                double hPix = qMax( object->h().pt() * mScale, 24.0 );

                // If the object is rotated by roughly 90/270 degrees, swap so the editor covers it
                double theta = std::fmod( object->rotation(), 180.0 );
                if ( theta > 45.0 && theta < 135.0 )
                {
                        std::swap( wPix, hPix );
                }

                if ( !mTextEditIsBarcode )
                {
                        QFont font( object->fontFamily() );
                        font.setPixelSize( qMax( 1, int( object->fontSize() * mScale + 0.5 ) ) );
                        font.setWeight( object->fontWeight() );
                        font.setItalic( object->fontItalicFlag() );
                        font.setUnderline( object->fontUnderlineFlag() );
                        font.setLetterSpacing( QFont::AbsoluteSpacing, object->textLetterSpacing() * mScale );
                        mTextEditor->setFont( font );

                        // Horizontal alignment for all paragraphs
                        QTextCursor cursor( mTextEditor->document() );
                        cursor.select( QTextCursor::Document );
                        QTextBlockFormat fmt;
                        fmt.setAlignment( object->textHAlign() );
                        fmt.setLineHeight( object->textLineSpacing()*100, QTextBlockFormat::ProportionalHeight );
                        cursor.mergeBlockFormat( fmt );
                }
                else
                {
                        QFont font = this->font();
                        font.setPixelSize( qBound( 10, int( hPix/3 ), 24 ) );
                        mTextEditor->setFont( font );
                        hPix = qMax( 24.0, mTextEditor->fontMetrics().height() * 1.6 );
                }

                // Grow to fit content height
                double docH = mTextEditor->document()->size().height() + 4;
                hPix = qMax( hPix, docH );

                QRect r( int( cPix.x() - wPix/2 ), int( cPix.y() - hPix/2 ), int( wPix ), int( hPix ) );
                mTextEditor->setGeometry( r );
        }


        ///
        /// End in-place editing
        ///
        void LabelEditor::endTextEdit( bool commit )
        {
                if ( !mTextEditor || mTextEditEnding )
                {
                        return;
                }
                mTextEditEnding = true;

                QTextEdit* editor = mTextEditor.data();
                mTextEditor = nullptr;

                if ( commit && mTextEditObject && mModel && mUndoRedoModel )
                {
                        QString newText = merge::DataSourceUtil::fromDisplay( editor->toPlainText() );
                        auto* object = mTextEditObject.data();

                        if ( mTextEditIsBarcode )
                        {
                                newText = newText.trimmed();
                                if ( newText != object->bcData() )
                                {
                                        mUndoRedoModel->checkpoint( tr("Edit Barcode Data") );
                                        object->setBcData( newText );
                                }
                        }
                        else
                        {
                                if ( newText != object->text() )
                                {
                                        mUndoRedoModel->checkpoint( tr("Edit Text") );
                                        object->setText( newText );
                                }
                        }
                }

                mTextEditObject = nullptr;

                editor->removeEventFilter( this );
                editor->hide();
                editor->deleteLater();

                mTextEditEnding = false;

                setFocus();
                update();
        }


        ///
        /// Event filter for in-place editor
        ///
        bool LabelEditor::eventFilter( QObject* watched, QEvent* event )
        {
                if ( mTextEditor && watched == mTextEditor.data() )
                {
                        if ( event->type() == QEvent::KeyPress )
                        {
                                auto* keyEvent = static_cast<QKeyEvent*>( event );
                                bool isEnter = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;

                                if ( keyEvent->key() == Qt::Key_Escape )
                                {
                                        endTextEdit( false );
                                        return true;
                                }
                                if ( isEnter && ( mTextEditIsBarcode || (keyEvent->modifiers() & Qt::ControlModifier) ) )
                                {
                                        endTextEdit( true );
                                        return true;
                                }
                        }
                        else if ( event->type() == QEvent::KeyRelease )
                        {
                                // Content may have changed height
                                QMetaObject::invokeMethod( this, [this]{ positionTextEditor(); }, Qt::QueuedConnection );
                        }
                        else if ( event->type() == QEvent::FocusOut )
                        {
                                auto* focusEvent = static_cast<QFocusEvent*>( event );
                                if ( focusEvent->reason() != Qt::PopupFocusReason &&
                                     focusEvent->reason() != Qt::ActiveWindowFocusReason )
                                {
                                        QMetaObject::invokeMethod( this, [this]{ endTextEdit( true ); }, Qt::QueuedConnection );
                                }
                        }
                }

                return QWidget::eventFilter( watched, event );
        }



        ///////////////////////////////////////////////////////////////////////
        // Smart guides and measure tape
        ///////////////////////////////////////////////////////////////////////

        namespace
        {
                struct SnapLine
                {
                        double v;      // x (vertical line) or y (horizontal line), in points
                        QRectF src;    // rectangle the line belongs to
                };

                void addSnapLines( QList<SnapLine>& xs, QList<SnapLine>& ys, const QRectF& r )
                {
                        xs << SnapLine{ r.left(), r } << SnapLine{ r.center().x(), r } << SnapLine{ r.right(), r };
                        ys << SnapLine{ r.top(), r }  << SnapLine{ r.center().y(), r } << SnapLine{ r.bottom(), r };
                }

                /// Smallest correction that brings one of the features onto a snap line
                bool bestSnap( const QList<SnapLine>& lines, const QList<double>& features,
                               double tol, double& delta )
                {
                        bool found = false;
                        for ( const auto& line : lines )
                        {
                                for ( double f : features )
                                {
                                        double d = line.v - f;
                                        if ( qAbs( d ) <= tol && ( !found || qAbs( d ) < qAbs( delta ) ) )
                                        {
                                                delta = d;
                                                found = true;
                                        }
                                }
                        }
                        return found;
                }
        }


        ///
        /// Bounding box of the selected objects, in points (null if nothing is selected)
        ///
        QRectF
        LabelEditor::selectionExtent() const
        {
                QRectF box;
                if ( mModel )
                {
                        for ( auto* object : mModel->objectList() )
                        {
                                if ( object->isSelected() )
                                {
                                        QRectF r = object->getExtent().rect().normalized();
                                        box = box.isNull() ? r : box.united( r );
                                }
                        }
                }
                return box;
        }


        ///
        /// Bounding boxes of the objects that are not selected, in points
        ///
        QList<QRectF>
        LabelEditor::otherExtents() const
        {
                QList<QRectF> list;
                if ( mModel )
                {
                        for ( auto* object : mModel->objectList() )
                        {
                                if ( !object->isSelected() )
                                {
                                        list << object->getExtent().rect().normalized();
                                }
                        }
                }
                return list;
        }


        QRectF
        LabelEditor::labelRect() const
        {
                return QRectF( 0, 0, labelWidth().pt(), labelHeight().pt() );
        }


        ///
        /// Snap a measure-tape end to nearby object edges/centers and label edges/center
        ///
        QPointF
        LabelEditor::snapMeasurePoint( const QPointF& p ) const
        {
                QList<SnapLine> xs, ys;
                addSnapLines( xs, ys, labelRect() );
                if ( mModel )
                {
                        for ( auto* object : mModel->objectList() )
                        {
                                addSnapLines( xs, ys, object->getExtent().rect().normalized() );
                        }
                }

                double tol = snapTolerancePixels / mScale;
                QPointF out = p;
                double d = 0;
                if ( bestSnap( xs, { p.x() }, tol, d ) ) out.rx() += d;
                d = 0;
                if ( bestSnap( ys, { p.y() }, tol, d ) ) out.ry() += d;
                return out;
        }


        ///
        /// Snap a moving selection box to other objects and to the label, and
        /// record the alignment lines that now match
        ///
        QRectF
        LabelEditor::snapMoveBox( const QRectF& box )
        {
                QList<SnapLine> xs, ys;
                addSnapLines( xs, ys, labelRect() );
                for ( const auto& r : otherExtents() )
                {
                        addSnapLines( xs, ys, r );
                }

                double tol = snapTolerancePixels / mScale;
                QRectF out = box;
                double d = 0;
                if ( bestSnap( xs, { box.left(), box.center().x(), box.right() }, tol, d ) )
                {
                        out.translate( d, 0 );
                }
                d = 0;
                if ( bestSnap( ys, { box.top(), box.center().y(), box.bottom() }, tol, d ) )
                {
                        out.translate( 0, d );
                }

                const double eps = 0.01;
                for ( const auto& line : xs )
                {
                        for ( double f : { out.left(), out.center().x(), out.right() } )
                        {
                                if ( qAbs( line.v - f ) < eps )
                                {
                                        mAlignGuides << QLineF( line.v, qMin( out.top(), line.src.top() ),
                                                                line.v, qMax( out.bottom(), line.src.bottom() ) );
                                }
                        }
                }
                for ( const auto& line : ys )
                {
                        for ( double f : { out.top(), out.center().y(), out.bottom() } )
                        {
                                if ( qAbs( line.v - f ) < eps )
                                {
                                        mAlignGuides << QLineF( qMin( out.left(), line.src.left() ), line.v,
                                                                qMax( out.right(), line.src.right() ), line.v );
                                }
                        }
                }

                return out;
        }


        ///
        /// Distance in the measurement units chosen in Preferences
        ///
        QString
        LabelEditor::formatDistance( double pts ) const
        {
                model::Units units = model::Settings::units();
                return QString( "%1 %2" )
                        .arg( QString::number( model::Distance::pt( pts ).inUnits( units ), 'f', units.resolutionDigits() ),
                              units.toIdString() );
        }


        namespace
        {
                /// Small filled label with white text, centered on a pixel position
                void drawPill( QPainter* painter, const QPointF& center, const QString& text )
                {
                        QFont font = painter->font();
                        font.setPointSizeF( 8 );
                        font.setBold( true );
                        painter->setFont( font );

                        QFontMetricsF fm( font );
                        QSizeF size( fm.horizontalAdvance( text ) + 8, fm.height() + 2 );
                        QRectF r( center.x() - size.width()/2, center.y() - size.height()/2,
                                  size.width(), size.height() );

                        painter->setPen( Qt::NoPen );
                        painter->setBrush( guideColor );
                        painter->drawRoundedRect( r, 3, 3 );
                        painter->setPen( guideTextColor );
                        painter->drawText( r, Qt::AlignCenter, text );
                }

                /// Dimension line with end ticks and its length label (pixel coordinates)
                void drawDimension( QPainter* painter, const QPointF& a, const QPointF& b, const QString& text )
                {
                        QPen pen( guideColor, 1 );
                        painter->setPen( pen );
                        painter->drawLine( a, b );

                        QLineF line( a, b );
                        QLineF normal = line.normalVector();
                        if ( normal.length() > 0 )
                        {
                                normal.setLength( 4 );
                                QPointF n = normal.p2() - normal.p1();
                                painter->drawLine( a - n, a + n );
                                painter->drawLine( b - n, b + n );
                        }

                        drawPill( painter, line.center(), text );
                }
        }


        ///
        /// While moving or resizing: alignment lines and the distances from the
        /// selection to the nearest object (or the label edge) on each side
        ///
        void
        LabelEditor::drawGuidesLayer( QPainter* painter )
        {
                if ( mState != ArrowMove && mState != ArrowResize )
                {
                        return;
                }
                QRectF sel = selectionExtent();
                if ( sel.isNull() )
                {
                        return;
                }

                QTransform toPx;
                toPx.scale( mScale, mScale );
                toPx.translate( mX0.pt(), mY0.pt() );

                painter->save();
                painter->resetTransform();
                painter->setRenderHint( QPainter::Antialiasing, true );

                // Alignment lines
                if ( mState == ArrowMove )
                {
                        QPen pen( guideColor, 1 );
                        painter->setPen( pen );
                        for ( const auto& line : mAlignGuides )
                        {
                                painter->drawLine( toPx.map( line ) );
                        }
                }

                // Distances on each side
                const QRectF label = labelRect();
                const auto others = otherExtents();
                const double eps = 0.001;

                struct Gap { bool found{ false }; double d{ 0 }; QPointF a; QPointF b; };
                Gap left, right, top, bottom;

                auto consider = [&]( Gap& g, double d, QPointF a, QPointF b )
                {
                        if ( d > eps && ( !g.found || d < g.d ) )
                        {
                                g = Gap{ true, d, a, b };
                        }
                };

                // Label edges (only when the selection is inside the label on that axis)
                double cy = sel.center().y();
                double cx = sel.center().x();
                if ( sel.left() >= label.left() && sel.right() <= label.right() )
                {
                        consider( left,  sel.left() - label.left(),   QPointF( label.left(), cy ),  QPointF( sel.left(), cy ) );
                        consider( right, label.right() - sel.right(), QPointF( sel.right(), cy ),   QPointF( label.right(), cy ) );
                }
                if ( sel.top() >= label.top() && sel.bottom() <= label.bottom() )
                {
                        consider( top,    sel.top() - label.top(),       QPointF( cx, label.top() ), QPointF( cx, sel.top() ) );
                        consider( bottom, label.bottom() - sel.bottom(), QPointF( cx, sel.bottom() ), QPointF( cx, label.bottom() ) );
                }

                // Nearest neighbouring objects that overlap the selection on the other axis
                for ( const auto& r : others )
                {
                        bool overlapY = r.top() < sel.bottom() && r.bottom() > sel.top();
                        bool overlapX = r.left() < sel.right() && r.right() > sel.left();
                        if ( overlapY )
                        {
                                double y = ( qMax( r.top(), sel.top() ) + qMin( r.bottom(), sel.bottom() ) ) / 2;
                                if ( r.right() <= sel.left() + eps )
                                {
                                        consider( left, sel.left() - r.right(), QPointF( r.right(), y ), QPointF( sel.left(), y ) );
                                }
                                if ( r.left() >= sel.right() - eps )
                                {
                                        consider( right, r.left() - sel.right(), QPointF( sel.right(), y ), QPointF( r.left(), y ) );
                                }
                        }
                        if ( overlapX )
                        {
                                double x = ( qMax( r.left(), sel.left() ) + qMin( r.right(), sel.right() ) ) / 2;
                                if ( r.bottom() <= sel.top() + eps )
                                {
                                        consider( top, sel.top() - r.bottom(), QPointF( x, r.bottom() ), QPointF( x, sel.top() ) );
                                }
                                if ( r.top() >= sel.bottom() - eps )
                                {
                                        consider( bottom, r.top() - sel.bottom(), QPointF( x, sel.bottom() ), QPointF( x, r.top() ) );
                                }
                        }
                }

                for ( const Gap* g : { &left, &right, &top, &bottom } )
                {
                        if ( g->found )
                        {
                                drawDimension( painter, toPx.map( g->a ), toPx.map( g->b ), formatDistance( g->d ) );
                        }
                }

                painter->restore();
        }


        ///
        /// Measure tape: the measured line, its length, and its width/height
        /// when it is diagonal
        ///
        void
        LabelEditor::drawMeasureLayer( QPainter* painter )
        {
                if ( !mMeasureVisible )
                {
                        return;
                }

                QTransform toPx;
                toPx.scale( mScale, mScale );
                toPx.translate( mX0.pt(), mY0.pt() );

                QPointF a = toPx.map( mMeasureP1 );
                QPointF b = toPx.map( mMeasureP2 );
                double dx = qAbs( mMeasureP2.x() - mMeasureP1.x() );
                double dy = qAbs( mMeasureP2.y() - mMeasureP1.y() );
                double length = std::hypot( dx, dy );

                painter->save();
                painter->resetTransform();
                painter->setRenderHint( QPainter::Antialiasing, true );

                // Width/height legs for a diagonal measurement
                bool diagonal = dx > 0.01 && dy > 0.01;
                if ( diagonal )
                {
                        QPen dashed( guideColor, 1, Qt::DashLine );
                        painter->setPen( dashed );
                        QPointF corner( b.x(), a.y() );
                        painter->drawLine( a, corner );
                        painter->drawLine( corner, b );
                }

                QPen pen( guideColor, 1.5 );
                painter->setPen( pen );
                painter->drawLine( a, b );
                painter->setBrush( guideColor );
                painter->drawEllipse( a, 3, 3 );
                painter->drawEllipse( b, 3, 3 );

                QString text = formatDistance( length );
                if ( diagonal )
                {
                        text += QString( "   W %1 × H %2" ).arg( formatDistance( dx ), formatDistance( dy ) );
                }
                QLineF line( a, b );
                QPointF at = line.length() > 60 ? line.center() : b + QPointF( 0, -14 );
                drawPill( painter, at, text );

                painter->restore();
        }


} // namespace glabels
