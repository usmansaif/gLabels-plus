//  SelectProductDialog.cpp
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


#include "SelectProductDialog.hpp"

#include "DialogUtil.hpp"
#include "SimplePreview.hpp"
#include "TemplateDesigner.hpp"
#include "TemplatePicker.hpp"

#include "model/Db.hpp"
#include "model/Settings.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>


namespace glabels
{

        ///
        /// Constructor
        ///
        SelectProductDialog::SelectProductDialog( QWidget *parent )
                : QDialog(parent)
        {
                setObjectName( "selectProductDialog" );
                setWindowTitle( tr("gLabels Plus - Select Product") );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 0, 0, 0, 0 );
                root->setSpacing( 0 );

                //
                // Header: title + search
                //
                auto* header = new QWidget();
                header->setObjectName( "spHeader" );
                auto* headerLayout = new QHBoxLayout( header );
                headerLayout->setContentsMargins( 16, 12, 16, 12 );
                headerLayout->setSpacing( 16 );

                auto* titleBox = new QVBoxLayout();
                titleBox->setSpacing( 2 );
                auto* title = new QLabel( tr("Choose a product") );
                title->setObjectName( "spTitle" );
                auto* subtitle = new QLabel( tr("Pick the label, card or sheet you will print on.") );
                subtitle->setObjectName( "spMuted" );
                titleBox->addWidget( title );
                titleBox->addWidget( subtitle );
                headerLayout->addLayout( titleBox );
                headerLayout->addStretch( 1 );

                mSearchEntry = new QLineEdit();
                mSearchEntry->setObjectName( "spSearch" );
                mSearchEntry->setPlaceholderText( tr("Search brand, part #, description or size (e.g. avery 5160, 2x4)") );
                mSearchEntry->setClearButtonEnabled( true );
                mSearchEntry->setToolTip( tr("Type one or more words. Partial words and sizes like 2x4 or 50 x 25 also work.\n"
                                             "Ctrl+F to focus, Down to move into the results, Enter to choose.") );
                mSearchEntry->setMinimumWidth( 220 );
                mSearchEntry->setMaximumWidth( 460 );
                mSearchEntry->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                mSearchEntry->installEventFilter( this );
                headerLayout->addWidget( mSearchEntry, 2 );

                root->addWidget( header );

                //
                // Body: sidebar | results | details
                //
                mSplitter = new QSplitter( Qt::Horizontal );
                mSplitter->setObjectName( "spSplitter" );
                mSplitter->setChildrenCollapsible( false );
                mSplitter->setHandleWidth( 1 );
                mSplitter->addWidget( createSidebar() );
                mSplitter->addWidget( createResultsPane() );
                mSplitter->addWidget( createDetailsPane() );
                mSplitter->setStretchFactor( 0, 0 );
                mSplitter->setStretchFactor( 1, 1 );
                mSplitter->setStretchFactor( 2, 0 );
                root->addWidget( mSplitter, 1 );

                root->addWidget( createFooter() );

                applyStyle();

                // Debounce typing: fuzzy matching over the whole product database
                mSearchTimer = new QTimer( this );
                mSearchTimer->setSingleShot( true );
                mSearchTimer->setInterval( 150 );
                connect( mSearchTimer, &QTimer::timeout, this, &SelectProductDialog::applyFilter );
                connect( mSearchEntry, &QLineEdit::textChanged, this, [this]( const QString& text ) {
                        // Searching always looks through all products
                        if ( !text.trimmed().isEmpty() && scope() != SCOPE_ALL )
                        {
                                setScope( SCOPE_ALL );
                        }
                        mSearchTimer->start();
                } );

                auto* findShortcut = new QShortcut( QKeySequence::Find, this );
                connect( findShortcut, &QShortcut::activated, this, [this]{
                        mSearchEntry->setFocus();
                        mSearchEntry->selectAll();
                } );

                // Populate
                mPicker->setTemplates( model::Db::templates() );
                updateViewModeButtons();
                updateScopeCounts();

                setScope( model::Settings::recentTemplateList().isEmpty() ? SCOPE_ALL : SCOPE_RECENT );
                updateDetails();

                // A real top-level window, so the window manager offers minimize/maximize/close
                DialogUtil::makeResizableWindow( this, true );

                // Restore last size, or pick a size that fits comfortably on this screen
                QSettings settings;
                settings.beginGroup( "SelectProductDialog" );
                QByteArray geometry = settings.value( "geometry" ).toByteArray();
                QByteArray panes    = settings.value( "layout" ).toByteArray();
                settings.endGroup();

                if ( geometry.isEmpty() || !restoreGeometry( geometry ) )
                {
                        QScreen* screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
                        QSize avail = screen ? screen->availableSize() : QSize( 1280, 800 );
                        resize( qMin( 1180, int(avail.width()  * 0.85) ),
                                qMin(  780, int(avail.height() * 0.85) ) );
                        if ( parent )
                        {
                                QRect r = parent->window()->frameGeometry();
                                move( r.center() - rect().center() );
                        }
                }
                if ( panes.isEmpty() || !mSplitter->restoreState( panes ) )
                {
                        mSplitter->setSizes( { 230, 620, 320 } );
                }

                mSearchEntry->setFocus();
        }


        ///
        /// Destructor
        ///
        SelectProductDialog::~SelectProductDialog()
        {
                QSettings settings;
                settings.beginGroup( "SelectProductDialog" );
                settings.setValue( "geometry", saveGeometry() );
                settings.setValue( "layout",   mSplitter->saveState() );
                settings.endGroup();
        }


        ///
        /// Get selected template
        ///
        model::Template SelectProductDialog::tmplate() const
        {
                if ( mHasSelection )
                {
                        return mPicker->selectedTemplate();
                }
                return model::Template();
        }


        ///
        /// Section title helper
        ///
        QLabel* SelectProductDialog::createSectionTitle( const QString& text )
        {
                auto* label = new QLabel( text.toUpper() );
                label->setObjectName( "spSection" );
                return label;
        }


        ///
        /// Sidebar: browse scopes and filters
        ///
        QWidget* SelectProductDialog::createSidebar()
        {
                auto* sidebar = new QWidget();
                sidebar->setObjectName( "spSidebar" );
                sidebar->setMinimumWidth( 190 );

                auto* layout = new QVBoxLayout( sidebar );
                layout->setContentsMargins( 12, 12, 12, 12 );
                layout->setSpacing( 6 );

                // Browse
                layout->addWidget( createSectionTitle( tr("Browse") ) );
                mScopeList = new QListWidget();
                mScopeList->setObjectName( "spScopeList" );
                mScopeList->setFrameShape( QFrame::NoFrame );
                mScopeList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mScopeList->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mScopeList->setIconSize( QSize( 16, 16 ) );
                mScopeList->addItem( new QListWidgetItem( QIcon::fromTheme( "glabels-general" ),      tr("All products") ) );
                mScopeList->addItem( new QListWidgetItem( QIcon::fromTheme( "glabels-file-recent" ),  tr("Recently used") ) );
                mScopeList->addItem( new QListWidgetItem( QIcon::fromTheme( "glabels-favorite" ),     tr("Favorites") ) );
                mScopeList->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
                mScopeList->ensurePolished();
                mScopeList->setFixedHeight( 3 * qMax( mScopeList->sizeHintForRow( 0 ), fontMetrics().height() + 16 ) + 8 );
                connect( mScopeList, &QListWidget::currentRowChanged, this, [this]{ applyFilter(); } );
                layout->addWidget( mScopeList );

                // Filters (only meaningful when browsing all products)
                mFiltersWidget = new QWidget();
                auto* filters = new QVBoxLayout( mFiltersWidget );
                filters->setContentsMargins( 0, 10, 0, 0 );
                filters->setSpacing( 6 );

                filters->addWidget( createSectionTitle( tr("Paper size") ) );
                auto* chips = new QHBoxLayout();
                chips->setSpacing( 6 );
                auto makeChip = [&]( const QString& text, const QString& tip, bool checked ) {
                        auto* chip = new QToolButton();
                        chip->setObjectName( "spChip" );
                        chip->setText( text );
                        chip->setToolTip( tip );
                        chip->setCheckable( true );
                        chip->setChecked( checked );
                        chip->setCursor( Qt::PointingHandCursor );
                        connect( chip, &QToolButton::toggled, this, [this]{
                                saveFilterSettings();
                                applyFilter();
                        } );
                        chips->addWidget( chip );
                        return chip;
                };
                mIsoChip   = makeChip( tr("ISO"),   tr("A4, A5 and other ISO paper sizes"), model::Settings::searchIsoPaperSizes() );
                mUsChip    = makeChip( tr("US"),    tr("US Letter, Legal and other US paper sizes"), model::Settings::searchUsPaperSizes() );
                mOtherChip = makeChip( tr("Other"), tr("Rolls, custom and other paper sizes"), model::Settings::searchOtherPaperSizes() );
                chips->addStretch( 1 );
                filters->addLayout( chips );

                auto* catHeader = new QHBoxLayout();
                catHeader->setContentsMargins( 0, 10, 0, 0 );
                catHeader->addWidget( createSectionTitle( tr("Category") ) );
                catHeader->addStretch( 1 );
                mClearCategoriesLink = new QLabel( QString( "<a href='clear'>%1</a>" ).arg( tr("Clear") ) );
                mClearCategoriesLink->setToolTip( tr("Show all categories") );
                connect( mClearCategoriesLink, &QLabel::linkActivated, this, [this]{
                        mUpdatingCategories = true;
                        for ( auto* check : mCategoryChecks )
                        {
                                check->setChecked( false );
                        }
                        mUpdatingCategories = false;
                        saveFilterSettings();
                        applyFilter();
                } );
                catHeader->addWidget( mClearCategoriesLink );
                filters->addLayout( catHeader );

                auto* categoryScroll = new QScrollArea();
                categoryScroll->setFrameShape( QFrame::NoFrame );
                categoryScroll->setWidgetResizable( true );
                categoryScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                auto* categoryBox = new QWidget();
                categoryBox->setObjectName( "spCategoryBox" );
                categoryBox->setToolTip( tr("Tick categories to narrow the results. None ticked shows all categories.") );
                auto* categoryLayout = new QVBoxLayout( categoryBox );
                categoryLayout->setContentsMargins( 2, 0, 0, 0 );
                categoryLayout->setSpacing( 4 );

                bool allCategories = model::Settings::searchAllCategories();
                QStringList categoryIds = model::Settings::searchCategoryList();
                for ( auto& category : model::Db::categories() )
                {
                        auto* check = new QCheckBox( category.name() );
                        check->setProperty( "categoryId", category.id() );
                        check->setChecked( !allCategories && categoryIds.contains( category.id() ) );
                        connect( check, &QCheckBox::toggled, this, [this]{
                                if ( !mUpdatingCategories )
                                {
                                        saveFilterSettings();
                                        applyFilter();
                                }
                        } );
                        categoryLayout->addWidget( check );
                        mCategoryChecks << check;
                }
                categoryLayout->addStretch( 1 );
                categoryScroll->setWidget( categoryBox );
                filters->addWidget( categoryScroll, 1 );

                layout->addWidget( mFiltersWidget, 1 );

                return sidebar;
        }


        ///
        /// Results pane: summary bar, product grid and empty state
        ///
        QWidget* SelectProductDialog::createResultsPane()
        {
                auto* pane = new QWidget();
                pane->setObjectName( "spResults" );
                pane->setMinimumWidth( 260 );

                auto* layout = new QVBoxLayout( pane );
                layout->setContentsMargins( 12, 10, 12, 0 );
                layout->setSpacing( 8 );

                auto* bar = new QHBoxLayout();
                bar->setSpacing( 8 );
                mResultsLabel = new QLabel();
                mResultsLabel->setObjectName( "spResultsLabel" );
                bar->addWidget( mResultsLabel );

                bar->addSpacing( 6 );
                mClearFiltersLink = new QLabel( QString( "<a href='clear'>%1</a>" ).arg( tr("Clear search and filters") ) );
                connect( mClearFiltersLink, &QLabel::linkActivated, this, [this]{ clearFilters(); } );
                bar->addWidget( mClearFiltersLink );
                bar->addStretch( 1 );

                auto* segmented = new QHBoxLayout();
                segmented->setSpacing( 0 );
                bar->addLayout( segmented );
                auto makeViewButton = [&]( const QString& icon, const QString& tip, const char* segment ) {
                        auto* b = new QToolButton();
                        b->setObjectName( "spViewButton" );
                        b->setProperty( "segment", segment );
                        b->setIcon( QIcon::fromTheme( icon ) );
                        b->setIconSize( QSize( 16, 16 ) );
                        b->setToolTip( tip );
                        b->setCheckable( true );
                        b->setAutoExclusive( true );
                        segmented->addWidget( b );
                        return b;
                };
                mGridButton = makeViewButton( "glabels-view-grid", tr("Grid view"), "first" );
                mListButton = makeViewButton( "glabels-view-list", tr("List view"), "last" );
                connect( mGridButton, &QToolButton::clicked, this, [this]{ mPicker->setMode( QListView::IconMode ); updateViewModeButtons(); } );
                connect( mListButton, &QToolButton::clicked, this, [this]{ mPicker->setMode( QListView::ListMode ); updateViewModeButtons(); } );

                layout->addLayout( bar );

                mPicker = new TemplatePicker();
                mPicker->setObjectName( "spPicker" );
                mPicker->setFrameShape( QFrame::NoFrame );
                mPicker->setMinimumSize( 200, 150 );
                connect( mPicker, &TemplatePicker::selectionChanged, this, &SelectProductDialog::updateDetails );
                connect( mPicker, &QAbstractItemView::doubleClicked, this, [this]{ chooseSelected(); } );
                connect( mPicker, &TemplatePicker::favoritesChanged, this, [this]{
                        updateScopeCounts();
                        updateDetails();
                        if ( scope() == SCOPE_FAVORITES )
                        {
                                applyFilter();
                        }
                } );
                mPicker->installEventFilter( this );

                mEmptyLabel = new QLabel();
                mEmptyLabel->setObjectName( "spEmpty" );
                mEmptyLabel->setAlignment( Qt::AlignCenter );
                mEmptyLabel->setWordWrap( true );
                mEmptyLabel->setTextFormat( Qt::RichText );
                connect( mEmptyLabel, &QLabel::linkActivated, this, [this]( const QString& link ) {
                        if      ( link == "clear" )    clearFilters();
                        else if ( link == "all" )      setScope( SCOPE_ALL );
                        else if ( link == "designer" ) openTemplateDesigner();
                } );

                mResultsStack = new QStackedWidget();
                mResultsStack->addWidget( mPicker );
                mResultsStack->addWidget( mEmptyLabel );
                layout->addWidget( mResultsStack, 1 );

                return pane;
        }


        ///
        /// Details pane: preview and specification of the selected product
        ///
        QWidget* SelectProductDialog::createDetailsPane()
        {
                mDetailsStack = new QStackedWidget();
                mDetailsStack->setObjectName( "spDetails" );
                mDetailsStack->setMinimumWidth( 240 );

                // Placeholder
                auto* placeholder = new QLabel( tr("Select a product to see its details.\n\n"
                                                   "Double-click a product, or press Enter, to choose it.") );
                placeholder->setObjectName( "spPlaceholder" );
                placeholder->setAlignment( Qt::AlignCenter );
                placeholder->setWordWrap( true );
                placeholder->setMargin( 24 );
                mDetailsStack->addWidget( placeholder );

                // Details
                auto* scroll = new QScrollArea();
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidgetResizable( true );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

                auto* details = new QWidget();
                details->setObjectName( "spDetailsBody" );
                auto* layout = new QVBoxLayout( details );
                layout->setContentsMargins( 16, 14, 16, 14 );
                layout->setSpacing( 10 );

                mPreview = new SimplePreview();
                mPreview->setMinimumHeight( 200 );
                mPreview->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
                layout->addWidget( mPreview, 3 );

                auto* titleRow = new QHBoxLayout();
                mTitleLabel = new QLabel();
                mTitleLabel->setObjectName( "spProductTitle" );
                mTitleLabel->setWordWrap( true );
                mTitleLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
                titleRow->addWidget( mTitleLabel, 1 );

                mFavoriteButton = new QToolButton();
                mFavoriteButton->setObjectName( "spFavorite" );
                mFavoriteButton->setCheckable( true );
                mFavoriteButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
                mFavoriteButton->setCursor( Qt::PointingHandCursor );
                connect( mFavoriteButton, &QToolButton::clicked, this, [this]( bool checked ) {
                        mPicker->setSelectedFavorite( checked );
                } );
                titleRow->addWidget( mFavoriteButton, 0, Qt::AlignTop );
                layout->addLayout( titleRow );

                mDescriptionLabel = new QLabel();
                mDescriptionLabel->setObjectName( "spMuted" );
                mDescriptionLabel->setWordWrap( true );
                layout->addWidget( mDescriptionLabel );

                auto* form = new QFormLayout();
                form->setLabelAlignment( Qt::AlignLeft );
                form->setHorizontalSpacing( 14 );
                form->setVerticalSpacing( 6 );
                form->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
                auto addRow = [&]( const QString& name, QLabel** label ) {
                        auto* key = new QLabel( name );
                        key->setObjectName( "spKey" );
                        *label = new QLabel();
                        (*label)->setWordWrap( true );
                        (*label)->setTextInteractionFlags( Qt::TextBrowserInteraction );
                        (*label)->setOpenExternalLinks( true );
                        form->addRow( key, *label );
                        return key;
                };
                addRow( tr("Brand"),      &mBrandValue );
                addRow( tr("Part #"),     &mPartValue );
                addRow( tr("Label size"), &mLabelSizeValue );
                addRow( tr("Layout"),     &mLayoutValue );
                addRow( tr("Page size"),  &mPageSizeValue );
                mEquivLabel = addRow( tr("Also sold as"), &mEquivValue );
                layout->addLayout( form );
                layout->addStretch( 1 );

                scroll->setWidget( details );
                mDetailsStack->addWidget( scroll );

                return mDetailsStack;
        }


        ///
        /// Footer: custom template link and dialog buttons
        ///
        QWidget* SelectProductDialog::createFooter()
        {
                auto* footer = new QWidget();
                footer->setObjectName( "spFooter" );
                auto* layout = new QHBoxLayout( footer );
                layout->setContentsMargins( 16, 10, 16, 12 );
                layout->setSpacing( 8 );

                auto* designerLink = new QLabel( QString( "%1 <a href='designer'>%2</a>" )
                                                 .arg( tr("Can't find your product?"),
                                                       tr("Create a custom product template...") ) );
                designerLink->setObjectName( "spMuted" );
                connect( designerLink, &QLabel::linkActivated, this, [this]{ openTemplateDesigner(); } );
                layout->addWidget( designerLink );
                layout->addStretch( 1 );

                auto* cancelButton = new QPushButton( tr("Cancel") );
                cancelButton->setAutoDefault( false );
                connect( cancelButton, &QPushButton::clicked, this, &QDialog::reject );
                layout->addWidget( cancelButton );

                mSelectButton = new QPushButton( tr("Select") );
                mSelectButton->setObjectName( "spSelect" );
                mSelectButton->setDefault( true );
                mSelectButton->setEnabled( false );
                connect( mSelectButton, &QPushButton::clicked, this, &SelectProductDialog::chooseSelected );
                layout->addWidget( mSelectButton );

                return footer;
        }


        ///
        /// Look and feel (palette-based, so it follows light and dark themes)
        ///
        void SelectProductDialog::applyStyle()
        {
                // Secondary text: halfway between text and background colors
                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted( (fg.red()   + bg.red())   / 2 + (fg.red()   - bg.red())   / 6,
                              (fg.green() + bg.green()) / 2 + (fg.green() - bg.green()) / 6,
                              (fg.blue()  + bg.blue())  / 2 + (fg.blue()  - bg.blue())  / 6 );

                setStyleSheet( QString(
                        "#spHeader, #spFooter { background: palette(window); }"
                        "#spHeader { border-bottom: 1px solid palette(mid); }"
                        "#spFooter { border-top: 1px solid palette(mid); }"
                        "#spTitle { font-size: 14pt; font-weight: 600; }"
                        "#spMuted, #spPlaceholder, #spEmpty, #spKey, #spResultsLabel { color: %1; }"
                        "#spSection { color: %1; font-size: 8pt; font-weight: 600;"
                        "  letter-spacing: 1px; }"
                        "#spSearch { padding: 6px 8px; border: 1px solid palette(mid); border-radius: 6px;"
                        "  background: palette(base); }"
                        "#spSearch:focus { border: 1px solid palette(highlight); }"
                        "#spSidebar { background: palette(window); }"
                        "#spScopeList { background: transparent; outline: none; }"
                        "#spCategoryBox { background: palette(window); }"
                        "#spScopeList::item { padding: 6px 6px; border-radius: 6px; }"
                        "#spScopeList::item:hover { background: palette(midlight); }"
                        "#spScopeList::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
                        "QToolButton#spChip { padding: 3px 10px; border: 1px solid palette(mid); border-radius: 11px;"
                        "  background: palette(button); }"
                        "QToolButton#spChip:checked { background: palette(highlight); color: palette(highlighted-text);"
                        "  border-color: palette(highlight); }"
                        "#spResults, #spPicker, #spDetails, #spDetailsBody { background: palette(base); }"
                        "#spSplitter::handle { background: palette(mid); }"
                        "QToolButton#spViewButton { padding: 3px 6px; border: 1px solid palette(mid); background: palette(button); }"
                        "QToolButton#spViewButton[segment=\"first\"] { border-top-left-radius: 5px; border-bottom-left-radius: 5px;"
                        "  border-right: none; }"
                        "QToolButton#spViewButton[segment=\"last\"] { border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"
                        "QToolButton#spViewButton:checked { background: palette(highlight); border-color: palette(highlight); }"
                        "#spProductTitle { font-size: 13pt; font-weight: 600; }"
                        "QToolButton#spFavorite { padding: 3px 8px; border: 1px solid palette(mid); border-radius: 5px;"
                        "  background: palette(button); }"
                        "QToolButton#spFavorite:checked { border-color: #d4a017; color: #b8860b; }"
                        "#spEmpty { font-size: 11pt; padding: 24px; }"
                ).arg( muted.name() ) );
        }


        ///
        /// Current browse scope
        ///
        SelectProductDialog::Scope SelectProductDialog::scope() const
        {
                int row = mScopeList->currentRow();
                return ( row < 0 ) ? SCOPE_ALL : Scope( row );
        }


        void SelectProductDialog::setScope( Scope scope )
        {
                if ( mScopeList->currentRow() != int(scope) )
                {
                        mScopeList->setCurrentRow( int(scope) ); // triggers applyFilter()
                }
                else
                {
                        applyFilter();
                }
        }


        ///
        /// Apply scope, search text and filters to the product grid
        ///
        void SelectProductDialog::applyFilter()
        {
                mSearchTimer->stop();

                bool isAll = scope() == SCOPE_ALL;
                mFiltersWidget->setEnabled( isAll );
                mFiltersWidget->setToolTip( isAll ? QString() : tr("Filters apply when browsing all products") );

                switch ( scope() )
                {
                case SCOPE_RECENT:
                        mPicker->applyFilter( model::Settings::recentTemplateList() );
                        break;
                case SCOPE_FAVORITES:
                        mPicker->applyFilter( model::Settings::favoriteTemplateList() );
                        break;
                default:
                {
                        QStringList categoryIds = checkedCategoryIds();
                        mPicker->applyFilter( mSearchEntry->text(),
                                              mIsoChip->isChecked(), mUsChip->isChecked(), mOtherChip->isChecked(),
                                              categoryIds.isEmpty(), categoryIds );
                        mClearCategoriesLink->setVisible( !categoryIds.isEmpty() );
                        break;
                }
                }

                updateResultsHeader();
                updateDetails();
        }


        ///
        /// Counts next to each browse scope
        ///
        void SelectProductDialog::updateScopeCounts()
        {
                const int counts[3] = { mPicker->totalCount(),
                                        int(model::Settings::recentTemplateList().size()),
                                        int(model::Settings::favoriteTemplateList().size()) };
                const QString names[3] = { tr("All products"), tr("Recently used"), tr("Favorites") };
                for ( int i = 0; i < 3; i++ )
                {
                        mScopeList->item( i )->setText( QString( "%1  (%2)" ).arg( names[i] ).arg( counts[i] ) );
                }
        }


        ///
        /// Results summary and empty state
        ///
        void SelectProductDialog::updateResultsHeader()
        {
                int shown = mPicker->visibleCount();

                switch ( scope() )
                {
                case SCOPE_RECENT:
                        mResultsLabel->setText( shown == 1 ? tr("1 recently used product")
                                                           : tr("%1 recently used products").arg( shown ) );
                        break;
                case SCOPE_FAVORITES:
                        mResultsLabel->setText( shown == 1 ? tr("1 favorite")
                                                           : tr("%1 favorites").arg( shown ) );
                        break;
                default:
                        mResultsLabel->setText( filtersActive()
                                                ? tr("%1 of %2 products").arg( shown ).arg( mPicker->totalCount() )
                                                : ( shown == 1 ? tr("1 product") : tr("%1 products").arg( shown ) ) );
                        break;
                }
                mClearFiltersLink->setVisible( scope() == SCOPE_ALL && filtersActive() );

                if ( shown > 0 )
                {
                        mResultsStack->setCurrentWidget( mPicker );
                        return;
                }

                QString text;
                switch ( scope() )
                {
                case SCOPE_RECENT:
                        text = tr("<p><b>No recently used products yet.</b></p>"
                                  "<p>Products you create labels with will appear here.</p>"
                                  "<p><a href='all'>Browse all products</a></p>");
                        break;
                case SCOPE_FAVORITES:
                        text = tr("<p><b>No favorites yet.</b></p>"
                                  "<p>Select a product and click <i>Add to favorites</i>,<br>"
                                  "or right-click any product.</p>"
                                  "<p><a href='all'>Browse all products</a></p>");
                        break;
                default:
                        text = tr("<p><b>No products match.</b></p>"
                                  "<p>Try fewer or shorter words, or a size such as 2x4.</p>"
                                  "<p><a href='clear'>Clear search and filters</a> &nbsp;·&nbsp; "
                                  "<a href='designer'>Create a custom template</a></p>");
                        break;
                }
                mEmptyLabel->setText( text );
                mResultsStack->setCurrentWidget( mEmptyLabel );
        }


        ///
        /// Details of the selected product
        ///
        void SelectProductDialog::updateDetails()
        {
                auto tmplate = mPicker->selectedTemplate();
                if ( tmplate.isNull() )
                {
                        mDetailsStack->setCurrentIndex( 0 );
                        mSelectButton->setEnabled( false );
                        return;
                }

                auto frame = tmplate.frame();
                auto units = model::Settings::units();

                mPreview->setTemplate( tmplate );

                mTitleLabel->setText( QString( "%1 %2" ).arg( tmplate.brand(), tmplate.part() ) );
                mDescriptionLabel->setText( tmplate.description() );
                mDescriptionLabel->setVisible( !tmplate.description().isEmpty() );

                QString brand = tmplate.brand().toHtmlEscaped();
                if ( model::Db::isVendorNameKnown( tmplate.brand() ) )
                {
                        auto vendor = model::Db::lookupVendorFromName( tmplate.brand() );
                        if ( !vendor.url().isEmpty() )
                        {
                                brand = QString( "<a href='%1'>%2</a>" ).arg( vendor.url(), vendor.name().toHtmlEscaped() );
                        }
                }
                mBrandValue->setText( brand );

                mPartValue->setText( tmplate.productUrl().isEmpty()
                                     ? tmplate.part().toHtmlEscaped()
                                     : QString( "<a href='%1'>%2</a>" ).arg( tmplate.productUrl(), tmplate.part().toHtmlEscaped() ) );
                mLabelSizeValue->setText( frame->sizeDescription( units ) );
                mLayoutValue->setText( frame->layoutDescription() );
                mPageSizeValue->setText( tmplate.paperDescription( units ) );

                bool hasEquiv = !tmplate.equivPart().isEmpty();
                mEquivLabel->setVisible( hasEquiv );
                mEquivValue->setVisible( hasEquiv );
                mEquivValue->setText( tmplate.equivPart() );

                bool favorite = mPicker->isSelectedFavorite();
                mFavoriteButton->setChecked( favorite );
                mFavoriteButton->setText( favorite ? tr("★ Favorite") : tr("☆ Add to favorites") );
                mFavoriteButton->setToolTip( favorite ? tr("Remove from favorites") : tr("Add to favorites") );

                mDetailsStack->setCurrentIndex( 1 );
                mSelectButton->setEnabled( true );
        }


        ///
        /// Grid/list toggle state
        ///
        void SelectProductDialog::updateViewModeButtons()
        {
                bool grid = mPicker->mode() == QListView::IconMode;
                mGridButton->setChecked( grid );
                mListButton->setChecked( !grid );
        }


        ///
        /// Is the "all products" view narrowed by search text or filters?
        ///
        bool SelectProductDialog::filtersActive() const
        {
                if ( !mSearchEntry->text().trimmed().isEmpty() )
                {
                        return true;
                }
                if ( !mIsoChip->isChecked() || !mUsChip->isChecked() || !mOtherChip->isChecked() )
                {
                        return true;
                }
                return !checkedCategoryIds().isEmpty();
        }


        ///
        /// Ids of the ticked categories (empty = all categories)
        ///
        QStringList SelectProductDialog::checkedCategoryIds() const
        {
                QStringList ids;
                for ( auto* check : mCategoryChecks )
                {
                        if ( check->isChecked() )
                        {
                                ids << check->property( "categoryId" ).toString();
                        }
                }
                return ids;
        }


        ///
        /// Reset search text and filters
        ///
        void SelectProductDialog::clearFilters()
        {
                mUpdatingCategories = true;
                for ( auto* chip : { mIsoChip, mUsChip, mOtherChip } )
                {
                        QSignalBlocker blocker( chip );
                        chip->setChecked( true );
                }
                for ( auto* check : mCategoryChecks )
                {
                        check->setChecked( false );
                }
                mUpdatingCategories = false;

                {
                        QSignalBlocker blocker( mSearchEntry );
                        mSearchEntry->clear();
                }
                saveFilterSettings();
                setScope( SCOPE_ALL );
                mSearchEntry->setFocus();
        }


        ///
        /// Remember filters for next time
        ///
        void SelectProductDialog::saveFilterSettings()
        {
                model::Settings::setSearchIsoPaperSizes( mIsoChip->isChecked() );
                model::Settings::setSearchUsPaperSizes( mUsChip->isChecked() );
                model::Settings::setSearchOtherPaperSizes( mOtherChip->isChecked() );

                QStringList categoryIds = checkedCategoryIds();
                model::Settings::setSearchAllCategories( categoryIds.isEmpty() );
                if ( !categoryIds.isEmpty() )
                {
                        model::Settings::setSearchCategoryList( categoryIds );
                }
        }


        ///
        /// Accept the selected product
        ///
        void SelectProductDialog::chooseSelected()
        {
                if ( mPicker->selectedTemplate().isNull() )
                {
                        return;
                }
                mHasSelection = true;
                accept();
        }


        ///
        /// Create a custom template, then refresh the product list
        ///
        void SelectProductDialog::openTemplateDesigner()
        {
                TemplateDesigner designer( this );
                designer.exec();

                mPicker->setTemplates( model::Db::templates() );
                updateScopeCounts();
                applyFilter();
        }


        ///
        /// Keyboard navigation between the search field and the results
        ///
        bool SelectProductDialog::eventFilter( QObject* watched, QEvent* event )
        {
                if ( event->type() == QEvent::KeyPress )
                {
                        auto* keyEvent = static_cast<QKeyEvent*>( event );

                        if ( watched == mSearchEntry )
                        {
                                switch ( keyEvent->key() )
                                {
                                case Qt::Key_Down:
                                case Qt::Key_PageDown:
                                        applyFilter();
                                        if ( mPicker->selectedTemplate().isNull() )
                                        {
                                                mPicker->selectFirstVisible();
                                        }
                                        mPicker->setFocus();
                                        return true;
                                case Qt::Key_Return:
                                case Qt::Key_Enter:
                                        applyFilter();
                                        if ( mPicker->selectedTemplate().isNull() && mPicker->visibleCount() == 1 )
                                        {
                                                mPicker->selectFirstVisible();
                                        }
                                        if ( !mPicker->selectedTemplate().isNull() )
                                        {
                                                chooseSelected();
                                        }
                                        else
                                        {
                                                mPicker->selectFirstVisible();
                                                mPicker->setFocus();
                                        }
                                        return true;
                                case Qt::Key_Escape:
                                        if ( !mSearchEntry->text().isEmpty() )
                                        {
                                                mSearchEntry->clear();
                                                return true;
                                        }
                                        break;
                                default:
                                        break;
                                }
                        }
                        else if ( watched == mPicker )
                        {
                                if ( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter )
                                {
                                        chooseSelected();
                                        return true;
                                }
                                // Start typing anywhere in the grid to search
                                QString text = keyEvent->text();
                                if ( !text.isEmpty() && text.at( 0 ).isPrint() && !text.at( 0 ).isSpace() &&
                                     !( keyEvent->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) )
                                {
                                        mSearchEntry->setFocus();
                                        mSearchEntry->insert( text );
                                        return true;
                                }
                        }
                }
                return QDialog::eventFilter( watched, event );
        }


        ///
        /// Dialog-level keys
        ///
        void SelectProductDialog::keyPressEvent( QKeyEvent* event )
        {
                if ( event->key() == Qt::Key_Slash && !mSearchEntry->hasFocus() )
                {
                        mSearchEntry->setFocus();
                        mSearchEntry->selectAll();
                        return;
                }
                QDialog::keyPressEvent( event );
        }


} // namespace glabels
