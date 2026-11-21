//  SheetLayout.cpp
//
//  Copyright (C) 2026  gLabels Plus contributors (Usman Saif)
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

#include "SheetLayout.hpp"

#include <algorithm>
#include <cmath>


namespace glabels::model
{

        namespace
        {
                // Tolerance so that items that fit exactly are not lost to rounding
                const double EPSILON = 1e-6;

                // Levels of mixed blocks/strips to try (2 = block + strip + strip)
                const int SEARCH_DEPTH = 2;


                int fitCount( double length, double size )
                {
                        if ( size <= 0 || length <= 0 ) return 0;
                        return int( std::floor( length / size + EPSILON ) );
                }


                ///
                /// Guillotine search for identical items of size w x h (bleed box)
                ///
                class Packer
                {
                public:
                        Packer( double w, double h ) : mW( w ), mH( h ) {}

                        // Best number of items in an A x B region
                        int count( double a, double b, int depth ) const
                        {
                                Choice c = choose( a, b, depth );
                                return c.total;
                        }

                        // Place the best layout of an A x B region at (x, y)
                        void place( double x, double y, double a, double b, int depth,
                                    QList<SheetLayout::Item>& out ) const
                        {
                                Choice c = choose( a, b, depth );
                                if ( c.total == 0 ) return;

                                double ow = c.rotated ? mH : mW;
                                double oh = c.rotated ? mW : mH;

                                int cols, rows;
                                if ( c.split == Split::NONE )
                                {
                                        cols = fitCount( a, ow );
                                        rows = fitCount( b, oh );
                                }
                                else if ( c.split == Split::COLUMNS )
                                {
                                        cols = c.n;
                                        rows = fitCount( b, oh );
                                }
                                else
                                {
                                        cols = fitCount( a, ow );
                                        rows = c.n;
                                }

                                for ( int i = 0; i < cols; i++ )
                                {
                                        for ( int j = 0; j < rows; j++ )
                                        {
                                                SheetLayout::Item item;
                                                item.bleedBox = QRectF( x + i*ow, y + j*oh, ow, oh );
                                                item.rotated  = c.rotated;
                                                out.append( item );
                                        }
                                }

                                if ( c.split == Split::COLUMNS )
                                {
                                        place( x + cols*ow, y, a - cols*ow, b, depth - 1, out );
                                }
                                else if ( c.split == Split::ROWS )
                                {
                                        place( x, y + rows*oh, a, b - rows*oh, depth - 1, out );
                                }
                        }

                private:
                        enum class Split { NONE, COLUMNS, ROWS };

                        struct Choice
                        {
                                int   total{ 0 };
                                bool  rotated{ false };
                                Split split{ Split::NONE };
                                int   n{ 0 };   // columns or rows in the first block
                        };

                        Choice choose( double a, double b, int depth ) const
                        {
                                Choice best;

                                // Plain grids first: on a tie the simplest layout wins
                                for ( bool rotated : { false, true } )
                                {
                                        double ow = rotated ? mH : mW;
                                        double oh = rotated ? mW : mH;
                                        int total = fitCount( a, ow ) * fitCount( b, oh );
                                        if ( total > best.total )
                                        {
                                                best = { total, rotated, Split::NONE, 0 };
                                        }
                                }

                                if ( depth <= 0 || best.total == 0 )
                                {
                                        return best;
                                }

                                // A block of one orientation, the rest of the region filled again
                                for ( bool rotated : { false, true } )
                                {
                                        double ow = rotated ? mH : mW;
                                        double oh = rotated ? mW : mH;

                                        int rowsPerCol = fitCount( b, oh );
                                        int maxCols    = fitCount( a, ow );
                                        if ( rowsPerCol > 0 )
                                        {
                                                for ( int i = 1; i < maxCols; i++ )
                                                {
                                                        int total = i*rowsPerCol + count( a - i*ow, b, depth - 1 );
                                                        if ( total > best.total )
                                                        {
                                                                best = { total, rotated, Split::COLUMNS, i };
                                                        }
                                                }
                                        }

                                        int colsPerRow = fitCount( a, ow );
                                        int maxRows    = fitCount( b, oh );
                                        if ( colsPerRow > 0 )
                                        {
                                                for ( int j = 1; j < maxRows; j++ )
                                                {
                                                        int total = j*colsPerRow + count( a, b - j*oh, depth - 1 );
                                                        if ( total > best.total )
                                                        {
                                                                best = { total, rotated, Split::ROWS, j };
                                                        }
                                                }
                                        }
                                }

                                return best;
                        }

                        double mW;
                        double mH;
                };
        }


        SheetLayout SheetLayout::compute( double sheetW, double sheetH,
                                          double itemW,  double itemH,
                                          double bleed,  double margin )
        {
                SheetLayout layout;
                layout.mSheetW = sheetW;
                layout.mSheetH = sheetH;
                layout.mItemW  = itemW;
                layout.mItemH  = itemH;
                layout.mBleed  = std::max( 0.0, bleed );
                layout.mMargin = std::max( 0.0, margin );

                double areaW = sheetW - 2*layout.mMargin;
                double areaH = sheetH - 2*layout.mMargin;
                double boxW  = itemW + 2*layout.mBleed;
                double boxH  = itemH + 2*layout.mBleed;

                if ( itemW <= 0 || itemH <= 0 || areaW <= 0 || areaH <= 0 )
                {
                        return layout;
                }

                Packer packer( boxW, boxH );
                packer.place( 0, 0, areaW, areaH, SEARCH_DEPTH, layout.mItems );

                if ( layout.mItems.isEmpty() )
                {
                        return layout;
                }

                // Center the whole group of items on the printable area
                QRectF bounds;
                for ( const auto& item : layout.mItems )
                {
                        bounds = bounds.united( item.bleedBox );
                }
                double dx = layout.mMargin + (areaW - bounds.width()) / 2 - bounds.left();
                double dy = layout.mMargin + (areaH - bounds.height()) / 2 - bounds.top();

                for ( auto& item : layout.mItems )
                {
                        item.bleedBox.translate( dx, dy );
                        item.trimBox = item.bleedBox.adjusted( layout.mBleed, layout.mBleed,
                                                               -layout.mBleed, -layout.mBleed );
                }

                return layout;
        }


        int SheetLayout::rotatedCount() const
        {
                return int( std::count_if( mItems.begin(), mItems.end(),
                                           []( const Item& item ) { return item.rotated; } ) );
        }


        int SheetLayout::sheetsFor( int quantity ) const
        {
                if ( perSheet() == 0 || quantity <= 0 ) return 0;
                return (quantity + perSheet() - 1) / perSheet();
        }


        double SheetLayout::sheetWaste() const
        {
                double sheetArea = mSheetW * mSheetH;
                if ( sheetArea <= 0 ) return 1.0;
                return std::clamp( 1.0 - perSheet() * mItemW * mItemH / sheetArea, 0.0, 1.0 );
        }


        double SheetLayout::jobWaste( int quantity ) const
        {
                int sheets = sheetsFor( quantity );
                double paperArea = sheets * mSheetW * mSheetH;
                if ( paperArea <= 0 ) return 1.0;
                return std::clamp( 1.0 - quantity * mItemW * mItemH / paperArea, 0.0, 1.0 );
        }

}
