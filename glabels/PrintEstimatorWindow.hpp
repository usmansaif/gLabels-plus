//  PrintEstimatorWindow.hpp
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

#ifndef PrintEstimatorWindow_hpp
#define PrintEstimatorWindow_hpp


#include "model/SheetLayout.hpp"
#include "model/Template.hpp"
#include "model/Units.hpp"

#include <QMainWindow>
#include <QPointer>


class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPainter;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimer;


namespace glabels
{

        class SheetPreview;


        ///
        /// Print Sheet Estimator (Tools menu): an independent window that works out
        /// how to lay a label or page out on a commercial printing sheet with the
        /// least waste, and how many sheets a job needs.
        ///
        class PrintEstimatorWindow : public QMainWindow
        {
                Q_OBJECT

        public:
                /// Show (or raise) the single estimator window; a non-null template
                /// becomes the product to estimate.
                static void showWindow( const model::Template& tmplate );

        protected:
                void closeEvent( QCloseEvent* event ) override;

        private:
                PrintEstimatorWindow();

                enum ProductType { LABEL = 0, PAGE = 1, CUSTOM = 2 };

                struct Line
                {
                        QString key;
                        QString value;
                };

                void      buildUi();
                void      loadSettings();
                void      saveSettings() const;

                void      setTemplate( const model::Template& tmplate );
                void      chooseProduct();
                void      clearProduct();
                void      onProductTypeChanged();
                void      onSheetChanged();
                void      onUnitsChanged();
                void      scheduleEstimate();
                void      estimate();
                void      saveEstimate();

                ProductType productType() const;
                double    standardBleedPts( ProductType type ) const;
                double    toPts( double value ) const;
                double    fromPts( double pts ) const;
                void      setSpinPts( QDoubleSpinBox* spin, double pts );
                void      configureSpin( QDoubleSpinBox* spin, double maxPts );
                QString   formatLength( double pts ) const;
                QString   formatSize( double wPts, double hPts ) const;
                QString   productDescription() const;
                QString   sheetDescription() const;
                void      setStatus( const QString& text, bool isError );
                void      showError( const QString& message );

                QList<Line> reportLines() const;
                QString   reportText() const;
                void      renderReport( QPainter& painter, const QSizeF& canvas ) const;
                bool      writeText( const QString& fileName ) const;
                bool      writeImage( const QString& fileName ) const;
                bool      writePdf( const QString& fileName ) const;

                static QPointer<PrintEstimatorWindow> sInstance;

                // Inputs
                model::Units    mUnits;
                model::Template mTemplate;
                QComboBox*      mUnitsCombo{ nullptr };
                QButtonGroup*   mTypeGroup{ nullptr };
                QRadioButton*   mLabelRadio{ nullptr };
                QRadioButton*   mPageRadio{ nullptr };
                QRadioButton*   mCustomRadio{ nullptr };
                QLabel*         mProductLabel{ nullptr };
                QPushButton*    mChooseButton{ nullptr };
                QPushButton*    mClearButton{ nullptr };
                QDoubleSpinBox* mItemWSpin{ nullptr };
                QDoubleSpinBox* mItemHSpin{ nullptr };
                QSpinBox*       mQuantitySpin{ nullptr };
                QComboBox*      mSheetCombo{ nullptr };
                QDoubleSpinBox* mSheetWSpin{ nullptr };
                QDoubleSpinBox* mSheetHSpin{ nullptr };
                QDoubleSpinBox* mMarginSpin{ nullptr };
                QDoubleSpinBox* mBleedSpin{ nullptr };
                QLabel*         mBleedHint{ nullptr };
                QTimer*         mEstimateTimer{ nullptr };

                // Results
                SheetPreview*   mPreview{ nullptr };
                QLabel*         mPerSheetValue{ nullptr };
                QLabel*         mSheetsValue{ nullptr };
                QLabel*         mWasteValue{ nullptr };
                QLabel*         mDetailsLabel{ nullptr };
                QLabel*         mStatusLabel{ nullptr };
                QPushButton*    mSaveButton{ nullptr };

                // Last estimate (what is shown and saved)
                bool              mHasEstimate{ false };
                model::SheetLayout mLayout;
                int               mQuantity{ 0 };
                QString           mProductText;
                QString           mSheetText;
                bool              mUpdating{ false };
        };

}


#endif // PrintEstimatorWindow_hpp
