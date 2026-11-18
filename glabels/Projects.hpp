//  Projects.hpp
//
//  Copyright (C) 2026  Usman Saif <usmansaif.com>
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

#ifndef Projects_hpp
#define Projects_hpp


#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QList>
#include <QString>
#include <QStringList>


class QWidget;


namespace glabels
{

        ///
        /// Projects: named folders that group related labels (e.g. one per business).
        ///
        /// Each project is a folder under rootDir() ("Documents/gLabels Plus Projects"),
        /// and its labels are the ordinary .glabels files inside that folder, so they
        /// can be backed up, copied or shared like any other files.
        ///
        class Projects
        {
                Q_DECLARE_TR_FUNCTIONS(Projects)

        public:
                struct LabelInfo
                {
                        QString   filePath;
                        QString   name;       // file name without extension
                        QString   product;    // e.g. "Barcode Labels 50 × 25 mm" (may be empty)
                        QDateTime modified;
                };

                /// Parent folder of all projects (created on demand)
                static QDir rootDir();

                /// Project names, sorted alphabetically (case-insensitive)
                static QStringList list();

                /// Absolute folder path of a project
                static QString path( const QString& project );

                /// Number of labels in a project
                static int labelCount( const QString& project );

                /// Labels in a project, most recently modified first
                static QList<LabelInfo> labels( const QString& project );

                /// Project a file belongs to, or empty if it isn't inside a project folder
                static QString projectOf( const QString& filePath );

                /// Check a project or label name; returns the cleaned-up name, or an empty
                /// string with *error set when the name can't be used as a file name
                static QString validateName( const QString& name, QString* error );

                static bool create( const QString& project, QString* error );
                static bool rename( const QString& project, const QString& newName, QString* error );
                static bool moveToTrash( const QString& project, QString* error );

                /// Path for a new label called labelName in project (not created)
                static QString labelPath( const QString& project, const QString& labelName );

                /// A label name not yet used in project: "Label 1", "Label 2", ...
                static QString nextLabelName( const QString& project );

                /// Is filePath (or any file inside it, for a folder) open in a window?
                static bool isOpen( const QString& path );

                /// Interactive helpers (show dialogs; return empty/false when cancelled)
                static QString askNewProject( QWidget* parent );
                static QString askName( QWidget*       parent,
                                        const QString& title,
                                        const QString& prompt,
                                        const QString& initial,
                                        const QString& existsDir,
                                        const QString& existsSuffix );
        };

}


#endif // Projects_hpp
