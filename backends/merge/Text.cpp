//  Merge/Text.cpp
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


#include "Text.hpp"

#include "Record.hpp"

#include <QDebug>
#include <QMap>
#include <QStringDecoder>


namespace glabels::merge
{

        ///
        /// Constructor
        ///
        Text::Text( QChar delimiter, bool line1HasKeys )
                : Merge(),
                  mDelimeter(delimiter),
                  mLine1HasKeys(line1HasKeys),
                  mNFieldsMax(0)
        {
        }


        ///
        /// Constructor (auto-detect delimiter)
        ///
        Text::Text( bool line1HasKeys )
                : Merge(),
                  mDelimeter(','),
                  mLine1HasKeys(line1HasKeys),
                  mAutoDetect(true),
                  mNFieldsMax(0)
        {
        }


        ///
        /// Constructor
        ///
        Text::Text( const Text* merge )
                : Merge( merge ),
                  mDelimeter(merge->mDelimeter),
                  mLine1HasKeys(merge->mLine1HasKeys),
                  mAutoDetect(merge->mAutoDetect),
                  mLatin1(merge->mLatin1),
                  mKeys(merge->mKeys),
                  mNFieldsMax(merge->mNFieldsMax)
        {
        }


        ///
        /// Get key list
        ///
        QStringList Text::sourceKeys() const
        {
                QStringList keys;
                for ( int iField = 0; iField < mNFieldsMax; iField++ )
                {
                        keys << keyFromIndex(iField);
                }
                return keys;
        }


        ///
        /// Get primary key
        ///
        QString Text::sourcePrimaryKey() const
        {
                return keyFromIndex(0);
        }


        ///
        /// Open source
        ///
        void Text::open()
        {
                mKeys.clear();
                mNFieldsMax = 0;

                mFile.setFileName( source() );
                if (mFile.open( QIODevice::ReadOnly|QIODevice::Text ))
                {
                        // Encoding: UTF-8 (with or without BOM), falling back to Latin-1
                        QByteArray all = mFile.peek( 4*1024*1024 );
                        QStringDecoder decoder( QStringDecoder::Utf8 );
                        QString dummy = decoder.decode( all );
                        mLatin1 = decoder.hasError();
                        Q_UNUSED( dummy );

                        if ( mFile.peek( 3 ) == QByteArray( "\xEF\xBB\xBF" ) )
                        {
                                mFile.read( 3 );   // skip UTF-8 byte order mark
                        }

                        if ( mAutoDetect )
                        {
                                mDelimeter = detectDelimiter( mFile.peek( 64*1024 ) );
                        }

                        if ( mLine1HasKeys )
                        {
                                mKeys = parseLine();
                                if ( (mKeys.size() == 1) && (mKeys[0] == "") )
                                {
                                        mKeys.clear();
                                }
                                else
                                {
                                        mNFieldsMax = mKeys.size();
                                }
                        }
                }
        }


        ///
        /// Close source
        ///
        void Text::close()
        {
                if ( mFile.isOpen() )
                {
                        mFile.close();
                }
        }


        ///
        /// Read next record
        ///
        Record Text::readNextRecord()
        {
                QStringList values = parseLine();
                if ( !values.isEmpty() )
                {
                        Record record;

                        int iField = 0;
                        for ( const auto& value : values )
                        {
                                record[ keyFromIndex(iField) ] = value;
                                iField++;
                        }
                        mNFieldsMax = std::max( mNFieldsMax, iField );

                        return record;
                }

                return NullRecord();
        }


        ///
        /// Delimiter in use
        ///
        QChar Text::delimiter() const
        {
                return mDelimeter;
        }


        ///
        /// Read as Latin-1?
        ///
        bool Text::isLatin1() const
        {
                return mLatin1;
        }


        ///
        /// Convert raw field bytes to a string
        ///
        QString Text::toQString( const QByteArray& bytes ) const
        {
                return mLatin1 ? QString::fromLatin1( bytes ) : QString::fromUtf8( bytes );
        }


        ///
        /// Guess delimiter
        ///
        /// Counts candidate delimiters (outside of quotes) on the first lines and
        /// picks the one that occurs the same, non-zero number of times on the most
        /// lines.  Ties go to the more frequent delimiter, then to the order below.
        ///
        QChar Text::detectDelimiter( const QByteArray& sample )
        {
                const char candidates[] = { ',', '\t', ';', '|', ':' };
                const int nCandidates = sizeof(candidates);

                // Split into (at most 30) logical lines, honoring quoted newlines
                QList<QByteArray> lines;
                QByteArray line;
                bool quoted = false;
                for ( char c : sample )
                {
                        if ( c == '"' )
                        {
                                quoted = !quoted;
                        }
                        if ( c == '\n' && !quoted )
                        {
                                if ( !line.trimmed().isEmpty() )
                                {
                                        lines << line;
                                }
                                line.clear();
                                if ( lines.size() >= 30 )
                                {
                                        break;
                                }
                        }
                        else if ( c != '\r' )
                        {
                                line.append( c );
                        }
                }
                if ( lines.size() < 30 && !line.trimmed().isEmpty() )
                {
                        lines << line;
                }
                if ( lines.isEmpty() )
                {
                        return ',';
                }

                int    bestScore = -1;
                int    bestCount = 0;
                QChar  best      = ',';
                for ( int iC = 0; iC < nCandidates; iC++ )
                {
                        char d = candidates[iC];
                        QMap<int,int> histogram;   // count per line -> number of lines
                        for ( const auto& l : lines )
                        {
                                int  n = 0;
                                bool q = false;
                                for ( char c : l )
                                {
                                        if ( c == '"' ) q = !q;
                                        else if ( c == d && !q ) n++;
                                }
                                histogram[n]++;
                        }

                        // Most common non-zero count
                        int mode = 0, modeLines = 0;
                        for ( auto it = histogram.constBegin(); it != histogram.constEnd(); ++it )
                        {
                                if ( it.key() > 0 && it.value() > modeLines )
                                {
                                        mode = it.key();
                                        modeLines = it.value();
                                }
                        }
                        if ( mode == 0 )
                        {
                                continue;
                        }

                        if ( modeLines > bestScore || ( modeLines == bestScore && mode > bestCount ) )
                        {
                                bestScore = modeLines;
                                bestCount = mode;
                                best      = QChar( d );
                        }
                }

                return best;
        }


        ///
        /// Key from field index
        ///
        QString Text::keyFromIndex( int iField ) const
        {
                if ( mLine1HasKeys && ( iField < mKeys.size() ) )
                {
                        return mKeys[iField];
                }
                else
                {
                        return QString::number( iField+1 );
                }
        }


        ///
        /// Parse line.
        ///
        /// Attempt to be a robust parser of various CSV (and similar) formats.
        ///
        /// Based on CSV format described in RFC 4180 section 2.
        ///
        /// Additions to RFC 4180 rules:
        ///   - delimeters and other special characters may be "escaped" by a leading
        ///     backslash (\)
        ///   - C escape sequences for newline (\n) and tab (\t) are also translated.
        ///   - if quoted text is not followed by a delimeter, any additional text is
        ///     concatenated with quoted portion.
        ///
        /// Returns a list of fields.  A blank line is considered a line with one
        /// empty field.  Returns an empty list when done.
        ///
        QStringList Text::parseLine()
        {
                QStringList fields;

                enum State
                        {
                                DELIM, QUOTED, QUOTED_QUOTE1, QUOTED_ESCAPED, SIMPLE, SIMPLE_ESCAPED, DONE
                        } state = DELIM;

                QByteArray field;

                while ( state != DONE )
                {
                        char c;
                        if ( mFile.getChar( &c ) )
                        {
                                switch (state)
                                {

                                case DELIM:
                                        switch (c)
                                        {
                                        case '\n':
                                                /* last field is empty. */
                                                fields << "";
                                                state = DONE;
                                                break;
                                        case '\r':
                                                /* ignore */
                                                state = DELIM;
                                                break;
                                        case '"':
                                                /* start a quoted field. */
                                                state = QUOTED;
                                                break;
                                        case '\\':
                                                /* simple field, but 1st character is an escape. */
                                                state = SIMPLE_ESCAPED;
                                                break;
                                        default:
                                                if ( c == mDelimeter )
                                                {
                                                        /* field is empty. */
                                                        fields << "";
                                                        state = DELIM;
                                                }
                                                else
                                                {
                                                        /* beginning of a simple field. */
                                                        field.append( c );
                                                        state = SIMPLE;
                                                }
                                                break;
                                        }
                                        break;

                                case QUOTED:
                                        switch (c)
                                        {
                                        case '"':
                                                /* Possible end of field, but could be 1st of a pair. */
                                                state = QUOTED_QUOTE1;
                                                break;
                                        case '\\':
                                                /* Escape next character, or special escape, e.g. \n. */
                                                state = QUOTED_ESCAPED;
                                                break;
                                        default:
                                                /* Use character literally. */
                                                field.append( c );
                                                break;
                                        }
                                        break;

                                case QUOTED_QUOTE1:
                                        switch (c)
                                        {
                                        case '\n':
                                                /* line ended after quoted item */
                                                fields << toQString( field );
                                                state = DONE;
                                                break;
                                        case '"':
                                                /* second quote, insert and stay quoted. */
                                                field.append( c );
                                                state = QUOTED;
                                                break;
                                        case '\r':
                                                /* ignore and go to fallback */
                                                state = SIMPLE;
                                                break;
                                        default:
                                                if ( c == mDelimeter )
                                                {
                                                        /* end of field. */
                                                        fields << toQString( field );
                                                        field.clear();
                                                        state = DELIM;
                                                }
                                                else
                                                {
                                                        /* fallback if not a delim or another quote. */
                                                        field.append( c );
                                                        state = SIMPLE;
                                                }
                                                break;
                                        }
                                        break;

                                case QUOTED_ESCAPED:
                                        switch (c)
                                        {
                                        case 'n':
                                                /* Decode "\n" as newline. */
                                                field.append( '\n' );
                                                state = QUOTED;
                                                break;
                                        case 't':
                                                /* Decode "\t" as tab. */
                                                field.append( '\t' );
                                                state = QUOTED;
                                                break;
                                        default:
                                                /* Use character literally. */
                                                field.append( c );
                                                state = QUOTED;
                                                break;
                                        }
                                        break;

                                case SIMPLE:
                                        switch (c)
                                        {
                                        case '\n':
                                                /* line ended */
                                                fields << toQString( field );
                                                state = DONE;
                                                break;
                                        case '\r':
                                                /* ignore */
                                                state = SIMPLE;
                                                break;
                                        case '\\':
                                                /* Escape next character, or special escape, e.g. \n. */
                                                state = SIMPLE_ESCAPED;
                                                break;
                                        default:
                                                if ( c == mDelimeter )
                                                {
                                                        /* end of field. */
                                                        fields << toQString( field );
                                                        field.clear();
                                                        state = DELIM;
                                                }
                                                else
                                                {
                                                        /* Use character literally. */
                                                        field.append( c );
                                                        state = SIMPLE;
                                                }
                                                break;
                                        }
                                        break;

                                case SIMPLE_ESCAPED:
                                        switch (c)
                                        {
                                        case 'n':
                                                /* Decode "\n" as newline. */
                                                field.append( '\n' );
                                                state = SIMPLE;
                                                break;
                                        case 't':
                                                /* Decode "\t" as tab. */
                                                field.append( '\t' );
                                                state = SIMPLE;
                                                break;
                                        default:
                                                /* Use character literally. */
                                                field.append( (char)c );
                                                state = SIMPLE;
                                                break;
                                        }
                                        break;

                                default:
                                        qWarning( "merge::Text::parseLine()::Should not be reached! #1" );
                                        break;
                                }

                        }
                        else
                        {
                                /* Handle EOF (could also be an error while reading). */
                                switch (state)
                                {

                                case DELIM:
                                        /* EOF, no more lines. */
                                        break;

                                case QUOTED:
                                        /* File ended midway through quoted item. Truncate field. */
                                        fields << toQString( field );
                                        break;

                                case QUOTED_QUOTE1:
                                        /* File ended after quoted item. */
                                        fields << toQString( field );
                                        break;

                                case QUOTED_ESCAPED:
                                        /* File ended midway through quoted item. Truncate field. */
                                        fields << toQString( field );
                                        break;

                                case SIMPLE:
                                        /* File ended after simple item. */
                                        fields << toQString( field );
                                        break;

                                case SIMPLE_ESCAPED:
                                        /* File ended midway through escaped item. */
                                        fields << toQString( field );
                                        break;

                                default:
                                        qWarning( "merge::Text::parseLine()::Should not be reached! #2" );
                                        break;
                                }

                                state = DONE;
                        }
                }


                return fields;
        }

}
