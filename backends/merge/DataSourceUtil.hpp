//  Merge/DataSourceUtil.hpp
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

#ifndef merge_DataSourceUtil_hpp
#define merge_DataSourceUtil_hpp


#include "Merge.hpp"

#include <QString>
#include <QStringList>


namespace glabels::merge
{

        ///
        /// Helpers for turning data-source columns into label variables
        ///
        namespace DataSourceUtil
        {
                /// Field types
                inline const QString TYPE_TEXT    = QStringLiteral( "text" );
                inline const QString TYPE_BARCODE = QStringLiteral( "barcode" );
                inline const QString TYPE_IMAGE   = QStringLiteral( "image" );
                inline const QString TYPE_NUMBER  = QStringLiteral( "number" );

                QStringList types();

                /// Is name a valid variable name (letters, digits and underscores)?
                bool isValidName( const QString& name );

                /// Suggest a friendly variable name for a column, e.g. "prod_sku_01" -> "Product_SKU_01"
                QString suggestName( const QString& column, int index );

                /// Make names unique by appending _2, _3, ... (case-insensitive)
                QStringList makeUnique( const QStringList& names );

                /// Guess the field type from the column name and sample values
                QString inferType( const QString& column, const QStringList& samples );

                /// Build a default mapping for a merge source (names, types, all included)
                QList<Field> defaultFields( const Merge* merge );

                /// Replace references to variable oldName by newName in raw text,
                /// e.g. "${Old}", "${Old:%5s}" -> "${New}", "${New:%5s}"
                QString renameReferences( const QString& text, const QString& oldName, const QString& newName );

                /// Display form of raw text: "${name...}" -> "{{name...}}"
                QString toDisplay( const QString& raw );

                /// Raw form of display text: "{{name...}}" -> "${name...}"
                QString fromDisplay( const QString& display );
        }

}


#endif // merge_DataSourceUtil_hpp
