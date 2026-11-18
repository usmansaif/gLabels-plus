//  PreferencesDialog.hpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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

#ifndef PreferencesDialog_hpp
#define PreferencesDialog_hpp


#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>


namespace glabels
{

        ///
        /// Preferences Dialog
        ///
        /// Settings are organized into categories listed on the left. Every change
        /// is applied immediately.
        ///
        class PreferencesDialog : public QDialog
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                PreferencesDialog( QWidget *parent = nullptr );


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onUnitsChanged();
                void onGridOriginChanged();
                void onGridSpacingSpinChanged();
                void onGridSpacingResetButtonClicked();
                void onAppearanceChanged();
                void onStartupLocationChanged();
                void onStartupOtherBrowseButtonClicked();
                void onClearRecentFilesClicked();
                void onShortcutsButtonClicked();
                void onRestoreDefaultsClicked();
                void onSearchTextChanged( const QString& text );
                void onSettingsChanged();


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                QWidget* addPage( const QString& iconName, const QString& title,
                                  const QString& description, const QStringList& keywords );
                QGroupBox* addCard( QWidget* page, const QString& title );
                QLabel* addHelp( QLayout* layout, const QString& text );
                QRadioButton* addRadio( QLayout* layout, QButtonGroup* group,
                                        const QString& text, const QString& help = QString() );

                void buildGeneralPage();
                void buildUnitsPage();
                void buildEditorPage();
                void buildAppearancePage();
                void buildLayoutPage();
                void buildPrintingPage();
                void buildKeyboardPage();

                void loadFromSettings();
                void updateUnitsExample();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QLineEdit*      mSearchEdit;
                QListWidget*    mCategoryList;
                QStackedWidget* mPages;
                QList<QStringList> mPageKeywords;
                bool            mBlocked{ false };

                // General
                QButtonGroup*   mStartupGroup;
                QRadioButton*   mStartupParentRadio;
                QRadioButton*   mStartupHomeRadio;
                QRadioButton*   mStartupDocumentsRadio;
                QRadioButton*   mStartupOtherRadio;
                QLineEdit*      mStartupOtherEdit;
                QPushButton*    mStartupBrowseButton;
                QPushButton*    mClearRecentButton;

                // Units
                QButtonGroup*   mUnitsGroup;
                QLabel*         mUnitsExampleLabel;
                QCheckBox*      mPropertiesUnitsCheck;
                QComboBox*      mPageFamilyCombo;

                // Editor
                QCheckBox*      mShowGridCheck;
                QCheckBox*      mShowMarkupCheck;
                QCheckBox*      mShowRulersCheck;
                QButtonGroup*   mGridOriginGroup;
                QDoubleSpinBox* mGridSpacingSpin;
                QComboBox*      mAlignRefCombo;

                // Appearance
                QButtonGroup*   mColorSchemeGroup;
                QRadioButton*   mSystemSchemeRadio;
                QButtonGroup*   mIconStyleGroup;

                // Layout
                QComboBox*      mSidebarStyleCombo;
                QComboBox*      mSidebarIconSizeCombo;
                QComboBox*      mToolbarStyleCombo;

                // Printing
                QCheckBox*      mPrintOutlinesCheck;
                QCheckBox*      mPrintCropMarksCheck;
        };

}


#endif // PreferencesDialog_hpp
