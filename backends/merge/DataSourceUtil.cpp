//  Merge/DataSourceUtil.cpp
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

#include "DataSourceUtil.hpp"

#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>


namespace glabels::merge::DataSourceUtil
{

        QStringList types()
        {
                return { TYPE_TEXT, TYPE_BARCODE, TYPE_IMAGE, TYPE_NUMBER };
        }


        bool isValidName( const QString& name )
        {
                static const QRegularExpression re( "^[A-Za-z0-9_]+$" );
                return re.match( name ).hasMatch();
        }


        QString suggestName( const QString& column, int index )
        {
                static const QSet<QString> acronyms = {
                        "id", "sku", "ean", "upc", "gtin", "isbn", "url", "qr", "mrp", "vat",
                        "gst", "ntn", "cnic", "po", "uom", "hs", "sn", "imei", "pdf", "rgb"
                };
                static const QMap<QString,QString> expansions = {
                        { "prod", "Product" }, { "desc", "Description" }, { "qty", "Quantity" },
                        { "img", "Image" },    { "addr", "Address" },     { "amt", "Amount" },
                        { "num", "Number" },   { "cat", "Category" },     { "mfg", "Manufactured" },
                        { "exp", "Expiry" },   { "pic", "Picture" },      { "tel", "Phone" }
                };

                // Split camelCase and anything that is not a letter or digit
                QString s = column.trimmed();
                s.replace( QRegularExpression( "([a-z0-9])([A-Z])" ), "\\1_\\2" );

                QStringList parts;
                QString part;
                for ( QChar c : s )
                {
                        if ( c.unicode() < 128 && c.isLetterOrNumber() )
                        {
                                part.append( c );
                        }
                        else if ( !part.isEmpty() )
                        {
                                parts << part;
                                part.clear();
                        }
                }
                if ( !part.isEmpty() )
                {
                        parts << part;
                }

                QStringList words;
                for ( const auto& p : parts )
                {
                        QString lower = p.toLower();
                        if ( acronyms.contains( lower ) )
                        {
                                words << p.toUpper();
                        }
                        else if ( expansions.contains( lower ) )
                        {
                                words << expansions[lower];
                        }
                        else
                        {
                                words << ( lower.left( 1 ).toUpper() + lower.mid( 1 ) );
                        }
                }

                QString name = words.join( "_" );
                if ( name.isEmpty() || name[0].isDigit() )
                {
                        name = name.isEmpty() ? QString( "Column_%1" ).arg( index + 1 )
                                              : "Column_" + name;
                }
                return name;
        }


        QStringList makeUnique( const QStringList& names )
        {
                QStringList result;
                QSet<QString> used;
                for ( const auto& name : names )
                {
                        QString candidate = name;
                        int n = 2;
                        while ( used.contains( candidate.toLower() ) )
                        {
                                candidate = QString( "%1_%2" ).arg( name ).arg( n++ );
                        }
                        used.insert( candidate.toLower() );
                        result << candidate;
                }
                return result;
        }


        QString inferType( const QString& column, const QStringList& samples )
        {
                QString col = column.toLower();

                QStringList values;
                for ( const auto& v : samples )
                {
                        if ( !v.trimmed().isEmpty() )
                        {
                                values << v.trimmed();
                        }
                }

                // Image paths: by column name or by file extension of the values
                static const QRegularExpression imageExt(
                        "\\.(png|jpe?g|gif|bmp|svg|webp|tiff?|pbm|pgm|ppm|xbm|xpm)$",
                        QRegularExpression::CaseInsensitiveOption );
                bool allImages = !values.isEmpty();
                for ( const auto& v : values )
                {
                        if ( !imageExt.match( v ).hasMatch() )
                        {
                                allImages = false;
                                break;
                        }
                }
                static const QRegularExpression imageCol( "(^|[^a-z])(image|img|photo|picture|pic|logo)" );
                if ( allImages || ( imageCol.match( col ).hasMatch() && values.isEmpty() ) )
                {
                        return TYPE_IMAGE;
                }

                // Barcodes: by column name
                static const QRegularExpression barcodeCol(
                        "(barcode|sku|ean|upc|gtin|isbn|qr|code128|code39|serial|imei|tracking)" );
                if ( barcodeCol.match( col ).hasMatch() )
                {
                        return TYPE_BARCODE;
                }

                if ( values.isEmpty() )
                {
                        return TYPE_TEXT;
                }

                // Barcodes: 8, 12, 13 or 14 digit codes (EAN/UPC/GTIN)
                static const QRegularExpression gtin( "^(\\d{8}|\\d{12,14})$" );
                bool allGtin = true;
                for ( const auto& v : values )
                {
                        if ( !gtin.match( v ).hasMatch() )
                        {
                                allGtin = false;
                                break;
                        }
                }
                if ( allGtin )
                {
                        return TYPE_BARCODE;
                }

                // Numbers: plain integers or decimals (optionally with thousands separators)
                static const QRegularExpression number( "^[-+]?(\\d{1,3}(,\\d{3})+|\\d+)(\\.\\d+)?$" );
                bool allNumbers = true;
                for ( const auto& v : values )
                {
                        if ( !number.match( v ).hasMatch() )
                        {
                                allNumbers = false;
                                break;
                        }
                }
                if ( allNumbers )
                {
                        return TYPE_NUMBER;
                }

                return TYPE_TEXT;
        }


        QList<Field> defaultFields( const Merge* merge )
        {
                QList<Field> fields;
                if ( !merge )
                {
                        return fields;
                }

                QStringList columns = merge->sourceColumns();

                // Names: keep an existing mapping, suggest names for new columns
                QStringList names;
                for ( int i = 0; i < columns.size(); i++ )
                {
                        names << suggestName( columns[i], i );
                }
                names = makeUnique( names );

                // Samples: first 20 records (raw columns are not available through the
                // public API once mapped, so read them from an unmapped clone)
                Merge* raw = merge->clone();
                raw->setFields( {} );
                const auto& records = raw->recordList();

                for ( int i = 0; i < columns.size(); i++ )
                {
                        QStringList samples;
                        for ( int r = 0; r < records.size() && r < 20; r++ )
                        {
                                samples << records[r].value( columns[i] );
                        }

                        Field f;
                        f.source  = columns[i];
                        f.name    = names[i];
                        f.type    = inferType( columns[i], samples );
                        f.include = true;
                        fields << f;
                }
                delete raw;

                return fields;
        }


        QString renameReferences( const QString& text, const QString& oldName, const QString& newName )
        {
                if ( oldName.isEmpty() || oldName == newName )
                {
                        return text;
                }
                QRegularExpression re( "\\$\\{" + QRegularExpression::escape( oldName ) + "(?=[:}])" );
                QString result = text;
                result.replace( re, "${" + newName );
                return result;
        }


        QString toDisplay( const QString& raw )
        {
                static const QRegularExpression re( "\\$\\{([^{}\\n]+)\\}" );
                QString result = raw;
                result.replace( re, "{{\\1}}" );
                return result;
        }


        QString fromDisplay( const QString& display )
        {
                static const QRegularExpression re( "\\{\\{([^{}\\n]+)\\}\\}" );
                QString result = display;
                result.replace( re, "${\\1}" );
                return result;
        }

}
