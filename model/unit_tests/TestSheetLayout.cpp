//  TestSheetLayout.cpp
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


#include "TestSheetLayout.hpp"

#include "model/SheetLayout.hpp"


QTEST_MAIN(TestSheetLayout)


using namespace glabels::model;


namespace
{
        // Every item inside the printable area and no two items overlapping
        bool isValid( const SheetLayout& layout )
        {
                QRectF area( layout.margin(), layout.margin(),
                             layout.sheetW() - 2*layout.margin(), layout.sheetH() - 2*layout.margin() );
                area.adjust( -1e-6, -1e-6, 1e-6, 1e-6 );

                const auto& items = layout.items();
                for ( int i = 0; i < items.size(); i++ )
                {
                        if ( !area.contains( items[i].bleedBox ) ) return false;
                        for ( int j = i + 1; j < items.size(); j++ )
                        {
                                QRectF overlap = items[i].bleedBox.intersected( items[j].bleedBox );
                                if ( overlap.width() > 1e-6 && overlap.height() > 1e-6 ) return false;
                        }
                }
                return true;
        }
}


void TestSheetLayout::simpleGrid()
{
        auto layout = SheetLayout::compute( 100, 200, 50, 50, 0, 0 );
        QCOMPARE( layout.perSheet(), 8 );
        QCOMPARE( layout.rotatedCount(), 0 );
        QVERIFY( isValid( layout ) );
}


void TestSheetLayout::mixedOrientation()
{
        // A plain grid of 30 x 40 fits 6 either way; a row of upright items
        // with rotated items below fits 7.
        auto layout = SheetLayout::compute( 100, 100, 30, 40, 0, 0 );
        QCOMPARE( layout.perSheet(), 7 );
        QVERIFY( layout.rotatedCount() > 0 );
        QVERIFY( isValid( layout ) );
}


void TestSheetLayout::bleedAndMargin()
{
        // Printable area 90 x 90; items are 25 x 25 with bleed
        auto layout = SheetLayout::compute( 100, 100, 20, 20, 2.5, 5 );
        QCOMPARE( layout.perSheet(), 9 );
        QVERIFY( isValid( layout ) );

        const auto& item = layout.items().first();
        QCOMPARE( item.trimBox.width(), 20.0 );
        QCOMPARE( item.bleedBox.width(), 25.0 );
        QCOMPARE( item.trimBox.center(), item.bleedBox.center() );
}


void TestSheetLayout::nothingFits()
{
        auto layout = SheetLayout::compute( 100, 100, 120, 20, 0, 0 );
        QCOMPARE( layout.perSheet(), 0 );
        QCOMPARE( layout.sheetsFor( 10 ), 0 );

        // Fits only when turned
        layout = SheetLayout::compute( 100, 200, 120, 20, 0, 0 );
        QCOMPARE( layout.perSheet(), 5 );
        QCOMPARE( layout.rotatedCount(), 5 );
}


void TestSheetLayout::sheetsAndWaste()
{
        auto layout = SheetLayout::compute( 100, 100, 50, 50, 0, 0 );
        QCOMPARE( layout.perSheet(), 4 );
        QCOMPARE( layout.sheetsFor( 4 ), 1 );
        QCOMPARE( layout.sheetsFor( 5 ), 2 );
        QCOMPARE( layout.sheetWaste(), 0.0 );
        QCOMPARE( layout.jobWaste( 3 ), 0.25 );

        layout = SheetLayout::compute( 100, 100, 40, 40, 0, 0 );
        QCOMPARE( layout.sheetWaste(), 1.0 - 4*1600.0/10000.0 );
}
