//  Samples.hpp
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

#ifndef Samples_hpp
#define Samples_hpp


#include <QCoreApplication>
#include <QDir>
#include <QList>
#include <QString>


class QWidget;


namespace glabels
{

        namespace model
        {
                class Model;
        }

        class MainWindow;


        ///
        /// Sample library: ready-made label designs to start from.
        ///
        /// Built-in samples (shipping label, receipt, invoice, SKU, product and asset
        /// labels, ...) are compiled in as resources.  "My templates" are ordinary
        /// .glabels files in userDir() ("Documents/gLabels Plus Templates"), saved
        /// with File > Save as Template.
        ///
        /// Opening a sample always creates an untitled copy: saving asks for a new
        /// file name, so the sample itself is never changed.
        ///
        class Samples
        {
                Q_DECLARE_TR_FUNCTIONS(Samples)

        public:
                struct Info
                {
                        QString path;          // ":/samples/..." or a file in userDir()
                        QString name;          // e.g. "Shipping Label 4×6"
                        QString category;      // e.g. "Shipping"; "My templates" for user templates
                        QString description;
                        bool    builtIn{ true };
                };

                /// Built-in samples followed by the user's own templates
                static QList<Info> list();

                /// Folder of the user's own templates (not created until needed)
                static QDir userDir();

                /// Category name used for the user's own templates
                static QString userCategory();

                /// Load a sample as a new, untitled, unmodified label (nullptr on error)
                static model::Model* load( const Info& sample );

                /// Open a sample as an untitled copy in this window (if empty) or a new one.
                /// Returns the window showing it, or nullptr.
                static MainWindow* open( const Info& sample, MainWindow* window );

                /// Save a copy of the window's label into the user's templates
                static bool saveAsTemplate( MainWindow* window );
        };

}


#endif // Samples_hpp
