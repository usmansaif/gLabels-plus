//  Merge/Text.hpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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

#ifndef merge_Text_hpp
#define merge_Text_hpp


#include "Merge.hpp"

#include <QFile>


namespace glabels::merge
{

        ///
        /// Text Merge Backend
        ///
        struct Text : public Merge
        {

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        protected:
                Text( QChar delimiter, bool line1HasKeys );
                Text( bool line1HasKeys );   // auto-detect delimiter
                Text( const Text* merge );
                virtual ~Text() = default;


                /////////////////////////////////
                // Implementation of virtual methods
                /////////////////////////////////
        public:
                /// Guess the delimiter of delimited text: ',', '\t', ';', ':' or '|'
                static QChar detectDelimiter( const QByteArray& sample );

                /// Delimiter in use (after open() for auto-detecting backends)
                QChar delimiter() const;

                /// True when the source was not valid UTF-8 and was read as Latin-1
                bool isLatin1() const;

                QStringList sourceKeys() const override;
                QString sourcePrimaryKey() const override;
        protected:
                void open() override;
                void close() override;
                Record readNextRecord() override;


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
                QString keyFromIndex( int iField ) const;
                QStringList parseLine();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QString toQString( const QByteArray& bytes ) const;

                QChar mDelimeter;
                bool  mLine1HasKeys;
                bool  mAutoDetect{ false };
                bool  mLatin1{ false };

                QFile          mFile;
                QStringList    mKeys;
                int            mNFieldsMax;
        };

}


#endif // merge_Text_hpp
