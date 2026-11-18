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

#include "model/Settings.hpp"
#include "model/Version.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileSystemWatcher>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QShortcut>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>


namespace
{
        const int   cardIconSize   = 32;
        const int   contentMaxWidth = 1120;
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

                // Everything lives in a scroll area so small windows still work
                auto* outerLayout = new QVBoxLayout( this );
                outerLayout->setContentsMargins( 0, 0, 0, 0 );

                auto* scroll = new QScrollArea();
                scroll->setObjectName( "startupScroll" );
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidgetResizable( true );
                outerLayout->addWidget( scroll );

                auto* canvas = new QWidget();
                canvas->setObjectName( "startupCanvas" );
                scroll->setWidget( canvas );

                // Center a column of bounded width
                auto* canvasLayout = new QHBoxLayout( canvas );
                canvasLayout->setContentsMargins( 32, 32, 32, 32 );
                canvasLayout->addStretch( 1 );

                auto* column = new QWidget();
                column->setMaximumWidth( contentMaxWidth );
                column->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
                canvasLayout->addWidget( column, 100 );
                canvasLayout->addStretch( 1 );

                auto* columnLayout = new QVBoxLayout( column );
                columnLayout->setContentsMargins( 0, 0, 0, 0 );
                columnLayout->setSpacing( 24 );

                //
                // Header: logo, tagline and version
                //
                auto* header = new QWidget();
                auto* headerLayout = new QVBoxLayout( header );
                headerLayout->setContentsMargins( 0, 0, 0, 0 );
                headerLayout->setSpacing( 6 );

                mLogoLabel = new QLabel();
                mLogoLabel->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
                headerLayout->addWidget( mLogoLabel );

                auto* tagline = new QLabel( tr("Design and print labels, business cards and badges.") );
                tagline->setObjectName( "startupTagline" );
                tagline->setWordWrap( true );
                headerLayout->addWidget( tagline );

                mVersionLabel = new QLabel( tr("Version %1").arg( model::Version::STRING ) );
                mVersionLabel->setObjectName( "startupMuted" );
                headerLayout->addWidget( mVersionLabel );

                columnLayout->addWidget( header );

                //
                // Body: "Start" cards on the left, "Recent" list on the right
                //
                auto* body = new QHBoxLayout();
                body->setSpacing( 24 );
                columnLayout->addLayout( body, 1 );

                // Start
                auto* startColumn = new QVBoxLayout();
                startColumn->setSpacing( 8 );
                auto* startTitle = new QLabel( tr("Start") );
                startTitle->setObjectName( "startupSectionTitle" );
                startColumn->addWidget( startTitle );

                mNewButton = createActionCard( "glabels-file-new",
                                               tr("New label..."),
                                               tr("Pick a label or card product and start designing") );
                mOpenButton = createActionCard( "glabels-file-open",
                                                tr("Open label..."),
                                                tr("Browse for an existing gLabels Plus label file") );
                mDesignerButton = createActionCard( "glabels-file-new",
                                                    tr("Product template designer..."),
                                                    tr("Create a template for a product that isn't listed") );
                connect( mNewButton,      &QToolButton::clicked, this, &StartupView::onNewProjectButtonClicked );
                connect( mOpenButton,     &QToolButton::clicked, this, &StartupView::onOpenProjectButtonClicked );
                connect( mDesignerButton, &QToolButton::clicked, this, &StartupView::onTemplateDesignerButtonClicked );

                startColumn->addWidget( mNewButton );
                startColumn->addWidget( mOpenButton );
                startColumn->addWidget( mDesignerButton );
                startColumn->addStretch( 1 );

                auto* startWidget = new QWidget();
                startWidget->setLayout( startColumn );
                startColumn->setContentsMargins( 0, 0, 0, 0 );
                startWidget->setMinimumWidth( 260 );
                body->addWidget( startWidget, 5 );

                // Recent
                auto* recentColumn = new QVBoxLayout();
                recentColumn->setContentsMargins( 0, 0, 0, 0 );
                recentColumn->setSpacing( 8 );
                auto* recentTitle = new QLabel( tr("Recent labels") );
                recentTitle->setObjectName( "startupSectionTitle" );
                recentColumn->addWidget( recentTitle );

                mRecentList = new QListWidget();
                mRecentList->setObjectName( "startupRecentList" );
                mRecentList->setIconSize( QSize( 24, 24 ) );
                mRecentList->setUniformItemSizes( true );
                mRecentList->setTextElideMode( Qt::ElideMiddle );
                mRecentList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mRecentList->setContextMenuPolicy( Qt::CustomContextMenu );
                mRecentList->setMinimumHeight( 180 );
                connect( mRecentList, &QListWidget::itemActivated, this, &StartupView::onRecentItemActivated );
                connect( mRecentList, &QListWidget::itemClicked,   this, &StartupView::onRecentItemActivated );
                connect( mRecentList, &QWidget::customContextMenuRequested, this, &StartupView::onRecentContextMenu );

                mRecentEmptyLabel = new QLabel( tr("No recent labels yet.\n\n"
                                                   "Labels you open or save will appear here.") );
                mRecentEmptyLabel->setObjectName( "startupEmpty" );
                mRecentEmptyLabel->setAlignment( Qt::AlignCenter );
                mRecentEmptyLabel->setWordWrap( true );

                mRecentStack = new QStackedWidget();
                mRecentStack->addWidget( mRecentList );
                mRecentStack->addWidget( mRecentEmptyLabel );
                recentColumn->addWidget( mRecentStack, 1 );

                auto* recentWidget = new QWidget();
                recentWidget->setLayout( recentColumn );
                recentWidget->setMinimumWidth( 260 );
                body->addWidget( recentWidget, 6 );

                //
                // Projects: folders that group labels, e.g. one per business
                //
                columnLayout->addWidget( createProjectsSection(), 2 );

                //
                // Footer: quick links
                //
                // (Link text is set in updateStyle() so link colors follow the theme.)
                auto* footer = new QLabel();
                mFooterLabel = footer;
                footer->setObjectName( "startupMuted" );
                footer->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
                connect( footer, &QLabel::linkActivated, this, [this]( const QString& link ) {
                        if      ( link == "prefs" ) QMetaObject::invokeMethod( mWindow, "editPreferences" );
                        else if ( link == "keys" )  QMetaObject::invokeMethod( mWindow, "editShortcuts" );
                        else if ( link == "help" )  QMetaObject::invokeMethod( mWindow, "helpContents" );
                        else if ( link == "whatsnew" )
                        {
                                WhatsNewDialog dialog( this );
                                dialog.exec();
                        }
                } );

                //
                // Contribution banner
                //
                auto* banner = new QLabel();
                mBannerLabel = banner;
                banner->setObjectName( "startupBanner" );
                banner->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                banner->setWordWrap( true );
                banner->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );
                banner->setMinimumWidth( 260 );
                banner->setMaximumWidth( 320 );
                banner->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
                banner->setToolTip( "https://usmansaif.com" );
                connect( banner, &QLabel::linkActivated, this, []( const QString& link ) {
                        QDesktopServices::openUrl( QUrl( link ) );
                } );

                //
                // Bottom row: quick links on the left, banner in the bottom-right corner
                //
                auto* bottomRow = new QHBoxLayout();
                bottomRow->setSpacing( 24 );
                bottomRow->addWidget( footer, 1, Qt::AlignLeft | Qt::AlignBottom );
                bottomRow->addWidget( banner, 0, Qt::AlignRight | Qt::AlignBottom );
                columnLayout->addLayout( bottomRow );

                updateStyle();
                loadRecentList();
                loadProjects( QSettings().value( "Projects/current" ).toString() );

                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                connect( AppearanceModel::instance(), SIGNAL(changed()), this, SLOT(onAppearanceChanged()) );
        }


        ///
        /// Create a large, flat "card" style button
        ///
        QToolButton* StartupView::createActionCard( const QString& iconName,
                                                    const QString& title,
                                                    const QString& description )
        {
                auto* button = new QToolButton();
                button->setObjectName( "startupCard" );
                button->setIcon( QIcon::fromTheme( iconName ) );
                button->setIconSize( QSize( cardIconSize, cardIconSize ) );
                button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                button->setText( title + "\n" + description );
                button->setToolTip( description );
                button->setAccessibleName( title );
                button->setAccessibleDescription( description );
                button->setCursor( Qt::PointingHandCursor );
                button->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                button->setMinimumHeight( cardIconSize + 28 );
                return button;
        }


        ///
        /// Re-apply colors and the logo whenever the palette changes
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

                mLogoLabel->setPixmap( QPixmap( dark ? ":images/glabels-label-designer-dark.png"
                                                     : ":images/glabels-label-designer.png" ) );

                // QLabel bakes link colors in when its text is set, so rebuild the
                // rich text with an explicit, theme-appropriate link color.
                QString link = QString( "style='color:%1;'" )
                        .arg( dark ? QColor( 110, 180, 240 ).name() : palette().color( QPalette::Link ).name() );
                mFooterLabel->setText(
                        QString( "<a %4 href='prefs'>%1</a> &nbsp;·&nbsp; "
                                 "<a %4 href='keys'>%2</a> &nbsp;·&nbsp; "
                                 "<a %4 href='help'>%3</a> &nbsp;·&nbsp; "
                                 "<a %4 href='whatsnew'>%5</a>" )
                        .arg( tr("Preferences"), tr("Keyboard shortcuts"), tr("Help"), link, tr("What's New") ) );
                mBannerLabel->setText(
                        QString( "%1<br>%3: <a %2 href='https://usmansaif.com'><b>Usman Saif</b></a>" )
                        .arg( tr("Enhanced with new features and a refreshed interface."), link, tr("Maintainer") ) );

                setStyleSheet(
                        "#startupCanvas { background: palette(base); }"
                        "#startupTagline { font-size: 13pt; }"
                        "#startupMuted { color: palette(placeholder-text); }"
                        "#startupBanner {"
                        "  padding: 8px 14px; margin-top: 4px;"
                        "  border: 1px solid palette(highlight); border-radius: 8px;"
                        "  background: palette(alternate-base); }"
                        "#startupSectionTitle { font-weight: 600; font-size: 11pt; padding-bottom: 2px; }"
                        "#startupEmpty { color: palette(placeholder-text);"
                        "  border: 1px dashed palette(mid); border-radius: 8px; padding: 16px; }"
                        "QToolButton#startupCard {"
                        "  text-align: left; padding: 10px 14px;"
                        "  border: 1px solid palette(mid); border-radius: 8px;"
                        "  background: palette(window); }"
                        "QToolButton#startupCard:hover {"
                        "  border-color: palette(highlight); background: palette(alternate-base); }"
                        "QToolButton#startupCard:pressed { background: palette(midlight); }"
                        "QToolButton#startupCard:focus { border: 2px solid palette(highlight); }"
                        "#startupRecentList, #startupProjectList, #startupProjectLabels {"
                        "  border: 1px solid palette(mid); border-radius: 8px; padding: 4px;"
                        "  background: palette(window); }"
                        "#startupRecentList::item, #startupProjectList::item, #startupProjectLabels::item {"
                        "  padding: 6px 4px; border-radius: 6px; }"
                        "#startupRecentList::item:hover, #startupProjectList::item:hover,"
                        "#startupProjectLabels::item:hover { background: palette(alternate-base); }"
                        // Every list needs an explicit selected style: once ::item is styled,
                        // Qt keeps the highlighted (white) text but drops the highlight
                        // background, which leaves selected rows unreadable.
                        "#startupRecentList::item:selected, #startupProjectList::item:selected,"
                        "#startupProjectLabels::item:selected,"
                        "#startupRecentList::item:selected:hover, #startupProjectList::item:selected:hover,"
                        "#startupProjectLabels::item:selected:hover {"
                        "  background: palette(highlight); color: palette(highlighted-text); }"
                        "#startupProjectTitle { font-weight: 600; font-size: 11pt; }"
                        "QToolButton#startupSmallButton {"
                        "  padding: 5px 12px; border: 1px solid palette(mid); border-radius: 6px;"
                        "  background: palette(window); }"
                        "QToolButton#startupSmallButton:hover {"
                        "  border-color: palette(highlight); background: palette(alternate-base); }"
                        "QToolButton#startupSmallButton:disabled { color: palette(placeholder-text); }"
                        "QToolButton#startupPrimaryButton {"
                        "  padding: 5px 14px; border: 1px solid palette(highlight); border-radius: 6px;"
                        "  background: palette(highlight); color: palette(highlighted-text); font-weight: 600; }"
                        "QToolButton#startupPrimaryButton:hover { background: palette(link); }"
                );

                mUpdatingStyle = false;
        }


        ///
        /// Palette/style change events
        ///
        void StartupView::changeEvent( QEvent* event )
        {
                QWidget::changeEvent( event );
                if ( event->type() == QEvent::PaletteChange )
                {
                        updateStyle();
                }
        }


        ///
        /// "New Project" Button Clicked Slot
        ///
        void StartupView::onNewProjectButtonClicked()
        {
                File::newLabel( mWindow );
        }


        ///
        /// "Open Project" Button Clicked Slot
        ///
        void StartupView::onOpenProjectButtonClicked()
        {
                File::open( mWindow );
        }


        ///
        /// "Template Designer" Button Clicked Slot
        ///
        void StartupView::onTemplateDesignerButtonClicked()
        {
                File::templateDesigner( mWindow );
        }


        ///
        /// Recent item activated
        ///
        void StartupView::onRecentItemActivated( QListWidgetItem* item )
        {
                if ( !item )
                {
                        return;
                }
                QString filename = item->data( Qt::UserRole ).toString();
                if ( !filename.isEmpty() )
                {
                        File::open( filename, mWindow );
                }
        }


        ///
        /// Recent list context menu
        ///
        void StartupView::onRecentContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mRecentList->itemAt( pos );
                if ( !item )
                {
                        return;
                }
                QString filename = item->data( Qt::UserRole ).toString();

                QMenu menu;
                menu.addAction( QIcon::fromTheme( "glabels-file-open" ), tr("Open"), this, [this, filename]{
                        File::open( filename, mWindow );
                } );
                menu.addSeparator();
                menu.addAction( tr("Remove from list"), this, [filename]{
                        model::Settings::removeFromRecentFileList( filename );
                } );
                menu.exec( mRecentList->viewport()->mapToGlobal( pos ) );
        }


        ///
        /// Settings changed Slot
        ///
        void StartupView::onSettingsChanged()
        {
                loadRecentList();
                loadProjectLabels();   // a save may have changed a project's labels
        }


        ///
        /// Appearance changed Slot
        ///
        void StartupView::onAppearanceChanged()
        {
                updateStyle();
        }


        ///
        /// (Re)load recent projects list
        ///
        void StartupView::loadRecentList()
        {
                auto fileList = model::Settings::recentFileList();

                mRecentList->clear();
                for ( auto& filename : fileList )
                {
                        QFileInfo info( filename );
                        QString folder = QDir::toNativeSeparators( info.absolutePath() );
                        QString home   = QDir::toNativeSeparators( QDir::homePath() );
                        if ( folder.startsWith( home ) )
                        {
                                folder = "~" + folder.mid( home.length() );
                        }

                        auto* item = new QListWidgetItem( QIcon::fromTheme( "glabels" ),
                                                          info.completeBaseName() + "\n" + folder );
                        item->setData( Qt::UserRole, filename );
                        item->setToolTip( QDir::toNativeSeparators( filename ) );
                        if ( !info.exists() )
                        {
                                item->setFlags( item->flags() & ~Qt::ItemIsEnabled );
                                item->setToolTip( tr("%1 (file not found)").arg( item->toolTip() ) );
                        }
                        mRecentList->addItem( item );
                }

                mRecentStack->setCurrentWidget( fileList.isEmpty() ? static_cast<QWidget*>( mRecentEmptyLabel )
                                                                   : static_cast<QWidget*>( mRecentList ) );
        }


        ///
        /// Build the Projects section: project list on the left, the selected
        /// project's labels on the right
        ///
        QWidget* StartupView::createProjectsSection()
        {
                auto* section = new QWidget();
                auto* layout = new QVBoxLayout( section );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 8 );

                auto makeButton = []( const QString& text, const QString& tip, const char* objectName,
                                      const QIcon& icon = QIcon() ) {
                        auto* b = new QToolButton();
                        b->setObjectName( objectName );
                        b->setText( text );
                        b->setToolTip( tip );
                        b->setIcon( icon );
                        b->setToolButtonStyle( icon.isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon );
                        b->setCursor( Qt::PointingHandCursor );
                        return b;
                };

                // Title row
                auto* titleRow = new QHBoxLayout();
                auto* title = new QLabel( tr("Projects") );
                title->setObjectName( "startupSectionTitle" );
                auto* subtitle = new QLabel( tr("Keep each business or client's labels together") );
                subtitle->setObjectName( "startupMuted" );
                auto* newProject = makeButton( tr("+  New project..."),
                                               tr("Create a project, for example for a business or client"),
                                               "startupPrimaryButton" );
                connect( newProject, &QToolButton::clicked, this, &StartupView::onCreateProject );
                titleRow->addWidget( title, 0, Qt::AlignVCenter );
                titleRow->addSpacing( 8 );
                titleRow->addWidget( subtitle, 0, Qt::AlignVCenter );
                titleRow->addStretch( 1 );
                titleRow->addWidget( newProject );
                layout->addLayout( titleRow );

                // Body
                auto* body = new QHBoxLayout();
                body->setSpacing( 16 );
                layout->addLayout( body, 1 );

                // Left: projects
                mProjectList = new QListWidget();
                mProjectList->setObjectName( "startupProjectList" );
                mProjectList->setIconSize( QSize( 24, 24 ) );
                mProjectList->setTextElideMode( Qt::ElideRight );
                mProjectList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mProjectList->setContextMenuPolicy( Qt::CustomContextMenu );
                mProjectList->setMinimumHeight( 200 );
                connect( mProjectList, &QListWidget::itemSelectionChanged, this, &StartupView::onProjectSelectionChanged );
                connect( mProjectList, &QWidget::customContextMenuRequested, this, &StartupView::onProjectContextMenu );

                // Delete = move to Trash, Shift+Delete = delete permanently
                auto* projectTrashKey = new QShortcut( QKeySequence( Qt::Key_Delete ), mProjectList );
                projectTrashKey->setContext( Qt::WidgetShortcut );
                connect( projectTrashKey, &QShortcut::activated, this, [this]{
                        if ( !currentProject().isEmpty() ) trashProject( currentProject() );
                } );
                auto* projectDeleteKey = new QShortcut( QKeySequence( Qt::SHIFT | Qt::Key_Delete ), mProjectList );
                projectDeleteKey->setContext( Qt::WidgetShortcut );
                connect( projectDeleteKey, &QShortcut::activated, this, [this]{
                        if ( !currentProject().isEmpty() ) deleteProject( currentProject() );
                } );

                mProjectsEmptyLabel = new QLabel( tr("No projects yet.\n\n"
                                                     "Create a project for each business or client,\n"
                                                     "then add its labels to it.") );
                mProjectsEmptyLabel->setObjectName( "startupEmpty" );
                mProjectsEmptyLabel->setAlignment( Qt::AlignCenter );
                mProjectsEmptyLabel->setWordWrap( true );

                mProjectsStack = new QStackedWidget();
                mProjectsStack->addWidget( mProjectList );
                mProjectsStack->addWidget( mProjectsEmptyLabel );
                mProjectsStack->setMinimumWidth( 260 );
                body->addWidget( mProjectsStack, 4 );

                // Right: labels in the selected project
                auto* labelsColumn = new QVBoxLayout();
                labelsColumn->setSpacing( 8 );
                auto* labelsHeader = new QHBoxLayout();
                mProjectTitle = new QLabel();
                mProjectTitle->setObjectName( "startupProjectTitle" );
                mProjectTitle->setTextFormat( Qt::PlainText );
                mNewLabelInProjectButton = makeButton( tr("New label..."),
                                                       tr("Choose a product and add a new label to this project"),
                                                       "startupSmallButton",
                                                       QIcon::fromTheme( "glabels-file-new" ) );
                mProjectFolderButton = makeButton( tr("Open folder"),
                                                   tr("Show this project's folder in your file manager"),
                                                   "startupSmallButton",
                                                   QIcon::fromTheme( "glabels-file-open" ) );
                connect( mNewLabelInProjectButton, &QToolButton::clicked, this, &StartupView::onNewLabelInProject );
                connect( mProjectFolderButton, &QToolButton::clicked, this, &StartupView::onOpenProjectFolder );
                labelsHeader->addWidget( mProjectTitle, 1 );
                labelsHeader->addWidget( mNewLabelInProjectButton );
                labelsHeader->addWidget( mProjectFolderButton );
                labelsColumn->addLayout( labelsHeader );

                mProjectLabelList = new QListWidget();
                mProjectLabelList->setObjectName( "startupProjectLabels" );
                mProjectLabelList->setIconSize( QSize( 24, 24 ) );
                mProjectLabelList->setTextElideMode( Qt::ElideRight );
                mProjectLabelList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mProjectLabelList->setContextMenuPolicy( Qt::CustomContextMenu );
                connect( mProjectLabelList, &QListWidget::itemActivated, this, &StartupView::onProjectLabelActivated );
                connect( mProjectLabelList, &QListWidget::itemDoubleClicked, this, &StartupView::onProjectLabelActivated );
                connect( mProjectLabelList, &QWidget::customContextMenuRequested, this, &StartupView::onProjectLabelContextMenu );

                // Delete = move to Trash, Shift+Delete = delete permanently
                auto selectedLabel = [this]() -> QString {
                        auto* item = mProjectLabelList->currentItem();
                        return ( item && item->isSelected() ) ? item->data( Qt::UserRole ).toString() : QString();
                };
                auto* labelTrashKey = new QShortcut( QKeySequence( Qt::Key_Delete ), mProjectLabelList );
                labelTrashKey->setContext( Qt::WidgetShortcut );
                connect( labelTrashKey, &QShortcut::activated, this, [this, selectedLabel]{
                        QString path = selectedLabel();
                        if ( !path.isEmpty() ) trashLabel( path );
                } );
                auto* labelDeleteKey = new QShortcut( QKeySequence( Qt::SHIFT | Qt::Key_Delete ), mProjectLabelList );
                labelDeleteKey->setContext( Qt::WidgetShortcut );
                connect( labelDeleteKey, &QShortcut::activated, this, [this, selectedLabel]{
                        QString path = selectedLabel();
                        if ( !path.isEmpty() ) deleteLabel( path );
                } );

                mProjectLabelsEmptyLabel = new QLabel();
                mProjectLabelsEmptyLabel->setObjectName( "startupEmpty" );
                mProjectLabelsEmptyLabel->setAlignment( Qt::AlignCenter );
                mProjectLabelsEmptyLabel->setWordWrap( true );

                mProjectLabelsStack = new QStackedWidget();
                mProjectLabelsStack->addWidget( mProjectLabelList );
                mProjectLabelsStack->addWidget( mProjectLabelsEmptyLabel );
                labelsColumn->addWidget( mProjectLabelsStack, 1 );

                auto* labelsWidget = new QWidget();
                labelsColumn->setContentsMargins( 0, 0, 0, 0 );
                labelsWidget->setLayout( labelsColumn );
                body->addWidget( labelsWidget, 7 );

                // Keep lists current when files change on disk (saves, file manager edits)
                mReloadTimer = new QTimer( this );
                mReloadTimer->setSingleShot( true );
                mReloadTimer->setInterval( 250 );
                connect( mReloadTimer, &QTimer::timeout, this, &StartupView::onProjectsChangedOnDisk );

                mWatcher = new QFileSystemWatcher( this );
                connect( mWatcher, &QFileSystemWatcher::directoryChanged, mReloadTimer, qOverload<>( &QTimer::start ) );

                return section;
        }


        ///
        /// Name of the selected project, or empty
        ///
        QString StartupView::currentProject() const
        {
                auto items = mProjectList->selectedItems();
                return items.isEmpty() ? QString() : items.first()->data( Qt::UserRole ).toString();
        }


        ///
        /// Select a project by name
        ///
        void StartupView::selectProject( const QString& project )
        {
                loadProjects( project );
        }


        ///
        /// (Re)load the project list, keeping or setting the selection
        ///
        void StartupView::loadProjects( const QString& select )
        {
                QString wanted = select.isEmpty() ? currentProject() : select;

                QIcon folderIcon = QIcon::fromTheme( "folder", style()->standardIcon( QStyle::SP_DirIcon ) );

                mProjectList->blockSignals( true );
                mProjectList->clear();
                QListWidgetItem* selectItem = nullptr;
                for ( const auto& name : Projects::list() )
                {
                        int n = Projects::labelCount( name );
                        auto* item = new QListWidgetItem( folderIcon,
                                                          name + "\n" + ( n == 1 ? tr("1 label") : tr("%1 labels").arg( n ) ) );
                        item->setData( Qt::UserRole, name );
                        item->setToolTip( QDir::toNativeSeparators( Projects::path( name ) ) );
                        mProjectList->addItem( item );
                        if ( name == wanted )
                        {
                                selectItem = item;
                        }
                }
                if ( !selectItem && mProjectList->count() > 0 )
                {
                        selectItem = mProjectList->item( 0 );
                }
                if ( selectItem )
                {
                        selectItem->setSelected( true );
                        mProjectList->setCurrentItem( selectItem );
                        mProjectList->scrollToItem( selectItem );
                }
                mProjectList->blockSignals( false );

                mProjectsStack->setCurrentWidget( mProjectList->count() ? static_cast<QWidget*>( mProjectList )
                                                                        : static_cast<QWidget*>( mProjectsEmptyLabel ) );
                watchProjectFolders();
                loadProjectLabels();
        }


        ///
        /// (Re)load the labels of the selected project
        ///
        void StartupView::loadProjectLabels()
        {
                QString project = currentProject();
                bool hasProject = !project.isEmpty();

                mNewLabelInProjectButton->setEnabled( hasProject );
                mProjectFolderButton->setEnabled( hasProject );
                mProjectLabelList->clear();

                if ( !hasProject )
                {
                        mProjectTitle->setText( tr("Labels") );
                        mProjectLabelsEmptyLabel->setText( tr("Select or create a project to see its labels.") );
                        mProjectLabelsStack->setCurrentWidget( mProjectLabelsEmptyLabel );
                        return;
                }

                QSettings().setValue( "Projects/current", project );
                mProjectTitle->setText( project );

                QLocale locale;
                for ( const auto& label : Projects::labels( project ) )
                {
                        QString details = label.modified.toString( locale.dateTimeFormat( QLocale::ShortFormat ) );
                        if ( !label.product.isEmpty() )
                        {
                                details = label.product + "  ·  " + details;
                        }
                        auto* item = new QListWidgetItem( QIcon::fromTheme( "glabels" ), label.name + "\n" + details );
                        item->setData( Qt::UserRole, label.filePath );
                        item->setToolTip( QDir::toNativeSeparators( label.filePath ) );
                        mProjectLabelList->addItem( item );
                }

                if ( mProjectLabelList->count() == 0 )
                {
                        mProjectLabelsEmptyLabel->setText(
                                tr("\"%1\" has no labels yet.\n\nClick \"New label...\" to add one.").arg( project ) );
                        mProjectLabelsStack->setCurrentWidget( mProjectLabelsEmptyLabel );
                }
                else
                {
                        mProjectLabelsStack->setCurrentWidget( mProjectLabelList );
                }
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
                mWatcher->addPaths( dirs );
        }


        void StartupView::onProjectsChangedOnDisk()
        {
                loadProjects();
        }


        void StartupView::onProjectSelectionChanged()
        {
                loadProjectLabels();
        }


        void StartupView::onCreateProject()
        {
                QString name = Projects::askNewProject( this );
                if ( !name.isEmpty() )
                {
                        loadProjects( name );
                }
        }


        void StartupView::onNewLabelInProject()
        {
                QString project = currentProject();
                if ( !project.isEmpty() )
                {
                        File::newLabelInProject( mWindow, project );
                        loadProjects( project );
                }
        }


        void StartupView::onOpenProjectFolder()
        {
                QString project = currentProject();
                if ( !project.isEmpty() )
                {
                        QDesktopServices::openUrl( QUrl::fromLocalFile( Projects::path( project ) ) );
                }
        }


        void StartupView::onProjectLabelActivated( QListWidgetItem* item )
        {
                if ( item )
                {
                        File::open( item->data( Qt::UserRole ).toString(), mWindow );
                }
        }


        ///
        /// Project context menu
        ///
        void StartupView::onProjectContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mProjectList->itemAt( pos );
                if ( !item )
                {
                        return;
                }
                QString project = item->data( Qt::UserRole ).toString();
                loadProjects( project );

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
                menu.exec( mProjectList->viewport()->mapToGlobal( pos ) );
        }


        ///
        /// Label context menu
        ///
        void StartupView::onProjectLabelContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mProjectLabelList->itemAt( pos );
                if ( !item )
                {
                        return;
                }
                QString filePath = item->data( Qt::UserRole ).toString();

                QMenu menu;
                menu.addAction( QIcon::fromTheme( "glabels-file-open" ), tr("Open"), this, [this, filePath]{
                        File::open( filePath, mWindow );
                } );
                menu.addAction( tr("Rename..."), this, [this, filePath]{ renameLabel( filePath ); } );
                menu.addAction( QIcon::fromTheme( "glabels-copy" ), tr("Duplicate"), this, [this, filePath]{ duplicateLabel( filePath ); } );
                menu.addAction( tr("Show in folder"), this, [filePath]{
                        QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( filePath ).absolutePath() ) );
                } );
                menu.addSeparator();
                menu.addAction( QIcon::fromTheme( "glabels-delete" ), tr("Move to Trash...\tDel"),
                                this, [this, filePath]{ trashLabel( filePath ); } );
                menu.addAction( tr("Delete Permanently...\tShift+Del"),
                                this, [this, filePath]{ deleteLabel( filePath ); } );
                menu.exec( mProjectLabelList->viewport()->mapToGlobal( pos ) );
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
                loadProjects( currentProject() );
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
                loadProjects( currentProject() );
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
                loadProjects( currentProject() );
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
                loadProjects( currentProject() );
        }


} // namespace glabels
