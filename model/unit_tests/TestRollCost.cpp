//  TestRollCost.cpp
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

#include "TestRollCost.hpp"

#include "model/RollCost.hpp"


QTEST_MAIN(TestRollCost)


using namespace glabels::model;


namespace
{
        // 50 x 25 mm labels with a 3 mm gap (in mm), rolls of 1000 at 500,
        // ribbon of 300 m at 1200
        RollCost::Input example()
        {
                RollCost::Input in;
                in.quantity      = 1000;
                in.labelLength   = 25;
                in.gap           = 3;
                in.across        = 1;
                in.labelsPerRoll = 1000;
                in.rollPrice     = 500;
                in.useRibbon     = true;
                in.ribbonLength  = 300000;
                in.ribbonPrice   = 1200;
                return in;
        }
}


void TestRollCost::example()
{
        auto r = RollCost::compute( ::example() );
        QVERIFY( r.isValid() );

        QCOMPARE( r.printedLabels(), 1000 );
        QCOMPARE( r.rows(), 1000 );
        QCOMPARE( r.rolls(), 1 );
        QCOMPARE( r.labelsLeft(), 0 );
        QCOMPARE( r.labelCost(), 500.0 );
        QCOMPARE( r.labelUsedCost(), 500.0 );

        QCOMPARE( r.ribbonUsed(), 28000.0 );
        QCOMPARE( r.ribbons(), 1 );
        QCOMPARE( r.ribbonLeft(), 272000.0 );
        QCOMPARE( r.ribbonCost(), 1200.0 );
        QCOMPARE( r.ribbonUsedCost(), 112.0 );

        QCOMPARE( r.totalCost(), 1700.0 );
        QCOMPARE( r.usedCost(), 612.0 );
        QCOMPARE( r.costPerLabel(), 0.612 );
}


void TestRollCost::labelsAcross()
{
        auto in = ::example();
        in.quantity = 1001;
        in.across   = 2;
        auto r = RollCost::compute( in );
        QVERIFY( r.isValid() );

        // 1001 labels two across: 501 rows, ribbon moves once per row
        QCOMPARE( r.rows(), 501 );
        QCOMPARE( r.ribbonUsed(), 501 * 28.0 );
        QCOMPARE( r.rolls(), 2 );
        QCOMPARE( r.labelsLeft(), 999 );
        QCOMPARE( r.labelCost(), 1000.0 );
}


void TestRollCost::waste()
{
        auto in = ::example();
        in.wastePercent = 2.5;
        auto r = RollCost::compute( in );
        QVERIFY( r.isValid() );

        QCOMPARE( r.wasteLabels(), 25 );
        QCOMPARE( r.printedLabels(), 1025 );
        QCOMPARE( r.rolls(), 2 );
        QCOMPARE( r.ribbonUsed(), 1025 * 28.0 );

        // Cost per label is per good label, so waste raises it
        QVERIFY( r.costPerLabel() > 0.612 );
}


void TestRollCost::exactFit()
{
        // Exactly one ribbon's worth must not round up to two
        auto in = ::example();
        in.ribbonLength = 28000;
        auto r = RollCost::compute( in );
        QCOMPARE( r.ribbons(), 1 );
        QCOMPARE( r.ribbonLeft(), 0.0 );

        in.quantity = 1001;
        r = RollCost::compute( in );
        QCOMPARE( r.ribbons(), 2 );
}


void TestRollCost::directThermal()
{
        auto in = ::example();
        in.useRibbon    = false;
        in.ribbonLength = 0;
        auto r = RollCost::compute( in );
        QVERIFY( r.isValid() );

        QCOMPARE( r.ribbons(), 0 );
        QCOMPARE( r.ribbonUsed(), 0.0 );
        QCOMPARE( r.ribbonCost(), 0.0 );
        QCOMPARE( r.totalCost(), 500.0 );
        QCOMPARE( r.costPerLabel(), 0.5 );
}


void TestRollCost::invalidInput()
{
        auto in = ::example();
        in.quantity = 0;
        QVERIFY( !RollCost::compute( in ).isValid() );

        in = ::example();
        in.labelsPerRoll = 0;
        QVERIFY( !RollCost::compute( in ).isValid() );

        in = ::example();
        in.ribbonLength = 0;
        QVERIFY( !RollCost::compute( in ).isValid() );

        in = ::example();
        in.across = 0;
        QVERIFY( !RollCost::compute( in ).isValid() );
}
