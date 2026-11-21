//  SheetLayout.hpp
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

#ifndef model_SheetLayout_hpp
#define model_SheetLayout_hpp


#include <QList>
#include <QRectF>


namespace glabels::model
{

        ///
        /// Imposition of identical items on a printing (press) sheet.
        ///
        /// Every item takes its trim size plus bleed on each side; items are placed
        /// bleed to bleed inside the printable area (sheet minus margin on each side).
        /// The layout mixes upright and rotated items (guillotine blocks and strips)
        /// to fit as many items as possible on one sheet.
        ///
        /// All distances are in points.
        ///
        class SheetLayout
        {
        public:
                struct Item
                {
                        QRectF bleedBox;   // item including bleed, in sheet coordinates
                        QRectF trimBox;    // finished item, in sheet coordinates
                        bool   rotated;    // turned 90 degrees
                };

                static SheetLayout compute( double sheetW, double sheetH,
                                            double itemW,  double itemH,
                                            double bleed,  double margin );

                double sheetW() const { return mSheetW; }
                double sheetH() const { return mSheetH; }
                double itemW() const { return mItemW; }
                double itemH() const { return mItemH; }
                double bleed() const { return mBleed; }
                double margin() const { return mMargin; }

                const QList<Item>& items() const { return mItems; }
                int perSheet() const { return int(mItems.size()); }
                int rotatedCount() const;

                /// Sheets needed to print a quantity (0 if nothing fits)
                int sheetsFor( int quantity ) const;

                /// Share of one full sheet not covered by finished (trimmed) items, 0..1
                double sheetWaste() const;

                /// Share of all sheets of a job not covered by finished items, 0..1
                double jobWaste( int quantity ) const;

        private:
                double mSheetW{ 0 };
                double mSheetH{ 0 };
                double mItemW{ 0 };
                double mItemH{ 0 };
                double mBleed{ 0 };
                double mMargin{ 0 };
                QList<Item> mItems;
        };

}


#endif // model_SheetLayout_hpp
