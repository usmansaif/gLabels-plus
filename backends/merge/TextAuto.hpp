//  Merge/TextAuto.hpp
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

#ifndef merge_TextAuto_hpp
#define merge_TextAuto_hpp


#include "Text.hpp"


namespace glabels::merge
{

        ///
        /// TextAuto Merge Backend: delimited text, delimiter detected automatically
        ///
        struct TextAuto : public Text
        {
        private:
                TextAuto();
                TextAuto( const TextAuto* merge );
                virtual ~TextAuto() = default;

        public:
                TextAuto* clone() const override;

                static QString id();
                static Merge* create();
        };

}


#endif // merge_TextAuto_hpp
