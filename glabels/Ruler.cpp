//  Ruler.cpp
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

#include "Ruler.hpp"

#include "LabelEditor.hpp"

#include "model/Settings.hpp"

#include <QPainter>
#include <QPaintEvent>

#include <cmath>


namespace
{
        const int RULER_THICKNESS = 22;
        const double MIN_MAJOR_SPACING_PX = 56.0;
}


namespace glabels
{

        Ruler::Ruler( Qt::Orientation orientation, LabelEditor* editor, QWidget* parent )
                : QWidget(parent), mOrientation(orientation), mEditor(editor)
        {
                if ( mOrientation == Qt::Horizontal )
                {
                        setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                }
                else
                {
                        setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
                }

                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(update()) );
        }


        QSize Ruler::sizeHint() const
        {
                return minimumSizeHint();
        }


        QSize Ruler::minimumSizeHint() const
        {
                return (mOrientation == Qt::Horizontal) ? QSize( 0, RULER_THICKNESS ) : QSize( RULER_THICKNESS, 0 );
        }


        void Ruler::setPointer( model::Point p )
        {
                mPointer = p;
                mPointerValid = true;
                update();
        }


        void Ruler::clearPointer()
        {
                mPointerValid = false;
                update();
        }


        void Ruler::paintEvent( QPaintEvent* event )
        {
                QPainter painter( this );

                const QPalette& pal = palette();
                QColor bg      = pal.color( QPalette::Window );
                QColor fg      = pal.color( QPalette::WindowText );
                QColor labelBg = pal.color( QPalette::Base );
                QColor accent  = pal.color( QPalette::Highlight );

                painter.fillRect( rect(), bg );

                bool horiz = mOrientation == Qt::Horizontal;
                int length = horiz ? width() : height();
                int thick  = horiz ? height() : width();

                // Bottom/right border
                painter.setPen( fg );
                if ( horiz )
                {
                        painter.drawLine( 0, height()-1, width(), height()-1 );
                }
                else
                {
                        painter.drawLine( width()-1, 0, width()-1, height() );
                }

                if ( !mEditor || !mEditor->isVisible() || mEditor->scale() <= 0 )
                {
                        return;
                }

                // Map label origin (in editor pixels) into this widget's coordinates
                QPoint originGlobal = mEditor->mapToGlobal( mEditor->labelOriginPixels().toPoint() );
                QPoint originLocal  = mapFromGlobal( originGlobal );
                double origin = horiz ? originLocal.x() : originLocal.y();

                double scale = mEditor->scale(); // pixels per point

                // Label extent highlight
                double labelLenPx = horiz ? mEditor->labelWidth().pt()*scale : mEditor->labelHeight().pt()*scale;
                if ( labelLenPx > 0 )
                {
                        if ( horiz )
                        {
                                painter.fillRect( QRectF( origin, 2, labelLenPx, thick-3 ), labelBg );
                        }
                        else
                        {
                                painter.fillRect( QRectF( 2, origin, thick-3, labelLenPx ), labelBg );
                        }
                }

                // Units
                auto units = model::Settings::units();
                double ptsPerUnit = model::Distance( 1.0, units ).pt();
                double pxPerUnit  = ptsPerUnit * scale;

                // Choose a "nice" major spacing (in units)
                static const double nice[] = { 0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000 };
                double major = nice[ sizeof(nice)/sizeof(nice[0]) - 1 ];
                for ( double n : nice )
                {
                        if ( n * pxPerUnit >= MIN_MAJOR_SPACING_PX )
                        {
                                major = n;
                                break;
                        }
                }

                // Subdivisions
                int nSub = 10;
                if ( units.toEnum() == model::Units::IN && major >= 1 )
                {
                        nSub = 8; // 1/8 inch
                }
                while ( nSub > 1 && (major/nSub)*pxPerUnit < 5 )
                {
                        nSub = (nSub == 10) ? 5 : (nSub == 8 ? 4 : (nSub == 5 ? 2 : (nSub == 4 ? 2 : 1)));
                }
                double minor = major / nSub;

                // Range of values visible
                double vStart = (0 - origin) / pxPerUnit;
                double vEnd   = (length - origin) / pxPerUnit;
                long iStart = long( std::floor( vStart / minor ) ) - 1;
                long iEnd   = long( std::ceil( vEnd / minor ) ) + 1;

                QFont font = this->font();
                font.setPixelSize( 9 );
                painter.setFont( font );
                painter.setPen( fg );

                int digits = 0;
                if ( major < 1 )   digits = 1;
                if ( major < 0.1 ) digits = 2;
                if ( std::fabs( major - 0.25 ) < 1e-9 ) digits = 2;

                for ( long i = iStart; i <= iEnd; i++ )
                {
                        double v  = i * minor;
                        double px = origin + v * pxPerUnit;
                        if ( px < 0 || px > length )
                        {
                                continue;
                        }

                        int tickLen;
                        bool isMajor = (i % nSub) == 0;
                        if ( isMajor )
                        {
                                tickLen = thick - 2;
                        }
                        else if ( nSub % 2 == 0 && (i % (nSub/2)) == 0 )
                        {
                                tickLen = thick / 2;
                        }
                        else
                        {
                                tickLen = thick / 4;
                        }

                        if ( horiz )
                        {
                                painter.drawLine( QPointF( px, thick-1 ), QPointF( px, thick-1-tickLen ) );
                        }
                        else
                        {
                                painter.drawLine( QPointF( thick-1, px ), QPointF( thick-1-tickLen, px ) );
                        }

                        if ( isMajor )
                        {
                                QString text = QString::number( v, 'f', digits );
                                if ( horiz )
                                {
                                        painter.drawText( QPointF( px + 2, 9 ), text );
                                }
                                else
                                {
                                        painter.save();
                                        painter.translate( 9, px + 2 );
                                        painter.rotate( -90 );
                                        painter.drawText( QPointF( -painter.fontMetrics().horizontalAdvance( text ) - 2, 0 ), text );
                                        painter.restore();
                                }
                        }
                }
                // Pointer marker
                if ( mPointerValid )
                {
                        double v = horiz ? mPointer.x().pt() : mPointer.y().pt();
                        double px = origin + v * scale;
                        QPen pen( accent );
                        pen.setWidth( 1 );
                        painter.setPen( pen );
                        if ( horiz )
                        {
                                painter.drawLine( QPointF( px, 0 ), QPointF( px, thick ) );
                        }
                        else
                        {
                                painter.drawLine( QPointF( 0, px ), QPointF( thick, px ) );
                        }
                }
        }

}
