//  BuiltInFonts.hpp
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

#ifndef model_BuiltInFonts_hpp
#define model_BuiltInFonts_hpp


#include <QStringList>


namespace glabels::model
{

        ///
        /// Fonts shipped inside the application
        ///
        /// Registered with Qt at startup so they appear in the font picker and
        /// are embedded in printed / PDF output on every platform, whether or not
        /// the font is installed on the system. Currently: Noto Nastaliq Urdu
        /// (Regular and Bold) for accurate Urdu Nastaliq labels.
        ///
        class BuiltInFonts
        {
        public:
                /// Register all built-in fonts (call once, after QGuiApplication exists)
                static void init();

                /// Family names that were registered successfully
                static QStringList families();

                /// Family name of the built-in Urdu font
                static QString urduFamily();
        };

}


#endif // model_BuiltInFonts_hpp
