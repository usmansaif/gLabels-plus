//  BarcodeCode128.cpp
//
//  Copyright (C) 2026  Usman Saif
//
//  This file is part of glbarcode++.
//
//  glbarcode++ is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  glbarcode++ is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with glbarcode++.  If not, see <http://www.gnu.org/licenses/>.
//

#include "BarcodeCode128.hpp"

#include "Constants.hpp"

#include <algorithm>
#include <vector>


using namespace glbarcode::Constants;


namespace
{
        /*
         * Bar/space widths (in modules) of the 107 Code 128 symbols, starting with
         * a bar.  Position is the symbol value; 106 is the stop pattern.
         */
        const char* const patterns[] = {
                "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312",
                "132212", "221213", "221312", "231212", "112232", "122132", "122231", "113222",
                "123122", "123221", "223211", "221132", "221231", "213212", "223112", "312131",
                "311222", "321122", "321221", "312212", "322112", "322211", "212123", "212321",
                "232121", "111323", "131123", "131321", "112313", "132113", "132311", "211313",
                "231113", "231311", "112133", "112331", "132131", "113123", "113321", "133121",
                "313121", "211331", "231131", "213113", "213311", "213131", "311123", "311321",
                "331121", "312113", "312311", "332111", "314111", "221411", "431111", "111224",
                "111422", "121124", "121421", "141122", "141221", "112214", "112412", "122114",
                "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111",
                "111242", "121142", "121241", "114212", "124112", "124211", "411212", "421112",
                "421211", "212141", "214121", "412121", "111143", "111341", "131141", "114113",
                "114311", "411113", "411311", "113141", "114131", "311141", "411131", "211412",
                "211214", "211232", "2331112"
        };

        enum CodeSet { SetA, SetB, SetC };

        const int CODE_C  = 99;   // switch to code set C (from A or B)
        const int CODE_B  = 100;  // switch to code set B (from A or C)
        const int CODE_A  = 101;  // switch to code set A (from B or C)
        const int START_A = 103;
        const int START_B = 104;
        const int START_C = 105;
        const int STOP    = 106;

        /* Vectorization constants */
        const double MIN_X       = ( 0.0075 * PTS_PER_INCH );   // narrowest module
        const double MIN_HEIGHT  = ( 0.19685 * PTS_PER_INCH );
        const double MIN_QUIET   = ( 10 * MIN_X );

        const double MIN_TEXT_AREA_HEIGHT = 12.0;
        const double MIN_TEXT_SIZE        = 8.0;


        bool isDigit( char c )
        {
                return c >= '0' && c <= '9';
        }

        /* Number of consecutive digits starting at position i */
        size_t digitRun( const std::string& s, size_t i )
        {
                size_t n = 0;
                while ( i + n < s.size() && isDigit( s[i + n] ) )
                {
                        n++;
                }
                return n;
        }

        /* Value of a character in code set A or B */
        int valueIn( CodeSet set, unsigned char c )
        {
                if ( set == SetA )
                {
                        return ( c < 32 ) ? c + 64 : c - 32;
                }
                return c - 32;
        }

        /* Can the character be encoded in code set A or B? */
        bool fitsIn( CodeSet set, unsigned char c )
        {
                if ( set == SetA )
                {
                        return c < 96;
                }
                return c >= 32 && c < 128;
        }

        /* Code set A or B for the character at position i (A only for control characters) */
        CodeSet setFor( const std::string& s, size_t i )
        {
                for ( ; i < s.size(); i++ )
                {
                        auto c = (unsigned char)s[i];
                        if ( c < 32 )  return SetA;
                        if ( c >= 96 ) return SetB;
                }
                return SetB;
        }
}


namespace glbarcode
{

        /*
         * Static Code128 barcode creation method
         */
        Barcode* BarcodeCode128::create( )
        {
                return new BarcodeCode128();
        }


        /*
         * Code128 data validation, implements Barcode1dBase::validate()
         */
        bool BarcodeCode128::validate( const std::string& rawData )
        {
                if ( rawData.empty() )
                {
                        return false;
                }
                for ( char c : rawData )
                {
                        if ( (unsigned char)c > 127 )
                        {
                                return false;
                        }
                }
                return true;
        }


        /*
         * Code128 data encoding, implements Barcode1dBase::encode()
         *
         * Produces the string of bar/space module widths of the whole symbol
         * (start, data, check character and stop).
         */
        std::string BarcodeCode128::encode( const std::string& cookedData )
        {
                const std::string& s = cookedData;
                std::vector<int> values;

                // Start code: C for a leading run of 4+ digits (or an all-digit even
                // length string), otherwise A or B
                CodeSet set;
                size_t lead = digitRun( s, 0 );
                if ( lead >= 4 || ( lead == s.size() && lead >= 2 && lead % 2 == 0 ) )
                {
                        set = SetC;
                        values.push_back( START_C );
                }
                else
                {
                        set = setFor( s, 0 );
                        values.push_back( set == SetA ? START_A : START_B );
                }

                size_t i = 0;
                while ( i < s.size() )
                {
                        if ( set == SetC )
                        {
                                if ( digitRun( s, i ) >= 2 )
                                {
                                        values.push_back( (s[i] - '0') * 10 + (s[i+1] - '0') );
                                        i += 2;
                                        continue;
                                }
                                set = setFor( s, i );
                                values.push_back( set == SetA ? CODE_A : CODE_B );
                                continue;
                        }

                        // In A or B: switch to C for 4+ digits at the end or 6+ in the middle
                        size_t run = digitRun( s, i );
                        bool atEnd = ( i + run == s.size() );
                        if ( run >= 6 || ( atEnd && run >= 4 ) )
                        {
                                if ( run % 2 == 1 )
                                {
                                        values.push_back( valueIn( set, (unsigned char)s[i] ) );
                                        i++;
                                }
                                set = SetC;
                                values.push_back( CODE_C );
                                continue;
                        }

                        auto c = (unsigned char)s[i];
                        if ( !fitsIn( set, c ) )
                        {
                                set = ( set == SetA ) ? SetB : SetA;
                                values.push_back( set == SetA ? CODE_A : CODE_B );
                        }
                        values.push_back( valueIn( set, c ) );
                        i++;
                }

                // Modulo 103 check character
                int sum = values[0];
                for ( size_t k = 1; k < values.size(); k++ )
                {
                        sum += int(k) * values[k];
                }
                values.push_back( sum % 103 );
                values.push_back( STOP );

                std::string code;
                for ( int v : values )
                {
                        code += patterns[v];
                }
                return code;
        }


        /*
         * Code128 prepare text for display, implements Barcode1dBase::prepareText()
         */
        std::string BarcodeCode128::prepareText( const std::string& rawData )
        {
                std::string displayText;
                for ( char c : rawData )
                {
                        // Control characters are not printable
                        displayText += ( (unsigned char)c < 32 ) ? ' ' : c;
                }
                return displayText;
        }


        /*
         * Code128 vectorization, implements Barcode1dBase::vectorize()
         */
        void BarcodeCode128::vectorize( const std::string& codedData,
                                        const std::string& displayText,
                                        const std::string& cookedData,
                                        double&            w,
                                        double&            h )
        {
                /* total number of modules */
                int modules = 0;
                for ( char c : codedData )
                {
                        modules += c - '0';
                }
                double minL = modules * MIN_X;

                double scale;
                if ( w == 0 )
                {
                        scale = 1.0;
                }
                else
                {
                        scale = w / (minL + 2*MIN_QUIET);
                        if ( scale < 1.0 )
                        {
                                scale = 1.0;
                        }
                }
                double width = minL * scale;

                /* determine text parameters */
                double hTextArea = scale * MIN_TEXT_AREA_HEIGHT;
                double textSize  = scale * MIN_TEXT_SIZE;

                /* determine height of barcode */
                double height = showText() ? h - hTextArea : h;
                height = std::max( height, std::max( 0.15*width, MIN_HEIGHT ) );

                /* horizontal quiet zone */
                double xQuiet = std::max( (10 * scale * MIN_X), MIN_QUIET );

                /* bars and spaces alternate, starting with a bar */
                double x1 = xQuiet;
                bool bar = true;
                for ( char c : codedData )
                {
                        double lwidth = (c - '0') * scale * MIN_X;
                        if ( bar )
                        {
                                addLine( x1, 0.0, lwidth, height );
                        }
                        x1 += lwidth;
                        bar = !bar;
                }

                if ( showText() )
                {
                        addText( xQuiet + width/2, height + (hTextArea+0.7*textSize)/2, textSize, displayText );
                }

                /* Overwrite requested size with actual size. */
                w = width + 2*xQuiet;
                h = showText() ? height + hTextArea : height;
        }

}
