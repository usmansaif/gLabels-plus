//  Merge/Merge.hpp
//
//  Copyright (C) 2015-2026  Jaye Evins <evins@snaught.com>
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

#ifndef merge_Merge_hpp
#define merge_Merge_hpp


#include "Record.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>


namespace glabels::merge
{

        // Forward references
        class Record;


        ///
        /// Field: how one source column is presented to the label
        ///
        /// A field maps a column of the data source ("source") to the variable
        /// name used on the label ("name"), e.g. "prod_sku_01" -> "Product_SKU".
        /// Excluded fields are hidden from the label entirely.
        ///
        struct Field
        {
                QString source;            // original column name in the data source
                QString name;              // variable name used on the label: ${name}
                QString type{ "text" };    // "text", "barcode", "image" or "number"
                bool    include{ true };

                bool operator==( const Field& other ) const
                {
                        return source == other.source && name == other.name &&
                               type == other.type && include == other.include;
                }
                bool operator!=( const Field& other ) const { return !( *this == other ); }
        };


        ///
        /// Merge Object
        ///
        class Merge : public QObject
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        protected:
                Merge() = default;
                Merge( const Merge* merge );
        public:
                virtual ~Merge() = default;


                /////////////////////////////////
                // Object duplication
                /////////////////////////////////
                virtual Merge* clone() const = 0;


                /////////////////////////////////
                // Properties
                /////////////////////////////////
        public:
                QString id() const;
                QString source() const;
                void setSource( const QString& source );
                void reloadSource();

                const QList<Record>& recordList( ) const;


                /////////////////////////////////
                // Field mapping (source columns -> label variables)
                /////////////////////////////////
        public:
                /// Variable names visible to the label (renamed, excluded columns removed)
                QStringList keys() const;
                QString primaryKey() const;

                /// Column names exactly as they appear in the source
                QStringList sourceColumns() const;

                /// Field mapping; empty means "use every column under its own name"
                QList<Field> fields() const;
                void setFields( const QList<Field>& fields );

                /// Field info for a variable name (default-constructed if unknown)
                Field field( const QString& name ) const;
                QString fieldType( const QString& name ) const;

                /// Indices of records that are not selected (for saving selections)
                QList<int> unselectedIndices() const;
                void setUnselectedIndices( const QList<int>& indices );


                /////////////////////////////////
                // Selection methods
                /////////////////////////////////
        public:
                void setSelected( int i, bool state = true );
                void selectAll();
                void unselectAll();

                int nSelectedRecords() const;
                const QList<Record> selectedRecords() const;


                /////////////////////////////////
                // Virtual methods
                /////////////////////////////////
        public:
                virtual QStringList sourceKeys() const = 0;
                virtual QString sourcePrimaryKey() const = 0;
        protected:
                virtual void open() = 0;
                virtual void close() = 0;
                virtual Record readNextRecord() = 0;


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                void sourceChanged();
                void selectionChanged();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        protected:
                QString       mId;
        private:
                Record mapRecord( const Record& raw ) const;
                void   rebuildRecords();

                QString       mSource;
                QList<Record> mRawRecordList;   // records keyed by source column
                QList<Record> mRecordList;      // records keyed by variable name
                QList<Field>  mFields;
        };

}


#endif // merge_Merge_hpp
