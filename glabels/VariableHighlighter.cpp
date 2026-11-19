//  VariableHighlighter.cpp
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

#include "VariableHighlighter.hpp"

#include <QRegularExpression>
#include <QTextCharFormat>


namespace glabels
{

        VariableHighlighter::VariableHighlighter( QTextDocument* document )
                : QSyntaxHighlighter( document )
        {
        }


        void VariableHighlighter::highlightBlock( const QString& text )
        {
                static const QRegularExpression re( "\\{\\{[^{}]+\\}\\}" );

                QTextCharFormat pill;
                pill.setBackground( QColor( 52, 101, 164, 40 ) );
                pill.setForeground( QColor( 32, 74, 135 ) );
                pill.setFontWeight( QFont::DemiBold );

                QTextCharFormat brace = pill;
                brace.setForeground( QColor( 52, 101, 164, 150 ) );

                auto it = re.globalMatch( text );
                while ( it.hasNext() )
                {
                        auto m = it.next();
                        int start = int( m.capturedStart() );
                        int len   = int( m.capturedLength() );
                        setFormat( start, len, pill );
                        setFormat( start, 2, brace );
                        setFormat( start + len - 2, 2, brace );
                }
        }

}
