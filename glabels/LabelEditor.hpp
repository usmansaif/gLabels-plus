//  LabelEditor.hpp
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

#ifndef LabelEditor_hpp
#define LabelEditor_hpp


#include "model/Handle.hpp"
#include "model/Model.hpp"
#include "model/ModelObject.hpp"
#include "model/Region.hpp"

#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QScrollArea>
#include <QTextEdit>
#include <QWidget>


namespace glabels
{

        // Forward References
        class UndoRedoModel;


        ///
        /// LabelEditor Widget
        ///
        class LabelEditor : public QWidget
        {
                Q_OBJECT

                /////////////////////////////////////
                // Lifecycle
                /////////////////////////////////////
        public:
                LabelEditor( QScrollArea* scrollArea, QWidget* parent = nullptr );


                /////////////////////////////////////
                // Signals
                /////////////////////////////////////
        signals:
                void contextMenuActivate( model::Point p );
                void zoomChanged();
                void pointerMoved( model::Point p );
                void pointerExited();
                void modeChanged();
                void viewChanged();
                void previewRecordChanged();


                /////////////////////////////////////
                // Parameters
                /////////////////////////////////////
        public:
                double zoom() const;
                double scale() const;
                bool markupVisible() const;
                bool qridVisible() const;

                /// Position of the label origin (0,0) in widget pixel coordinates
                QPointF labelOriginPixels() const;
                model::Distance labelWidth() const;
                model::Distance labelHeight() const;


                /////////////////////////////////////
                // Model
                /////////////////////////////////////
        public:
                void setModel( model::Model* model, UndoRedoModel* undoRedoModel );


                /////////////////////////////////////
                // Visibility operations
                /////////////////////////////////////
        public:
                void setGridVisible( bool visibleFlag );
                void setMarkupVisible( bool visibleFlag );


                /////////////////////////////////////
                // Zoom operations
                /////////////////////////////////////
        public:
                void zoomIn();
                void zoomOut();
                void zoom1To1();
                void zoomToFit();
                bool isZoomMax() const;
                bool isZoomMin() const;
                void setZoom( double zoom );
                static QList<double> standardZoomLevels();
        private:
                void setZoomReal( double zoom, bool zoomToFitFlag );


                /////////////////////////////////////
                // Mode operations
                /////////////////////////////////////
        public:
                void arrowMode();
                void createBoxMode();
                void createEllipseMode();
                void createLineMode();
                void createImageMode();
                void createTextMode();
                void createBarcodeMode();
                void measureMode();


                /////////////////////////////////////
                // Merge record preview
                /////////////////////////////////////
        public:
                int previewRecord() const;
                int nPreviewRecords() const;
                void setPreviewRecord( int index ); // -1 = show field names

                /// Add a text, barcode or image object bound to a data variable
                /// (at the center of the label, or at p when given)
                void addVariableObject( const QString& name, const QString& type );
                void addVariableObject( const QString& name, const QString& type, const model::Point& p );


                /////////////////////////////////////
                // In-place text editing
                /////////////////////////////////////
        public:
                bool isTextEditActive() const;
                void beginTextEdit( model::ModelObject* object );
                void endTextEdit( bool commit );
        private:
                void positionTextEditor();


                /////////////////////////////////////
                // Event handlers
                /////////////////////////////////////
        protected:
                void resizeEvent( QResizeEvent* event ) override;
                void wheelEvent( QWheelEvent* event ) override;
                void mousePressEvent( QMouseEvent* event ) override;
                void mouseMoveEvent( QMouseEvent* event ) override;
                void mouseReleaseEvent( QMouseEvent* event ) override;
                void mouseDoubleClickEvent( QMouseEvent* event ) override;
                bool eventFilter( QObject* watched, QEvent* event ) override;
                void leaveEvent( QEvent* event ) override;
                void keyPressEvent( QKeyEvent* event ) override;
                void paintEvent( QPaintEvent* event ) override;
                void dragEnterEvent( QDragEnterEvent *event ) override;
                void dragMoveEvent( QDragMoveEvent *event ) override;
                void dropEvent( QDropEvent *event ) override;


                /////////////////////////////////////
                // Private methods
                /////////////////////////////////////
        private:
                void handleResizeMotion( model::Distance xWorld,
                                         model::Distance yWorld );

                void showDragInfo( const QPoint& pos );
                void hideDragInfo();

                void drawBgLayer( QPainter* painter );
                void drawGridLayer( QPainter* painter );
                void drawMarkupLayer( QPainter* painter );
                void drawObjectsLayer( QPainter* painter );
                void drawFgLayer( QPainter* painter );
                void drawHighlightLayer( QPainter* painter );
                void drawSelectRegionLayer( QPainter* painter );
                void drawGuidesLayer( QPainter* painter );
                void drawMeasureLayer( QPainter* painter );

                /* Smart guides and measuring */
                QRectF        selectionExtent() const;
                QList<QRectF> otherExtents() const;
                QRectF        labelRect() const;
                QPointF       snapMeasurePoint( const QPointF& p ) const;
                QRectF        snapMoveBox( const QRectF& box );
                QString       formatDistance( double pts ) const;


                /////////////////////////////////////
                // Private slots
                /////////////////////////////////////
        private slots:
                void onSettingsChanged();
                void onModelSizeChanged();
                void onMergeChanged();


                /////////////////////////////////////
                // Private data
                /////////////////////////////////////
        private:
                enum State {
                        IdleState,
                        ArrowSelectRegion,
                        ArrowMove,
                        ArrowResize,
                        CreateIdle,
                        CreateDrag,
                        MeasureIdle,
                        MeasureDrag
                };

                enum CreateType {
                        Box,
                        Ellipse,
                        Line,
                        Image,
                        Text,
                        Barcode
                };

                QScrollArea*         mScrollArea;

                model::Model*        mModel;
                UndoRedoModel*       mUndoRedoModel;

                double               mZoom;
                int                  mWheelAccum{ 0 };
                bool                 mZoomToFitFlag;
                double               mScale;
                model::Distance      mX0;
                model::Distance      mY0;

                bool                 mMarkupVisible;
                bool                 mGridVisible;

                double               mGridSpacing;
                model::Distance      mStepSize;

                State                mState;

                /* ArrowSelectRegion state */
                bool                 mSelectRegionVisible;
                model::Region        mSelectRegion;

                /* ArrowMove state */
                model::Distance      mMoveLastX;
                model::Distance      mMoveLastY;
                QPointF              mMoveStart;      // pointer at drag start (pt)
                QRectF               mMoveStartBox;   // selection extent at drag start (pt)
                QList<QLineF>        mAlignGuides;    // alignment lines to draw (pt)

                /* Measure tool */
                bool                 mMeasureVisible{ false };
                QPointF              mMeasureP1;
                QPointF              mMeasureP2;

                /* ArrowResize state */
                model::ModelObject*     mResizeObject;
                model::Handle::Location mResizeHandleLocation;
                bool                    mResizeHonorAspect;

                /* CreateDrag state */
                CreateType           mCreateObjectType;
                model::ModelObject*  mCreateObject;
                model::Distance      mCreateX0;
                model::Distance      mCreateY0;

                /* Merge record preview */
                int                  mPreviewRecord{ -1 };

                /* In-place text editing */
                QPointer<QTextEdit>          mTextEditor;
                QPointer<model::ModelObject> mTextEditObject;
                bool                         mTextEditIsBarcode{ false };
                bool                         mTextEditEnding{ false };

                /* Position/size readout that follows the cursor while dragging */
                QLabel*                      mDragInfo{ nullptr };

        };

}


#endif // LabelEditor_hpp
