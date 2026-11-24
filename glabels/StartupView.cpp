//  StartupView.cpp
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


#include "StartupView.hpp"

#include "AppearanceModel.hpp"
#include "File.hpp"
#include "MainWindow.hpp"
#include "Projects.hpp"
#include "WhatsNewDialog.hpp"

#include "model/Model.hpp"
#include "model/Settings.hpp"
#include "model/Version.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>


namespace
{
        using glabels::startup::LabelRowDelegate;

        const int sidebarWidth   = 250;
        const int inspectorWidth = 320;

        /// Show a folder with the home directory abbreviated to "~"
        QString displayFolder( const QString& folder )
        {
                QString native = QDir::toNativeSeparators( folder );
                QString home   = QDir::toNativeSeparators( QDir::homePath() );
                if ( native == home || native.startsWith( home + QDir::separator() ) )
                {
                        return "~" + native.mid( home.length() );
                }
                return native;
        }

        QString shortDateTime( const QDateTime& when )
        {
                QLocale locale;
                return when.isValid() ? locale.toString( when, locale.dateTimeFormat( QLocale::ShortFormat ) )
                                      : QString( "—" );
        }

        QString labelCountText( int n )
        {
                return n == 1 ? QCoreApplication::translate( "glabels::StartupView", "1 label" )
                              : QCoreApplication::translate( "glabels::StartupView", "%1 labels" ).arg( n );
        }
}


namespace glabels
{

        ///
        /// Constructor
        ///
        StartupView::StartupView( MainWindow* window )
                : QWidget(window), mWindow(window)
        {
                setObjectName( "startupView" );
                setAttribute( Qt::WA_StyledBackground );

                bool dark = AppearanceModel::style() == AppearanceModel::STYLE_DARK ||
                            palette().color( QPalette::Window ).lightness() < 128;
                mTheme = startup::Theme::make( QApplication::palette(), dark );

                auto* layout = new QVBoxLayout( this );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 0 );

                layout->addWidget( createTopBar() );

                auto* workspace = new QHBoxLayout();
                workspace->setContentsMargins( 0, 0, 0, 0 );
                workspace->setSpacing( 0 );
                workspace->addWidget( createSidebar() );
                workspace->addWidget( createCenter(), 1 );
                workspace->addWidget( createInspector() );
                layout->addLayout( workspace, 1 );

                layout->addWidget( createStatusBar() );

                // Keyboard: "/" searches, arrows browse, Enter opens
                auto* searchKey = new QShortcut( QKeySequence( "/" ), this );
                searchKey->setContext( Qt::WidgetWithChildrenShortcut );
                connect( searchKey, &QShortcut::activated, this, [this]{
                        mSearch->setFocus( Qt::ShortcutFocusReason );
                        mSearch->selectAll();
                } );
                mSearch->installEventFilter( this );
                mList->installEventFilter( this );

                // Delete = move to Trash (or remove from Recent), Shift+Delete = delete permanently
                auto* trashKey = new QShortcut( QKeySequence( Qt::Key_Delete ), mList );
                trashKey->setContext( Qt::WidgetShortcut );
                connect( trashKey, &QShortcut::activated, this, [this]{
                        QString path = selectedPath();
                        if ( path.isEmpty() ) return;
                        if ( mMode == ProjectMode ) trashLabel( path );
                        else if ( mMode == SampleMode )
                        {
                                int sample = sampleIndex( path );
                                if ( sample >= 0 && !mSamples[sample].builtIn ) trashTemplate( path );
                        }
                        else model::Settings::removeFromRecentFileList( path );
                } );
                auto* deleteKey = new QShortcut( QKeySequence( Qt::SHIFT | Qt::Key_Delete ), mList );
                deleteKey->setContext( Qt::WidgetShortcut );
                connect( deleteKey, &QShortcut::activated, this, [this]{
                        QString path = selectedPath();
                        if ( !path.isEmpty() && mMode == ProjectMode ) deleteLabel( path );
                } );

                // Keep lists current when files change on disk (saves, file manager edits)
                mReloadTimer = new QTimer( this );
                mReloadTimer->setSingleShot( true );
                mReloadTimer->setInterval( 250 );
                connect( mReloadTimer, &QTimer::timeout, this, &StartupView::onProjectsChangedOnDisk );

                mWatcher = new QFileSystemWatcher( this );
                connect( mWatcher, &QFileSystemWatcher::directoryChanged, mReloadTimer, qOverload<>( &QTimer::start ) );

                // Details of the selected label load just after the selection settles
                mInspectorTimer = new QTimer( this );
                mInspectorTimer->setSingleShot( true );
                mInspectorTimer->setInterval( 60 );
                connect( mInspectorTimer, &QTimer::timeout, this, &StartupView::updateInspectorDetails );

                updateStyle();
                loadProjects( QSettings().value( "Projects/current" ).toString() );
                setMode( RecentMode );

                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                connect( AppearanceModel::instance(), SIGNAL(changed()), this, SLOT(onAppearanceChanged()) );
        }


        ///
        /// Top bar: app icon, name, version, tagline and search
        ///
        QWidget* StartupView::createTopBar()
        {
                auto* bar = new QWidget();
                bar->setObjectName( "startupTopBar" );
                bar->setAttribute( Qt::WA_StyledBackground );
                auto* layout = new QHBoxLayout( bar );
                layout->setContentsMargins( 24, 14, 24, 14 );
                layout->setSpacing( 16 );

                mAppIcon = new QLabel();
                mAppIcon->setFixedSize( 44, 44 );
                mAppIcon->setPixmap( QIcon::fromTheme( "glabels" ).pixmap( 44, 44 ) );
                layout->addWidget( mAppIcon );

                auto* brand = new QVBoxLayout();
                brand->setSpacing( 2 );
                auto* titleRow = new QHBoxLayout();
                titleRow->setSpacing( 10 );

                auto* name = new QLabel( "gLabels Plus" );
                name->setObjectName( "startupAppName" );
                QFont nameFont = name->font();
                nameFont.setPointSizeF( nameFont.pointSizeF() * 1.45 );
                nameFont.setWeight( QFont::Bold );
                name->setFont( nameFont );

                auto* kind = new QLabel( tr("Label Designer") );
                kind->setObjectName( "startupAppKind" );
                QFont kindFont = nameFont;
                kindFont.setWeight( QFont::Normal );
                kind->setFont( kindFont );

                mVersionPill = new QLabel( tr("Version %1").arg( model::Version::STRING ) );
                mVersionPill->setObjectName( "startupPill" );

                titleRow->addWidget( name, 0, Qt::AlignVCenter );
                titleRow->addWidget( kind, 0, Qt::AlignVCenter );
                titleRow->addWidget( mVersionPill, 0, Qt::AlignVCenter );
                titleRow->addStretch( 1 );
                brand->addLayout( titleRow );

                auto* tagline = new QLabel( tr("Design and print labels, business cards and badges.") );
                tagline->setObjectName( "startupText2" );
                brand->addWidget( tagline );
                layout->addLayout( brand, 1 );

                mSearch = new QLineEdit();
                mSearch->setObjectName( "startupSearch" );
                mSearch->setPlaceholderText( tr("Search labels and projects...") );
                mSearch->setClearButtonEnabled( true );
                mSearch->setFixedWidth( 300 );
                mSearch->setToolTip( tr("Search labels (/)") );
                connect( mSearch, &QLineEdit::textChanged, this, &StartupView::applyFilter );
                layout->addWidget( mSearch, 0, Qt::AlignVCenter );

                return bar;
        }


        ///
        /// Uppercase section heading ("START", "LIBRARY", ...)
        ///
        QLabel* StartupView::createSectionLabel( const QString& text )
        {
                auto* label = new QLabel( text.toUpper() );
                label->setObjectName( "startupSectionLabel" );
                QFont f = label->font();
                f.setPointSizeF( f.pointSizeF() * 0.8 );
                f.setWeight( QFont::Bold );
                f.setLetterSpacing( QFont::PercentageSpacing, 108 );
                label->setFont( f );
                return label;
        }


        ///
        /// Left pane: start actions, recent labels, projects and utility links
        ///
        QWidget* StartupView::createSidebar()
        {
                mSidebar = new QWidget();
                mSidebar->setObjectName( "startupSidebar" );
                mSidebar->setAttribute( Qt::WA_StyledBackground );
                mSidebar->setFixedWidth( sidebarWidth );

                auto* layout = new QVBoxLayout( mSidebar );
                layout->setContentsMargins( 12, 18, 12, 14 );
                layout->setSpacing( 2 );

                auto addHeading = [&]( QWidget* heading, bool first ) {
                        if ( !first ) layout->addSpacing( 20 );
                        layout->addWidget( heading );
                        layout->addSpacing( 6 );
                };

                auto* startLabel = createSectionLabel( tr("Start") );
                startLabel->setContentsMargins( 8, 0, 8, 0 );
                addHeading( startLabel, true );

                auto* navNew = new startup::NavButton( startup::icon::NewLabel, tr("New label..."), &mTheme );
                navNew->setTrailing( QKeySequence( QKeySequence::New ).toString( QKeySequence::NativeText ).replace( "+", " " ),
                                     startup::NavButton::Key );
                auto* navOpen = new startup::NavButton( startup::icon::Open, tr("Open label..."), &mTheme );
                navOpen->setTrailing( QKeySequence( QKeySequence::Open ).toString( QKeySequence::NativeText ).replace( "+", " " ),
                                      startup::NavButton::Key );
                auto* navDesigner = new startup::NavButton( startup::icon::Designer, tr("Template designer..."), &mTheme );
                navDesigner->setToolTip( tr("Create a template for a product that isn't listed") );
                connect( navNew,      &QAbstractButton::clicked, this, [this]{ File::newLabel( mWindow ); } );
                connect( navOpen,     &QAbstractButton::clicked, this, [this]{ File::open( mWindow ); } );
                connect( navDesigner, &QAbstractButton::clicked, this, [this]{ File::templateDesigner( mWindow ); } );
                layout->addWidget( navNew );
                layout->addWidget( navOpen );
                layout->addWidget( navDesigner );

                auto* libraryLabel = createSectionLabel( tr("Library") );
                libraryLabel->setContentsMargins( 8, 0, 8, 0 );
                addHeading( libraryLabel, false );

                mRecentNav = new startup::NavButton( startup::icon::Recent, tr("Recent labels"), &mTheme );
                connect( mRecentNav, &QAbstractButton::clicked, this, [this]{ setMode( RecentMode ); } );
                layout->addWidget( mRecentNav );

                mSamplesNav = new startup::NavButton( startup::icon::Samples, tr("Sample labels"), &mTheme );
                mSamplesNav->setToolTip( tr("Ready-made shipping labels, receipts, invoices, SKU, product and asset "
                                            "labels to start from") );
                connect( mSamplesNav, &QAbstractButton::clicked, this, [this]{ setMode( SampleMode ); } );
                layout->addWidget( mSamplesNav );

                // Projects heading with a "+" button
                auto* projectsHeading = new QWidget();
                auto* phLayout = new QHBoxLayout( projectsHeading );
                phLayout->setContentsMargins( 8, 0, 4, 0 );
                phLayout->setSpacing( 4 );
                phLayout->addWidget( createSectionLabel( tr("Projects") ), 1 );
                mNewProjectButton = new QToolButton();
                mNewProjectButton->setObjectName( "startupIconButton" );
                mNewProjectButton->setToolTip( tr("New project... (for example one per business or client)") );
                mNewProjectButton->setAccessibleName( tr("New project") );
                mNewProjectButton->setCursor( Qt::PointingHandCursor );
                mNewProjectButton->setIconSize( QSize( 14, 14 ) );
                connect( mNewProjectButton, &QToolButton::clicked, this, &StartupView::onCreateProject );
                phLayout->addWidget( mNewProjectButton );
                addHeading( projectsHeading, false );

                // Project list (scrolls when there are many)
                auto* scroll = new QScrollArea();
                scroll->setObjectName( "startupProjectScroll" );
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidgetResizable( true );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

                mProjectNavBox = new QWidget();
                mProjectNavBox->setObjectName( "startupProjectBox" );
                mProjectNavLayout = new QVBoxLayout( mProjectNavBox );
                mProjectNavLayout->setContentsMargins( 0, 0, 0, 0 );
                mProjectNavLayout->setSpacing( 2 );

                mNoProjectsLabel = new QLabel( tr("No projects yet. Use + to keep each business or "
                                                  "client's labels together.") );
                mNoProjectsLabel->setObjectName( "startupText3" );
                mNoProjectsLabel->setWordWrap( true );
                mNoProjectsLabel->setContentsMargins( 8, 2, 8, 2 );
                mProjectNavLayout->addWidget( mNoProjectsLabel );
                mProjectNavLayout->addStretch( 1 );
                scroll->setWidget( mProjectNavBox );
                layout->addWidget( scroll, 1 );

                // Delete = move to Trash, Shift+Delete = delete permanently
                auto* trashKey = new QShortcut( QKeySequence( Qt::Key_Delete ), mProjectNavBox );
                trashKey->setContext( Qt::WidgetWithChildrenShortcut );
                connect( trashKey, &QShortcut::activated, this, [this]{
                        if ( auto* nav = dynamic_cast<startup::NavButton*>( focusWidget() ) )
                                trashProject( nav->property( "project" ).toString() );
                } );
                auto* deleteKey = new QShortcut( QKeySequence( Qt::SHIFT | Qt::Key_Delete ), mProjectNavBox );
                deleteKey->setContext( Qt::WidgetWithChildrenShortcut );
                connect( deleteKey, &QShortcut::activated, this, [this]{
                        if ( auto* nav = dynamic_cast<startup::NavButton*>( focusWidget() ) )
                                deleteProject( nav->property( "project" ).toString() );
                } );

                // Utility links
                auto* links = new QFrame();
                links->setObjectName( "startupLinks" );
                auto* linksLayout = new QVBoxLayout( links );
                linksLayout->setContentsMargins( 8, 12, 8, 0 );
                mLinksLabel = new QLabel();
                mLinksLabel->setObjectName( "startupLinksLabel" );
                mLinksLabel->setWordWrap( true );
                mLinksLabel->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
                connect( mLinksLabel, &QLabel::linkActivated, this, [this]( const QString& link ) {
                        if      ( link == "prefs" ) QMetaObject::invokeMethod( mWindow, "editPreferences" );
                        else if ( link == "keys" )  QMetaObject::invokeMethod( mWindow, "editShortcuts" );
                        else if ( link == "help" )  QMetaObject::invokeMethod( mWindow, "helpContents" );
                        else if ( link == "whatsnew" )
                        {
                                WhatsNewDialog dialog( this );
                                dialog.exec();
                        }
                        else if ( link.startsWith( "http" ) )
                        {
                                QDesktopServices::openUrl( QUrl( link ) );
                        }
                } );
                connect( mLinksLabel, &QLabel::linkHovered, this, [this]( const QString& link ) {
                        // QLabel has no :hover for links, so underline the hovered one by hand
                        if ( link != mHoveredLink )
                        {
                                mHoveredLink = link;
                                updateLinksText();
                        }
                } );
                linksLayout->addWidget( mLinksLabel );
                layout->addSpacing( 10 );
                layout->addWidget( links );

                return mSidebar;
        }


        ///
        /// Center pane: quick start tiles and the label list
        ///
        QWidget* StartupView::createCenter()
        {
                auto* center = new QWidget();
                center->setObjectName( "startupCenter" );
                center->setAttribute( Qt::WA_StyledBackground );
                center->setMinimumWidth( 420 );

                auto* layout = new QVBoxLayout( center );
                layout->setContentsMargins( 28, 24, 28, 24 );
                layout->setSpacing( 12 );

                // Quick start
                auto* quickTitle = new QLabel( tr("Quick start") );
                quickTitle->setObjectName( "startupH2" );
                layout->addWidget( quickTitle );

                auto* tiles = new QHBoxLayout();
                tiles->setSpacing( 12 );
                auto* newTile = new startup::TileButton( startup::icon::NewLabel, tr("New label..."),
                                                         tr("Pick a label or card product and start designing"),
                                                         true, &mTheme );
                auto* sampleTile = new startup::TileButton( startup::icon::Samples, tr("Start from a sample"),
                                                            tr("Shipping, receipt, invoice, SKU, product and asset labels"),
                                                            false, &mTheme );
                auto* openTile = new startup::TileButton( startup::icon::Open, tr("Open label..."),
                                                          tr("Browse for an existing gLabels Plus label file"),
                                                          false, &mTheme );
                auto* designerTile = new startup::TileButton( startup::icon::Designer, tr("Product template designer..."),
                                                              tr("Create a template for a product that isn't listed"),
                                                              false, &mTheme );
                connect( newTile,      &QAbstractButton::clicked, this, [this]{ File::newLabel( mWindow ); } );
                connect( sampleTile,   &QAbstractButton::clicked, this, [this]{ showSamples(); } );
                connect( openTile,     &QAbstractButton::clicked, this, [this]{ File::open( mWindow ); } );
                connect( designerTile, &QAbstractButton::clicked, this, [this]{ File::templateDesigner( mWindow ); } );
                tiles->addWidget( newTile, 1 );
                tiles->addWidget( sampleTile, 1 );
                tiles->addWidget( openTile, 1 );
                tiles->addWidget( designerTile, 1 );
                layout->addLayout( tiles );
                layout->addSpacing( 14 );

                // Tabs and project actions
                auto* head = new QHBoxLayout();
                head->setSpacing( 12 );

                auto* tabs = new QFrame();
                tabs->setObjectName( "startupTabs" );
                auto* tabsLayout = new QHBoxLayout( tabs );
                tabsLayout->setContentsMargins( 3, 3, 3, 3 );
                tabsLayout->setSpacing( 2 );
                auto makeTab = [&]( const QString& text ) {
                        auto* tab = new QToolButton();
                        tab->setObjectName( "startupTab" );
                        tab->setText( text );
                        tab->setCheckable( true );
                        tab->setCursor( Qt::PointingHandCursor );
                        tabsLayout->addWidget( tab );
                        return tab;
                };
                mRecentTab  = makeTab( tr("Recent") );
                mSamplesTab = makeTab( tr("Samples") );
                mSamplesTab->setToolTip( tr("Ready-made labels: open a copy, edit it and print") );
                mProjectTab = makeTab( QString() );
                mProjectTab->setToolTip( tr("Labels in this project") );
                auto* tabGroup = new QButtonGroup( this );
                tabGroup->addButton( mRecentTab );
                tabGroup->addButton( mSamplesTab );
                tabGroup->addButton( mProjectTab );
                connect( mRecentTab,  &QToolButton::clicked, this, [this]{ setMode( RecentMode ); } );
                connect( mSamplesTab, &QToolButton::clicked, this, [this]{ setMode( SampleMode ); } );
                connect( mProjectTab, &QToolButton::clicked, this, [this]{ setMode( ProjectMode, mProject ); } );
                head->addWidget( tabs, 0, Qt::AlignVCenter );
                head->addStretch( 1 );

                // Grid tile size: Small | Medium
                auto* sizes = new QFrame();
                sizes->setObjectName( "startupTabs" );
                auto* sizesLayout = new QHBoxLayout( sizes );
                sizesLayout->setContentsMargins( 3, 3, 3, 3 );
                sizesLayout->setSpacing( 2 );
                auto makeSize = [&]( const QString& text, const QString& tip ) {
                        auto* b = new QToolButton();
                        b->setObjectName( "startupTab" );
                        b->setText( text );
                        b->setToolTip( tip );
                        b->setCheckable( true );
                        b->setCursor( Qt::PointingHandCursor );
                        sizesLayout->addWidget( b );
                        return b;
                };
                mSmallTiles  = makeSize( tr("Small"), tr("Small thumbnails") );
                mMediumTiles = makeSize( tr("Medium"), tr("Medium thumbnails") );
                auto* sizeGroup = new QButtonGroup( this );
                sizeGroup->addButton( mSmallTiles );
                sizeGroup->addButton( mMediumTiles );
                connect( mSmallTiles,  &QToolButton::clicked, this, [this]{ setTileSize( startup::LabelTileDelegate::Small ); } );
                connect( mMediumTiles, &QToolButton::clicked, this, [this]{ setTileSize( startup::LabelTileDelegate::Medium ); } );
                head->addWidget( sizes, 0, Qt::AlignVCenter );

                mProjectActions = new QWidget();
                auto* actionsLayout = new QHBoxLayout( mProjectActions );
                actionsLayout->setContentsMargins( 0, 0, 0, 0 );
                actionsLayout->setSpacing( 8 );
                auto makeButton = [this]( const QString& text, const QString& tip, const char* objectName ) {
                        auto* b = new QToolButton();
                        b->setObjectName( objectName );
                        b->setText( text );
                        b->setToolTip( tip );
                        b->setCursor( Qt::PointingHandCursor );
                        b->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                        return b;
                };
                auto* newInProject = makeButton( tr("New label..."),
                                                 tr("Choose a product and add a new label to this project"),
                                                 "startupButton" );
                auto* openFolder = makeButton( tr("Open folder"),
                                               tr("Show this project's folder in your file manager"),
                                               "startupButton" );
                connect( newInProject, &QToolButton::clicked, this, &StartupView::onNewLabelInProject );
                connect( openFolder,   &QToolButton::clicked, this, &StartupView::onOpenProjectFolder );
                actionsLayout->addWidget( newInProject );
                actionsLayout->addWidget( openFolder );
                head->addWidget( mProjectActions, 0, Qt::AlignVCenter );
                layout->addLayout( head );

                mProjectNote = new QLabel( tr("Keep each business or client's labels together") );
                mProjectNote->setObjectName( "startupText3" );
                layout->addWidget( mProjectNote );

                // The list
                mList = new QListWidget();
                mList->setObjectName( "startupList" );
                mTiles = new startup::LabelTileDelegate( &mTheme, mList );
                mList->setItemDelegate( mTiles );
                mList->setViewMode( QListView::IconMode );
                mList->setFlow( QListView::LeftToRight );
                mList->setWrapping( true );
                mList->setResizeMode( QListView::Adjust );
                mList->setMovement( QListView::Static );
                mList->setSpacing( 0 );
                mList->setUniformItemSizes( true );
                mList->setMouseTracking( true );
                mList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mList->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
                mList->setContextMenuPolicy( Qt::CustomContextMenu );
                mList->setSelectionMode( QAbstractItemView::SingleSelection );
                mList->setAccessibleName( tr("Labels") );
                connect( mList, &QListWidget::currentItemChanged, this, &StartupView::onCurrentItemChanged );
                connect( mList, &QListWidget::itemDoubleClicked, this, [this]{ openSelected( false ); } );
                connect( mList, &QWidget::customContextMenuRequested, this, &StartupView::onListContextMenu );

                mEmptyLabel = new QLabel();
                mEmptyLabel->setObjectName( "startupEmpty" );
                mEmptyLabel->setAlignment( Qt::AlignCenter );
                mEmptyLabel->setWordWrap( true );

                setTileSize( QSettings().value( "Startup/tileSize" ).toString() == "small"
                             ? startup::LabelTileDelegate::Small : startup::LabelTileDelegate::Medium );

                mListStack = new QStackedWidget();
                mListStack->addWidget( mList );
                mListStack->addWidget( mEmptyLabel );
                layout->addWidget( mListStack, 1 );

                return center;
        }


        ///
        /// Right pane: preview and details of the selected label
        ///
        QWidget* StartupView::createInspector()
        {
                mInspector = new QWidget();
                mInspector->setObjectName( "startupInspector" );
                mInspector->setAttribute( Qt::WA_StyledBackground );
                mInspector->setFixedWidth( inspectorWidth );

                auto* layout = new QVBoxLayout( mInspector );
                layout->setContentsMargins( 20, 20, 20, 20 );
                layout->setSpacing( 18 );

                mPreview = new startup::PreviewWell( &mTheme );
                layout->addWidget( mPreview );

                auto* titles = new QVBoxLayout();
                titles->setSpacing( 2 );
                mInspTitle = new QLabel();
                mInspTitle->setObjectName( "startupInspTitle" );
                mInspTitle->setTextFormat( Qt::PlainText );
                mInspTitle->setWordWrap( true );
                QFont tf = mInspTitle->font();
                tf.setPointSizeF( tf.pointSizeF() * 1.25 );
                tf.setWeight( QFont::Bold );
                mInspTitle->setFont( tf );
                mInspSub = new QLabel();
                mInspSub->setObjectName( "startupText3" );
                mInspSub->setTextFormat( Qt::PlainText );
                mInspSub->setWordWrap( true );
                titles->addWidget( mInspTitle );
                titles->addWidget( mInspSub );
                layout->addLayout( titles );

                mInspDetails = new QWidget();
                auto* grid = new QGridLayout( mInspDetails );
                grid->setContentsMargins( 0, 0, 0, 0 );
                grid->setHorizontalSpacing( 12 );
                grid->setVerticalSpacing( 9 );
                grid->setColumnMinimumWidth( 0, 72 );
                grid->setColumnStretch( 1, 1 );
                auto addRow = [&]( int row, const QString& term, bool mono, QLabel** termLabel = nullptr ) {
                        auto* dt = new QLabel( term );
                        if ( termLabel ) *termLabel = dt;
                        dt->setObjectName( "startupText3" );
                        auto* dd = new QLabel();
                        dd->setObjectName( mono ? "startupMono" : "startupText" );
                        dd->setTextFormat( Qt::PlainText );
                        dd->setWordWrap( true );
                        dd->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        if ( mono )
                        {
                                QFont f = QFontDatabase::systemFont( QFontDatabase::FixedFont );
                                f.setPointSizeF( dd->font().pointSizeF() * 0.88 );
                                dd->setFont( f );
                        }
                        grid->addWidget( dt, row, 0, Qt::AlignTop );
                        grid->addWidget( dd, row, 1, Qt::AlignTop );
                        return dd;
                };
                mInspProject  = addRow( 0, tr("Project"), false, &mInspProjectTerm );
                mInspSize     = addRow( 1, tr("Size"), false );
                mInspModified = addRow( 2, tr("Modified"), false, &mInspModifiedTerm );
                mInspPath     = addRow( 3, tr("Location"), true, &mInspPathTerm );
                layout->addWidget( mInspDetails );
                layout->addStretch( 1 );

                // Actions
                auto* actions = new QGridLayout();
                actions->setSpacing( 8 );
                auto makeButton = []( const QString& text, const char* objectName ) {
                        auto* b = new QToolButton();
                        b->setObjectName( objectName );
                        b->setText( text );
                        b->setCursor( Qt::PointingHandCursor );
                        b->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                        b->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                        return b;
                };
                mOpenButton       = makeButton( tr("Open label"), "startupPrimaryButton" );
                mPrintButton      = makeButton( tr("Print..."), "startupButton" );
                mShowFolderButton = makeButton( tr("Show folder"), "startupButton" );
                connect( mOpenButton,  &QToolButton::clicked, this, [this]{ openSelected( false ); } );
                connect( mPrintButton, &QToolButton::clicked, this, [this]{ openSelected( true ); } );
                connect( mShowFolderButton, &QToolButton::clicked, this, [this]{
                        QString path = selectedPath();
                        if ( !path.isEmpty() )
                        {
                                QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( path ).absolutePath() ) );
                        }
                } );
                actions->addWidget( mOpenButton, 0, 0, 1, 2 );
                actions->addWidget( mPrintButton, 1, 0 );
                actions->addWidget( mShowFolderButton, 1, 1 );
                layout->addLayout( actions );

                return mInspector;
        }


        ///
        /// Bottom bar: keyboard hints and the maintainer note
        ///
        QWidget* StartupView::createStatusBar()
        {
                auto* bar = new QWidget();
                bar->setObjectName( "startupStatusBar" );
                bar->setAttribute( Qt::WA_StyledBackground );
                auto* layout = new QHBoxLayout( bar );
                layout->setContentsMargins( 24, 8, 24, 8 );
                layout->setSpacing( 12 );

                auto keyText = []( QKeySequence::StandardKey key ) {
                        return QKeySequence( key ).toString( QKeySequence::NativeText ).replace( "+", " " );
                };
                mHints = new startup::KeyHints( {
                        { { "/" }, tr("Search") },
                        { { keyText( QKeySequence::New ) }, tr("New label") },
                        { { keyText( QKeySequence::Open ) }, tr("Open") },
                        { { QString::fromUtf8( "↑" ), QString::fromUtf8( "↓" ) }, tr("Browse") },
                        { { tr("Enter") }, tr("Open") },
                }, &mTheme );
                layout->addWidget( mHints, 1, Qt::AlignVCenter );

                auto* pill = new QFrame();
                pill->setObjectName( "startupStatusPill" );
                auto* pillLayout = new QHBoxLayout( pill );
                pillLayout->setContentsMargins( 12, 4, 13, 4 );
                pillLayout->setSpacing( 8 );
                auto* dot = new QLabel();
                dot->setObjectName( "startupDot" );
                dot->setFixedSize( 7, 7 );
                mStatusLabel = new QLabel();
                mStatusLabel->setObjectName( "startupStatusText" );
                mStatusLabel->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
                mStatusLabel->setToolTip( "https://usmansaif.com" );
                connect( mStatusLabel, &QLabel::linkActivated, this, []( const QString& link ) {
                        QDesktopServices::openUrl( QUrl( link ) );
                } );
                pillLayout->addWidget( dot, 0, Qt::AlignVCenter );
                pillLayout->addWidget( mStatusLabel, 0, Qt::AlignVCenter );
                layout->addWidget( pill, 0, Qt::AlignRight | Qt::AlignVCenter );

                return bar;
        }


        ///
        /// Sidebar utility links in the accent color; the hovered one is underlined
        ///
        void StartupView::updateLinksText()
        {
                const startup::Theme& t = mTheme;
                const struct { const char* href; QString text; } items[] = {
                        { "prefs",    tr("Preferences") },
                        { "keys",     tr("Keyboard shortcuts") },
                        { "help",     tr("Help") },
                        { "whatsnew", tr("What's New") },
                        { "https://usmansaif.github.io/gLabels-plus", tr("Website") },
                };

                QStringList parts;
                for ( const auto& item : items )
                {
                        const bool hovered = ( mHoveredLink == item.href );
                        parts << QString( "<a style='color:%1; text-decoration:%2;' href='%3'>%4</a>" )
                                 .arg( ( hovered ? t.primaryDark : t.primary ).name(),
                                       hovered ? "underline" : "none",
                                       item.href, item.text.toHtmlEscaped() );
                }
                mLinksLabel->setText( parts.join( "&nbsp;&nbsp;&nbsp; " ) );
        }


        ///
        /// Re-apply colors whenever the palette or appearance changes
        ///
        void StartupView::updateStyle()
        {
                if ( mUpdatingStyle )
                {
                        return;
                }
                mUpdatingStyle = true;

                bool dark = AppearanceModel::style() == AppearanceModel::STYLE_DARK ||
                            palette().color( QPalette::Window ).lightness() < 128;
                // Read the application palette: our own palette is unaffected by
                // the style sheet below, but stay on the safe side.
                mTheme = startup::Theme::make( QApplication::palette(), dark );
                const startup::Theme& t = mTheme;

                mAppIcon->setPixmap( QIcon::fromTheme( "glabels" ).pixmap( 44, 44 ) );
                mNewProjectButton->setIcon( QIcon( startup::iconPixmap( startup::icon::Plus, t.text3, 14,
                                                                         devicePixelRatioF() ) ) );

                // QLabel bakes link colors in when its text is set, so rebuild the
                // rich text with explicit theme colors.
                updateLinksText();
                mStatusLabel->setText(
                        QString( "%1 &bull; %2: <a style='color:%3; text-decoration:none;' "
                                 "href='https://usmansaif.com'><b>Usman Saif</b></a>" )
                        .arg( tr("Enhanced with new features and a refreshed interface"), tr("Maintainer"),
                              t.text.name() ) );

                QString css = QString(
                        "#startupView, #startupCenter { background: %1; }"
                        "#startupTopBar { background: %2; border-bottom: 1px solid %4; }"
                        "#startupSidebar { background: %3; border-right: 1px solid %4; }"
                        "#startupProjectScroll, #startupProjectBox { background: transparent; }"
                        "#startupLinks { border-top: 1px solid %4; background: transparent; }"
                        "#startupInspector { background: %2; border-left: 1px solid %4; }"
                        "#startupStatusBar { background: %2; border-top: 1px solid %4; }"

                        "#startupAppName, #startupText, #startupMono, #startupH2, #startupInspTitle { color: %6; }"
                        "#startupAppKind, #startupText3, #startupSectionLabel, #startupLinksLabel { color: %8; }"
                        "#startupText2, #startupStatusText { color: %7; }"
                        "#startupH2 { font-size: 12pt; font-weight: 600; }"

                        "#startupPill { color: %10; background: %11; border: 1px solid %12;"
                        "  border-radius: 9px; padding: 1px 9px; font-size: 8pt; font-weight: 600; }"

                        "QLineEdit#startupSearch { padding: 7px 10px; border: 1px solid %5; border-radius: 9px;"
                        "  background: %3; color: %6; selection-background-color: %9; }"
                        "QLineEdit#startupSearch:focus { border: 1px solid %9; background: %2; }"

                        "QToolButton#startupIconButton { border: 1px solid transparent; border-radius: 6px;"
                        "  background: transparent; padding: 4px; }"
                        "QToolButton#startupIconButton:hover { background: %11; border-color: %12; }"

                        "#startupTabs { background: %13; border-radius: 9px; }"
                        "QToolButton#startupTab { border: 0; background: transparent; padding: 6px 14px;"
                        "  border-radius: 7px; font-weight: 600; color: %7; }"
                        "QToolButton#startupTab:hover { color: %6; }"
                        "QToolButton#startupTab:checked { background: %2; color: %10; }"

                        "QToolButton#startupButton { padding: 6px 13px; border: 1px solid %5; border-radius: 8px;"
                        "  background: %2; color: %6; font-weight: 600; }"
                        "QToolButton#startupButton:hover { border-color: %9; background: %11; color: %10; }"
                        "QToolButton#startupButton:pressed { background: %12; }"
                        "QToolButton#startupButton:disabled { color: %8; border-color: %4; }"
                        "QToolButton#startupPrimaryButton { padding: 9px 13px; border: 1px solid %9; border-radius: 8px;"
                        "  background: %9; color: white; font-weight: 600; }"
                        "QToolButton#startupPrimaryButton:hover { background: %10; border-color: %10; }"
                        "QToolButton#startupPrimaryButton:disabled { background: %5; border-color: %5; }"

                        "QListWidget#startupList { background: %2; border: 1px solid %4; border-radius: 12px;"
                        "  padding: 1px; outline: 0; }"
                        "#startupEmpty { color: %8; background: %2; border: 1px solid %4; border-radius: 12px;"
                        "  padding: 36px 16px; }"

                        "#startupStatusPill { background: %3; border: 1px solid %4; border-radius: 13px; }"
                        "#startupDot { background: #22c55e; border-radius: 3px; }"
                )
                .arg( t.bg.name(), t.surface.name(), t.muted.name(), t.border.name(), t.borderStrong.name() )
                .arg( t.text.name(), t.text2.name(), t.text3.name() )
                .arg( t.primary.name(), t.dark ? t.primary.lighter( 125 ).name() : t.primaryDark.name(),
                      t.tint.name(), t.tintBorder.name(), t.hover.name() );
                setStyleSheet( css );

                update();
                mUpdatingStyle = false;
        }


        void StartupView::changeEvent( QEvent* event )
        {
                QWidget::changeEvent( event );
                if ( event->type() == QEvent::PaletteChange )
                {
                        updateStyle();
                }
        }


        ///
        /// Narrow windows: drop the inspector first, then the sidebar
        ///
        void StartupView::resizeEvent( QResizeEvent* event )
        {
                QWidget::resizeEvent( event );
                mInspector->setVisible( width() >= 1040 );
                mSidebar->setVisible( width() >= 720 );
        }


        ///
        /// Search box: arrows browse the list, Enter opens, Escape clears.
        /// List: Enter opens the selected label.
        ///
        bool StartupView::eventFilter( QObject* watched, QEvent* event )
        {
                if ( event->type() == QEvent::KeyPress )
                {
                        auto* key = static_cast<QKeyEvent*>( event );
                        bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;

                        if ( watched == mSearch )
                        {
                                if ( key->key() == Qt::Key_Down || key->key() == Qt::Key_Up )
                                {
                                        int step = key->key() == Qt::Key_Down ? 1 : -1;
                                        int row = mList->currentRow();
                                        for ( int i = row + step; i >= 0 && i < mList->count(); i += step )
                                        {
                                                if ( !mList->item( i )->isHidden() )
                                                {
                                                        mList->setCurrentRow( i );
                                                        break;
                                                }
                                        }
                                        return true;
                                }
                                if ( enter )
                                {
                                        openSelected( false );
                                        return true;
                                }
                                if ( key->key() == Qt::Key_Escape )
                                {
                                        mSearch->clear();
                                        mList->setFocus();
                                        return true;
                                }
                        }
                        else if ( watched == mList && enter )
                        {
                                openSelected( false );
                                return true;
                        }
                }
                return QWidget::eventFilter( watched, event );
        }


        ///
        /// Switch between the Recent list and a project's labels
        ///
        void StartupView::setMode( Mode mode, const QString& project )
        {
                if ( mode == ProjectMode )
                {
                        if ( !project.isEmpty() )
                        {
                                mProject = project;
                        }
                        if ( mProject.isEmpty() )
                        {
                                mode = RecentMode;
                        }
                        else
                        {
                                QSettings().setValue( "Projects/current", mProject );
                        }
                }
                mMode = mode;

                mRecentNav->setActive( mode == RecentMode );
                mSamplesNav->setActive( mode == SampleMode );
                for ( auto* nav : mProjectNavs )
                {
                        nav->setActive( mode == ProjectMode && nav->property( "project" ).toString() == mProject );
                }
                mProjectTab->setText( mProject );
                mProjectTab->setVisible( !mProject.isEmpty() );
                mRecentTab->setChecked( mode == RecentMode );
                mSamplesTab->setChecked( mode == SampleMode );
                mProjectTab->setChecked( mode == ProjectMode );
                mProjectActions->setVisible( mode == ProjectMode );
                mProjectNote->setVisible( mode == ProjectMode );

                loadList();
        }


        ///
        /// Public: show a project's labels
        ///
        void StartupView::selectProject( const QString& project )
        {
                loadProjects( project );
                setMode( ProjectMode, project );
        }


        ///
        /// Public: show the sample label library
        ///
        void StartupView::showSamples()
        {
                setMode( SampleMode );
                mList->setFocus();
        }


        ///
        /// (Re)build the project entries in the sidebar
        ///
        void StartupView::loadProjects( const QString& select )
        {
                QStringList projects = Projects::list();

                if ( !select.isEmpty() && projects.contains( select ) )
                {
                        mProject = select;
                }
                if ( !projects.contains( mProject ) )
                {
                        mProject = projects.isEmpty() ? QString() : projects.first();
                        if ( mMode == ProjectMode && mProject.isEmpty() )
                        {
                                mMode = RecentMode;
                        }
                }

                // deleteLater: this may run from a project button's own context menu
                for ( auto* nav : mProjectNavs )
                {
                        mProjectNavLayout->removeWidget( nav );
                        nav->hide();
                        nav->deleteLater();
                }
                mProjectNavs.clear();

                int index = 0;
                for ( const auto& name : projects )
                {
                        auto* nav = new startup::NavButton( startup::icon::Folder, name, &mTheme );
                        nav->setProperty( "project", name );
                        nav->setTrailing( labelCountText( Projects::labelCount( name ) ), startup::NavButton::Count );
                        nav->setToolTip( QDir::toNativeSeparators( Projects::path( name ) ) );
                        nav->setContextMenuPolicy( Qt::CustomContextMenu );
                        nav->setActive( mMode == ProjectMode && name == mProject );
                        connect( nav, &QAbstractButton::clicked, this, [this, name]{ setMode( ProjectMode, name ); } );
                        connect( nav, &QWidget::customContextMenuRequested, this, [this, nav, name]( const QPoint& pos ) {
                                showProjectMenu( name, nav->mapToGlobal( pos ) );
                        } );
                        mProjectNavLayout->insertWidget( index++, nav );
                        mProjectNavs << nav;
                }
                mNoProjectsLabel->setVisible( projects.isEmpty() );

                watchProjectFolders();
        }


        ///
        /// (Re)fill the label list for the current mode
        ///
        void StartupView::loadList()
        {
                QString keep = selectedPath();
                mRecentFiles = model::Settings::recentFileList();

                mList->blockSignals( true );
                mList->clear();

                auto addRow = [this]( const QString& path, const QString& name, const QString& folder,
                                      const QString& project, const QString& when, bool exists ) {
                        auto* item = new QListWidgetItem();
                        item->setData( LabelRowDelegate::PathRole, path );
                        item->setData( LabelRowDelegate::NameRole, name );
                        item->setData( LabelRowDelegate::FolderRole, folder );
                        item->setData( LabelRowDelegate::ProjectRole, project );
                        item->setData( LabelRowDelegate::WhenRole, when );
                        item->setData( LabelRowDelegate::ExistsRole, exists );
                        item->setData( Qt::DisplayRole, name );   // accessibility
                        item->setToolTip( exists ? QDir::toNativeSeparators( path )
                                                 : tr("%1 (file not found)").arg( QDir::toNativeSeparators( path ) ) );
                        if ( mThumbs.contains( path ) &&
                             mThumbTimes.value( path ) == QFileInfo( path ).lastModified() )
                        {
                                item->setData( LabelRowDelegate::ThumbRole, mThumbs.value( path ) );
                        }
                        else if ( exists && !mThumbQueue.contains( path ) )
                        {
                                mThumbQueue << path;
                        }
                        mList->addItem( item );
                };

                if ( mMode == SampleMode )
                {
                        mSamples = Samples::list();
                        for ( const auto& sample : mSamples )
                        {
                                addRow( sample.path, sample.name, sample.description, sample.category,
                                        sample.builtIn ? tr("Sample") : tr("Template"), true );
                                auto* item = mList->item( mList->count() - 1 );
                                item->setToolTip( sample.builtIn ? sample.description
                                                                 : QDir::toNativeSeparators( sample.path ) );
                        }
                }
                else if ( mMode == RecentMode )
                {
                        for ( const auto& file : model::Settings::recentFileList() )
                        {
                                QFileInfo info( file );
                                addRow( file, info.completeBaseName(), displayFolder( info.absolutePath() ),
                                        Projects::projectOf( file ),
                                        info.exists() ? shortDateTime( info.lastModified() ) : tr("Not found"),
                                        info.exists() );
                        }
                }
                else
                {
                        for ( const auto& label : Projects::labels( mProject ) )
                        {
                                addRow( label.filePath, label.name,
                                        displayFolder( QFileInfo( label.filePath ).absolutePath() ),
                                        QString(), shortDateTime( label.modified ), true );
                        }
                }

                mRecentNav->setTrailing( QString::number( model::Settings::recentFileList().size() ),
                                         startup::NavButton::Count );
                if ( mMode != SampleMode )
                {
                        mSamples = Samples::list();
                }
                mSamplesNav->setTrailing( QString::number( mSamples.size() ), startup::NavButton::Count );

                mList->blockSignals( false );

                applyFilter();

                // Keep the previous selection when it is still listed, else select the first row
                QListWidgetItem* select = nullptr;
                for ( int i = 0; i < mList->count(); i++ )
                {
                        auto* item = mList->item( i );
                        if ( item->isHidden() ) continue;
                        if ( !select ) select = item;
                        if ( item->data( LabelRowDelegate::PathRole ).toString() == keep )
                        {
                                select = item;
                                break;
                        }
                }
                mList->setCurrentItem( select );
                if ( !select )
                {
                        updateInspector();
                }

                if ( !mThumbQueue.isEmpty() )
                {
                        QTimer::singleShot( 0, this, &StartupView::loadNextThumbnail );
                }
        }


        ///
        /// Hide rows that don't match the search text
        ///
        void StartupView::applyFilter()
        {
                QString query = mSearch->text().trimmed();
                int shown = 0;
                for ( int i = 0; i < mList->count(); i++ )
                {
                        auto* item = mList->item( i );
                        bool match = query.isEmpty() ||
                                     item->data( LabelRowDelegate::NameRole ).toString().contains( query, Qt::CaseInsensitive ) ||
                                     item->data( LabelRowDelegate::FolderRole ).toString().contains( query, Qt::CaseInsensitive ) ||
                                     item->data( LabelRowDelegate::ProjectRole ).toString().contains( query, Qt::CaseInsensitive );
                        item->setHidden( !match );
                        if ( match ) shown++;
                }

                if ( shown > 0 )
                {
                        mListStack->setCurrentWidget( mList );
                        auto* current = mList->currentItem();
                        if ( !current || current->isHidden() )
                        {
                                for ( int i = 0; i < mList->count(); i++ )
                                {
                                        if ( !mList->item( i )->isHidden() )
                                        {
                                                mList->setCurrentRow( i );
                                                break;
                                        }
                                }
                        }
                        return;
                }

                if ( mList->count() > 0 )
                {
                        mEmptyLabel->setText( tr("No labels match your search.") );
                }
                else if ( mMode == SampleMode )
                {
                        mEmptyLabel->setText( tr("No sample labels found.") );
                }
                else if ( mMode == RecentMode )
                {
                        mEmptyLabel->setText( tr("No recent labels yet.\n\nLabels you open or save will appear here.") );
                }
                else
                {
                        mEmptyLabel->setText( tr("\"%1\" has no labels yet.\n\nClick \"New label...\" to add one.")
                                              .arg( mProject ) );
                }
                mListStack->setCurrentWidget( mEmptyLabel );
                mList->setCurrentItem( nullptr );
                updateInspector();
        }


        ///
        /// Render row thumbnails one at a time so the screen stays responsive
        ///
        void StartupView::loadNextThumbnail()
        {
                if ( mThumbQueue.isEmpty() )
                {
                        return;
                }
                QString path = mThumbQueue.takeFirst();

                qreal dpr = devicePixelRatioF();
                QSize area = startup::LabelTileDelegate::MaxThumbSize;
                QImage image;
                int sample = sampleIndex( path );
                if ( sample >= 0 && mSamples[sample].builtIn )
                {
                        // Built-in samples are resources: render them directly
                        std::unique_ptr<model::Model> label( Samples::load( mSamples[sample] ) );
                        if ( label )
                        {
                                image = startup::renderLabel( *label, area * dpr );
                        }
                }
                else
                {
                        image = startup::labelThumbnail( path, area * dpr );
                }
                image.setDevicePixelRatio( dpr );
                mThumbs.insert( path, image );
                mThumbTimes.insert( path, QFileInfo( path ).lastModified() );

                for ( int i = 0; i < mList->count(); i++ )
                {
                        auto* item = mList->item( i );
                        if ( item->data( LabelRowDelegate::PathRole ).toString() == path )
                        {
                                item->setData( LabelRowDelegate::ThumbRole, image );
                        }
                }

                if ( !mThumbQueue.isEmpty() )
                {
                        QTimer::singleShot( 0, this, &StartupView::loadNextThumbnail );
                }
        }


        QString StartupView::selectedPath() const
        {
                auto* item = mList->currentItem();
                return ( item && !item->isHidden() ) ? item->data( LabelRowDelegate::PathRole ).toString() : QString();
        }


        ///
        /// Grid tile size (remembered between sessions)
        ///
        void StartupView::setTileSize( startup::LabelTileDelegate::Size size )
        {
                mTiles->setTileSize( size );
                mSmallTiles->setChecked( size == startup::LabelTileDelegate::Small );
                mMediumTiles->setChecked( size == startup::LabelTileDelegate::Medium );
                mList->setGridSize( mTiles->tileExtent() );
                mList->doItemsLayout();
                QSettings().setValue( "Startup/tileSize",
                                      size == startup::LabelTileDelegate::Small ? "small" : "medium" );
        }


        int StartupView::sampleIndex( const QString& path ) const
        {
                for ( int i = 0; i < mSamples.size(); i++ )
                {
                        if ( mSamples[i].path == path )
                        {
                                return i;
                        }
                }
                return -1;
        }


        void StartupView::onCurrentItemChanged()
        {
                updateInspector();
        }


        ///
        /// Show what we know about the selected label right away; the preview and
        /// product details follow once the selection settles.
        ///
        void StartupView::updateInspector()
        {
                QString path = selectedPath();
                bool has = !path.isEmpty();
                bool exists = has && QFileInfo::exists( path );

                bool samples = mMode == SampleMode;
                int sample = samples ? sampleIndex( path ) : -1;
                bool builtIn = sample >= 0 && mSamples[sample].builtIn;

                mOpenButton->setText( samples ? tr("Use this sample") : tr("Open label") );
                mOpenButton->setToolTip( samples ? tr("Open an untitled copy to edit; the sample itself never changes")
                                                 : QString() );
                mPrintButton->setToolTip( samples ? tr("Open an untitled copy on the Print page") : QString() );
                mShowFolderButton->setVisible( !builtIn );
                mInspProjectTerm->setText( samples ? tr("Category") : tr("Project") );
                mInspModifiedTerm->setVisible( !builtIn );
                mInspModified->setVisible( !builtIn );
                mInspPath->setVisible( !builtIn );
                mInspPathTerm->setVisible( !builtIn );

                mOpenButton->setEnabled( exists );
                mPrintButton->setEnabled( exists );
                mShowFolderButton->setEnabled( has && !builtIn && QFileInfo( path ).absoluteDir().exists() );
                mInspDetails->setVisible( has );
                mInspectorTimer->stop();

                if ( !has )
                {
                        mInspTitle->setText( mMode == ProjectMode ? mProject
                                             : samples          ? tr("No sample selected")
                                                                : tr("No label selected") );
                        mInspSub->setText( tr("Select a label to see its preview and details.") );
                        mPreview->setImage( QImage(), QString() );
                        return;
                }

                auto* item = mList->currentItem();
                if ( sample >= 0 )
                {
                        const auto& info = mSamples[sample];
                        mInspTitle->setText( info.name );
                        mInspSub->setText( info.builtIn ? info.description
                                                        : tr("Your template. Opening it creates an untitled copy.") );
                        mInspProject->setText( info.category );
                        mInspSize->setText( QString::fromUtf8( "—" ) );
                        mInspModified->setText( shortDateTime( QFileInfo( path ).lastModified() ) );
                        mInspPath->setText( displayFolder( QFileInfo( path ).absolutePath() ) );
                        mPreview->setImage( QImage(), QString() );
                        mInspectorTimer->start();
                        return;
                }
                QString project = Projects::projectOf( path );
                mInspTitle->setText( item->data( LabelRowDelegate::NameRole ).toString() );
                mInspSub->setText( exists ? QString() : tr("File not found") );
                mInspProject->setText( project.isEmpty() ? tr("None") : project );
                mInspSize->setText( QString::fromUtf8( "—" ) );
                mInspModified->setText( exists ? shortDateTime( QFileInfo( path ).lastModified() )
                                               : QString::fromUtf8( "—" ) );
                mInspPath->setText( item->data( LabelRowDelegate::FolderRole ).toString() );
                mPreview->setImage( QImage(), exists ? QString() : tr("This file was moved or deleted.") );

                if ( exists )
                {
                        mInspectorTimer->start();
                }
        }


        ///
        /// Load the selected label to show its preview, product and size
        ///
        void StartupView::updateInspectorDetails()
        {
                QString path = selectedPath();
                if ( path.isEmpty() )
                {
                        return;
                }

                int sample = mMode == SampleMode ? sampleIndex( path ) : -1;
                std::unique_ptr<model::Model> label( sample >= 0 ? Samples::load( mSamples[sample] )
                                                                 : model::XmlLabelParser::readFile( path ) );
                if ( !label )
                {
                        mInspSub->setText( tr("This file couldn't be read.") );
                        mPreview->setImage( QImage(), tr("No preview available") );
                        return;
                }

                const auto& tmplate = label->tmplate();
                QString product = QString( "%1 %2" ).arg( tmplate.brand(), tmplate.part() ).trimmed();
                QString description = tmplate.description();
                if ( sample < 0 )   // samples keep their own description
                {
                        mInspSub->setText( description.isEmpty() ? product
                                                                  : ( product.isEmpty() ? description
                                                                                        : product + QString::fromUtf8( " · " ) + description ) );
                }

                model::Units units = model::Settings::units();
                QLocale locale;
                mInspSize->setText( QString::fromUtf8( "%1 × %2 %3" )
                                    .arg( locale.toString( label->w().inUnits( units ), 'g', 4 ),
                                          locale.toString( label->h().inUnits( units ), 'g', 4 ),
                                          units.toIdString() ) );

                qreal dpr = devicePixelRatioF();
                QImage image = startup::renderLabel( *label, mPreview->imageArea() * dpr );
                image.setDevicePixelRatio( dpr );
                mPreview->setImage( image, tr("No preview available") );
        }


        ///
        /// Open the selected label in the editor, or on the Print page
        ///
        void StartupView::openSelected( bool print )
        {
                QString path = selectedPath();
                if ( path.isEmpty() || !QFileInfo::exists( path ) )
                {
                        return;
                }
                int sample = mMode == SampleMode ? sampleIndex( path ) : -1;
                MainWindow* target = sample >= 0 ? Samples::open( mSamples[sample], mWindow )
                                                 : File::open( path, mWindow );
                if ( print && target )
                {
                        QMetaObject::invokeMethod( target, "fileShowPrintPage" );
                }
        }


        ///
        /// Label context menu
        ///
        void StartupView::onListContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mList->itemAt( pos );
                if ( !item )
                {
                        return;
                }
                mList->setCurrentItem( item );
                QString filePath = item->data( LabelRowDelegate::PathRole ).toString();
                bool exists = item->data( LabelRowDelegate::ExistsRole ).toBool();

                if ( mMode == SampleMode )
                {
                        int sample = sampleIndex( filePath );
                        bool builtIn = sample >= 0 && mSamples[sample].builtIn;
                        QMenu menu;
                        menu.addAction( QIcon::fromTheme( "glabels-file-new" ), tr("Use this sample"),
                                        this, [this]{ openSelected( false ); } );
                        menu.addAction( tr("Print a copy..."), this, [this]{ openSelected( true ); } );
                        if ( !builtIn )
                        {
                                menu.addSeparator();
                                menu.addAction( tr("Edit original template"), this, [this, filePath]{
                                        File::open( filePath, mWindow );
                                } );
                                menu.addAction( tr("Show in folder"), this, [filePath]{
                                        QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( filePath ).absolutePath() ) );
                                } );
                                menu.addSeparator();
                                menu.addAction( QIcon::fromTheme( "glabels-delete" ), tr("Move to Trash..."),
                                                this, [this, filePath]{ trashTemplate( filePath ); } );
                        }
                        menu.exec( mList->viewport()->mapToGlobal( pos ) );
                        return;
                }

                QMenu menu;
                auto* open = menu.addAction( QIcon::fromTheme( "glabels-file-open" ), tr("Open"),
                                             this, [this]{ openSelected( false ); } );
                auto* print = menu.addAction( tr("Print..."), this, [this]{ openSelected( true ); } );
                open->setEnabled( exists );
                print->setEnabled( exists );

                if ( mMode == ProjectMode )
                {
                        menu.addAction( tr("Rename..."), this, [this, filePath]{ renameLabel( filePath ); } );
                        menu.addAction( QIcon::fromTheme( "glabels-copy" ), tr("Duplicate"),
                                        this, [this, filePath]{ duplicateLabel( filePath ); } );
                }
                menu.addAction( tr("Show in folder"), this, [filePath]{
                        QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( filePath ).absolutePath() ) );
                } );
                menu.addSeparator();
                if ( mMode == ProjectMode )
                {
                        menu.addAction( QIcon::fromTheme( "glabels-delete" ), tr("Move to Trash...\tDel"),
                                        this, [this, filePath]{ trashLabel( filePath ); } );
                        menu.addAction( tr("Delete Permanently...\tShift+Del"),
                                        this, [this, filePath]{ deleteLabel( filePath ); } );
                }
                else
                {
                        menu.addAction( tr("Remove from list\tDel"), this, [filePath]{
                                model::Settings::removeFromRecentFileList( filePath );
                        } );
                }
                menu.exec( mList->viewport()->mapToGlobal( pos ) );
        }


        ///
        /// Project context menu
        ///
        void StartupView::showProjectMenu( const QString& project, const QPoint& globalPos )
        {
                setMode( ProjectMode, project );

                QMenu menu;
                menu.addAction( QIcon::fromTheme( "glabels-file-new" ), tr("New label..."),
                                this, &StartupView::onNewLabelInProject );
                menu.addAction( QIcon::fromTheme( "glabels-file-open" ), tr("Open folder"),
                                this, &StartupView::onOpenProjectFolder );
                menu.addSeparator();
                menu.addAction( tr("Rename..."), this, [this, project]{ renameProject( project ); } );
                menu.addAction( QIcon::fromTheme( "glabels-delete" ), tr("Move to Trash...\tDel"),
                                this, [this, project]{ trashProject( project ); } );
                menu.addAction( tr("Delete Permanently...\tShift+Del"),
                                this, [this, project]{ deleteProject( project ); } );
                menu.exec( globalPos );
        }


        ///
        /// Settings changed Slot (recent files list, units, ...)
        ///
        void StartupView::onSettingsChanged()
        {
                // Settings change for many reasons (units, toolbars, ...); only the
                // recent files list affects this screen.  Thumbnails are redrawn only
                // for files whose modification time changed.
                QStringList recent = model::Settings::recentFileList();
                if ( recent != mRecentFiles )
                {
                        loadProjects();
                        loadList();
                }
        }


        void StartupView::onAppearanceChanged()
        {
                updateStyle();
        }


        ///
        /// Watch the projects root and each project folder for changes
        ///
        void StartupView::watchProjectFolders()
        {
                if ( !mWatcher->directories().isEmpty() )
                {
                        mWatcher->removePaths( mWatcher->directories() );
                }
                QStringList dirs { Projects::rootDir().absolutePath() };
                for ( const auto& name : Projects::list() )
                {
                        dirs << Projects::path( name );
                }
                if ( Samples::userDir().exists() )
                {
                        dirs << Samples::userDir().absolutePath();
                }
                mWatcher->addPaths( dirs );
        }


        void StartupView::onProjectsChangedOnDisk()
        {
                loadProjects();
                setMode( mMode );
        }


        void StartupView::onCreateProject()
        {
                QString name = Projects::askNewProject( this );
                if ( !name.isEmpty() )
                {
                        loadProjects( name );
                        setMode( ProjectMode, name );
                }
        }


        void StartupView::onNewLabelInProject()
        {
                QString project = mProject;
                if ( !project.isEmpty() )
                {
                        File::newLabelInProject( mWindow, project );
                        loadProjects( project );
                        setMode( ProjectMode, project );
                }
        }


        void StartupView::onOpenProjectFolder()
        {
                if ( !mProject.isEmpty() )
                {
                        QDesktopServices::openUrl( QUrl::fromLocalFile( Projects::path( mProject ) ) );
                }
        }


        void StartupView::renameProject( const QString& project )
        {
                QString name = Projects::askName( this, tr("Rename Project"), tr("Project name:"),
                                                  project, Projects::rootDir().absolutePath(), QString() );
                if ( name.isEmpty() || name == project )
                {
                        return;
                }
                QString error;
                if ( !Projects::rename( project, name, &error ) )
                {
                        QMessageBox::warning( this, tr("Rename Project"), error );
                        return;
                }

                // Keep the recent-files list pointing at the moved labels
                QString oldDir = Projects::rootDir().absoluteFilePath( project ) + "/";
                for ( const auto& file : model::Settings::recentFileList() )
                {
                        if ( file.startsWith( oldDir ) )
                        {
                                model::Settings::removeFromRecentFileList( file );
                        }
                }
                loadProjects( name );
                setMode( ProjectMode, name );
        }


        void StartupView::trashProject( const QString& project )
        {
                int n = Projects::labelCount( project );
                QMessageBox box( QMessageBox::Warning, tr("Move Project to Trash"),
                                 tr("Move the project \"%1\" to the Trash?").arg( project ),
                                 QMessageBox::NoButton, this );
                box.setInformativeText( ( n == 1 ? tr("Its folder and 1 label will be moved to the Trash.")
                                                 : tr("Its folder and %1 labels will be moved to the Trash.").arg( n ) )
                                        + " " + tr("You can restore them from there.") );
                auto* trash = box.addButton( tr("Move to Trash"), QMessageBox::DestructiveRole );
                box.addButton( QMessageBox::Cancel );
                box.setDefaultButton( QMessageBox::Cancel );
                box.exec();
                if ( box.clickedButton() != static_cast<QAbstractButton*>( trash ) )
                {
                        return;
                }

                QString error;
                if ( !Projects::moveToTrash( project, &error ) )
                {
                        QMessageBox::warning( this, tr("Move Project to Trash"), error );
                        return;
                }
                QSettings().remove( "Projects/current" );
                loadProjects();
                setMode( RecentMode );
        }


        void StartupView::deleteProject( const QString& project )
        {
                int n = Projects::labelCount( project );
                QMessageBox box( QMessageBox::Warning, tr("Delete Project Permanently"),
                                 tr("Permanently delete the project \"%1\"?").arg( project ),
                                 QMessageBox::NoButton, this );
                box.setInformativeText( ( n == 1 ? tr("Its folder and 1 label will be deleted.")
                                                 : tr("Its folder and %1 labels will be deleted.").arg( n ) )
                                        + " " + tr("This can't be undone.") );
                auto* del = box.addButton( tr("Delete Permanently"), QMessageBox::DestructiveRole );
                box.addButton( QMessageBox::Cancel );
                box.setDefaultButton( QMessageBox::Cancel );
                box.exec();
                if ( box.clickedButton() != static_cast<QAbstractButton*>( del ) )
                {
                        return;
                }

                QString error;
                if ( !Projects::deletePermanently( project, &error ) )
                {
                        QMessageBox::warning( this, tr("Delete Project Permanently"), error );
                        return;
                }
                QString dir = Projects::rootDir().absoluteFilePath( project ) + "/";
                for ( const auto& file : model::Settings::recentFileList() )
                {
                        if ( file.startsWith( dir ) )
                        {
                                model::Settings::removeFromRecentFileList( file );
                        }
                }
                QSettings().remove( "Projects/current" );
                loadProjects();
                setMode( RecentMode );
        }


        void StartupView::renameLabel( const QString& filePath )
        {
                if ( Projects::isOpen( filePath ) )
                {
                        QMessageBox::information( this, tr("Rename Label"), tr("Close this label first, then rename it.") );
                        return;
                }
                QFileInfo info( filePath );
                QString name = Projects::askName( this, tr("Rename Label"), tr("Label name:"),
                                                  info.completeBaseName(), info.absolutePath(), ".glabels" );
                if ( name.isEmpty() || name == info.completeBaseName() )
                {
                        return;
                }
                QString newPath = info.absoluteDir().absoluteFilePath( name + ".glabels" );
                if ( !QFile::rename( filePath, newPath ) )
                {
                        QMessageBox::warning( this, tr("Rename Label"), tr("Couldn't rename the label file.") );
                        return;
                }
                model::Settings::removeFromRecentFileList( filePath );
                loadProjects();
                setMode( mMode );
        }


        void StartupView::duplicateLabel( const QString& filePath )
        {
                QFileInfo info( filePath );
                QDir dir = info.absoluteDir();
                QString base = tr("%1 copy").arg( info.completeBaseName() );
                QString name = base;
                for ( int i = 2; QFileInfo::exists( dir.absoluteFilePath( name + ".glabels" ) ); i++ )
                {
                        name = QString( "%1 %2" ).arg( base ).arg( i );
                }
                if ( !QFile::copy( filePath, dir.absoluteFilePath( name + ".glabels" ) ) )
                {
                        QMessageBox::warning( this, tr("Duplicate Label"), tr("Couldn't copy the label file.") );
                        return;
                }
                loadProjects();
                setMode( mMode );
        }


        void StartupView::trashLabel( const QString& filePath )
        {
                if ( Projects::isOpen( filePath ) )
                {
                        QMessageBox::information( this, tr("Move Label to Trash"),
                                                  tr("Close this label first, then move it to the Trash.") );
                        return;
                }
                QString name = QFileInfo( filePath ).completeBaseName();
                if ( QMessageBox::question( this, tr("Move Label to Trash"),
                                            tr("Move the label \"%1\" to the Trash?").arg( name ),
                                            QMessageBox::Yes | QMessageBox::Cancel,
                                            QMessageBox::Cancel ) != QMessageBox::Yes )
                {
                        return;
                }
                if ( !QFile::moveToTrash( filePath ) )
                {
                        QMessageBox::warning( this, tr("Move Label to Trash"), tr("Couldn't move the label to the Trash.") );
                        return;
                }
                model::Settings::removeFromRecentFileList( filePath );
                loadProjects();
                setMode( mMode );
        }


        void StartupView::trashTemplate( const QString& filePath )
        {
                QString name = QFileInfo( filePath ).completeBaseName();
                if ( QMessageBox::question( this, tr("Move Template to Trash"),
                                            tr("Move your template \"%1\" to the Trash?").arg( name ),
                                            QMessageBox::Yes | QMessageBox::Cancel,
                                            QMessageBox::Cancel ) != QMessageBox::Yes )
                {
                        return;
                }
                if ( !QFile::moveToTrash( filePath ) )
                {
                        QMessageBox::warning( this, tr("Move Template to Trash"),
                                              tr("Couldn't move the template to the Trash.") );
                        return;
                }
                setMode( mMode );
        }


        void StartupView::deleteLabel( const QString& filePath )
        {
                if ( Projects::isOpen( filePath ) )
                {
                        QMessageBox::information( this, tr("Delete Label Permanently"),
                                                  tr("Close this label first, then delete it.") );
                        return;
                }
                QString name = QFileInfo( filePath ).completeBaseName();
                QMessageBox box( QMessageBox::Warning, tr("Delete Label Permanently"),
                                 tr("Permanently delete the label \"%1\"?").arg( name ),
                                 QMessageBox::NoButton, this );
                box.setInformativeText( tr("This can't be undone.") );
                auto* del = box.addButton( tr("Delete Permanently"), QMessageBox::DestructiveRole );
                box.addButton( QMessageBox::Cancel );
                box.setDefaultButton( QMessageBox::Cancel );
                box.exec();
                if ( box.clickedButton() != static_cast<QAbstractButton*>( del ) )
                {
                        return;
                }
                if ( !QFile::remove( filePath ) )
                {
                        QMessageBox::warning( this, tr("Delete Label Permanently"), tr("Couldn't delete the label file.") );
                        return;
                }
                model::Settings::removeFromRecentFileList( filePath );
                loadProjects();
                setMode( mMode );
        }


} // namespace glabels
