//  SelectProductDialog.hpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
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

#ifndef SelectProductDialog_hpp
#define SelectProductDialog_hpp


#include "model/Template.hpp"

#include <QDialog>


class QCheckBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;


namespace glabels
{

        // Forward references
        class SimplePreview;
        class TemplatePicker;


        ///
        /// Select Product Dialog
        ///
        /// Layout:  search bar on top; a sidebar to browse (All / Recent / Favorites)
        /// and filter (paper size, category); the product grid; and a details pane
        /// with a preview of the selected product.
        ///
        class SelectProductDialog : public QDialog
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                SelectProductDialog( QWidget *parent = nullptr );
                ~SelectProductDialog() override;


                /////////////////////////////////
                // Accessors
                /////////////////////////////////
                model::Template tmplate() const;


                /////////////////////////////////
                // Events
                /////////////////////////////////
        protected:
                bool eventFilter( QObject* watched, QEvent* event ) override;
                void keyPressEvent( QKeyEvent* event ) override;


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                enum Scope { SCOPE_ALL = 0, SCOPE_RECENT = 1, SCOPE_FAVORITES = 2 };

                QWidget* createSidebar();
                QWidget* createResultsPane();
                QWidget* createDetailsPane();
                QWidget* createFooter();

                QLabel*  createSectionTitle( const QString& text );
                void     applyStyle();

                Scope    scope() const;
                void     setScope( Scope scope );
                void     applyFilter();
                void     updateScopeCounts();
                void     updateResultsHeader();
                void     updateDetails();
                void     updateViewModeButtons();
                bool     filtersActive() const;
                QStringList checkedCategoryIds() const;
                void     clearFilters();
                void     saveFilterSettings();
                void     chooseSelected();
                void     openTemplateDesigner();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QSplitter*      mSplitter{ nullptr };

                // Header
                QLineEdit*      mSearchEntry{ nullptr };
                QTimer*         mSearchTimer{ nullptr };

                // Sidebar
                QListWidget*    mScopeList{ nullptr };
                QWidget*        mFiltersWidget{ nullptr };
                QToolButton*    mIsoChip{ nullptr };
                QToolButton*    mUsChip{ nullptr };
                QToolButton*    mOtherChip{ nullptr };
                QList<QCheckBox*> mCategoryChecks;
                QLabel*         mClearCategoriesLink{ nullptr };

                // Results
                QLabel*         mResultsLabel{ nullptr };
                QLabel*         mClearFiltersLink{ nullptr };
                QToolButton*    mGridButton{ nullptr };
                QToolButton*    mListButton{ nullptr };
                QStackedWidget* mResultsStack{ nullptr };
                TemplatePicker* mPicker{ nullptr };
                QLabel*         mEmptyLabel{ nullptr };

                // Details
                QStackedWidget* mDetailsStack{ nullptr };
                SimplePreview*  mPreview{ nullptr };
                QLabel*         mTitleLabel{ nullptr };
                QLabel*         mDescriptionLabel{ nullptr };
                QLabel*         mBrandValue{ nullptr };
                QLabel*         mPartValue{ nullptr };
                QLabel*         mLabelSizeValue{ nullptr };
                QLabel*         mLayoutValue{ nullptr };
                QLabel*         mPageSizeValue{ nullptr };
                QLabel*         mEquivLabel{ nullptr };
                QLabel*         mEquivValue{ nullptr };
                QToolButton*    mFavoriteButton{ nullptr };

                // Footer
                QPushButton*    mSelectButton{ nullptr };

                bool            mHasSelection{ false };
                bool            mUpdatingCategories{ false };

        };

}


#endif // SelectProductDialog_hpp
