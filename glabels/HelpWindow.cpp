//  HelpWindow.cpp
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

#include "HelpWindow.hpp"

#include "ShortcutsDialog.hpp"

#include "model/Version.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>


namespace glabels
{

        namespace
        {
                const QString HELP_ROOT = QStringLiteral( "qrc:/help/" );
                const QString HOME_TOPIC = QStringLiteral( "index" );


                ///
                /// Text browser that builds help pages on the fly (theme styles,
                /// live shortcut table, previous/next links) instead of loading raw files.
                ///
                class HelpBrowser : public QTextBrowser
                {
                public:
                        std::function<QString(const QString&)> pageProvider;

                        QVariant loadResource( int type, const QUrl& url ) override
                        {
                                if ( type == QTextDocument::HtmlResource && pageProvider &&
                                     url.scheme() == "qrc" && url.path().startsWith( "/help/" ) )
                                {
                                        return pageProvider( QFileInfo( url.path() ).completeBaseName() );
                                }
                                return QTextBrowser::loadResource( type, url );
                        }
                };


                ///
                /// Line edit that forwards Up/Down/Enter to a result list
                ///
                class SearchEdit : public QLineEdit
                {
                public:
                        QListWidget* results{ nullptr };
                        std::function<void()> onActivate;

                protected:
                        void keyPressEvent( QKeyEvent* event ) override
                        {
                                if ( results && results->isVisible() && results->count() > 0 )
                                {
                                        int row = results->currentRow();
                                        switch ( event->key() )
                                        {
                                        case Qt::Key_Down:
                                                results->setCurrentRow( qMin( row + 1, results->count() - 1 ) );
                                                return;
                                        case Qt::Key_Up:
                                                results->setCurrentRow( qMax( row - 1, 0 ) );
                                                return;
                                        case Qt::Key_Return:
                                        case Qt::Key_Enter:
                                                if ( onActivate ) onActivate();
                                                return;
                                        default:
                                                break;
                                        }
                                }
                                if ( event->key() == Qt::Key_Escape && !text().isEmpty() )
                                {
                                        clear();
                                        return;
                                }
                                QLineEdit::keyPressEvent( event );
                        }
                };


                ///
                /// Page shorthand: [[Ctrl+G]] key caps, <tip>...</tip> and <note>...</note> callouts
                ///
                QString expandMacros( QString body )
                {
                        static const QRegularExpression keyRe( "\\[\\[([^\\]]+)\\]\\]" );
                        body.replace( keyRe, "<span class='key'>&nbsp;\\1&nbsp;</span>" );
                        body.replace( "<tip>", "<table class='tip' width='100%' cellspacing='0'><tr><td><b>" +
                                      HelpWindow::tr("Tip:") + "</b> " );
                        body.replace( "</tip>", "</td></tr></table>" );
                        body.replace( "<note>", "<table class='warn' width='100%' cellspacing='0'><tr><td><b>" +
                                      HelpWindow::tr("Note:") + "</b> " );
                        body.replace( "</note>", "</td></tr></table>" );
                        return body;
                }


                QString mix( const QColor& a, const QColor& b, double t )
                {
                        return QColor::fromRgbF( a.redF()   * (1-t) + b.redF()   * t,
                                                 a.greenF() * (1-t) + b.greenF() * t,
                                                 a.blueF()  * (1-t) + b.blueF()  * t ).name();
                }
        }


        QPointer<HelpWindow>     HelpWindow::sInstance;
        QList<QPointer<QAction>> HelpWindow::sShortcutActions;


        ///
        /// Show the help window (creating it once) at a topic
        ///
        void HelpWindow::showTopic( const QString& topic )
        {
                bool created = false;
                if ( !sInstance )
                {
                        sInstance = new HelpWindow();
                        created = true;
                }

                if ( created || !topic.isEmpty() )
                {
                        sInstance->openTopic( topic.isEmpty() ? HOME_TOPIC : topic );
                }

                if ( created )
                {
                        sInstance->resize( 1280, 860 );   // size if the window manager can't maximize
                        sInstance->showMaximized();
                }
                else
                {
                        if ( sInstance->isMinimized() )
                        {
                                sInstance->showNormal();
                        }
                        sInstance->show();
                }
                sInstance->raise();
                sInstance->activateWindow();
        }


        void HelpWindow::setShortcutActions( const QList<QAction*>& actions )
        {
                sShortcutActions.clear();
                for ( auto* action : actions )
                {
                        sShortcutActions.append( action );
                }
        }


        ///
        /// Constructor
        ///
        HelpWindow::HelpWindow()
                : QMainWindow( nullptr )
        {
                setAttribute( Qt::WA_DeleteOnClose );
                setWindowTitle( tr("gLabels Plus Help") );
                setWindowIcon( QApplication::windowIcon() );

                QSettings settings;
                mZoom = settings.value( "Help/zoom", 0 ).toInt();

                buildContents();
                buildUi();
                applyStyle();
        }


        ///
        /// Table of contents: page file names, titles and groups (in reading order)
        ///
        void HelpWindow::buildContents()
        {
                const QString start    = tr("Getting started");
                const QString design   = tr("Designing labels");
                const QString data     = tr("Data and variables");
                const QString output   = tr("Printing");
                const QString organize = tr("Organizing and settings");
                const QString ref      = tr("Reference");

                mTopics = {
                        { "index",       tr("Welcome"),                       start },
                        { "quickstart",  tr("Your first label in 5 minutes"), start },
                        { "window",      tr("The main window"),               start },

                        { "products",    tr("Products, sheets and rolls"),    design },
                        { "objects",     tr("Adding and arranging objects"),  design },
                        { "guides",      tr("Smart guides, rulers and measuring"), design },
                        { "layers",      tr("Objects panel, layers and groups"), design },
                        { "text",        tr("Text"),                          design },
                        { "barcodes",    tr("Barcodes and QR codes"),         design },
                        { "images",      tr("Images, boxes, lines and ellipses"), design },
                        { "properties",  tr("Object properties reference"),   design },
                        { "templates",   tr("Designing your own product"),    design },

                        { "merge",       tr("Mail merge from CSV"),           data },
                        { "variables",   tr("Variables and counters"),        data },
                        { "fields",      tr("Substitution field syntax"),     data },

                        { "printing",    tr("Printing labels"),               output },
                        { "batch",       tr("Command-line printing"),         output },

                        { "projects",    tr("Projects and files"),            organize },
                        { "preferences", tr("Preferences and appearance"),    organize },

                        { "shortcuts",   tr("Keyboard shortcuts"),            ref },
                        { "faq",         tr("Troubleshooting and FAQ"),       ref },
                        { "glossary",    tr("Glossary"),                      ref },
                };
        }


        ///
        /// Widgets
        ///
        void HelpWindow::buildUi()
        {
                // Navigation pane: search box over contents tree / search results
                auto* searchEdit = new SearchEdit();
                mSearchEdit = searchEdit;
                mSearchEdit->setPlaceholderText( tr("Search the guide (Ctrl+K)") );
                mSearchEdit->setClearButtonEnabled( true );

                mToc = new QTreeWidget();
                mToc->setHeaderHidden( true );
                mToc->setRootIsDecorated( false );
                mToc->setIndentation( 14 );
                mToc->setFrameShape( QFrame::NoFrame );
                mToc->setUniformRowHeights( true );

                QTreeWidgetItem* groupItem = nullptr;
                QString currentGroup;
                for ( const auto& topic : mTopics )
                {
                        if ( topic.group != currentGroup || !groupItem )
                        {
                                currentGroup = topic.group;
                                groupItem = new QTreeWidgetItem( mToc, { currentGroup } );
                                QFont f = groupItem->font( 0 );
                                f.setBold( true );
                                groupItem->setFont( 0, f );
                                groupItem->setFlags( Qt::ItemIsEnabled );
                                groupItem->setExpanded( true );
                        }
                        auto* item = new QTreeWidgetItem( groupItem, { topic.title } );
                        item->setData( 0, Qt::UserRole, topic.name );
                        item->setToolTip( 0, topic.title );
                        mTocItems.insert( topic.name, item );
                }
                mToc->expandAll();
                connect( mToc, &QTreeWidget::itemClicked, this, &HelpWindow::onTocItemClicked );
                connect( mToc, &QTreeWidget::itemActivated, this, &HelpWindow::onTocItemClicked );

                mResults = new QListWidget();
                mResults->setFrameShape( QFrame::NoFrame );
                mResults->setWordWrap( true );
                mResults->setSpacing( 2 );
                searchEdit->results = mResults;
                auto openResult = [this]( QListWidgetItem* item ) {
                        if ( !item ) return;
                        QString name = item->data( Qt::UserRole ).toString();
                        if ( name.isEmpty() ) return;
                        openTopic( name );
                        // Highlight the search words on the page
                        mFindEdit->setText( mSearchEdit->text().simplified() );
                        mFindBar->show();
                        onFindChanged();
                };
                connect( mResults, &QListWidget::itemClicked, this, openResult );
                connect( mResults, &QListWidget::itemActivated, this, openResult );
                searchEdit->onActivate = [this, openResult]() {
                        openResult( mResults->currentItem() ? mResults->currentItem() : mResults->item( 0 ) );
                };

                mNavStack = new QStackedWidget();
                mNavStack->addWidget( mToc );
                mNavStack->addWidget( mResults );

                connect( mSearchEdit, &QLineEdit::textChanged, this, &HelpWindow::onSearchChanged );

                auto* navPane = new QWidget();
                auto* navLayout = new QVBoxLayout( navPane );
                navLayout->setContentsMargins( 8, 8, 0, 8 );
                navLayout->setSpacing( 6 );
                navLayout->addWidget( mSearchEdit );
                navLayout->addWidget( mNavStack, 1 );

                // Page view
                auto* browser = new HelpBrowser();
                browser->pageProvider = [this]( const QString& name ) { return pageHtml( name ); };
                mBrowser = browser;
                mBrowser->setOpenLinks( false );
                mBrowser->setFrameShape( QFrame::NoFrame );
                mBrowser->document()->setDocumentMargin( 28 );
                connect( mBrowser, &QTextBrowser::sourceChanged, this, &HelpWindow::onSourceChanged );
                connect( mBrowser, &QTextBrowser::anchorClicked, this, &HelpWindow::onAnchorClicked );
                connect( mBrowser, &QTextBrowser::highlighted, this, [this]( const QUrl& url ) {
                        statusBar()->showMessage( url.scheme().startsWith( "http" ) || url.scheme() == "mailto"
                                                  ? url.toString() : QString() );
                } );

                // Find bar
                mFindBar = new QWidget();
                auto* findLayout = new QHBoxLayout( mFindBar );
                findLayout->setContentsMargins( 8, 4, 8, 4 );
                mFindEdit = new QLineEdit();
                mFindEdit->setPlaceholderText( tr("Find on this page") );
                mFindEdit->setClearButtonEnabled( true );
                mFindEdit->setMaximumWidth( 320 );
                auto* prevButton = new QToolButton();
                prevButton->setText( tr("Previous") );
                prevButton->setToolTip( tr("Previous match (Shift+F3)") );
                auto* nextButton = new QToolButton();
                nextButton->setText( tr("Next") );
                nextButton->setToolTip( tr("Next match (F3)") );
                mFindStatus = new QLabel();
                mFindStatus->setStyleSheet( "color: palette(placeholder-text);" );
                auto* closeFind = new QToolButton();
                closeFind->setIcon( style()->standardIcon( QStyle::SP_DialogCloseButton ) );
                closeFind->setAutoRaise( true );
                closeFind->setToolTip( tr("Close (Esc)") );
                findLayout->addWidget( new QLabel( tr("Find:") ) );
                findLayout->addWidget( mFindEdit );
                findLayout->addWidget( prevButton );
                findLayout->addWidget( nextButton );
                findLayout->addWidget( mFindStatus );
                findLayout->addStretch( 1 );
                findLayout->addWidget( closeFind );
                mFindBar->hide();
                connect( mFindEdit, &QLineEdit::textChanged, this, &HelpWindow::onFindChanged );
                connect( mFindEdit, &QLineEdit::returnPressed, this, [this]() {
                        findNext( QApplication::keyboardModifiers() & Qt::ShiftModifier );
                } );
                connect( prevButton, &QToolButton::clicked, this, [this]() { findNext( true ); } );
                connect( nextButton, &QToolButton::clicked, this, [this]() { findNext( false ); } );
                connect( closeFind, &QToolButton::clicked, this, &HelpWindow::hideFindBar );

                auto* pagePane = new QWidget();
                auto* pageLayout = new QVBoxLayout( pagePane );
                pageLayout->setContentsMargins( 0, 0, 0, 0 );
                pageLayout->setSpacing( 0 );
                pageLayout->addWidget( mBrowser, 1 );
                pageLayout->addWidget( mFindBar );

                auto* splitter = new QSplitter();
                splitter->addWidget( navPane );
                splitter->addWidget( pagePane );
                splitter->setStretchFactor( 0, 0 );
                splitter->setStretchFactor( 1, 1 );
                splitter->setSizes( { 300, 1000 } );
                splitter->setChildrenCollapsible( false );
                navPane->setMinimumWidth( 200 );
                setCentralWidget( splitter );

                // Toolbar
                auto* toolbar = addToolBar( tr("Navigation") );
                toolbar->setObjectName( "helpToolbar" );
                toolbar->setMovable( false );
                toolbar->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );

                mBackAction = toolbar->addAction( style()->standardIcon( QStyle::SP_ArrowBack ), tr("Back") );
                mBackAction->setShortcuts( { QKeySequence::Back, QKeySequence( Qt::Key_Backspace ) } );
                mBackAction->setToolTip( tr("Back (Alt+Left)") );
                mBackAction->setEnabled( false );
                connect( mBackAction, &QAction::triggered, mBrowser, &QTextBrowser::backward );
                connect( mBrowser, &QTextBrowser::backwardAvailable, mBackAction, &QAction::setEnabled );

                mForwardAction = toolbar->addAction( style()->standardIcon( QStyle::SP_ArrowForward ), tr("Forward") );
                mForwardAction->setShortcut( QKeySequence::Forward );
                mForwardAction->setToolTip( tr("Forward (Alt+Right)") );
                mForwardAction->setEnabled( false );
                connect( mForwardAction, &QAction::triggered, mBrowser, &QTextBrowser::forward );
                connect( mBrowser, &QTextBrowser::forwardAvailable, mForwardAction, &QAction::setEnabled );

                auto* homeAction = toolbar->addAction( style()->standardIcon( QStyle::SP_DirHomeIcon ), tr("Contents") );
                homeAction->setShortcut( QKeySequence( Qt::ALT | Qt::Key_Home ) );
                homeAction->setToolTip( tr("Start page (Alt+Home)") );
                connect( homeAction, &QAction::triggered, this, [this]() { openTopic( HOME_TOPIC ); } );

                toolbar->addSeparator();

                auto* zoomOut = toolbar->addAction( tr("A-") );
                zoomOut->setShortcut( QKeySequence::ZoomOut );
                zoomOut->setToolTip( tr("Smaller text (Ctrl+-)") );
                connect( zoomOut, &QAction::triggered, this, [this]() { zoom( -1 ); } );
                auto* zoomIn = toolbar->addAction( tr("A+") );
                zoomIn->setShortcuts( { QKeySequence::ZoomIn, QKeySequence( Qt::CTRL | Qt::Key_Equal ) } );
                zoomIn->setToolTip( tr("Larger text (Ctrl++)") );
                connect( zoomIn, &QAction::triggered, this, [this]() { zoom( +1 ); } );
                auto* zoomReset = new QAction( this );
                zoomReset->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_0 ) );
                connect( zoomReset, &QAction::triggered, this, [this]() { zoom( -mZoom ); } );
                addAction( zoomReset );

                toolbar->addSeparator();

                auto* findAction = toolbar->addAction( tr("Find") );
                findAction->setShortcut( QKeySequence::Find );
                findAction->setToolTip( tr("Find on this page (Ctrl+F)") );
                connect( findAction, &QAction::triggered, this, &HelpWindow::showFindBar );

                auto* printAction = toolbar->addAction( tr("Print...") );
                printAction->setShortcut( QKeySequence::Print );
                printAction->setToolTip( tr("Print this page (Ctrl+P)") );
                connect( printAction, &QAction::triggered, this, &HelpWindow::printPage );

                auto* spacer = new QWidget();
                spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
                toolbar->addWidget( spacer );

                auto* websiteAction = toolbar->addAction( tr("Website") );
                websiteAction->setToolTip( "https://usmansaif.github.io/gLabels-plus" );
                connect( websiteAction, &QAction::triggered, this, []() {
                        QDesktopServices::openUrl( QUrl( "https://usmansaif.github.io/gLabels-plus" ) );
                } );

                // Extra keyboard shortcuts
                auto* focusSearch = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_K ), this );
                connect( focusSearch, &QShortcut::activated, this, [this]() {
                        mSearchEdit->setFocus();
                        mSearchEdit->selectAll();
                } );
                auto* focusSearch2 = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_L ), this );
                connect( focusSearch2, &QShortcut::activated, focusSearch, &QShortcut::activated );
                auto* nextMatch = new QShortcut( QKeySequence::FindNext, this );
                connect( nextMatch, &QShortcut::activated, this, [this]() { showFindBar(); findNext( false ); } );
                auto* prevMatch = new QShortcut( QKeySequence::FindPrevious, this );
                connect( prevMatch, &QShortcut::activated, this, [this]() { showFindBar(); findNext( true ); } );
                auto* escape = new QShortcut( QKeySequence( Qt::Key_Escape ), this );
                connect( escape, &QShortcut::activated, this, [this]() {
                        if ( mFindBar->isVisible() ) hideFindBar();
                        else if ( !mSearchEdit->text().isEmpty() ) mSearchEdit->clear();
                } );
                auto* closeWindow = new QShortcut( QKeySequence::Close, this );
                connect( closeWindow, &QShortcut::activated, this, &QWidget::close );
                auto* refresh = new QShortcut( QKeySequence::HelpContents, this );
                connect( refresh, &QShortcut::activated, this, [this]() { openTopic( HOME_TOPIC ); } );

                statusBar()->setSizeGripEnabled( true );
        }


        ///
        /// Theme-aware styling (rebuilt when the palette changes)
        ///
        void HelpWindow::applyStyle()
        {
                QFont f = QApplication::font();
                f.setPointSizeF( qMax( 6.0, f.pointSizeF() * ( 1.0 + 0.1 * mZoom ) + 1.0 ) );
                mBrowser->document()->setDefaultFont( f );

                if ( !mBrowser->source().isEmpty() )
                {
                        mUpdatingStyle = true;
                        int scroll = mBrowser->verticalScrollBar()->value();
                        mBrowser->reload();
                        mBrowser->verticalScrollBar()->setValue( scroll );
                        mUpdatingStyle = false;
                }
        }


        void HelpWindow::zoom( int steps )
        {
                mZoom = qBound( -4, mZoom + steps, 12 );
                QSettings settings;
                settings.setValue( "Help/zoom", mZoom );
                applyStyle();
        }


        void HelpWindow::changeEvent( QEvent* event )
        {
                QMainWindow::changeEvent( event );
                if ( ( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationFontChange )
                     && !mUpdatingStyle && mBrowser )
                {
                        applyStyle();
                }
        }


        void HelpWindow::closeEvent( QCloseEvent* event )
        {
                hideFindBar();
                QMainWindow::closeEvent( event );
        }


        ///
        /// Navigate to a topic name, optionally with "#anchor"
        ///
        void HelpWindow::openTopic( const QString& topic )
        {
                QString name = topic.section( '#', 0, 0 );
                QString anchor = topic.section( '#', 1 );
                if ( !QFile::exists( ":/help/" + name + ".html" ) )
                {
                        name = HOME_TOPIC;
                        anchor.clear();
                }

                QUrl url( HELP_ROOT + name + ".html" );
                if ( !anchor.isEmpty() )
                {
                        url.setFragment( anchor );
                }
                mBrowser->setSource( url );
        }


        ///
        /// Build a complete, styled page from a content fragment
        ///
        QString HelpWindow::pageHtml( const QString& name ) const
        {
                QFile file( ":/help/" + name + ".html" );
                if ( !file.open( QIODevice::ReadOnly ) )
                {
                        return QString( "<h1>%1</h1><p>%2</p>" )
                                .arg( tr("Page not found"), tr("This help page is missing.") );
                }
                QString body = QString::fromUtf8( file.readAll() );
                body = expandMacros( body );
                body.replace( "{{version}}", model::Version::STRING );
                if ( body.contains( "{{shortcuts}}" ) )
                {
                        body.replace( "{{shortcuts}}", shortcutsTableHtml() );
                }

                QPalette pal = palette();
                QColor text   = pal.color( QPalette::Text );
                QColor base   = pal.color( QPalette::Base );
                QColor accent = pal.color( QPalette::Highlight );
                QColor link   = pal.color( QPalette::Link );
                bool dark = base.lightnessF() < 0.5;
                if ( dark )
                {
                        link = link.lighter( 150 );
                }

                QString css = QString(
                        "body { color: %1; background-color: %2; }"
                        "h1 { font-size: x-large; font-weight: 700; margin-top: 4px; margin-bottom: 4px; }"
                        "h2 { font-size: large; font-weight: 700; color: %3; margin-top: 22px; margin-bottom: 6px; }"
                        "h3 { font-size: medium; font-weight: 700; margin-top: 14px; margin-bottom: 4px; }"
                        "p, li { margin-top: 3px; margin-bottom: 5px; }"
                        "a { color: %4; text-decoration: none; }"
                        ".lead { font-size: large; color: %5; margin-bottom: 10px; }"
                        ".muted { color: %5; }"
                        ".key { font-family: monospace; font-weight: 600; background-color: %6; color: %1; }"
                        "code { font-family: monospace; background-color: %6; }"
                        "pre { font-family: monospace; background-color: %6; padding: 8px; }"
                        "table.grid { border-collapse: collapse; border-width: 1px; border-style: solid; border-color: %7; }"
                        "table.grid th { background-color: %6; text-align: left; font-weight: 700; }"
                        "table.grid td, table.grid th { border-width: 1px; border-style: solid; border-color: %7; padding: 6px; }"
                        "table.tip td { background-color: %8; border-width: 0px; padding: 10px; }"
                        "table.warn td { background-color: %9; border-width: 0px; padding: 10px; }"
                        "table.cards td { background-color: %6; padding: 12px; }"
                        "table.nav td { padding: 4px; }"
                        "hr { color: %7; }" )
                        .arg( text.name(), base.name(), accent.name(), link.name(),
                              mix( text, base, 0.35 ),
                              mix( base, text, dark ? 0.14 : 0.07 ),
                              mix( base, text, 0.25 ),
                              mix( base, accent, dark ? 0.25 : 0.12 ),
                              mix( base, QColor( 230, 140, 0 ), dark ? 0.28 : 0.16 ) );

                // Previous / next navigation
                QString nav;
                for ( int i = 0; i < mTopics.size(); ++i )
                {
                        if ( mTopics[i].name != name ) continue;
                        QString prev = ( i > 0 )
                                ? QString( "<a href='%1.html'>%2: %3</a>" )
                                        .arg( mTopics[i-1].name, tr("Previous"), mTopics[i-1].title.toHtmlEscaped() )
                                : QString();
                        QString next = ( i + 1 < mTopics.size() )
                                ? QString( "<a href='%1.html'>%2: %3</a>" )
                                        .arg( mTopics[i+1].name, tr("Next"), mTopics[i+1].title.toHtmlEscaped() )
                                : QString();
                        nav = QString( "<hr><table class='nav' width='100%'><tr>"
                                       "<td align='left'>%1</td><td align='right'>%2</td>"
                                       "</tr></table>" ).arg( prev, next );
                        break;
                }

                return QString( "<html><head><meta charset='utf-8'><style>%1</style></head>"
                                "<body>%2%3</body></html>" ).arg( css, body, nav );
        }


        ///
        /// Live table of the current keyboard shortcuts (including customized ones)
        ///
        QString HelpWindow::shortcutsTableHtml() const
        {
                const QList<QPair<QString, QString>> groups = {
                        { "file",    tr("File and pages") },
                        { "edit",    tr("Edit") },
                        { "view",    tr("View") },
                        { "merge",   tr("Merge preview") },
                        { "objects", tr("Objects and tools") },
                        { "help",    tr("Help") },
                };

                QMap<QString, QStringList> rows;
                for ( const auto& action : sShortcutActions )
                {
                        if ( !action || action->shortcuts().isEmpty() ) continue;

                        QStringList keys;
                        for ( const auto& seq : action->shortcuts() )
                        {
                                keys << "<span class='key'>&nbsp;" +
                                        seq.toString( QKeySequence::NativeText ).toHtmlEscaped() + "&nbsp;</span>";
                        }
                        QString group = action->objectName().section( '.', 0, 0 );
                        rows[group] << QString( "<tr><td>%1</td><td>%2</td></tr>" )
                                .arg( ShortcutsDialog::cleanText( action ).toHtmlEscaped(),
                                      keys.join( QString( " %1 " ).arg( tr("or") ) ) );
                }

                if ( rows.isEmpty() )
                {
                        return QString( "<p class='muted'>%1</p>" )
                                .arg( tr("Open a label to see the full list of shortcuts here.") );
                }

                QString html;
                for ( const auto& group : groups )
                {
                        if ( !rows.contains( group.first ) ) continue;
                        html += "<h3>" + group.second.toHtmlEscaped() + "</h3>"
                                "<table class='grid' width='100%' cellspacing='0'>"
                                "<tr><th width='55%'>" + tr("Command") + "</th><th>" + tr("Shortcut") + "</th></tr>" +
                                rows[group.first].join( "" ) + "</table>";
                }
                return html;
        }


        ///
        /// Plain text of a page (for full-text search)
        ///
        QString HelpWindow::plainText( const QString& name ) const
        {
                auto it = mPlainCache.constFind( name );
                if ( it != mPlainCache.constEnd() )
                {
                        return *it;
                }

                QFile file( ":/help/" + name + ".html" );
                QString text;
                if ( file.open( QIODevice::ReadOnly ) )
                {
                        QString body = QString::fromUtf8( file.readAll() );
                        body = expandMacros( body );
                        body.replace( "{{shortcuts}}", shortcutsTableHtml() );
                        QTextDocument doc;
                        doc.setHtml( body );
                        text = doc.toPlainText();
                        // Drop the page heading so snippets start with real content
                        int nl = text.indexOf( '\n' );
                        if ( body.trimmed().startsWith( "<h1>" ) && nl >= 0 )
                        {
                                text = text.mid( nl + 1 );
                        }
                }
                mPlainCache.insert( name, text );
                return text;
        }


        void HelpWindow::selectTocItem( const QString& name )
        {
                auto* item = mTocItems.value( name, nullptr );
                if ( item )
                {
                        mToc->setCurrentItem( item );
                        mToc->scrollToItem( item );
                }
                else
                {
                        mToc->clearSelection();
                }
        }


        void HelpWindow::onSourceChanged( const QUrl& url )
        {
                QString name = QFileInfo( url.path() ).completeBaseName();
                selectTocItem( name );

                for ( const auto& topic : mTopics )
                {
                        if ( topic.name == name )
                        {
                                setWindowTitle( tr("%1 - gLabels Plus Help").arg( topic.title ) );
                                break;
                        }
                }

                if ( !mUpdatingStyle && mFindBar->isVisible() && !mFindEdit->text().isEmpty() )
                {
                        onFindChanged();
                }
        }


        void HelpWindow::onAnchorClicked( const QUrl& url )
        {
                QString scheme = url.scheme();
                if ( scheme == "http" || scheme == "https" || scheme == "mailto" )
                {
                        QDesktopServices::openUrl( url );
                        return;
                }

                QUrl target = mBrowser->source().resolved( url );
                if ( target.adjusted( QUrl::RemoveFragment ) == mBrowser->source().adjusted( QUrl::RemoveFragment ) &&
                     target.hasFragment() )
                {
                        mBrowser->scrollToAnchor( target.fragment() );
                        return;
                }
                mBrowser->setSource( target );
        }


        void HelpWindow::onTocItemClicked( QTreeWidgetItem* item )
        {
                if ( !item ) return;
                QString name = item->data( 0, Qt::UserRole ).toString();
                if ( name.isEmpty() )
                {
                        item->setExpanded( !item->isExpanded() );
                        return;
                }
                if ( QFileInfo( mBrowser->source().path() ).completeBaseName() != name )
                {
                        openTopic( name );
                }
        }


        ///
        /// Full-text search over every page; results ranked by relevance
        ///
        void HelpWindow::onSearchChanged( const QString& text )
        {
                QStringList words = text.simplified().toLower().split( ' ', Qt::SkipEmptyParts );
                if ( words.isEmpty() )
                {
                        mNavStack->setCurrentWidget( mToc );
                        return;
                }

                struct Hit { int score; const Topic* topic; QString snippet; };
                QList<Hit> hits;

                for ( const auto& topic : mTopics )
                {
                        QString plain = plainText( topic.name );
                        QString lower = plain.toLower();
                        QString title = topic.title.toLower();

                        int score = 0;
                        bool all = true;
                        for ( const auto& word : words )
                        {
                                int count = lower.count( word );
                                if ( count == 0 && !title.contains( word ) )
                                {
                                        all = false;
                                        break;
                                }
                                score += qMin( count, 20 ) + ( title.contains( word ) ? 50 : 0 );
                        }
                        if ( !all ) continue;

                        // Snippet around the first match
                        int pos = lower.indexOf( words.first() );
                        QString snippet;
                        if ( pos >= 0 )
                        {
                                int start = qMax( 0, pos - 40 );
                                snippet = plain.mid( start, 120 ).simplified();
                                if ( start > 0 ) snippet.prepend( "..." );
                                if ( start + 120 < plain.size() ) snippet.append( "..." );
                        }
                        hits.append( { score, &topic, snippet } );
                }

                std::stable_sort( hits.begin(), hits.end(), []( const Hit& a, const Hit& b ) {
                        return a.score > b.score;
                } );

                mResults->clear();
                if ( hits.isEmpty() )
                {
                        auto* item = new QListWidgetItem( tr("No pages match \"%1\".").arg( text.simplified() ) );
                        item->setFlags( Qt::NoItemFlags );
                        mResults->addItem( item );
                }
                for ( const auto& hit : hits )
                {
                        auto* item = new QListWidgetItem( hit.topic->title +
                                                          ( hit.snippet.isEmpty() ? QString() : "\n" + hit.snippet ) );
                        item->setData( Qt::UserRole, hit.topic->name );
                        item->setToolTip( hit.topic->group + " / " + hit.topic->title );
                        mResults->addItem( item );
                }
                if ( !hits.isEmpty() )
                {
                        mResults->setCurrentRow( 0 );
                }
                mNavStack->setCurrentWidget( mResults );
        }


        ///
        /// Find on page: highlight all matches and jump to the first one
        ///
        void HelpWindow::onFindChanged()
        {
                QString needle = mFindEdit->text();
                QList<QTextEdit::ExtraSelection> selections;

                if ( !needle.isEmpty() )
                {
                        QColor hl = palette().color( QPalette::Highlight );
                        hl.setAlpha( 90 );
                        QTextCursor cursor( mBrowser->document() );
                        while ( true )
                        {
                                cursor = mBrowser->document()->find( needle, cursor );
                                if ( cursor.isNull() ) break;
                                QTextEdit::ExtraSelection sel;
                                sel.cursor = cursor;
                                sel.format.setBackground( hl );
                                selections.append( sel );
                        }
                }
                mBrowser->setExtraSelections( selections );

                if ( needle.isEmpty() )
                {
                        mFindStatus->clear();
                        mFindEdit->setStyleSheet( QString() );
                }
                else if ( selections.isEmpty() )
                {
                        mFindStatus->setText( tr("Not found") );
                }
                else
                {
                        mFindStatus->setText( selections.size() == 1 ? tr("1 match")
                                                                     : tr("%1 matches").arg( selections.size() ) );
                        QTextCursor start( mBrowser->document() );
                        mBrowser->setTextCursor( start );
                        findNext( false );
                }
        }


        void HelpWindow::findNext( bool backward )
        {
                QString needle = mFindEdit->text();
                if ( needle.isEmpty() ) return;

                QTextDocument::FindFlags flags;
                if ( backward ) flags |= QTextDocument::FindBackward;

                if ( !mBrowser->find( needle, flags ) )
                {
                        // Wrap around
                        QTextCursor cursor( mBrowser->document() );
                        if ( backward ) cursor.movePosition( QTextCursor::End );
                        mBrowser->setTextCursor( cursor );
                        mBrowser->find( needle, flags );
                }
        }


        void HelpWindow::showFindBar()
        {
                mFindBar->show();
                mFindEdit->setFocus();
                mFindEdit->selectAll();
                if ( !mFindEdit->text().isEmpty() ) onFindChanged();
        }


        void HelpWindow::hideFindBar()
        {
                mFindBar->hide();
                mBrowser->setExtraSelections( {} );
                mBrowser->setFocus();
        }


        void HelpWindow::printPage()
        {
                QPrinter printer( QPrinter::HighResolution );
                QPrintDialog dialog( &printer, this );
                dialog.setWindowTitle( tr("Print Help Page") );
                if ( dialog.exec() == QDialog::Accepted )
                {
                        mBrowser->document()->print( &printer );
                }
        }

}
