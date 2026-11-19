//  Merge/TextAutoKeys.cpp
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

#include "TextAutoKeys.hpp"


namespace
{
        const QString ID = "Text/Auto/Line1Keys";
}


namespace glabels::merge
{

        TextAutoKeys::TextAutoKeys() : Text( true )
        {
                mId = ID;
        }


        TextAutoKeys::TextAutoKeys( const TextAutoKeys* merge ) : Text( merge )
        {
        }


        TextAutoKeys* TextAutoKeys::clone() const
        {
                return new TextAutoKeys( this );
        }


        QString TextAutoKeys::id()
        {
                return ID;
        }


        Merge* TextAutoKeys::create()
        {
                return new TextAutoKeys();
        }

}
