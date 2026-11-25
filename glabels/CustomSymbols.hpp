//  CustomSymbols.hpp
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

#ifndef CustomSymbols_hpp
#define CustomSymbols_hpp


#include <QByteArray>
#include <QList>
#include <QString>


namespace glabels
{

        ///
        /// The user's own SVG symbols, shown in Tools > Insert Symbol next to the
        /// built-in set.
        ///
        /// Each symbol is a validated copy of the SVG file kept in the per-user
        /// application data folder (e.g. ~/.local/share/glabels.org/glabels-qt/symbols
        /// on Linux, %APPDATA%\glabels.org\glabels-qt\symbols on Windows), with its
        /// name, category and keywords in symbols.json beside it, so the symbols stay
        /// after the program or the computer is restarted.
        ///
        namespace CustomSymbols
        {
                struct Entry
                {
                        QString id;          // "custom-<slug>", unique
                        QString name;
                        QString category;
                        QString keywords;
                        QString path;        // absolute path of the stored SVG
                };

                // Limits enforced by validate() and add()
                const qint64 maxFileBytes   = 2 * 1024 * 1024;
                const int    maxNameLength  = 60;
                const int    maxCategoryLen = 40;
                const int    maxKeywordsLen = 200;

                /// Folder the symbols are stored in
                QString directory();

                /// All stored symbols, in the order they were added; entries whose
                /// SVG is missing or no longer valid are left out
                QList<Entry> load();

                /// Check that data is a safe, self-contained SVG drawing that renders.
                /// Returns an empty string when it is valid, or a message for the user.
                QString validate( const QByteArray& data );

                /// Validate and store a new symbol.  On success returns true and sets
                /// *added; otherwise returns false and sets *error.
                bool add( const QByteArray& svg,
                          const QString&    name,
                          const QString&    category,
                          const QString&    keywords,
                          Entry*            added,
                          QString*          error );

                /// Delete a stored symbol
                bool remove( const QString& id, QString* error );
        }

}


#endif // CustomSymbols_hpp
