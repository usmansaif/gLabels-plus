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

#include "model/Settings.hpp"
#include "model/Version.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>


namespace
{
        const int   cardIconSize   = 32;
        const int   contentMaxWidth = 880;
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
                                               tr("New project..."),
                                               tr("Pick a label or card product and start designing") );
                mOpenButton = createActionCard( "glabels-file-open",
                                                tr("Open project..."),
                                                tr("Browse for an existing gLabels Plus project") );
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
                auto* recentTitle = new QLabel( tr("Recent projects") );
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

                mRecentEmptyLabel = new QLabel( tr("No recent projects yet.\n\n"
                                                   "Projects you open or save will appear here.") );
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
                } );
                columnLayout->addWidget( footer );

                //
                // Contribution banner
                //
                auto* banner = new QLabel();
                mBannerLabel = banner;
                banner->setObjectName( "startupBanner" );
                banner->setAlignment( Qt::AlignCenter );
                banner->setWordWrap( true );
                banner->setTextInteractionFlags( Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard );
                banner->setToolTip( "https://usmansaif.com" );
                connect( banner, &QLabel::linkActivated, this, []( const QString& link ) {
                        QDesktopServices::openUrl( QUrl( link ) );
                } );
                columnLayout->addWidget( banner );

                updateStyle();
                loadRecentList();

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
                                 "<a %4 href='help'>%3</a>" )
                        .arg( tr("Preferences"), tr("Keyboard shortcuts"), tr("Help"), link ) );
                mBannerLabel->setText(
                        QString( "%1 &nbsp;<b>Usman Saif</b> &nbsp;·&nbsp; "
                                 "<a %2 href='https://usmansaif.com'>usmansaif.com</a>" )
                        .arg( tr("Enhanced with new features and a refreshed interface by"), link ) );

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
                        "#startupRecentList {"
                        "  border: 1px solid palette(mid); border-radius: 8px; padding: 4px;"
                        "  background: palette(window); }"
                        "#startupRecentList::item { padding: 6px 4px; border-radius: 6px; }"
                        "#startupRecentList::item:hover { background: palette(alternate-base); }"
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


} // namespace glabels
