//  TestHandles.cpp
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

#include "TestHandles.hpp"

#include "model/Handle.hpp"
#include "model/ModelBoxObject.hpp"

QTEST_MAIN(TestHandles)

using namespace glabels::model;

namespace
{
        // A 100 x 50 pt box at (10, 20), viewed at 1:1 so handle sizes are in points
        ModelBoxObject* makeBox()
        {
                auto* box = new ModelBoxObject( Distance::pt( 10 ), Distance::pt( 20 ), Distance::pt( 100 ), Distance::pt( 50 ),
                                                false, Distance::pt( 1 ), ColorNode(), ColorNode() );
                box->select();
                return box;
        }

        Handle::Location at( ModelObject* object, double x, double y )
        {
                return object->handleAt( 1.0, Distance::pt( x ), Distance::pt( y ) ).location();
        }
}


void TestHandles::edgesAnywhere()
{
        auto* box = makeBox();

        // Anywhere along each edge, not only at the middle handle
        QCOMPARE( at( box, 30, 20 ), Handle::N );      // top edge, near the left
        QCOMPARE( at( box, 95, 22 ), Handle::N );      // top edge, just inside
        QCOMPARE( at( box, 30, 70 ), Handle::S );
        QCOMPARE( at( box, 10, 30 ), Handle::W );
        QCOMPARE( at( box, 112, 60 ), Handle::E );     // just outside the right edge

        // The middle of the box is not a handle
        QCOMPARE( at( box, 60, 45 ), Handle::NULL_HANDLE );

        delete box;
}


void TestHandles::cornersWinOverEdges()
{
        auto* box = makeBox();

        QCOMPARE( at( box, 10, 20 ), Handle::NW );
        QCOMPARE( at( box, 14, 23 ), Handle::NW );     // inside the larger corner grab area
        QCOMPARE( at( box, 110, 70 ), Handle::SE );
        QCOMPARE( at( box, 106, 67 ), Handle::SE );

        delete box;
}


void TestHandles::rotatedObject()
{
        auto* box = makeBox();
        box->setMatrix( QTransform().rotate( 90 ) );   // about (x0, y0): the box now hangs down-left of it

        // Local top edge (0..100, 0) is now the vertical line x = 10, y = 20..120
        QCOMPARE( at( box, 10, 60 ), Handle::N );
        // Local left edge (0, 0..50) is now the horizontal line y = 20, x = -40..10
        QCOMPARE( at( box, -15, 20 ), Handle::W );

        delete box;
}


void TestHandles::unselectedHasNoHandles()
{
        auto* box = makeBox();
        box->unselect();

        QCOMPARE( at( box, 30, 20 ), Handle::NULL_HANDLE );
        QCOMPARE( at( box, 10, 20 ), Handle::NULL_HANDLE );

        delete box;
}
