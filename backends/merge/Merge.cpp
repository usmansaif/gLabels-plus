//  Merge/Merge.cpp
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


#include "Merge.hpp"


namespace glabels::merge
{

        ///
        /// Constructor
        ///
        Merge::Merge( const Merge* merge )
                : mId(merge->mId),
                  mSource(merge->mSource),
                  mRawRecordList(merge->mRawRecordList),
                  mRecordList(merge->mRecordList),
                  mFields(merge->mFields)
        {
        }


        ///
        /// Get id
        ///
        QString Merge::id() const
        {
                return mId;
        }


        ///
        /// Get source
        ///
        QString Merge::source() const
        {
                return mSource;
        }


        ///
        /// Set source
        ///
        void Merge::setSource( const QString& source )
        {
                mSource = source;

                // Clear out any old records
                mRawRecordList.clear();
                mRecordList.clear();

                open();
                for ( Record record = readNextRecord(); !record.isEmpty(); record = readNextRecord() )
                {
                        mRawRecordList.push_back( record );
                }
                close();

                for ( const auto& raw : mRawRecordList )
                {
                        mRecordList.push_back( mapRecord( raw ) );
                }

                emit sourceChanged();
        }


        ///
        /// Reload source
        ///
        void Merge::reloadSource()
        {
                //  temporary map of original selections
                QMap<QString,bool> origSelectionMap;

                QString pKey = sourcePrimaryKey();
                for ( int i = 0; i < mRawRecordList.size(); i++ )
                {
                        bool selected = (i < mRecordList.size()) ? mRecordList[i].isSelected()
                                                                 : mRawRecordList[i].isSelected();
                        origSelectionMap[ mRawRecordList[i][ pKey ] ] = selected;
                }

                // Clear the original records
                mRawRecordList.clear();
                mRecordList.clear();

                // Read the new records, preserve any original selections
                open();
                pKey = sourcePrimaryKey();
                for ( Record record = readNextRecord(); !record.isEmpty(); record = readNextRecord() )
                {
                        auto key = record[ pKey ];
                        auto it = origSelectionMap.find( key );
                        if ( it != origSelectionMap.end() )
                        {
                                record.setSelected( it.value() );
                        }

                        mRawRecordList.push_back( record );
                }
                close();

                for ( const auto& raw : mRawRecordList )
                {
                        mRecordList.push_back( mapRecord( raw ) );
                }

                emit sourceChanged();
        }


        ///
        /// Get record list
        ///
        const QList<Record>& Merge::recordList( ) const
        {
                return mRecordList;
        }


        ///
        /// Variable names visible to the label
        ///
        QStringList Merge::keys() const
        {
                QStringList sKeys = sourceKeys();
                if ( mFields.isEmpty() )
                {
                        return sKeys;
                }

                QStringList list;
                for ( const auto& sKey : sKeys )
                {
                        bool found = false;
                        for ( const auto& f : mFields )
                        {
                                if ( f.source == sKey )
                                {
                                        found = true;
                                        if ( f.include && !list.contains( f.name ) )
                                        {
                                                list << f.name;
                                        }
                                        break;
                                }
                        }
                        if ( !found && !list.contains( sKey ) )
                        {
                                list << sKey;   // column added to the source after mapping
                        }
                }
                return list;
        }


        ///
        /// Primary key (first visible variable)
        ///
        QString Merge::primaryKey() const
        {
                QStringList list = keys();
                return list.isEmpty() ? QString() : list.first();
        }


        ///
        /// Source column names
        ///
        QStringList Merge::sourceColumns() const
        {
                return sourceKeys();
        }


        ///
        /// Field mapping
        ///
        QList<Field> Merge::fields() const
        {
                return mFields;
        }


        ///
        /// Set field mapping
        ///
        void Merge::setFields( const QList<Field>& fields )
        {
                if ( fields == mFields )
                {
                        return;
                }
                mFields = fields;
                rebuildRecords();
                emit sourceChanged();
        }


        ///
        /// Field info for variable name
        ///
        Field Merge::field( const QString& name ) const
        {
                for ( const auto& f : mFields )
                {
                        if ( f.name == name && f.include )
                        {
                                return f;
                        }
                }

                Field f;
                if ( keys().contains( name ) )
                {
                        f.source = name;
                        f.name   = name;
                }
                return f;
        }


        ///
        /// Field type for variable name
        ///
        QString Merge::fieldType( const QString& name ) const
        {
                return field( name ).type;
        }


        ///
        /// Unselected record indices
        ///
        QList<int> Merge::unselectedIndices() const
        {
                QList<int> list;
                for ( int i = 0; i < mRecordList.size(); i++ )
                {
                        if ( !mRecordList[i].isSelected() )
                        {
                                list << i;
                        }
                }
                return list;
        }


        ///
        /// Set unselected record indices (all others selected)
        ///
        void Merge::setUnselectedIndices( const QList<int>& indices )
        {
                for ( int i = 0; i < mRecordList.size(); i++ )
                {
                        mRecordList[i].setSelected( !indices.contains( i ) );
                }
                emit selectionChanged();
        }


        ///
        /// Select/unselect i'th record
        ///
        void Merge::setSelected( int i, bool state )
        {
                if ( (i >= 0) && (i < mRecordList.size()) )
                {
                        mRecordList[i].setSelected( state );
                        emit selectionChanged();
                }
        }


        ///
        /// Select all records
        ///
        void Merge::selectAll()
        {
                for ( auto& record : mRecordList )
                {
                        record.setSelected( true );
                }
                emit selectionChanged();
        }


        ///
        /// Unselect all records
        ///
        void Merge::unselectAll()
        {
                for ( auto& record : mRecordList )
                {
                        record.setSelected( false );
                }
                emit selectionChanged();
        }


        ///
        /// Return count of selected records
        ///
        int Merge::nSelectedRecords() const
        {
                int count = 0;

                for ( const auto& record : mRecordList )
                {
                        if ( record.isSelected() )
                        {
                                count++;
                        }
                }

                return count;
        }


        ///
        /// Return list of selected records
        ///
        const QList<Record> Merge::selectedRecords() const
        {
                QList<Record> list;

                for ( const auto& record : mRecordList )
                {
                        if ( record.isSelected() )
                        {
                                list.append( record );
                        }
                }

                return list;
        }


        ///
        /// Map a raw record (keyed by source column) to variable names
        ///
        Record Merge::mapRecord( const Record& raw ) const
        {
                if ( mFields.isEmpty() )
                {
                        return raw;
                }

                Record record;
                record.setSelected( raw.isSelected() );
                for ( auto it = raw.constBegin(); it != raw.constEnd(); ++it )
                {
                        bool found = false;
                        for ( const auto& f : mFields )
                        {
                                if ( f.source == it.key() )
                                {
                                        found = true;
                                        if ( f.include )
                                        {
                                                record[ f.name ] = it.value();
                                        }
                                        break;
                                }
                        }
                        if ( !found )
                        {
                                record[ it.key() ] = it.value();
                        }
                }
                return record;
        }


        ///
        /// Rebuild mapped records, keeping the current selection
        ///
        void Merge::rebuildRecords()
        {
                QList<Record> list;
                for ( int i = 0; i < mRawRecordList.size(); i++ )
                {
                        Record record = mapRecord( mRawRecordList[i] );
                        if ( i < mRecordList.size() )
                        {
                                record.setSelected( mRecordList[i].isSelected() );
                        }
                        list.push_back( record );
                }
                mRecordList = list;
        }

}
