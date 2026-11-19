//  VariableTypeIcon.cpp
//
//  Copyright (C) 2026  gLabels Plus contributors
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

#include "VariableTypeIcon.hpp"

#include "merge/DataSourceUtil.hpp"

#include <QCoreApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>


namespace
{
        QPixmap drawIcon( const QString& type, int size )
        {
                QPixmap pixmap( size, size );
                pixmap.fill( Qt::transparent );

                QPainter p( &pixmap );
                p.setRenderHint( QPainter::Antialiasing );

                const double s = size;
                QColor bg, fg( Qt::white );
                if      ( type == "barcode" ) bg = QColor( 46, 125, 50 );
                else if ( type == "image" )   bg = QColor( 142, 68, 173 );
                else if ( type == "number" )  bg = QColor( 214, 120, 20 );
                else                          bg = QColor( 52, 101, 164 );

                QRectF box( s*0.06, s*0.06, s*0.88, s*0.88 );
                p.setPen( Qt::NoPen );
                p.setBrush( bg );
                p.drawRoundedRect( box, s*0.2, s*0.2 );

                p.setBrush( fg );
                if ( type == "barcode" )
                {
                        const double widths[] = { 0.06, 0.03, 0.08, 0.03, 0.05, 0.03, 0.07, 0.04 };
                        double x = s*0.2;
                        bool bar = true;
                        for ( double w : widths )
                        {
                                if ( bar )
                                {
                                        p.drawRect( QRectF( x, s*0.25, w*s, s*0.5 ) );
                                }
                                x += w*s + s*0.03;
                                bar = !bar;
                        }
                }
                else if ( type == "image" )
                {
                        QPainterPath mountains;
                        mountains.moveTo( s*0.18, s*0.75 );
                        mountains.lineTo( s*0.4, s*0.45 );
                        mountains.lineTo( s*0.55, s*0.62 );
                        mountains.lineTo( s*0.65, s*0.52 );
                        mountains.lineTo( s*0.82, s*0.75 );
                        mountains.closeSubpath();
                        p.drawPath( mountains );
                        p.drawEllipse( QPointF( s*0.66, s*0.32 ), s*0.08, s*0.08 );
                }
                else
                {
                        QFont font;
                        font.setBold( true );
                        font.setPixelSize( int( s*0.62 ) );
                        p.setFont( font );
                        p.setPen( fg );
                        p.drawText( box, Qt::AlignCenter, type == "number" ? "#" : "T" );
                }

                return pixmap;
        }
}


namespace glabels::VariableType
{

        QIcon icon( const QString& type )
        {
                QIcon icon;
                for ( int size : { 16, 24, 32, 48 } )
                {
                        icon.addPixmap( drawIcon( type, size ) );
                }
                return icon;
        }


        QString displayName( const QString& type )
        {
                if ( type == merge::DataSourceUtil::TYPE_BARCODE ) return QCoreApplication::translate( "VariableType", "Barcode" );
                if ( type == merge::DataSourceUtil::TYPE_IMAGE )   return QCoreApplication::translate( "VariableType", "Image Path" );
                if ( type == merge::DataSourceUtil::TYPE_NUMBER )  return QCoreApplication::translate( "VariableType", "Number" );
                return QCoreApplication::translate( "VariableType", "Text" );
        }


        QStringList types()
        {
                return merge::DataSourceUtil::types();
        }

}
