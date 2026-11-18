//  BuiltInFonts.cpp
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

#include "BuiltInFonts.hpp"

#include <QFontDatabase>
#include <QDebug>


// Q_INIT_RESOURCE must be used outside of any namespace. The resource lives in the
// static Model library, so it has to be initialized explicitly or the linker drops it.
static void initFontResources()
{
        Q_INIT_RESOURCE( fonts );
}


namespace
{
        QStringList sFamilies;

        const char* const sFontFiles[] = {
                ":/fonts/NotoNastaliqUrdu-Regular.ttf",
                ":/fonts/NotoNastaliqUrdu-Bold.ttf",
        };
}


namespace glabels::model
{

        void BuiltInFonts::init()
        {
                static bool initialized = false;
                if ( initialized )
                {
                        return;
                }
                initialized = true;

                initFontResources();

                for ( const char* file : sFontFiles )
                {
                        int id = QFontDatabase::addApplicationFont( file );
                        if ( id < 0 )
                        {
                                qWarning() << "Could not load built-in font" << file;
                                continue;
                        }
                        for ( const QString& family : QFontDatabase::applicationFontFamilies( id ) )
                        {
                                if ( !sFamilies.contains( family ) )
                                {
                                        sFamilies << family;
                                }
                        }
                }
        }


        QStringList BuiltInFonts::families()
        {
                return sFamilies;
        }


        QString BuiltInFonts::urduFamily()
        {
                return "Noto Nastaliq Urdu";
        }

}
