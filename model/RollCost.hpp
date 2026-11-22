//  RollCost.hpp
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

#ifndef model_RollCost_hpp
#define model_RollCost_hpp


namespace glabels::model
{

        ///
        /// Material and cost of a label job on a thermal printer: label rolls and
        /// (for thermal transfer) ink ribbons.
        ///
        /// The ribbon moves past every row of labels and the gap after it, so a job
        /// uses rows x (label length + gap) of ribbon; labels side by side on the
        /// roll (labels across) share one row.  Waste is extra labels printed for
        /// setup and test prints; they use labels and ribbon like any other.
        ///
        /// Lengths are in points (any one unit works, as long as all match).
        ///
        class RollCost
        {
        public:
                struct Input
                {
                        int    quantity{ 0 };        // good labels needed
                        double wastePercent{ 0 };    // extra labels printed, % of quantity

                        double labelLength{ 0 };     // label size along the roll (feed direction)
                        double gap{ 0 };             // gap between rows of labels
                        int    across{ 1 };          // labels side by side on the roll
                        int    labelsPerRoll{ 0 };
                        double rollPrice{ 0 };

                        bool   useRibbon{ true };    // false: direct thermal, no ribbon
                        double ribbonLength{ 0 };
                        double ribbonPrice{ 0 };
                };

                /// Work out the job; invalid input gives an invalid result
                static RollCost compute( const Input& input );

                bool   isValid() const { return mValid; }
                const Input& input() const { return mInput; }

                // Labels
                int    wasteLabels() const { return mWasteLabels; }    // extra labels printed
                int    printedLabels() const { return mPrinted; }      // quantity + waste
                int    rows() const { return mRows; }                  // rows of labels printed
                int    rolls() const { return mRolls; }                // whole rolls to buy
                int    labelsLeft() const { return mLabelsLeft; }      // unused labels on the last roll
                double labelCost() const { return mLabelCost; }        // rolls x roll price
                double labelUsedCost() const { return mLabelUsedCost; }// labels printed x price per label

                // Ribbon (all zero without a ribbon)
                double ribbonUsed() const { return mRibbonUsed; }      // length of ribbon used
                int    ribbons() const { return mRibbons; }            // whole ribbons to buy
                double ribbonLeft() const { return mRibbonLeft; }      // unused length of the last ribbon
                double ribbonCost() const { return mRibbonCost; }      // ribbons x ribbon price
                double ribbonUsedCost() const { return mRibbonUsedCost; } // length used x price per length

                // Totals
                double totalCost() const { return mLabelCost + mRibbonCost; }         // to buy
                double usedCost() const { return mLabelUsedCost + mRibbonUsedCost; }  // used by this job
                double costPerLabel() const;                                           // used cost per good label

        private:
                bool   mValid{ false };
                Input  mInput;

                int    mWasteLabels{ 0 };
                int    mPrinted{ 0 };
                int    mRows{ 0 };
                int    mRolls{ 0 };
                int    mLabelsLeft{ 0 };
                double mLabelCost{ 0 };
                double mLabelUsedCost{ 0 };

                double mRibbonUsed{ 0 };
                int    mRibbons{ 0 };
                double mRibbonLeft{ 0 };
                double mRibbonCost{ 0 };
                double mRibbonUsedCost{ 0 };
        };

}


#endif // model_RollCost_hpp
