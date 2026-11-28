//  ModelTextObject.cpp
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


#include "ModelTextObject.hpp"

#include "merge/DataSourceUtil.hpp"

#include "Size.hpp"

#include <QBrush>
#include <QDebug>
#include <QGlyphRun>
#include <QPainter>
#include <QPen>
#include <QRawFont>
#include <QtMath>
#include <QTextDocument>
#include <QTextBlock>
#include <QRegularExpression>


//
// Private
//
namespace
{
        const double marginPts = 3;


        ///
        /// Calculate pixel size
        ///
        /// Assume a virtual DPI of 72 pixels/inch for all QPainter contexts.
        /// Ideally, we should use pointSizes for device independence, but as
        /// of Qt-6.4, the  xcb (X11) and Wayland backends will render the same
        /// font differently. Currently this function is basically the unity
        /// function (i.e. 1 pt = 1 pixel) with rounding, but is a placeholder
        /// that may need to be tweaked for different backends.
        ///
        int pixelSize( double pointSize )
        {
                const double virtual_dpi = 72;
                return qMax( 1, qRound( pointSize * virtual_dpi/72.0 ) );
        }
}


namespace glabels::model
{

        ///
        /// Constructor
        ///
        ModelTextObject::ModelTextObject()
        {
                mOutline.setOwner( this );

                mHandles.push_back( Handle( this, Handle::NW ) );
                mHandles.push_back( Handle( this, Handle::N ) );
                mHandles.push_back( Handle( this, Handle::NE ) );
                mHandles.push_back( Handle( this, Handle::E ) );
                mHandles.push_back( Handle( this, Handle::SE ) );
                mHandles.push_back( Handle( this, Handle::S ) );
                mHandles.push_back( Handle( this, Handle::SW ) );
                mHandles.push_back( Handle( this, Handle::W ) );

                mText              = "";
                mFontFamily        = "Sans";
                mFontSize          = 10;
                mFontWeight        = QFont::Normal;
                mFontItalicFlag    = false;
                mFontUnderlineFlag = false;
                mTextColorNode     = ColorNode( QColor( 0, 0, 0 ) );
                mTextHAlign        = Qt::AlignLeft;
                mTextVAlign        = Qt::AlignTop;
                mTextWrapMode      = QTextOption::WordWrap;
                mTextLineSpacing   = 1;
                mTextAutoShrink    = false;
                mTextPath          = TextPath::Straight;
                mTextLetterSpacing = 0;
                mTextPathStart     = 0;
                mTextPathOffset    = 0;
        }


        ///
        /// Constructor
        ///
        ModelTextObject::ModelTextObject( Distance              x0,
                                          Distance              y0,
                                          Distance              w,
                                          Distance              h,
                                          bool                  lockAspectRatio,
                                          const QString&        text,
                                          const QString&        fontFamily,
                                          double                fontSize,
                                          QFont::Weight         fontWeight,
                                          bool                  fontItalicFlag,
                                          bool                  fontUnderlineFlag,
                                          ColorNode             textColorNode,
                                          Qt::Alignment         textHAlign,
                                          Qt::Alignment         textVAlign,
                                          QTextOption::WrapMode textWrapMode,
                                          double                textLineSpacing,
                                          bool                  textAutoShrink,
                                          const QTransform&     matrix,
                                          bool                  shadowState,
                                          Distance              shadowX,
                                          Distance              shadowY,
                                          double                shadowOpacity,
                                          const ColorNode&      shadowColorNode )
        : ModelObject( x0,
                       y0,
                       w,
                       h,
                       lockAspectRatio,
                       matrix,
                       shadowState,
                       shadowX,
                       shadowY,
                       shadowOpacity,
                       shadowColorNode )
        {
                mOutline.setOwner( this );

                mHandles.push_back( Handle( this, Handle::NW ) );
                mHandles.push_back( Handle( this, Handle::N ) );
                mHandles.push_back( Handle( this, Handle::NE ) );
                mHandles.push_back( Handle( this, Handle::E ) );
                mHandles.push_back( Handle( this, Handle::SE ) );
                mHandles.push_back( Handle( this, Handle::S ) );
                mHandles.push_back( Handle( this, Handle::SW ) );
                mHandles.push_back( Handle( this, Handle::W ) );

                mText              = text;
                mFontFamily        = fontFamily;
                mFontSize          = fontSize;
                mFontWeight        = fontWeight;
                mFontItalicFlag    = fontItalicFlag;
                mFontUnderlineFlag = fontUnderlineFlag;
                mTextColorNode     = textColorNode;
                mTextHAlign        = textHAlign;
                mTextVAlign        = textVAlign;
                mTextWrapMode      = textWrapMode;
                mTextLineSpacing   = textLineSpacing;
                mTextAutoShrink    = textAutoShrink;
                mTextPath          = TextPath::Straight;
                mTextLetterSpacing = 0;
                mTextPathStart     = 0;
                mTextPathOffset    = 0;

                update(); // Initialize cached editor layouts
        }


        ///
        /// Copy constructor
        ///
        ModelTextObject::ModelTextObject( const ModelTextObject* object )
                : ModelObject(object)
        {
                mText              = object->mText;
                mFontFamily        = object->mFontFamily;
                mFontSize          = object->mFontSize;
                mFontWeight        = object->mFontWeight;
                mFontItalicFlag    = object->mFontItalicFlag;
                mFontUnderlineFlag = object->mFontUnderlineFlag;
                mTextColorNode     = object->mTextColorNode;
                mTextHAlign        = object->mTextHAlign;
                mTextVAlign        = object->mTextVAlign;
                mTextWrapMode      = object->mTextWrapMode;
                mTextLineSpacing   = object->mTextLineSpacing;
                mTextAutoShrink    = object->mTextAutoShrink;
                mTextPath          = object->mTextPath;
                mTextLetterSpacing = object->mTextLetterSpacing;
                mTextPathStart     = object->mTextPathStart;
                mTextPathOffset    = object->mTextPathOffset;

                update(); // Initialize cached editor layouts
        }


        ///
        /// Destructor
        ///
        ModelTextObject::~ModelTextObject()
        {
                qDeleteAll( mEditorLayouts );
                mEditorLayouts.clear();
        }


        ///
        /// Clone
        ///
        ModelTextObject* ModelTextObject::clone() const
        {
                return new ModelTextObject( this );
        }


        ///
        /// Text Property Getter
        ///
        QString ModelTextObject::text() const
        {
                return mText.toString();
        }


        ///
        /// Text Property Setter
        ///
        void ModelTextObject::setText( const QString& value )
        {
                if ( mText.toString() != value )
                {
                        mText = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// FontFamily Property Getter
        ///
        QString ModelTextObject::fontFamily() const
        {
                return mFontFamily;
        }


        ///
        /// FontFamily Property Setter
        ///
        void ModelTextObject::setFontFamily( const QString& value )
        {
                if ( mFontFamily != value )
                {
                        mFontFamily = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// FontSize Property Getter
        ///
        double ModelTextObject::fontSize() const
        {
                return mFontSize;
        }


        ///
        /// FontSize Property Setter
        ///
        void ModelTextObject::setFontSize( double value )
        {
                if ( mFontSize != value )
                {
                        mFontSize = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// FontWeight Property Getter
        ///
        QFont::Weight ModelTextObject::fontWeight() const
        {
                return mFontWeight;
        }


        ///
        /// FontWeight Property Setter
        ///
        void ModelTextObject::setFontWeight( QFont::Weight value )
        {
                if ( mFontWeight != value )
                {
                        mFontWeight = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// FontItalicFlag Property Getter
        ///
        bool ModelTextObject::fontItalicFlag() const
        {
                return mFontItalicFlag;
        }


        ///
        /// FontItalicFlag Property Setter
        ///
        void ModelTextObject::setFontItalicFlag( bool value )
        {
                if ( mFontItalicFlag != value )
                {
                        mFontItalicFlag = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// FontUnderlineFlag Property Getter
        ///
        bool ModelTextObject::fontUnderlineFlag() const
        {
                return mFontUnderlineFlag;
        }


        ///
        /// FontUnderlineFlag Property Setter
        ///
        void ModelTextObject::setFontUnderlineFlag( bool value )
        {
                if ( mFontUnderlineFlag != value )
                {
                        mFontUnderlineFlag = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// Text Color Node Property Getter
        ///
        ColorNode ModelTextObject::textColorNode() const
        {
                return mTextColorNode;
        }


        ///
        /// Text Color Node Property Setter
        ///
        void ModelTextObject::setTextColorNode( const ColorNode& value )
        {
                if ( mTextColorNode != value )
                {
                        mTextColorNode = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextHAlign Property Getter
        ///
        Qt::Alignment ModelTextObject::textHAlign() const
        {
                return mTextHAlign;
        }


        ///
        /// TextHAlign Property Setter
        ///
        void ModelTextObject::setTextHAlign( Qt::Alignment value )
        {
                if ( mTextHAlign != value )
                {
                        mTextHAlign = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextVAlign Property Getter
        ///
        Qt::Alignment ModelTextObject::textVAlign() const
        {
                return mTextVAlign;
        }


        ///
        /// TextVAlign Property Setter
        ///
        void ModelTextObject::setTextVAlign( Qt::Alignment value )
        {
                if ( mTextVAlign != value )
                {
                        mTextVAlign = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// Text Wrap Mode Property Getter
        ///
        QTextOption::WrapMode ModelTextObject::textWrapMode() const
        {
                return mTextWrapMode;
        }


        ///
        /// Text Wrap Mode Property Setter
        ///
        void ModelTextObject::setTextWrapMode( QTextOption::WrapMode value )
        {
                if ( mTextWrapMode != value )
                {
                        mTextWrapMode = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextLineSpacing Property Getter
        ///
        double ModelTextObject::textLineSpacing() const
        {
                return mTextLineSpacing;
        }


        ///
        /// TextLineSpacing Property Setter
        ///
        void ModelTextObject::setTextLineSpacing( double value )
        {
                if ( mTextLineSpacing != value )
                {
                        mTextLineSpacing = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextAutoShrink Property Getter
        ///
        bool ModelTextObject::textAutoShrink() const
        {
                return mTextAutoShrink;
        }


        ///
        /// TextAutoShrink Property Setter
        ///
        void ModelTextObject::setTextAutoShrink( bool value )
        {
                if ( mTextAutoShrink != value )
                {
                        mTextAutoShrink = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextPath Property Getter
        ///
        TextPath ModelTextObject::textPath() const
        {
                return mTextPath;
        }


        ///
        /// TextPath Property Setter
        ///
        void ModelTextObject::setTextPath( TextPath value )
        {
                if ( mTextPath != value )
                {
                        mTextPath = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextLetterSpacing Property Getter
        ///
        double ModelTextObject::textLetterSpacing() const
        {
                return mTextLetterSpacing;
        }


        ///
        /// TextLetterSpacing Property Setter
        ///
        void ModelTextObject::setTextLetterSpacing( double value )
        {
                if ( mTextLetterSpacing != value )
                {
                        mTextLetterSpacing = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextPathStart Property Getter/Setter
        ///
        double ModelTextObject::textPathStart() const
        {
                return mTextPathStart;
        }


        void ModelTextObject::setTextPathStart( double value )
        {
                if ( mTextPathStart != value )
                {
                        mTextPathStart = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// TextPathOffset Property Getter/Setter
        ///
        double ModelTextObject::textPathOffset() const
        {
                return mTextPathOffset;
        }


        void ModelTextObject::setTextPathOffset( double value )
        {
                if ( mTextPathOffset != value )
                {
                        mTextPathOffset = value;
                        update();
                        emit changed();
                }
        }


        ///
        /// NaturalSize Property Getter
        ///
        Size ModelTextObject::naturalSize() const
        {
                QFont font = makeFont( mFontSize );

                QTextOption textOption;
                textOption.setAlignment( mTextHAlign | Qt::AlignAbsolute ); // honor L/C/R literally, also for RTL (Urdu, Arabic) text
                textOption.setWrapMode( mTextWrapMode );

                QFontMetricsF fontMetrics( font );
                double dy = fontMetrics.lineSpacing() * mTextLineSpacing;

                QString displayText = mText.isEmpty() ? tr("Text") : merge::DataSourceUtil::toDisplay( mText.toString() );
                QTextDocument document( displayText );

                // Do layouts
                double x = 0;
                double y = 0;
                QRectF boundingRect;
                for ( int i = 0; i < document.blockCount(); i++ )
                {
                        QTextLayout layout( document.findBlockByNumber(i).text() );

                        layout.setFont( font );
                        layout.setTextOption( textOption );
                        layout.setCacheEnabled(true);

                        layout.beginLayout();
                        for ( QTextLine l = layout.createLine(); l.isValid(); l = layout.createLine() )
                        {
                                l.setPosition( QPointF( x, y ) );
                                y += dy;
                        }
                        layout.endLayout();

                        boundingRect = layout.boundingRect().united( boundingRect );
                }

                return Size( boundingRect.width() + 2*marginPts, boundingRect.height() + 2*marginPts );
        }


        ///
        /// Size the box needs to show its text at the set font size (no shrinking).
        /// Wrapped text keeps the box width and only needs more height.
        ///
        Size ModelTextObject::fitSize() const
        {
                if ( mTextWrapMode == QTextOption::NoWrap )
                {
                        return naturalSize();
                }

                QFont font = makeFont( mFontSize );

                QTextOption textOption;
                textOption.setAlignment( mTextHAlign | Qt::AlignAbsolute );
                textOption.setWrapMode( mTextWrapMode );

                QFontMetricsF fontMetrics( font );
                double dy = fontMetrics.lineSpacing() * mTextLineSpacing;
                double lineWidth = std::max( 1.0, mW.pt() - 2*marginPts );

                QString displayText = mText.isEmpty() ? tr("Text") : merge::DataSourceUtil::toDisplay( mText.toString() );
                QTextDocument document( displayText );

                double y = 0;
                QRectF boundingRect;
                for ( int i = 0; i < document.blockCount(); i++ )
                {
                        QTextLayout layout( document.findBlockByNumber(i).text() );
                        layout.setFont( font );
                        layout.setTextOption( textOption );

                        layout.beginLayout();
                        for ( QTextLine l = layout.createLine(); l.isValid(); l = layout.createLine() )
                        {
                                l.setLineWidth( lineWidth );
                                l.setPosition( QPointF( 0, y ) );
                                y += dy;
                        }
                        layout.endLayout();

                        boundingRect = layout.boundingRect().united( boundingRect );
                }

                return Size( mW, Distance::pt( boundingRect.height() + 2*marginPts ) );
        }


        ///
        /// Can Text Capability Implementation
        ///
        bool ModelTextObject::canText() const
        {
                return true;
        }


        ///
        /// Draw shadow of object
        ///
        void ModelTextObject::drawShadow( QPainter*            painter,
                                          bool                 inEditor,
                                          const merge::Record& record,
                                          const Variables&     variables ) const
        {
                QColor textColor = mTextColorNode.color( record, variables );

                if ( textColor.alpha() )
                {
                        QColor shadowColor = mShadowColorNode.color( record, variables );
                        shadowColor.setAlphaF( mShadowOpacity );

                        if ( inEditor )
                        {
                                drawTextInEditor( painter, shadowColor, false );
                        }
                        else
                        {
                                drawText( painter, shadowColor, record, variables );
                        }
                }
        }


        ///
        /// Draw object itself
        ///
        void ModelTextObject::drawObject( QPainter*            painter,
                                          bool                 inEditor,
                                          const merge::Record& record,
                                          const Variables&     variables ) const
        {
                QColor textColor = mTextColorNode.color( record, variables );

                if ( inEditor )
                {
                        drawTextInEditor( painter, textColor );
                }
                else
                {
                        drawText( painter, textColor, record, variables );
                }
        }


        ///
        /// Path to test for hover condition
        ///
        QPainterPath ModelTextObject::hoverPath( double scale ) const
        {
                return mHoverPath;
        }


        ///
        /// Size updated
        ///
        void ModelTextObject::sizeUpdated()
        {
                update();
        }


        ///
        /// Update cached information for editor view
        ///
        void ModelTextObject::update()
        {
                QFont font = makeFont( mFontSize );

                QTextOption textOption;
                textOption.setAlignment( mTextHAlign | Qt::AlignAbsolute ); // honor L/C/R literally, also for RTL (Urdu, Arabic) text
                textOption.setWrapMode( mTextWrapMode );

                QFontMetricsF fontMetrics( font );
                double dy = fontMetrics.lineSpacing() * mTextLineSpacing;

                QString displayText = mText.isEmpty() ? tr("Text") : merge::DataSourceUtil::toDisplay( mText.toString() );
                QTextDocument document( displayText );

                qDeleteAll( mEditorLayouts );
                mEditorLayouts.clear();
                mEditorPills.clear();

                static const QRegularExpression pillRe( "\\{\\{[^{}]+\\}\\}" );

                // Pass #1 -- do initial layouts
                double x = 0;
                double y = 0;
                QRectF boundingRect;
                for ( int i = 0; i < document.blockCount(); i++ )
                {
                        QTextLayout* layout = new QTextLayout( document.findBlockByNumber(i).text() );

                        layout->setFont( font );
                        layout->setTextOption( textOption );
                        layout->setCacheEnabled(true);

                        layout->beginLayout();
                        for ( QTextLine l = layout->createLine(); l.isValid(); l = layout->createLine() )
                        {
                                l.setLineWidth( mW.pt() - 2*marginPts );
                                l.setPosition( QPointF( x, y ) );
                                y += dy;
                        }
                        layout->endLayout();

                        mEditorLayouts.append( layout );

                        QList<QPair<int,int>> pills;
                        if ( !mText.isEmpty() )
                        {
                                auto it = pillRe.globalMatch( layout->text() );
                                while ( it.hasNext() )
                                {
                                        auto m = it.next();
                                        pills.append( qMakePair( int( m.capturedStart() ), int( m.capturedLength() ) ) );
                                }
                        }
                        mEditorPills.append( pills );

                        boundingRect = layout->boundingRect().united( boundingRect );
                }
                double h = boundingRect.height();


                // Pass #2 -- adjust layout positions for vertical alignment and create hover path
                x = marginPts;
                switch ( mTextVAlign )
                {
                case Qt::AlignVCenter:
                        y = mH.pt()/2 - h/2;
                        break;
                case Qt::AlignBottom:
                        y = mH.pt() - h - marginPts;
                        break;
                default:
                        y = marginPts;
                        break;
                }
                QPainterPath hoverPath; // new empty hover path
                for ( QTextLayout* layout : mEditorLayouts )
                {
                        for ( int j = 0; j < layout->lineCount(); j++ )
                        {
                                QTextLine l = layout->lineAt(j);
                                l.setPosition( QPointF( x, y ) );
                                y += dy;

                                hoverPath.addRect( l.naturalTextRect() ); // add to new hover path
                        }
                }

                if ( mTextPath != TextPath::Straight )
                {
                        // Curved text can be anywhere in the box
                        hoverPath = QPainterPath();
                        hoverPath.addRect( QRectF( 0, 0, mW.pt(), mH.pt() ) );
                }

                mHoverPath = hoverPath; // save new hover path
        }


        ///
        /// Draw text in editor from cached information
        ///
        void ModelTextObject::drawTextInEditor( QPainter* painter, const QColor& color, bool drawPills ) const
        {
                painter->save();

                if ( mTextPath == TextPath::Straight )
                {
                        painter->setClipRect( QRectF( 0, 0, mW.pt(), mH.pt() ) );   // curved text may be moved out past the box
                }

                if ( mText.isEmpty() )
                {
                        QColor mutedColor = color;
                        mutedColor.setAlphaF( 0.5 * color.alphaF() );
                        painter->setPen( QPen( mutedColor ) );
                }
                else
                {
                        painter->setPen( QPen( color ) );
                }

                if ( mTextPath != TextPath::Straight )
                {
                        QString displayText = mText.isEmpty() ? tr("Text")
                                                              : merge::DataSourceUtil::toDisplay( mText.toString() );
                        drawTextOnPath( painter, painter->pen().color(), displayText, mFontSize );
                        painter->restore();
                        return;
                }

                // Variable placeholders are drawn as rounded "pills" behind the text
                if ( drawPills )
                {
                        QColor pillColor( 52, 101, 164 );
                        QColor fill = pillColor;
                        fill.setAlpha( 38 );
                        QColor border = pillColor;
                        border.setAlpha( 150 );
                        QPen pen( border, 0.6 );
                        painter->setBrush( fill );

                        for ( int i = 0; i < mEditorLayouts.size() && i < mEditorPills.size(); i++ )
                        {
                                QTextLayout* layout = mEditorLayouts[i];
                                for ( const auto& pill : mEditorPills[i] )
                                {
                                        int start = pill.first;
                                        int end   = pill.first + pill.second;
                                        for ( int j = 0; j < layout->lineCount(); j++ )
                                        {
                                                QTextLine l = layout->lineAt( j );
                                                int ls = l.textStart();
                                                int le = ls + l.textLength();
                                                if ( end <= ls || start >= le )
                                                {
                                                        continue;
                                                }
                                                qreal x1 = l.cursorToX( qMax( start, ls ) );
                                                qreal x2 = l.cursorToX( qMin( end, le ) );
                                                QRectF r( qMin( x1, x2 ), l.y(), qAbs( x2 - x1 ), l.height() );
                                                r.adjust( -0.8, 0.4, 0.8, -0.4 );
                                                qreal radius = r.height() / 3.0;
                                                painter->save();
                                                painter->setPen( pen );
                                                painter->drawRoundedRect( r, radius, radius );
                                                painter->restore();
                                        }
                                }
                        }
                        painter->setBrush( Qt::NoBrush );
                }

                for ( QTextLayout* layout : mEditorLayouts )
                {
                        layout->draw( painter, QPointF( 0, 0 ) );
                }

                painter->restore();
        }


        ///
        /// Draw text in final printout or preview
        ///
        void
        ModelTextObject::drawText( QPainter*            painter,
                                   const QColor&        color,
                                   const merge::Record& record,
                                   const Variables&     variables ) const
        {
                painter->save();

                if ( mTextPath == TextPath::Straight )
                {
                        painter->setClipRect( QRectF( 0, 0, mW.pt(), mH.pt() ) );   // curved text may be moved out past the box
                }

                if ( mTextPath != TextPath::Straight )
                {
                        drawTextOnPath( painter, color, mText.expand( record, variables ),
                                        mTextAutoShrink ? autoShrinkFontSize( record, variables ) : mFontSize );
                        painter->restore();
                        return;
                }

                QFont font = makeFont( mTextAutoShrink ? autoShrinkFontSize( record, variables ) : mFontSize );

                QTextOption textOption;
                textOption.setAlignment( mTextHAlign | Qt::AlignAbsolute ); // honor L/C/R literally, also for RTL (Urdu, Arabic) text
                textOption.setWrapMode( mTextWrapMode );

                QFontMetricsF fontMetrics( font );
                double dy = fontMetrics.lineSpacing() * mTextLineSpacing;

                QTextDocument document( mText.expand( record, variables ) );

                QList<QTextLayout*> layouts;

                // Pass #1 -- do initial layouts
                double x = 0;
                double y = 0;
                QRectF boundingRect;
                for ( int i = 0; i < document.blockCount(); i++ )
                {
                        QTextLayout* layout = new QTextLayout( document.findBlockByNumber(i).text() );

                        layout->setFont( font );
                        layout->setTextOption( textOption );
                        layout->setCacheEnabled(true);

                        layout->beginLayout();
                        for ( QTextLine l = layout->createLine(); l.isValid(); l = layout->createLine() )
                        {
                                l.setLineWidth( mW.pt() - 2*marginPts );
                                l.setPosition( QPointF( x, y ) );
                                y += dy;
                        }
                        layout->endLayout();

                        layouts.append( layout );

                        boundingRect = layout->boundingRect().united( boundingRect );
                }
                double h = boundingRect.height();


                // Pass #2 -- adjust layout positions for vertical alignment
                x = marginPts;
                switch ( mTextVAlign )
                {
                case Qt::AlignVCenter:
                        y = mH.pt()/2 - h/2;
                        break;
                case Qt::AlignBottom:
                        y = mH.pt() - h - marginPts;
                        break;
                default:
                        y = marginPts;
                        break;
                }
                for ( QTextLayout* layout : layouts )
                {
                        for ( int j = 0; j < layout->lineCount(); j++ )
                        {
                                QTextLine l = layout->lineAt(j);
                                l.setPosition( QPointF( x, y ) );
                                y += dy;
                        }
                }

                // Draw layouts
                painter->setPen( QPen( color ) );
                for ( QTextLayout* layout : layouts )
                {
                        layout->draw( painter, QPointF( 0, 0 ) );
                }

                // Cleanup
                qDeleteAll( layouts );

                painter->restore();
        }


        ///
        /// Determine auto shrink font size
        ///
        double
        ModelTextObject::autoShrinkFontSize( const merge::Record& record,
                                             const Variables&     variables ) const
        {
                if ( mTextPath != TextPath::Straight )
                {
                        // Shrink until the text fits along the path
                        QString text = mText.expand( record, variables );
                        double candidateSize = mFontSize;
                        while ( candidateSize > 1.0 )
                        {
                                QFont font = makeFont( candidateSize );
                                double fitLength;
                                bool   closed;
                                baselinePath( QFontMetricsF( font ), fitLength, closed );
                                if ( pathTextWidth( text, font ) <= fitLength )
                                {
                                        break;
                                }
                                candidateSize -= 0.5;
                        }
                        return candidateSize;
                }

                QFont font;
                font.setFamily( mFontFamily );
                font.setWeight( mFontWeight );
                font.setItalic( mFontItalicFlag );
                font.setUnderline( mFontUnderlineFlag );
                font.setLetterSpacing( QFont::AbsoluteSpacing, mTextLetterSpacing );

                QTextOption textOption;
                textOption.setAlignment( mTextHAlign | Qt::AlignAbsolute ); // honor L/C/R literally, also for RTL (Urdu, Arabic) text
                textOption.setWrapMode( mTextWrapMode );

                QTextDocument document( mText.expand( record, variables ) );

                double candidateSize = mFontSize;
                while ( candidateSize > 1.0 )
                {
                        font.setPixelSize( pixelSize( candidateSize ) );

                        // Line spacing is affected by font size
                        QFontMetricsF fontMetrics( font );
                        double dy = fontMetrics.lineSpacing() * mTextLineSpacing;

                        // Do candidate layouts, letting text flow according to wrap mode
                        double x = 0;
                        double y = 0;
                        QRectF layoutsRect;
                        for ( int i = 0; i < document.blockCount(); i++ )
                        {
                                QTextLayout layout( document.findBlockByNumber(i).text() );

                                layout.setFont( font );
                                layout.setTextOption( textOption );
                                layout.setCacheEnabled(true);

                                layout.beginLayout();
                                for ( QTextLine l = layout.createLine(); l.isValid(); l = layout.createLine() )
                                {
                                        l.setLineWidth( mW.pt() - 2*marginPts );
                                        l.setPosition( QPointF( x, y ) );
                                        y += dy;
                                }
                                layout.endLayout();

                                layoutsRect = layout.boundingRect().united( layoutsRect );
                        }

                        // Did this candidate fit in our object's bounding box?
                        if ( ( (layoutsRect.width() + 2*marginPts) <= mW.pt() ) &&
                             ( (layoutsRect.height() + 2*marginPts) <= mH.pt() ) )
                        {
                                break;
                        }

                        // If not, let's try a slightly smaller font size
                        candidateSize -= 0.5;
                }

                return candidateSize;
        }



        ///
        /// Font for the given size, with all of this object's font settings
        ///
        QFont ModelTextObject::makeFont( double fontSize ) const
        {
                QFont font;
                font.setFamily( mFontFamily );
                font.setPixelSize( pixelSize( fontSize ) );
                font.setWeight( mFontWeight );
                font.setItalic( mFontItalicFlag );
                font.setUnderline( mFontUnderlineFlag );
                font.setLetterSpacing( QFont::AbsoluteSpacing, mTextLetterSpacing );
                return font;
        }


        ///
        /// Baseline the letters sit on, fitted inside the object's box so the
        /// letters themselves stay inside it (moved in or out by the path offset).
        ///
        /// Closed paths (arcs, circle, rectangle) start at the point the text is
        /// aligned to: the left end of an arc, or the top of a circle or rectangle.
        /// "fitLength" is the length text can fill: half the ellipse for an arc.
        ///
        QPainterPath ModelTextObject::baselinePath( const QFontMetricsF& fm, double& fitLength, bool& closed ) const
        {
                const double w = mW.pt();
                const double h = mH.pt();
                const double ascent  = fm.ascent();
                const double descent = fm.descent();

                auto insetRect = [&]( double inset )
                {
                        double rw = qMax( 1.0, w - 2*inset );
                        double rh = qMax( 1.0, h - 2*inset );
                        return QRectF( w/2 - rw/2, h/2 - rh/2, rw, rh );
                };

                QPainterPath path;
                closed = true;
                switch ( mTextPath )
                {
                case TextPath::ArcTop:
                {
                        // Clockwise from the left: left to right over the top, letters standing on the curve
                        QRectF r = insetRect( marginPts + ascent + mTextPathOffset );
                        path.arcMoveTo( r, 180 );
                        path.arcTo( r, 180, -360 );
                        fitLength = path.length() / 2;
                        break;
                }
                case TextPath::ArcBottom:
                {
                        // Counter-clockwise from the left: left to right under the bottom, letters hanging from it
                        QRectF r = insetRect( marginPts + descent + mTextPathOffset );
                        path.arcMoveTo( r, 180 );
                        path.arcTo( r, 180, 360 );
                        fitLength = path.length() / 2;
                        break;
                }
                case TextPath::Circle:
                {
                        // All the way round, clockwise from the top
                        QRectF r = insetRect( marginPts + ascent + mTextPathOffset );
                        path.arcMoveTo( r, 90 );
                        path.arcTo( r, 90, -360 );
                        fitLength = path.length();
                        break;
                }
                case TextPath::Rectangle:
                {
                        // Round the edges, clockwise from the middle of the top
                        QRectF r = insetRect( marginPts + ascent + mTextPathOffset );
                        path.moveTo( r.center().x(), r.top() );
                        path.lineTo( r.topRight() );
                        path.lineTo( r.bottomRight() );
                        path.lineTo( r.bottomLeft() );
                        path.lineTo( r.topLeft() );
                        path.closeSubpath();
                        fitLength = path.length();
                        break;
                }
                case TextPath::Wave:
                {
                        // One sine wave across the box, centered vertically
                        closed = false;
                        double x0  = marginPts;
                        double x1  = qMax( x0 + 1, w - marginPts );
                        double yc  = h/2 + ( ascent - descent )/2;
                        double amp = qMax( 0.0, h/2 - marginPts - ( ascent + descent )/2 );
                        const int n = 96;
                        for ( int i = 0; i <= n; i++ )
                        {
                                double t = double( i ) / n;
                                QPointF p( x0 + t*( x1 - x0 ), yc - amp*qSin( 2*M_PI*t ) );
                                if ( i == 0 )
                                {
                                        path.moveTo( p );
                                }
                                else
                                {
                                        path.lineTo( p );
                                }
                        }
                        fitLength = path.length();
                        break;
                }
                default:
                        closed = false;
                        path.moveTo( marginPts, h/2 );
                        path.lineTo( qMax( marginPts + 1, w - marginPts ), h/2 );
                        fitLength = path.length();
                        break;
                }

                return path;
        }


        ///
        /// Width of the text set on one line (lines are joined with spaces)
        ///
        double ModelTextObject::pathTextWidth( const QString& text, const QFont& font ) const
        {
                QString oneLine = text;
                oneLine.replace( '\n', ' ' );

                QTextLayout layout( oneLine, font );
                layout.beginLayout();
                QTextLine line = layout.createLine();
                double width = 0;
                if ( line.isValid() )
                {
                        line.setPosition( QPointF( 0, 0 ) );
                        width = line.naturalTextWidth();
                }
                layout.endLayout();
                return width;
        }


        ///
        /// Draw text along the object's text path, one glyph at a time
        ///
        void ModelTextObject::drawTextOnPath( QPainter*      painter,
                                              const QColor&  color,
                                              const QString& text,
                                              double         fontSize ) const
        {
                QFont font = makeFont( fontSize );
                QFontMetricsF fm( font );

                double fitLength;
                bool   closed;
                QPainterPath path = baselinePath( fm, fitLength, closed );
                const double length = path.length();
                if ( length <= 0 )
                {
                        return;
                }

                QString oneLine = text;
                oneLine.replace( '\n', ' ' );

                QTextLayout layout( oneLine, font );
                layout.setCacheEnabled( true );
                layout.beginLayout();
                QTextLine line = layout.createLine();
                if ( !line.isValid() )
                {
                        layout.endLayout();
                        return;
                }
                line.setPosition( QPointF( 0, 0 ) );
                layout.endLayout();

                const double textWidth = line.naturalTextWidth();
                const double baselineY = line.ascent();

                // Where the text begins along the path
                double offset = 0;
                bool centeredOnStart = ( mTextPath == TextPath::Circle || mTextPath == TextPath::Rectangle );
                if ( centeredOnStart )
                {
                        // The path starts at the top: center means centered on the top
                        if ( mTextHAlign & Qt::AlignHCenter )
                        {
                                offset = -textWidth/2;
                        }
                        else if ( mTextHAlign & Qt::AlignRight )
                        {
                                offset = -textWidth;
                        }
                }
                else
                {
                        if ( mTextHAlign & Qt::AlignHCenter )
                        {
                                offset = ( fitLength - textWidth )/2;
                        }
                        else if ( mTextHAlign & Qt::AlignRight )
                        {
                                offset = fitLength - textWidth;
                        }
                }

                // Start angle: turn the text clockwise round a closed path
                if ( closed && mTextPathStart != 0 )
                {
                        double shift = mTextPathStart / 360.0 * length;
                        offset += ( mTextPath == TextPath::ArcBottom ) ? -shift : shift;   // bottom arc runs counter-clockwise
                }

                // Point and direction (degrees, counter-clockwise) at a distance along the path;
                // round a closed path, or carry on in a straight line beyond the ends of an open one
                auto placeAt = [&]( double s, QPointF& point, double& angle )
                {
                        if ( closed )
                        {
                                s = std::fmod( s, length );
                                if ( s < 0 )
                                {
                                        s += length;
                                }
                        }
                        double extra = 0;
                        if ( s < 0 )
                        {
                                extra = s;
                                s = 0;
                        }
                        else if ( s > length )
                        {
                                extra = s - length;
                                s = length;
                        }
                        double t = path.percentAtLength( s );
                        point = path.pointAtPercent( t );
                        angle = path.angleAtPercent( t );
                        if ( extra != 0 )
                        {
                                double a = qDegreesToRadians( angle );
                                point += QPointF( extra*qCos( a ), -extra*qSin( a ) );
                        }
                };

                painter->setPen( QPen( color ) );

                const auto runs = layout.glyphRuns();
                for ( const QGlyphRun& run : runs )
                {
                        QRawFont rawFont = run.rawFont();
                        const QVector<quint32> indexes   = run.glyphIndexes();
                        const QVector<QPointF> positions = run.positions();
                        const QVector<QPointF> advances  = rawFont.advancesForGlyphIndexes( indexes );

                        for ( int i = 0; i < indexes.size(); i++ )
                        {
                                double advance = advances.value( i ).x();
                                double center  = positions[i].x() + advance/2;

                                QPointF point;
                                double  angle;
                                placeAt( offset + center, point, angle );

                                QGlyphRun glyph;
                                glyph.setRawFont( rawFont );
                                glyph.setGlyphIndexes( { indexes[i] } );
                                glyph.setPositions( { QPointF( -advance/2, positions[i].y() - baselineY ) } );

                                painter->save();
                                painter->translate( point );
                                painter->rotate( -angle );
                                painter->drawGlyphRun( QPointF( 0, 0 ), glyph );
                                painter->restore();
                        }
                }
        }

}
