//  RollCostWindow.hpp
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

#ifndef RollCostWindow_hpp
#define RollCostWindow_hpp


#include "model/RollCost.hpp"
#include "model/Template.hpp"
#include "model/Units.hpp"

#include <QMainWindow>
#include <QPointer>


class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPainter;
class QPushButton;
class QSpinBox;


namespace glabels
{

        ///
        /// Ribbon & Label Cost Estimator (Tools menu): an independent window that works
        /// out how many label rolls and ink ribbons a thermal printing job needs and
        /// what they cost.
        ///
        class RollCostWindow : public QMainWindow
        {
                Q_OBJECT

        public:
                /// Show (or raise) the single window; a roll product (the open label) fills in the label
                static void showWindow( const model::Template& tmplate );

        protected:
                void closeEvent( QCloseEvent* event ) override;

        private:
                RollCostWindow();

                struct Line
                {
                        QString key;
                        QString value;
                        bool    heading{ false };
                        bool    strong{ false };
                        bool    total{ false };    // heading of the Totals section
                };

                void      buildUi();
                void      loadSettings();
                void      saveSettings() const;

                void      setTemplate( const model::Template& tmplate );
                void      applyTemplate( const model::Template& tmplate );
                void      chooseProduct();
                void      clearProduct();
                void      updateProductLabel();
                void      onUnitsChanged();
                void      onRibbonUnitChanged();
                void      onCurrencyChanged();
                void      onRibbonToggled();
                void      calculate();
                void      showSections();
                void      saveEstimate();

                QString   currency() const;
                QString   money( double amount ) const;
                double    toPts( double value ) const;
                double    fromPts( double pts ) const;
                double    ribbonToPts( double value ) const;
                double    ribbonFromPts( double pts ) const;
                void      configureSpin( QDoubleSpinBox* spin, double maxPts );
                QString   formatLength( double pts ) const;
                QString   formatRibbon( double pts ) const;
                void      setStatus( const QString& text, bool isError );
                void      showError( const QString& message );

                QList<Line> reportLines() const;
                QString   reportText() const;
                void      renderReport( QPainter& painter, const QSizeF& canvas ) const;
                bool      writeText( const QString& fileName ) const;
                bool      writeImage( const QString& fileName ) const;
                bool      writePdf( const QString& fileName ) const;

                static QPointer<RollCostWindow> sInstance;

                // Inputs
                model::Units    mUnits;
                QString         mRibbonUnit;           // "m" or "ft"
                QComboBox*      mCurrencyCombo{ nullptr };
                QLineEdit*      mCurrencyEdit{ nullptr };
                QComboBox*      mUnitsCombo{ nullptr };
                model::Template mTemplate;
                QLabel*         mProductLabel{ nullptr };
                QPushButton*    mChooseButton{ nullptr };
                QPushButton*    mClearButton{ nullptr };
                double          mProductW{ 0 };        // label size of the chosen product
                double          mProductL{ 0 };

                QDoubleSpinBox* mLabelWSpin{ nullptr };
                QDoubleSpinBox* mLabelLSpin{ nullptr };
                QDoubleSpinBox* mGapSpin{ nullptr };
                QSpinBox*       mAcrossSpin{ nullptr };
                QSpinBox*       mPerRollSpin{ nullptr };
                QDoubleSpinBox* mRollPriceSpin{ nullptr };

                QCheckBox*      mNoRibbonCheck{ nullptr };
                QGroupBox*      mRibbonBox{ nullptr };
                QDoubleSpinBox* mRibbonLSpin{ nullptr };
                QComboBox*      mRibbonUnitCombo{ nullptr };
                QDoubleSpinBox* mRibbonWSpin{ nullptr };
                QDoubleSpinBox* mRibbonPriceSpin{ nullptr };

                QSpinBox*       mQuantitySpin{ nullptr };
                QDoubleSpinBox* mWasteSpin{ nullptr };

                // Results
                QLabel*         mRibbonsValue{ nullptr };
                QLabel*         mRibbonsAmount{ nullptr };
                QLabel*         mRollsValue{ nullptr };
                QLabel*         mRollsAmount{ nullptr };
                QLabel*         mTotalValue{ nullptr };
                QLabel*         mTotalNote{ nullptr };
                QWidget*        mSections{ nullptr };
                QGridLayout*    mSectionsLayout{ nullptr };
                QLabel*         mStatusLabel{ nullptr };
                QPushButton*    mSaveButton{ nullptr };

                // Last calculation (what is shown and saved)
                bool            mHasResult{ false };
                model::RollCost mResult;
                double          mLabelW{ 0 };
                double          mRibbonW{ 0 };
                QString         mWarning;
                bool            mUpdating{ false };
        };

}


#endif // RollCostWindow_hpp
