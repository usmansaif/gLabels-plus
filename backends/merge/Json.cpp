//  Merge/Json.cpp
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

#include "Json.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>

#include <algorithm>


namespace
{
        const QString ID = "Json";

        QString valueToString( const QJsonValue& value )
        {
                switch ( value.type() )
                {
                case QJsonValue::String:
                        return value.toString();
                case QJsonValue::Double:
                {
                        double d = value.toDouble();
                        if ( d == double( qint64( d ) ) && qAbs( d ) < 1e15 )
                        {
                                return QString::number( qint64( d ) );
                        }
                        return QString::number( d, 'g', 15 );
                }
                case QJsonValue::Bool:
                        return value.toBool() ? "true" : "false";
                case QJsonValue::Array:
                        return QString::fromUtf8( QJsonDocument( value.toArray() ).toJson( QJsonDocument::Compact ) );
                case QJsonValue::Object:
                        return QString::fromUtf8( QJsonDocument( value.toObject() ).toJson( QJsonDocument::Compact ) );
                default:
                        return QString();
                }
        }
}


namespace glabels::merge
{

        Json::Json()
        {
                mId = ID;
        }


        Json::Json( const Json* merge )
                : Merge( merge ), mKeys( merge->mKeys )
        {
        }


        Json* Json::clone() const
        {
                return new Json( this );
        }


        QString Json::id()
        {
                return ID;
        }


        Merge* Json::create()
        {
                return new Json();
        }


        QStringList Json::sourceKeys() const
        {
                return mKeys;
        }


        QString Json::sourcePrimaryKey() const
        {
                return mKeys.isEmpty() ? QString() : mKeys.first();
        }


        void Json::open()
        {
                mKeys.clear();
                mPending.clear();
                mNext = 0;

                QFile file( source() );
                if ( !file.open( QIODevice::ReadOnly ) )
                {
                        return;
                }

                QByteArray bytes = file.readAll();
                QJsonParseError error;
                QJsonDocument doc = QJsonDocument::fromJson( bytes, &error );
                if ( error.error != QJsonParseError::NoError )
                {
                        return;
                }

                QJsonArray array;
                if ( doc.isArray() )
                {
                        array = doc.array();
                }
                else if ( doc.isObject() )
                {
                        QJsonObject obj = doc.object();
                        bool found = false;
                        for ( auto it = obj.begin(); it != obj.end(); ++it )
                        {
                                if ( it.value().isArray() )
                                {
                                        array = it.value().toArray();
                                        found = true;
                                        break;
                                }
                        }
                        if ( !found )
                        {
                                array.append( obj );   // a single record
                        }
                }

                for ( const auto& item : array )
                {
                        if ( !item.isObject() )
                        {
                                continue;
                        }
                        QJsonObject obj = item.toObject();
                        Record record;
                        // QJsonObject iterates keys alphabetically; the file order is
                        // not available, so new keys are appended as they are seen.
                        for ( auto it = obj.begin(); it != obj.end(); ++it )
                        {
                                if ( !mKeys.contains( it.key() ) )
                                {
                                        mKeys << it.key();
                                }
                                record[ it.key() ] = valueToString( it.value() );
                        }
                        if ( !record.isEmpty() )
                        {
                                mPending << record;
                        }
                }

                // Restore the order in which keys first appear in the file
                QMap<QString,qsizetype> position;
                for ( const auto& key : mKeys )
                {
                        QByteArray quoted = QJsonDocument( QJsonArray{ key } ).toJson( QJsonDocument::Compact );
                        quoted = quoted.mid( 1, quoted.size() - 2 );   // strip [ ]
                        qsizetype pos = bytes.indexOf( quoted );
                        position[key] = ( pos < 0 ) ? bytes.size() : pos;
                }
                std::stable_sort( mKeys.begin(), mKeys.end(), [&position]( const QString& a, const QString& b ) {
                        return position[a] < position[b];
                } );
        }


        void Json::close()
        {
                mPending.clear();
                mNext = 0;
        }


        Record Json::readNextRecord()
        {
                if ( mNext < mPending.size() )
                {
                        return mPending[ mNext++ ];
                }
                return NullRecord();
        }

}
