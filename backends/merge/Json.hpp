//  Merge/Json.hpp
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

#ifndef merge_Json_hpp
#define merge_Json_hpp


#include "Merge.hpp"


namespace glabels::merge
{

        ///
        /// Json Merge Backend
        ///
        /// Reads an array of objects, e.g. [ {"sku":"A1","name":"Tea"}, ... ].
        /// The array may also be the first array-valued member of a top-level
        /// object, e.g. { "records": [ ... ] }.  Keys are the union of all object
        /// keys, in order of first appearance.  Nested values are kept as compact JSON.
        ///
        struct Json : public Merge
        {
        private:
                Json();
                Json( const Json* merge );
                virtual ~Json() = default;

        public:
                Json* clone() const override;

                static QString id();
                static Merge* create();

                QStringList sourceKeys() const override;
                QString sourcePrimaryKey() const override;

        protected:
                void open() override;
                void close() override;
                Record readNextRecord() override;

        private:
                QStringList   mKeys;
                QList<Record> mPending;
                int           mNext{ 0 };
        };

}


#endif // merge_Json_hpp
