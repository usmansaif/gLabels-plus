//  RollCost.cpp
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

#include "RollCost.hpp"

#include <algorithm>
#include <cmath>


namespace glabels::model
{

        namespace
        {
                // Whole units needed for a amount; tolerates rounding error so that
                // exactly 300 m of a 300 m ribbon is 1 ribbon, not 2
                int wholeUnits( double amount, double unit )
                {
                        double n = amount / unit;
                        return int( std::ceil( n - 1e-9 ) );
                }
        }


        RollCost RollCost::compute( const Input& input )
        {
                RollCost r;
                r.mInput = input;

                if ( input.quantity <= 0 || input.across <= 0 || input.labelsPerRoll <= 0 ||
                     input.wastePercent < 0 || input.rollPrice < 0 )
                {
                        return r;
                }
                if ( input.useRibbon &&
                     (input.labelLength <= 0 || input.gap < 0 || input.ribbonLength <= 0 || input.ribbonPrice < 0) )
                {
                        return r;
                }

                // Labels printed, including setup and test prints
                r.mWasteLabels = int( std::ceil( input.quantity * input.wastePercent / 100.0 - 1e-9 ) );
                r.mPrinted     = input.quantity + r.mWasteLabels;
                r.mRows        = (r.mPrinted + input.across - 1) / input.across;

                // Label rolls
                r.mRolls         = (r.mPrinted + input.labelsPerRoll - 1) / input.labelsPerRoll;
                r.mLabelsLeft    = r.mRolls * input.labelsPerRoll - r.mPrinted;
                r.mLabelCost     = r.mRolls * input.rollPrice;
                r.mLabelUsedCost = r.mPrinted * input.rollPrice / input.labelsPerRoll;

                // Ribbon: one label length and gap per row
                if ( input.useRibbon )
                {
                        r.mRibbonUsed     = r.mRows * (input.labelLength + input.gap);
                        r.mRibbons        = std::max( 1, wholeUnits( r.mRibbonUsed, input.ribbonLength ) );
                        r.mRibbonLeft     = std::max( 0.0, r.mRibbons * input.ribbonLength - r.mRibbonUsed );
                        r.mRibbonCost     = r.mRibbons * input.ribbonPrice;
                        r.mRibbonUsedCost = r.mRibbonUsed * input.ribbonPrice / input.ribbonLength;
                }

                r.mValid = true;
                return r;
        }


        double RollCost::costPerLabel() const
        {
                return (mValid && mInput.quantity > 0) ? usedCost() / mInput.quantity : 0;
        }

}
