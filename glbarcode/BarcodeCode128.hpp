//  BarcodeCode128.hpp
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

#ifndef glbarcode_BarcodeCode128_hpp
#define glbarcode_BarcodeCode128_hpp


#include "Barcode1dBase.hpp"


namespace glbarcode
{

        /**
         * @class BarcodeCode128 BarcodeCode128.h glbarcode/BarcodeCode128.h
         *
         * *Code 128* 1D barcode symbology.
         *
         *
         * ### Input Data Format ###
         *
         * Any ASCII characters (0-127).  The encoder picks code sets A, B and C
         * automatically, using code set C for runs of digits so that numbers
         * produce short barcodes.
         *
         *
         * ### Checksum ###
         *
         * The modulo 103 check character is required by the specification and is
         * always added.  Scanners verify it and do not report it as data.
         *
         *
         * ### Show Text Property ###
         *
         * If the *Show Text* property is *true*, the input data is printed below
         * the barcode.
         *
         *
         * ### References ###
         *
         * - http://en.wikipedia.org/wiki/Code_128
         *
         */
        class BarcodeCode128 : public Barcode1dBase
        {
        public:
                /**
                 * Static Code128 barcode creation method
                 *
                 * Used by glbarcode::BarcodeFactory
                 */
                static Barcode* create();


        private:
                bool validate( const std::string& rawData ) override;

                std::string encode( const std::string& cookedData ) override;

                std::string prepareText( const std::string& rawData ) override;

                void vectorize( const std::string& codedData,
                                const std::string& displayText,
                                const std::string& cookedData,
                                double&            w,
                                double&            h ) override;
        };

}


#endif // glbarcode_BarcodeCode128_hpp
