//  MainWindow.cpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
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


#include "MainWindow.hpp"
#include "SymbolsDialog.hpp"

#include "File.hpp"
#include "Help.hpp"
#include "LabelEditor.hpp"
#include "MergeView.hpp"
#include "ObjectEditor.hpp"
#include "ObjectListView.hpp"
#include "PreferencesDialog.hpp"
#include "PrintEstimatorWindow.hpp"
#include "PrintView.hpp"
#include "Projects.hpp"
#include "PropertiesView.hpp"
#include "Ruler.hpp"
#include "SelectProductDialog.hpp"
#include "ShortcutsDialog.hpp"
#include "Samples.hpp"
#include "StartupView.hpp"
#include "VariablesView.hpp"
#include "VariablesPanel.hpp"
#include "ConnectDataSourceDialog.hpp"
#include "RecentView.hpp"
#include "merge/DataSourceUtil.hpp"
#include "WhatsNewDialog.hpp"
#include "HelpWindow.hpp"

#include "model/Db.hpp"
#include "model/Frame.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelTextObject.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QClipboard>
#include <QDebug>
#include <QStyle>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>

#include <algorithm>


namespace
{
        enum PageIndex
        {
                WELCOME_PAGE_INDEX    = 0,
                EDITOR_PAGE_INDEX     = 1,
                PROPERTIES_PAGE_INDEX = 2,
                MERGE_PAGE_INDEX      = 3,
                VARIABLES_PAGE_INDEX  = 4,
                PRINT_PAGE_INDEX      = 5,
                RECENT_PAGE_INDEX     = 6,
        };
}


namespace
{
        using namespace glabels;

        ///
        /// Exactly one text object and one circle/ellipse, box or line selected?
        ///
        bool findTextAndShape( model::Model*            model,
                                           model::ModelTextObject*& text,
                                           model::ModelObject*&     shape )
        {
                text  = nullptr;
                shape = nullptr;
                const auto selection = model->getSelection();
                if ( selection.size() != 2 )
                {
                        return false;
                }
                for ( auto* object : selection )
                {
                        if ( auto* t = dynamic_cast<model::ModelTextObject*>( object ) )
                        {
                                text = t;
                        }
                        else if ( dynamic_cast<model::ModelEllipseObject*>( object ) ||
                                  dynamic_cast<model::ModelBoxObject*>( object ) ||
                                  dynamic_cast<model::ModelLineObject*>( object ) )
                        {
                                shape = object;
                        }
                }
                return text && shape;
        }


        bool textAndShapeSelected( model::Model* model )
        {
                model::ModelTextObject* text;
                model::ModelObject*     shape;
                return findTextAndShape( model, text, shape );
        }
}


namespace glabels
{

        ///
        /// Constructor
        ///
        MainWindow::MainWindow()
        {
                setWindowIcon( QApplication::windowIcon() );

                setDockOptions( QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks );

                createActions();
                createMenus();
                createToolBars();
                createStatusBar();

                // Build pages
                QWidget* welcomePage = createWelcomePage();
                QWidget* editorPage = createEditorPage();
                QWidget* propertiesPage = createPropertiesPage();
                QWidget* mergePage = createMergePage();
                QWidget* variablesPage = createVariablesPage();
                QWidget* printPage = createPrintPage();

                // A QStackedWidget's minimum size is the largest minimum of *all* its pages.
                // Wrap the form-style pages in scroll areas so that they can never force the
                // main window to be larger than the screen (which also stops the window manager
                // from offering maximize/resize).
                auto scrollable = []( QWidget* content ) -> QWidget* {
                        auto* area = new QScrollArea();
                        area->setFrameShape( QFrame::NoFrame );
                        area->setWidgetResizable( true );
                        area->setWidget( content );
                        return area;
                };
                propertiesPage = scrollable( propertiesPage );
                mergePage      = scrollable( mergePage );
                variablesPage  = scrollable( variablesPage );
                printPage      = scrollable( printPage );

                // Table of contents widget
                mContents = new QToolBar();
                mContents->setObjectName( "sidebar" );
                mContents->setWindowTitle( tr("Pages") );
                mContents->setOrientation( Qt::Horizontal );
                mContents->setMovable( false );
                mContents->setFloatable( false );
                mContents->setAllowedAreas( Qt::BottomToolBarArea );
                mContents->setAutoFillBackground( true );
                mContents->setBackgroundRole( QPalette::AlternateBase );
                mContents->setContextMenuPolicy( Qt::CustomContextMenu );
                mContents->setToolTip( tr("Switch page. Right-click to change button style.") );
                connect( mContents, &QWidget::customContextMenuRequested, this, &MainWindow::onSidebarContextMenu );

                // Table of contents button group
                auto group = new QButtonGroup( this );
                group->setExclusive( true );

                // Pages widget
                mPages = new QStackedWidget();

                // Add "Welcome" page
                mPages->addWidget( welcomePage );
                mWelcomeButton = new QToolButton( this );
                mWelcomeButton->setIcon( QIcon::fromTheme( "glabels" ) );
                mWelcomeButton->setText( tr("Welcome") );
                mWelcomeButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mWelcomeButton->setCheckable( true );
                mWelcomeAction = mContents->addWidget( mWelcomeButton );
                group->addButton( mWelcomeButton );

                // Add "Editor" page
                mPages->addWidget( editorPage );
                mEditorButton = new QToolButton( this );
                mEditorButton->setIcon( QIcon::fromTheme( "glabels-edit" ) );
                mEditorButton->setText( tr("Edit") );
                mEditorButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mEditorButton->setCheckable( true );
                mEditorAction = mContents->addWidget( mEditorButton );
                group->addButton( mEditorButton );

                // Add "Properties" page
                mPages->addWidget( propertiesPage );
                mPropertiesButton = new QToolButton( this );
                mPropertiesButton->setIcon( QIcon::fromTheme( "glabels-properties" ) );
                mPropertiesButton->setText( tr("Properties") );
                mPropertiesButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mPropertiesButton->setCheckable( true );
                mPropertiesAction = mContents->addWidget( mPropertiesButton );
                group->addButton( mPropertiesButton );
                mPropertiesAction->setVisible( false );   // product details now live on the Print page

                // Add "Data Source" page
                mPages->addWidget( mergePage );
                mMergeButton = new QToolButton( this );
                mMergeButton->setIcon( QIcon::fromTheme( "glabels-merge" ) );
                mMergeButton->setText( tr("Data Source") );
                mMergeButton->setToolTip( tr("Data source: connect a CSV, TSV or JSON file and choose records") );
                mMergeButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mMergeButton->setCheckable( true );
                mMergeAction = mContents->addWidget( mMergeButton );
                group->addButton( mMergeButton );

                // Add "Variables" page
                mPages->addWidget( variablesPage );
                mVariablesButton = new QToolButton( this );
                mVariablesButton->setIcon( QIcon::fromTheme( "glabels-variables" ) );
                mVariablesButton->setText( tr("Custom Variables") );
                mVariablesButton->setToolTip( tr("Custom variables: counters and constants") );
                mVariablesButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mVariablesButton->setCheckable( true );
                mVariablesAction = mContents->addWidget( mVariablesButton );
                group->addButton( mVariablesButton );

                // Add "Print" page
                mPages->addWidget( printPage );
                mPrintButton = new QToolButton( this );
                mPrintButton->setIcon( QIcon::fromTheme( "glabels-print" ) );
                mPrintButton->setText( tr("Print") );
                mPrintButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mPrintButton->setCheckable( true );
                mPrintAction = mContents->addWidget( mPrintButton );
                group->addButton( mPrintButton );

                // Add "Recent" page (only offered while no label is open)
                mRecentView = new RecentView( this );
                connect( mRecentView, &RecentView::homeRequested, this, &MainWindow::showWelcomePage );
                mPages->addWidget( mRecentView );
                mRecentButton = new QToolButton( this );
                mRecentButton->setIcon( QIcon::fromTheme( "glabels-file-recent" ) );
                mRecentButton->setText( tr("Recent") );
                mRecentButton->setToolTip( tr("Recently used labels") );
                mRecentButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
                mRecentButton->setCheckable( true );
                mRecentAction = mContents->addWidget( mRecentButton );
                group->addButton( mRecentButton );

                // Set initial page selection
                mWelcomeButton->setChecked( true );
                mPages->setCurrentIndex( WELCOME_PAGE_INDEX );

                // Page switcher lives at the bottom-left of the window, above the status bar
                addToolBar( Qt::BottomToolBarArea, mContents );
                setCentralWidget( mPages );

                applySidebarStyle();

                createDocks();
                registerShortcuts();

                manageActions();
                setTitle();

                // Connect
                connect( mWelcomeButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mEditorButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mPropertiesButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mMergeButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mVariablesButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mPrintButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mRecentButton, SIGNAL(toggled(bool)), this, SLOT(changePage(bool)));
                connect( mLabelEditor, SIGNAL(zoomChanged()), this, SLOT(onZoomChanged()) );
                connect( mLabelEditor, SIGNAL(previewRecordChanged()), this, SLOT(onPreviewRecordChanged()) );
                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                connect( QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardChanged()) );
                connect( mLabelEditor, &LabelEditor::pointerMoved, this, &MainWindow::onPointerMoved );
                connect( mLabelEditor, &LabelEditor::pointerExited, this, &MainWindow::onPointerExit );

                readSettings();

                // Always open maximized (applies to every window, including new ones).
                // Size the window to the screen first so that even a window manager that ignores
                // the maximize hint shows a full-size window.
                if ( auto* scr = QGuiApplication::primaryScreen() )
                {
                        setGeometry( scr->availableGeometry() );
                }
                setWindowState( windowState() | Qt::WindowMaximized );
        }


        ///
        /// Re-assert the maximized state once the window is mapped (some X11 window managers
        /// drop a maximize request that arrives before the window is managed, e.g. behind a splash)
        ///
        void MainWindow::showEvent( QShowEvent* event )
        {
                QMainWindow::showEvent( event );
                if ( !mMaximizeChecked )
                {
                        mMaximizeChecked = true;
                        for ( int delay : { 0, 250, 750 } )
                        {
                                QTimer::singleShot( delay, this, [this]() {
                                        if ( !isMaximized() && !isFullScreen() && !isMinimized() )
                                        {
                                                showMaximized();
                                        }
                                } );
                        }
                }
        }


        ///
        /// Get model accessor
        ///
        model::Model* MainWindow::model() const
        {
                return mModel.get();
        }


        ///
        /// Set model accessor
        ///
        void MainWindow::setModel( model::Model* model )
        {
                mModel.reset( model );
                mUndoRedoModel = std::make_unique<UndoRedoModel>( mModel.get() );

                mModel->setAlignReference( model::Model::AlignReference( model::Settings::alignReference() ) );

                mPropertiesView->setModel( mModel.get(), mUndoRedoModel.get() );
                mLabelEditor->setModel( mModel.get(), mUndoRedoModel.get() );
                mObjectEditor->setModel( mModel.get(), mUndoRedoModel.get() );
                mObjectListView->setModel( mModel.get(), mUndoRedoModel.get() );
                mMergeView->setModel( mModel.get(), mUndoRedoModel.get() );
                mVariablesPanel->setModel( mModel.get(), mUndoRedoModel.get() );
                mVariablesView->setModel( mModel.get(), mUndoRedoModel.get() );
                mPrintView->setModel( mModel.get() );

                mEditorButton->setChecked( true );
                mPages->setCurrentIndex( EDITOR_PAGE_INDEX );

                manageActions();
                setTitle();

                connect( mLabelEditor, SIGNAL(contextMenuActivate(model::Point)), this, SLOT(onContextMenuActivate(model::Point)) );
                connect( mModel.get(), SIGNAL(nameChanged()), this, SLOT(onNameChanged()) );
                connect( mModel.get(), SIGNAL(modifiedChanged()), this, SLOT(onModifiedChanged()) );
                connect( mModel.get(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()) );
                connect( mModel.get(), SIGNAL(changed()), this, SLOT(onLabelChanged()) );
                connect( mUndoRedoModel.get(), SIGNAL(changed()), this, SLOT(onUndoRedoChanged()) );
                connect( mModel.get(), SIGNAL(sizeChanged()), this, SLOT(updateOrientationButton()) );

                updateOrientationButton();
                onPreviewRecordChanged();
                syncRotationSpin();
                syncPageButton();
                updateChromeVisibility();
        }


        ///
        /// Is window empty?
        ///
        bool MainWindow::isEmpty() const
        {
                return !mModel;
        }


        ///
        /// Close the document shown in this window, without quitting.
        ///
        /// With other windows open, this window simply closes.  If it is the
        /// last window, a fresh window with the welcome screen replaces it so
        /// the application keeps running.
        ///
        void MainWindow::closeDocument()
        {
                bool otherWindows = false;
                for ( QWidget* widget : QApplication::topLevelWidgets() )
                {
                        auto* window = qobject_cast<MainWindow*>( widget );
                        if ( window && window != this && window->isVisible() )
                        {
                                otherWindows = true;
                                break;
                        }
                }

                if ( otherWindows )
                {
                        close();   // asks to save if needed
                        return;
                }

                if ( isEmpty() )
                {
                        return;    // already just the welcome screen
                }

                if ( !isOkToClose() )
                {
                        return;    // user cancelled the save prompt
                }

                // The label editor was closed: show the recent labels in its place
                auto* welcome = new MainWindow();
                welcome->showRecentPage();
                welcome->show();

                mCloseConfirmed = true;
                setAttribute( Qt::WA_DeleteOnClose );   // free the closed document
                close();
        }


        ///
        /// Close Event Handler
        ///
        void MainWindow::closeEvent( QCloseEvent *event )
        {
                if ( mCloseConfirmed || isOkToClose() )
                {
                        writeSettings();
                        event->accept();
                }
                else
                {
                        event->ignore();
                }
        }


        ///
        /// Create Actions
        ///
        void MainWindow::createActions()
        {
                /* File actions */
                fileNewAction = new QAction( tr("&New..."), this );
                fileNewAction->setIcon( QIcon::fromTheme( "glabels-file-new" ) );
                fileNewAction->setShortcut( QKeySequence::New );
                fileNewAction->setStatusTip( tr("Create a new gLabels Plus label") );
                connect( fileNewAction, SIGNAL(triggered()), this, SLOT(fileNew()) );

                fileNewProjectAction = new QAction( tr("New &Project..."), this );
                fileNewProjectAction->setIcon( QIcon::fromTheme( "folder-new", QIcon::fromTheme( "glabels-file-new" ) ) );
                fileNewProjectAction->setStatusTip( tr("Create a project to keep a business or client's labels together") );
                connect( fileNewProjectAction, &QAction::triggered, this, &MainWindow::fileNewProject );

                fileNewFromSampleAction = new QAction( tr("New from Sa&mple..."), this );
                fileNewFromSampleAction->setIcon( QIcon::fromTheme( "glabels-file-new" ) );
                fileNewFromSampleAction->setStatusTip( tr("Start from a ready-made shipping label, receipt, invoice, "
                                                          "SKU, product or asset label") );
                connect( fileNewFromSampleAction, &QAction::triggered, this, &MainWindow::fileNewFromSample );

                fileOpenAction = new QAction( tr("&Open..."), this );
                fileOpenAction->setIcon( QIcon::fromTheme( "glabels-file-open" ) );
                fileOpenAction->setShortcut( QKeySequence::Open );
                fileOpenAction->setStatusTip( tr("Open an existing gLabels Plus label") );
                connect( fileOpenAction, SIGNAL(triggered()), this, SLOT(fileOpen()) );

                for ( int i = 0; i < std::min( model::Settings::maxRecentFiles(), 10 ); i++ )   // menu shows up to 10
                {
                        auto* action = new QAction( this );
                        action->setIcon( QIcon::fromTheme( "glabels" ) );
                        action->setVisible( false );
                        fileRecentActionList.append( action );
                        connect( action, SIGNAL(triggered()), this, SLOT(fileOpenRecent()) );
                }

                fileSaveAction = new QAction( tr("&Save"), this );
                fileSaveAction->setIcon( QIcon::fromTheme( "glabels-file-save" ) );
                fileSaveAction->setShortcut( QKeySequence::Save );
                fileSaveAction->setStatusTip( tr("Save current gLabels Plus label") );
                connect( fileSaveAction, SIGNAL(triggered()), this, SLOT(fileSave()) );

                fileSaveAsAction = new QAction( tr("Save &As..."), this );
                fileSaveAsAction->setIcon( QIcon::fromTheme( "glabels-file-save-as" ) );
                fileSaveAsAction->setShortcut( QKeySequence::SaveAs );
                fileSaveAsAction->setStatusTip( tr("Save current gLabels Plus label to a different name") );
                connect( fileSaveAsAction, SIGNAL(triggered()), this, SLOT(fileSaveAs()) );

                fileSaveAsTemplateAction = new QAction( tr("Save as &Template..."), this );
                fileSaveAsTemplateAction->setStatusTip( tr("Save a copy of this label to My templates in the sample library") );
                connect( fileSaveAsTemplateAction, &QAction::triggered, this, [this]{ Samples::saveAsTemplate( this ); } );

                fileShowEditorPageAction = new QAction( tr("&Edit") , this );
                fileShowEditorPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_1 ) );
                fileShowEditorPageAction->setStatusTip( tr("Select label Edit mode") );
                connect( fileShowEditorPageAction, SIGNAL(triggered()), this, SLOT(fileShowEditorPage()) );
                addAction( fileShowEditorPageAction );   // not in a menu: keep Ctrl+1 working

                fileShowPropertiesPageAction = new QAction( tr("P&roperties") , this );
                fileShowPropertiesPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_2 ) );
                fileShowPropertiesPageAction->setStatusTip( tr("Select label Properties mode") );
                connect( fileShowPropertiesPageAction, SIGNAL(triggered()), this, SLOT(fileShowPropertiesPage()) );

                fileShowMergePageAction = new QAction( tr("&Data Source") , this );
                fileShowMergePageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_3 ) );
                fileShowMergePageAction->setStatusTip( tr("Show the data source and its records") );
                connect( fileShowMergePageAction, SIGNAL(triggered()), this, SLOT(fileShowMergePage()) );

                fileShowVariablesPageAction = new QAction( tr("&Custom Variables") , this );
                fileShowVariablesPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_4 ) );
                fileShowVariablesPageAction->setStatusTip( tr("Edit custom variables (counters and constants)") );
                connect( fileShowVariablesPageAction, SIGNAL(triggered()), this, SLOT(fileShowVariablesPage()) );

                fileShowPrintPageAction = new QAction( tr("&Print") , this );
                fileShowPrintPageAction->setShortcut( QKeySequence::Print );
                fileShowPrintPageAction->setStatusTip( tr("Select label Print mode") );
                connect( fileShowPrintPageAction, SIGNAL(triggered()), this, SLOT(fileShowPrintPage()) );

                fileTemplateDesignerAction = new QAction( tr("Product Template &Designer..."), this );
                fileTemplateDesignerAction->setStatusTip( tr("Create custom templates") );
                connect( fileTemplateDesignerAction, SIGNAL(triggered()), this, SLOT(fileTemplateDesigner()) );

                fileCloseAction = new QAction( tr("&Close"), this );
                fileCloseAction->setIcon( QIcon::fromTheme( "window-close" ) );
                fileCloseAction->setShortcut( QKeySequence::Close );
                fileCloseAction->setStatusTip( tr("Close the current label (the application stays open)") );
                connect( fileCloseAction, SIGNAL(triggered()), this, SLOT(fileClose()) );

                fileExitAction = new QAction( tr("E&xit"), this );
                fileExitAction->setIcon( QIcon::fromTheme( "application-exit" ) );
                fileExitAction->setShortcut( QKeySequence::Quit );
                fileExitAction->setStatusTip( tr("Exit glabels") );
                connect( fileExitAction, SIGNAL(triggered()), this, SLOT(fileExit()) );


                /* Edit actions */
                editUndoAction = new QAction( tr("Undo"), this );
                editUndoAction->setIcon( QIcon::fromTheme( "edit-undo", QIcon::fromTheme( "glabels-edit-undo" ) ) );
                editUndoAction->setShortcut( QKeySequence( tr("Ctrl+Z") ) );
                editUndoAction->setStatusTip( tr("Undo") );
                connect( editUndoAction, SIGNAL(triggered()), this, SLOT(editUndo()) );

                editRedoAction = new QAction( tr("Redo"), this );
                editRedoAction->setIcon( QIcon::fromTheme( "edit-redo", QIcon::fromTheme( "glabels-edit-redo" ) ) );
                editRedoAction->setShortcuts( { QKeySequence( tr("Ctrl+Y") ), QKeySequence( tr("Ctrl+Shift+Z") ) } );
                editRedoAction->setStatusTip( tr("Redo") );
                connect( editRedoAction, SIGNAL(triggered()), this, SLOT(editRedo()) );

                editCutAction = new QAction( tr("Cut"), this );
                editCutAction->setIcon( QIcon::fromTheme( "glabels-edit-cut" ) );
                editCutAction->setShortcut( QKeySequence( tr("Ctrl+X") ) );   // not QKeySequence::Cut: it includes Shift+Del
                editCutAction->setStatusTip( tr("Cut the selection") );
                connect( editCutAction, SIGNAL(triggered()), this, SLOT(editCut()) );

                editCopyAction = new QAction( tr("&Copy"), this );
                editCopyAction->setIcon( QIcon::fromTheme( "glabels-edit-copy" ) );
                editCopyAction->setShortcut( QKeySequence( tr("Ctrl+C") ) );
                editCopyAction->setStatusTip( tr("Copy the selection") );
                connect( editCopyAction, SIGNAL(triggered()), this, SLOT(editCopy()) );

                editPasteAction = new QAction( tr("&Paste"), this );
                editPasteAction->setIcon( QIcon::fromTheme( "glabels-edit-paste" ) );
                editPasteAction->setShortcut( QKeySequence( tr("Ctrl+V") ) );
                editPasteAction->setStatusTip( tr("Paste the clipboard") );
                connect( editPasteAction, SIGNAL(triggered()), this, SLOT(editPaste()) );

                editDeleteAction = new QAction( tr("&Delete"), this );
                editDeleteAction->setIcon( QIcon::fromTheme( "edit-delete", QIcon::fromTheme( "glabels-edit-delete" ) ) );
                editDeleteAction->setShortcuts( { QKeySequence( Qt::Key_Delete ), QKeySequence( Qt::SHIFT | Qt::Key_Delete ) } );
                editDeleteAction->setStatusTip( tr("Delete the selected objects") );
                connect( editDeleteAction, SIGNAL(triggered()), this, SLOT(editDelete()) );

                // Single-key alternative for Delete (E for erase); follows the Delete action's enabled state
                editDeleteQuickAction = new QAction( tr("Delete (Quick Key)"), this );
                editDeleteQuickAction->setShortcut( QKeySequence( Qt::Key_E ) );
                editDeleteQuickAction->setStatusTip( tr("Delete the selected objects with a single key") );
                editDeleteQuickAction->setProperty( "quickKeyFor", QVariant::fromValue<QObject*>( editDeleteAction ) );
                connect( editDeleteQuickAction, &QAction::triggered, this, &MainWindow::editDelete );
                connect( editDeleteAction, &QAction::enabledChanged, editDeleteQuickAction, &QAction::setEnabled );

                editSelectAllAction = new QAction( tr("Select &All"), this );
                editSelectAllAction->setIcon( QIcon::fromTheme( "edit-select-all" ) );
                editSelectAllAction->setShortcut( QKeySequence::SelectAll );
                editSelectAllAction->setStatusTip( tr("Select all objects") );
                connect( editSelectAllAction, SIGNAL(triggered()), this, SLOT(editSelectAll()) );

                editUnSelectAllAction = new QAction( tr("Un-select All"), this );
                editUnSelectAllAction->setIcon( QIcon::fromTheme( "edit-unselect-all" ) );
                editUnSelectAllAction->setStatusTip( tr("Remove all selections") );
                connect( editUnSelectAllAction, SIGNAL(triggered()), this, SLOT(editUnSelectAll()) );

                editPageSetupAction = new QAction( tr("Page Setup..."), this );
                editPageSetupAction->setIcon( QIcon::fromTheme( "document-page-setup" ) );
                editPageSetupAction->setShortcut( QKeySequence( "Ctrl+Shift+P" ) );
                editPageSetupAction->setStatusTip( tr("Choose or change the page (product) without moving any objects") );
                connect( editPageSetupAction, &QAction::triggered, this, &MainWindow::onPageButtonClicked );

                editPreferencesAction = new QAction( tr("Preferences..."), this );
                editPreferencesAction->setIcon( QIcon::fromTheme( "edit-preferences" ) );
                editPreferencesAction->setShortcut( QKeySequence::Preferences );
                editPreferencesAction->setStatusTip( tr("Configure the application") );
                connect( editPreferencesAction, SIGNAL(triggered()), this, SLOT(editPreferences()) );

                editShortcutsAction = new QAction( tr("Keyboard Shortcuts..."), this );
                editShortcutsAction->setIcon( QIcon::fromTheme( "glabels-keyboard" ) );
                editShortcutsAction->setStatusTip( tr("View and customize keyboard shortcuts") );
                connect( editShortcutsAction, SIGNAL(triggered()), this, SLOT(editShortcuts()) );


                /* View actions */
                viewFileToolBarAction = new QAction( tr("Quick Access"), this );
                viewFileToolBarAction->setCheckable( true );
                viewFileToolBarAction->setStatusTip( tr("Change visibility of the \"Quick Access\" toolbar in current window") );
                connect( viewFileToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewFileToolBar(bool)) );

                viewEditorToolBarAction = new QAction( tr("Editor"), this );
                viewEditorToolBarAction->setCheckable( true );
                viewEditorToolBarAction->setStatusTip( tr("Change visibility of the \"Editor\" toolbar in current window") );
                connect( viewEditorToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewEditorToolBar(bool)) );


                viewViewToolBarAction = new QAction( tr("View && Zoom"), this );
                viewViewToolBarAction->setCheckable( true );
                viewViewToolBarAction->setStatusTip( tr("Change visibility of the \"View && Zoom\" toolbar") );
                connect( viewViewToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewToolBarToggled(bool)) );

                viewArrangeToolBarAction = new QAction( tr("Arrange"), this );
                viewArrangeToolBarAction->setCheckable( true );
                viewArrangeToolBarAction->setStatusTip( tr("Change visibility of the \"Arrange\" toolbar") );
                connect( viewArrangeToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewToolBarToggled(bool)) );

                viewMergeToolBarAction = new QAction( tr("Live Data Preview"), this );
                viewMergeToolBarAction->setCheckable( true );
                viewMergeToolBarAction->setStatusTip( tr("Change visibility of the \"Live Data Preview\" toolbar") );
                connect( viewMergeToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewToolBarToggled(bool)) );

                viewPropertiesDockAction = new QAction( tr("Object Properties Panel"), this );
                viewPropertiesDockAction->setCheckable( true );
                viewPropertiesDockAction->setIcon( QIcon::fromTheme( "glabels-object-properties" ) );
                viewPropertiesDockAction->setStatusTip( tr("Show or hide the object properties panel") );
                connect( viewPropertiesDockAction, SIGNAL(toggled(bool)), this, SLOT(viewDockToggled(bool)) );

                viewVariablesDockAction = new QAction( tr("Variables Panel"), this );
                viewVariablesDockAction->setCheckable( true );
                viewVariablesDockAction->setIcon( QIcon::fromTheme( "glabels-variables" ) );
                viewVariablesDockAction->setStatusTip( tr("Show or hide the data source variables panel") );
                connect( viewVariablesDockAction, SIGNAL(toggled(bool)), this, SLOT(viewDockToggled(bool)) );

                viewObjectListDockAction = new QAction( tr("Objects Panel"), this );
                viewObjectListDockAction->setCheckable( true );
                viewObjectListDockAction->setStatusTip( tr("Show or hide the list of objects (layers)") );
                connect( viewObjectListDockAction, SIGNAL(toggled(bool)), this, SLOT(viewDockToggled(bool)) );

                viewRulersAction = new QAction( tr("Rulers"), this );
                viewRulersAction->setCheckable( true );
                viewRulersAction->setIcon( QIcon::fromTheme( "glabels-rulers" ) );
                viewRulersAction->setStatusTip( tr("Show or hide horizontal and vertical rulers") );
                connect( viewRulersAction, SIGNAL(toggled(bool)), this, SLOT(viewRulers(bool)) );

                viewPropertiesUnitsAction = new QAction( tr("Units in Properties"), this );
                viewPropertiesUnitsAction->setCheckable( true );
                viewPropertiesUnitsAction->setStatusTip( tr("Show or hide measurement units next to values in the properties panel") );
                connect( viewPropertiesUnitsAction, SIGNAL(toggled(bool)), this, SLOT(viewPropertiesUnits(bool)) );

                viewGridAction = new QAction( tr("Grid"), this );
                viewGridAction->setIcon( QIcon::fromTheme( "glabels-grid" ) );
                viewGridAction->setCheckable( true );
                viewGridAction->setStatusTip( tr("Change visibility of the grid in current window") );
                connect( viewGridAction, SIGNAL(toggled(bool)), this, SLOT(viewGrid(bool)) );

                viewMarkupAction = new QAction( tr("Markup"), this );
                viewMarkupAction->setIcon( QIcon::fromTheme( "glabels-markup" ) );
                viewMarkupAction->setCheckable( true );
                viewMarkupAction->setStatusTip( tr("Change visibility of markup lines in current window") );
                connect( viewMarkupAction, SIGNAL(toggled(bool)), this, SLOT(viewMarkup(bool)) );

                viewZoomInAction = new QAction( tr("Zoom &In"), this );
                viewZoomInAction->setIcon( QIcon::fromTheme( "glabels-zoom-in" ) );
                viewZoomInAction->setShortcut( QKeySequence::ZoomIn );
                viewZoomInAction->setStatusTip( tr("Increase magnification") );
                connect( viewZoomInAction, SIGNAL(triggered()), this, SLOT(viewZoomIn()) );

                viewZoomOutAction = new QAction( tr("Zoom &Out"), this );
                viewZoomOutAction->setIcon( QIcon::fromTheme( "glabels-zoom-out" ) );
                viewZoomOutAction->setShortcut( QKeySequence::ZoomOut );
                viewZoomOutAction->setStatusTip( tr("Decrease magnification") );
                connect( viewZoomOutAction, SIGNAL(triggered()), this, SLOT(viewZoomOut()) );

                viewZoom1To1Action = new QAction( tr("Zoom &1 to 1"), this );
                viewZoom1To1Action->setIcon( QIcon::fromTheme( "glabels-zoom-one-to-one" ) );
                viewZoom1To1Action->setStatusTip( tr("Restore scale to 100%") );
                connect( viewZoom1To1Action, SIGNAL(triggered()), this, SLOT(viewZoom1To1()) );

                viewZoomToFitAction = new QAction( tr("Zoom to &Fit"), this );
                viewZoomToFitAction->setIcon( QIcon::fromTheme( "glabels-zoom-to-fit" ) );
                viewZoomToFitAction->setStatusTip( tr("Set scale to fit window") );
                connect( viewZoomToFitAction, SIGNAL(triggered()), this, SLOT(viewZoomToFit()) );


                /* Object actions */
                objectsArrowModeAction = new QAction( tr("Select Mode"), this );
                objectsArrowModeAction->setIcon( QIcon::fromTheme( "glabels-arrow" ) );
                objectsArrowModeAction->setShortcut( QKeySequence( Qt::Key_V ) );
                objectsArrowModeAction->setStatusTip( tr("Select, move and modify objects") );
                connect( objectsArrowModeAction, SIGNAL(triggered()), this, SLOT(objectsArrowMode()) );

                objectsMeasureModeAction = new QAction( tr("Measure"), this );
                objectsMeasureModeAction->setIcon( QIcon::fromTheme( "glabels-measure" ) );
                objectsMeasureModeAction->setShortcut( QKeySequence( Qt::Key_M ) );
                objectsMeasureModeAction->setStatusTip( tr("Measure distances on the label: drag between any two points "
                                                           "(Shift = horizontal/vertical, Esc = clear)") );
                connect( objectsMeasureModeAction, SIGNAL(triggered()), this, SLOT(objectsMeasureMode()) );

                objectsCreateTextAction = new QAction( tr("Text"), this );
                objectsCreateTextAction->setIcon( QIcon::fromTheme( "glabels-text" ) );
                objectsCreateTextAction->setShortcut( QKeySequence( Qt::Key_T ) );
                objectsCreateTextAction->setStatusTip( tr("Create text object") );
                connect( objectsCreateTextAction, SIGNAL(triggered()), this, SLOT(objectsCreateText()) );

                objectsCreateBoxAction = new QAction( tr("Box"), this );
                objectsCreateBoxAction->setIcon( QIcon::fromTheme( "glabels-box" ) );
                objectsCreateBoxAction->setShortcut( QKeySequence( Qt::Key_R ) );
                objectsCreateBoxAction->setStatusTip( tr("Create box object") );
                connect( objectsCreateBoxAction, SIGNAL(triggered()), this, SLOT(objectsCreateBox()) );

                objectsCreateLineAction = new QAction( tr("Line"), this );
                objectsCreateLineAction->setIcon( QIcon::fromTheme( "glabels-line" ) );
                objectsCreateLineAction->setShortcut( QKeySequence( Qt::Key_L ) );
                objectsCreateLineAction->setStatusTip( tr("Create line object") );
                connect( objectsCreateLineAction, SIGNAL(triggered()), this, SLOT(objectsCreateLine()) );

                objectsCreateEllipseAction = new QAction( tr("Ellipse"), this );
                objectsCreateEllipseAction->setIcon( QIcon::fromTheme( "glabels-ellipse" ) );
                objectsCreateEllipseAction->setShortcut( QKeySequence( Qt::Key_O ) );
                objectsCreateEllipseAction->setStatusTip( tr("Create ellipse/circle object") );
                connect( objectsCreateEllipseAction, SIGNAL(triggered()), this, SLOT(objectsCreateEllipse()) );

                objectsCreateImageAction = new QAction( tr("Image"), this );
                objectsCreateImageAction->setIcon( QIcon::fromTheme( "glabels-image" ) );
                objectsCreateImageAction->setShortcut( QKeySequence( Qt::Key_I ) );
                objectsCreateImageAction->setStatusTip( tr("Create image object") );
                connect( objectsCreateImageAction, SIGNAL(triggered()), this, SLOT(objectsCreateImage()) );

                objectsCreateBarcodeAction = new QAction( tr("Barcode"), this );
                objectsCreateBarcodeAction->setIcon( QIcon::fromTheme( "glabels-barcode" ) );
                objectsCreateBarcodeAction->setShortcut( QKeySequence( Qt::Key_B ) );
                objectsCreateBarcodeAction->setStatusTip( tr("Create barcode object") );
                connect( objectsCreateBarcodeAction, SIGNAL(triggered()), this, SLOT(objectsCreateBarcode()) );

                objectsOrderRaiseAction = new QAction( tr("Bring To Front"), this );
                objectsOrderRaiseAction->setIcon( QIcon::fromTheme( "glabels-order-top" ) );
                objectsOrderRaiseAction->setStatusTip( tr("Raise selection to top") );
                connect( objectsOrderRaiseAction, SIGNAL(triggered()), this, SLOT(objectsOrderRaise()) );

                objectsOrderLowerAction = new QAction( tr("Send To Back"), this );
                objectsOrderLowerAction->setIcon( QIcon::fromTheme( "glabels-order-bottom" ) );
                objectsOrderLowerAction->setStatusTip( tr("Lower selection to bottom") );
                connect( objectsOrderLowerAction, SIGNAL(triggered()), this, SLOT(objectsOrderLower()) );

                objectsOrderRaiseOneAction = new QAction( tr("Bring Forward"), this );
                objectsOrderRaiseOneAction->setIcon( QIcon::fromTheme( "glabels-order-raise" ) );
                objectsOrderRaiseOneAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_BracketRight ) );
                objectsOrderRaiseOneAction->setStatusTip( tr("Raise selection one step") );
                connect( objectsOrderRaiseOneAction, SIGNAL(triggered()), this, SLOT(objectsOrderRaiseOne()) );

                objectsOrderLowerOneAction = new QAction( tr("Send Backward"), this );
                objectsOrderLowerOneAction->setIcon( QIcon::fromTheme( "glabels-order-lower" ) );
                objectsOrderLowerOneAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_BracketLeft ) );
                objectsOrderLowerOneAction->setStatusTip( tr("Lower selection one step") );
                connect( objectsOrderLowerOneAction, SIGNAL(triggered()), this, SLOT(objectsOrderLowerOne()) );

                objectsOrderRaiseAction->setShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight ) );
                objectsOrderLowerAction->setShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft ) );

                objectsEditTextAction = new QAction( tr("Edit Text In Place"), this );
                objectsEditTextAction->setIcon( QIcon::fromTheme( "glabels-text" ) );
                objectsEditTextAction->setShortcut( QKeySequence( Qt::Key_F2 ) );
                objectsEditTextAction->setStatusTip( tr("Edit the text or barcode data of the selected object directly on the label (or double-click it)") );
                connect( objectsEditTextAction, SIGNAL(triggered()), this, SLOT(objectsEditText()) );

                /* Alignment reference */
                alignRefGroup = new QActionGroup( this );
                alignRefGroup->setExclusive( true );

                alignRefSelectionAction = new QAction( tr("Align Relative to Selection"), this );
                alignRefSelectionAction->setCheckable( true );
                alignRefSelectionAction->setData( int(model::Model::ALIGN_TO_SELECTION) );
                alignRefSelectionAction->setStatusTip( tr("Align objects to the outer edges of the whole selection") );
                alignRefGroup->addAction( alignRefSelectionAction );

                alignRefKeyAction = new QAction( tr("Align Relative to Last Selected Object"), this );
                alignRefKeyAction->setCheckable( true );
                alignRefKeyAction->setData( int(model::Model::ALIGN_TO_KEY_OBJECT) );
                alignRefKeyAction->setStatusTip( tr("Align objects to the object that was selected last (it does not move)") );
                alignRefGroup->addAction( alignRefKeyAction );

                alignRefLabelAction = new QAction( tr("Align Relative to Label"), this );
                alignRefLabelAction->setCheckable( true );
                alignRefLabelAction->setData( int(model::Model::ALIGN_TO_LABEL) );
                alignRefLabelAction->setStatusTip( tr("Align objects to the edges or center of the label (canvas)") );
                alignRefGroup->addAction( alignRefLabelAction );

                connect( alignRefGroup, &QActionGroup::triggered, this, [this]( QAction* a ) {
                        onAlignReferenceChanged( a->data().toInt() );
                } );

                /* Merge preview */
                mergePlaceholdersAction = new QAction( tr("Show Placeholders"), this );
                mergePlaceholdersAction->setCheckable( true );
                mergePlaceholdersAction->setChecked( true );
                mergePlaceholdersAction->setStatusTip( tr("Show variables as {{Variable_Name}} placeholders on the label") );
                connect( mergePlaceholdersAction, SIGNAL(triggered()), this, SLOT(dataShowPlaceholders()) );

                mergePreviewAction = new QAction( tr("Preview Live Data"), this );
                mergePreviewAction->setIcon( QIcon::fromTheme( "glabels-merge" ) );
                mergePreviewAction->setCheckable( true );
                mergePreviewAction->setStatusTip( tr("Show real records from the data source on the label instead of placeholders") );
                connect( mergePreviewAction, SIGNAL(toggled(bool)), this, SLOT(mergePreviewToggled(bool)) );

                dataConnectAction = new QAction( tr("&Connect Data Source..."), this );
                dataConnectAction->setIcon( QIcon::fromTheme( "glabels-merge" ) );
                dataConnectAction->setShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_D ) );
                dataConnectAction->setStatusTip( tr("Import a CSV, TSV or JSON file and set up its variables") );
                connect( dataConnectAction, SIGNAL(triggered()), this, SLOT(dataConnectSource()) );

                dataReloadAction = new QAction( tr("&Reload Data"), this );
                dataReloadAction->setStatusTip( tr("Read the data source file again") );
                connect( dataReloadAction, SIGNAL(triggered()), this, SLOT(dataReload()) );

                mergeFirstAction = new QAction( tr("First Record"), this );
                mergeFirstAction->setIcon( QIcon::fromTheme( "glabels-go-first" ) );
                connect( mergeFirstAction, SIGNAL(triggered()), this, SLOT(mergePreviewFirst()) );

                mergePreviousAction = new QAction( tr("< Previous"), this );
                mergePreviousAction->setToolTip( tr("Previous Record") );
                mergePreviousAction->setIcon( QIcon::fromTheme( "glabels-go-previous" ) );
                mergePreviousAction->setShortcut( QKeySequence( Qt::ALT | Qt::Key_PageUp ) );
                connect( mergePreviousAction, SIGNAL(triggered()), this, SLOT(mergePreviewPrevious()) );

                mergeNextAction = new QAction( tr("Next >"), this );
                mergeNextAction->setToolTip( tr("Next Record") );
                mergeNextAction->setIcon( QIcon::fromTheme( "glabels-go-next" ) );
                mergeNextAction->setShortcut( QKeySequence( Qt::ALT | Qt::Key_PageDown ) );
                connect( mergeNextAction, SIGNAL(triggered()), this, SLOT(mergePreviewNext()) );

                mergeLastAction = new QAction( tr("Last Record"), this );
                mergeLastAction->setIcon( QIcon::fromTheme( "glabels-go-last" ) );
                connect( mergeLastAction, SIGNAL(triggered()), this, SLOT(mergePreviewLast()) );

                objectsXformRotateLeftAction = new QAction( tr("Rotate Left"), this );
                objectsXformRotateLeftAction->setIcon( QIcon::fromTheme( "glabels-rotate-left" ) );
                objectsXformRotateLeftAction->setStatusTip( tr("Rotate object(s) 90 degrees counter-clockwise") );
                connect( objectsXformRotateLeftAction, SIGNAL(triggered()), this, SLOT(objectsXformRotateLeft()) );

                objectsXformRotateRightAction = new QAction( tr("Rotate Right"), this );
                objectsXformRotateRightAction->setIcon( QIcon::fromTheme( "glabels-rotate-right" ) );
                objectsXformRotateRightAction->setStatusTip( tr("Rotate object(s) 90 degrees clockwise") );
                connect( objectsXformRotateRightAction, SIGNAL(triggered()), this, SLOT(objectsXformRotateRight()) );

                objectsXformFlipHorizAction = new QAction( tr("Flip Horizontally"), this );
                objectsXformFlipHorizAction->setIcon( QIcon::fromTheme( "glabels-flip-horiz" ) );
                objectsXformFlipHorizAction->setStatusTip( tr("Flip object(s) horizontally") );
                connect( objectsXformFlipHorizAction, SIGNAL(triggered()), this, SLOT(objectsXformFlipHoriz()) );

                objectsXformFlipVertAction = new QAction( tr("Flip Vertically"), this );
                objectsXformFlipVertAction->setIcon( QIcon::fromTheme( "glabels-flip-vert" ) );
                objectsXformFlipVertAction->setStatusTip( tr("Flip object(s) vertically") );
                connect( objectsXformFlipVertAction, SIGNAL(triggered()), this, SLOT(objectsXformFlipVert()) );

                objectsAlignLeftAction = new QAction( tr("Align Left"), this );
                objectsAlignLeftAction->setIcon( QIcon::fromTheme( "glabels-align-left" ) );
                objectsAlignLeftAction->setStatusTip( tr("Align objects to left edges") );
                connect( objectsAlignLeftAction, SIGNAL(triggered()), this, SLOT(objectsAlignLeft()) );

                objectsAlignHCenterAction = new QAction( tr("Align Center"), this );
                objectsAlignHCenterAction->setIcon( QIcon::fromTheme( "glabels-align-hcenter" ) );
                objectsAlignHCenterAction->setStatusTip( tr("Align objects to horizontal centers") );
                connect( objectsAlignHCenterAction, SIGNAL(triggered()), this, SLOT(objectsAlignHCenter()) );

                objectsAlignRightAction = new QAction( tr("Align Right"), this );
                objectsAlignRightAction->setIcon( QIcon::fromTheme( "glabels-align-right" ) );
                objectsAlignRightAction->setStatusTip( tr("Align objects to right edges") );
                connect( objectsAlignRightAction, SIGNAL(triggered()), this, SLOT(objectsAlignRight()) );

                objectsAlignTopAction = new QAction( tr("Align Top"), this );
                objectsAlignTopAction->setIcon( QIcon::fromTheme( "glabels-align-top" ) );
                objectsAlignTopAction->setStatusTip( tr("Align objects to top edges") );
                connect( objectsAlignTopAction, SIGNAL(triggered()), this, SLOT(objectsAlignTop()) );

                objectsAlignVCenterAction = new QAction( tr("Align Middle"), this );
                objectsAlignVCenterAction->setIcon( QIcon::fromTheme( "glabels-align-vcenter" ) );
                objectsAlignVCenterAction->setStatusTip( tr("Align objects to vertical centers") );
                connect( objectsAlignVCenterAction, SIGNAL(triggered()), this, SLOT(objectsAlignVCenter()) );

                objectsAlignBottomAction = new QAction( tr("Align Bottom"), this );
                objectsAlignBottomAction->setIcon( QIcon::fromTheme( "glabels-align-bottom" ) );
                objectsAlignBottomAction->setStatusTip( tr("Align objects to bottom edges") );
                connect( objectsAlignBottomAction, SIGNAL(triggered()), this, SLOT(objectsAlignBottom()) );

                objectsCenterHorizAction = new QAction( tr("Center Horizontally"), this );
                objectsCenterHorizAction->setIcon( QIcon::fromTheme( "glabels-center-horiz" ) );
                objectsCenterHorizAction->setStatusTip( tr("Horizontally center objects in label") );
                connect( objectsCenterHorizAction, SIGNAL(triggered()), this, SLOT(objectsCenterHoriz()) );

                objectsCenterVertAction = new QAction( tr("Center Vertically"), this );
                objectsCenterVertAction->setIcon( QIcon::fromTheme( "glabels-center-vert" ) );
                objectsCenterVertAction->setStatusTip( tr("Vertically center objects in label") );
                connect( objectsCenterVertAction, SIGNAL(triggered()), this, SLOT(objectsCenterVert()) );

                objectsCenterAction = new QAction( tr("Center Both"), this );
                objectsCenterAction->setIcon( QIcon::fromTheme( "glabels-center" ) );
                objectsCenterAction->setStatusTip( tr("Center objects in label") );
                connect( objectsCenterAction, SIGNAL(triggered()), this, SLOT(objectsCenter()) );

                objectsPutTextOnShapeAction = new QAction( tr("Put Text on &Shape"), this );
                objectsPutTextOnShapeAction->setStatusTip( tr("Curve the selected text along the selected circle, ellipse, box or line") );
                objectsPutTextOnShapeAction->setToolTip( tr("Select one text object and one circle, ellipse, box or line, then fit the text to the shape's outline") );
                connect( objectsPutTextOnShapeAction, SIGNAL(triggered()), this, SLOT(objectsPutTextOnShape()) );


                /* Tools actions */
                toolsInsertSymbolAction = new QAction( tr("Insert &Symbol..."), this );
                toolsInsertSymbolAction->setIcon( QIcon::fromTheme( "glabels-image" ) );
                toolsInsertSymbolAction->setShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_Y ) );
                toolsInsertSymbolAction->setStatusTip( tr("Insert handling, food, contact and other symbols made for thermal label printers") );
                connect( toolsInsertSymbolAction, &QAction::triggered, this, &MainWindow::toolsInsertSymbol );

                toolsPrintEstimatorAction = new QAction( tr("Print Sheet &Estimator..."), this );
                toolsPrintEstimatorAction->setStatusTip( tr("Estimate how many printing sheets a job needs and the paper waste") );
                connect( toolsPrintEstimatorAction, &QAction::triggered, this, &MainWindow::toolsPrintEstimator );


                /* Help actions */
                helpContentsAction = new QAction( tr("&User Manual..."), this );
                helpContentsAction->setIcon( QIcon::fromTheme( "help-contents" ) );
                helpContentsAction->setShortcut( QKeySequence::HelpContents );
                helpContentsAction->setStatusTip( tr("Open gLabels Plus manual") );
                connect( helpContentsAction, SIGNAL(triggered()), this, SLOT(helpContents()) );

                helpReportBugAction = new QAction( tr("&Report Bug..."), this );
                helpReportBugAction->setStatusTip( tr("Report a bug to the developers") );
                connect( helpReportBugAction, SIGNAL(triggered()), this, SLOT(helpReportBug()) );

                helpWhatsNewAction = new QAction( tr("What's &New..."), this );
                helpWhatsNewAction->setStatusTip( tr("See the latest features in gLabels Plus") );
                connect( helpWhatsNewAction, SIGNAL(triggered()), this, SLOT(helpWhatsNew()) );

                helpAboutAction = new QAction( tr("&About..."), this );
                helpAboutAction->setIcon( QIcon::fromTheme( "help-about" ) );
                helpAboutAction->setStatusTip( tr("About gLabels Plus") );
                connect( helpAboutAction, SIGNAL(triggered()), this, SLOT(helpAbout()) );


                /* Context menu version of edit actions */
                contextCutAction = new QAction( tr("Cut"), this );
                contextCutAction->setIcon( QIcon::fromTheme( "glabels-edit-cut" ) );
                contextCutAction->setStatusTip( tr("Cut the selection") );
                connect( contextCutAction, SIGNAL(triggered()), this, SLOT(editCut()) );

                contextCopyAction = new QAction( tr("&Copy"), this );
                contextCopyAction->setIcon( QIcon::fromTheme( "glabels-edit-copy" ) );
                contextCopyAction->setStatusTip( tr("Copy the selection") );
                connect( contextCopyAction, SIGNAL(triggered()), this, SLOT(editCopy()) );

                contextPasteAction = new QAction( tr("&Paste"), this );
                contextPasteAction->setIcon( QIcon::fromTheme( "glabels-edit-paste" ) );
                contextPasteAction->setStatusTip( tr("Paste the clipboard") );
                connect( contextPasteAction, SIGNAL(triggered()), this, SLOT(editContextPaste()) );

                contextDeleteAction = new QAction( tr("&Delete"), this );
                contextDeleteAction->setIcon( QIcon::fromTheme( "edit-delete", QIcon::fromTheme( "glabels-edit-delete" ) ) );
                contextDeleteAction->setStatusTip( tr("Delete the selected objects") );
                connect( contextDeleteAction, SIGNAL(triggered()), this, SLOT(editDelete()) );
        }


        ///
        /// Create Menus
        ///
        void MainWindow::createMenus()
        {
                fileMenu = menuBar()->addMenu( tr("&File") );
                fileMenu->addAction( fileNewAction );
                fileMenu->addAction( fileNewFromSampleAction );
                fileMenu->addAction( fileNewProjectAction );
                fileMenu->addAction( fileOpenAction );
                // "Open Recent..." shows the Recent labels screen (thumbnail grid)
                fileOpenRecentAction = fileMenu->addAction( QIcon::fromTheme( "glabels-file-recent" ), tr("Open Recent...") );
                fileOpenRecentAction->setStatusTip( tr("Show recently used labels") );
                connect( fileOpenRecentAction, &QAction::triggered, this, &MainWindow::fileShowRecentScreen );
                fileMenu->addAction( fileSaveAction );
                fileMenu->addAction( fileSaveAsAction );
                fileMenu->addAction( fileSaveAsTemplateAction );
                fileMenu->addSeparator();
                fileMenu->addAction( fileShowPrintPageAction );
                fileMenu->addSeparator();
                fileMenu->addAction( fileCloseAction );
                fileMenu->addAction( fileExitAction );

                editMenu = menuBar()->addMenu( tr("&Edit") );
                editMenu->addAction( editUndoAction );
                editMenu->addAction( editRedoAction );
                editMenu->addSeparator();
                editMenu->addAction( editCutAction );
                editMenu->addAction( editCopyAction );
                editMenu->addAction( editPasteAction );
                editMenu->addAction( editDeleteAction );
                editMenu->addSeparator();
                editMenu->addAction( editSelectAllAction );
                editMenu->addAction( editUnSelectAllAction );
                editMenu->addSeparator();
                editMenu->addAction( editPageSetupAction );
                editMenu->addSeparator();
                editMenu->addAction( editPreferencesAction );

                viewMenu = menuBar()->addMenu( tr("&View") );
                viewToolBarsMenu = viewMenu->addMenu( tr("Toolbars") );
                viewToolBarsMenu->addAction( viewFileToolBarAction );
                viewToolBarsMenu->addAction( viewEditorToolBarAction );
                viewToolBarsMenu->addAction( viewViewToolBarAction );
                viewToolBarsMenu->addAction( viewArrangeToolBarAction );
                viewToolBarsMenu->addAction( viewMergeToolBarAction );
                auto* viewPanelsMenu = viewMenu->addMenu( tr("Panels") );
                viewPanelsMenu->addAction( viewPropertiesDockAction );
                viewPanelsMenu->addAction( viewObjectListDockAction );
                viewPanelsMenu->addAction( viewVariablesDockAction );
                viewMenu->addSeparator();
                viewMenu->addAction( viewGridAction );
                viewMenu->addAction( viewMarkupAction );
                viewMenu->addAction( viewRulersAction );
                viewMenu->addAction( viewPropertiesUnitsAction );
                viewMenu->addSeparator();

                viewMenu->addAction( viewZoomInAction );
                viewMenu->addAction( viewZoomOutAction );
                viewMenu->addAction( viewZoom1To1Action );
                viewMenu->addAction( viewZoomToFitAction );

                objectsMenu = menuBar()->addMenu( tr("&Objects") );
                objectsMenu->addAction( objectsArrowModeAction );
                objectsCreateMenu = objectsMenu->addMenu( tr("&Create") );
                objectsCreateMenu->addAction( objectsCreateTextAction );
                objectsCreateMenu->addAction( objectsCreateBoxAction );
                objectsCreateMenu->addAction( objectsCreateLineAction );
                objectsCreateMenu->addAction( objectsCreateEllipseAction );
                objectsCreateMenu->addAction( objectsCreateImageAction );
                objectsCreateMenu->addAction( objectsCreateBarcodeAction );
                objectsMenu->addAction( objectsEditTextAction );
                objectsMenu->addSeparator();
                objectsOrderMenu = objectsMenu->addMenu( tr("&Order") );
                objectsOrderMenu->addAction( objectsOrderRaiseAction );
                objectsOrderMenu->addAction( objectsOrderRaiseOneAction );
                objectsOrderMenu->addAction( objectsOrderLowerOneAction );
                objectsOrderMenu->addAction( objectsOrderLowerAction );
                objectsXformMenu = objectsMenu->addMenu( tr("&Rotate/Flip") );
                objectsXformMenu->addAction( objectsXformRotateLeftAction );
                objectsXformMenu->addAction( objectsXformRotateRightAction );
                objectsXformMenu->addAction( objectsXformFlipHorizAction );
                objectsXformMenu->addAction( objectsXformFlipVertAction );
                objectsAlignMenu = objectsMenu->addMenu( tr("&Alignment") );
                objectsAlignMenu->addAction( objectsAlignLeftAction );
                objectsAlignMenu->addAction( objectsAlignHCenterAction );
                objectsAlignMenu->addAction( objectsAlignRightAction );
                objectsAlignMenu->addSeparator();
                objectsAlignMenu->addAction( objectsAlignTopAction );
                objectsAlignMenu->addAction( objectsAlignVCenterAction );
                objectsAlignMenu->addAction( objectsAlignBottomAction );
                objectsAlignMenu->addSeparator();
                objectsAlignMenu->addAction( alignRefSelectionAction );
                objectsAlignMenu->addAction( alignRefKeyAction );
                objectsAlignMenu->addAction( alignRefLabelAction );
                objectsCenterMenu = objectsMenu->addMenu( tr("Center") );
                objectsCenterMenu->addAction( objectsCenterHorizAction );
                objectsCenterMenu->addAction( objectsCenterVertAction );
                objectsCenterMenu->addAction( objectsCenterAction );
                objectsMenu->addSeparator();
                objectsMenu->addAction( objectsPutTextOnShapeAction );

                dataMenu = menuBar()->addMenu( tr("&Data") );
                dataMenu->addAction( dataConnectAction );
                dataMenu->addAction( fileShowMergePageAction );
                dataMenu->addAction( viewVariablesDockAction );
                dataMenu->addAction( dataReloadAction );
                dataMenu->addSeparator();
                dataMenu->addAction( mergePlaceholdersAction );
                dataMenu->addAction( mergePreviewAction );
                dataMenu->addSeparator();
                dataMenu->addAction( mergeFirstAction );
                dataMenu->addAction( mergePreviousAction );
                dataMenu->addAction( mergeNextAction );
                dataMenu->addAction( mergeLastAction );
                dataMenu->addSeparator();
                dataMenu->addAction( fileShowVariablesPageAction );

                toolsMenu = menuBar()->addMenu( tr("&Tools") );
                toolsMenu->addAction( toolsInsertSymbolAction );
                toolsMenu->addAction( objectsMeasureModeAction );
                toolsMenu->addSeparator();
                toolsMenu->addAction( toolsPrintEstimatorAction );
                toolsMenu->addAction( fileTemplateDesignerAction );
                toolsMenu->addSeparator();
                toolsMenu->addAction( editShortcutsAction );

                helpMenu = menuBar()->addMenu( tr("&Help") );
                helpMenu->addAction( helpContentsAction );
                helpMenu->addAction( helpReportBugAction );
                helpMenu->addAction( helpWhatsNewAction );
                helpMenu->addAction( helpAboutAction );

                contextMenu = new QMenu();
                contextMenu->addAction( objectsEditTextAction );
                contextMenu->addSeparator();
                contextOrderMenu = contextMenu->addMenu( tr("&Order") );
                contextOrderMenu->addAction( objectsOrderRaiseAction );
                contextOrderMenu->addAction( objectsOrderRaiseOneAction );
                contextOrderMenu->addAction( objectsOrderLowerOneAction );
                contextOrderMenu->addAction( objectsOrderLowerAction );
                contextXformMenu = contextMenu->addMenu( tr("&Rotate/Flip") );
                contextXformMenu->addAction( objectsXformRotateLeftAction );
                contextXformMenu->addAction( objectsXformRotateRightAction );
                contextXformMenu->addAction( objectsXformFlipHorizAction );
                contextXformMenu->addAction( objectsXformFlipVertAction );
                contextAlignMenu = contextMenu->addMenu( tr("&Alignment") );
                contextAlignMenu->addAction( objectsAlignLeftAction );
                contextAlignMenu->addAction( objectsAlignHCenterAction );
                contextAlignMenu->addAction( objectsAlignRightAction );
                contextAlignMenu->addSeparator();
                contextAlignMenu->addAction( objectsAlignTopAction );
                contextAlignMenu->addAction( objectsAlignVCenterAction );
                contextAlignMenu->addAction( objectsAlignBottomAction );
                contextAlignMenu->addSeparator();
                contextAlignMenu->addAction( alignRefSelectionAction );
                contextAlignMenu->addAction( alignRefKeyAction );
                contextAlignMenu->addAction( alignRefLabelAction );
                contextCenterMenu = contextMenu->addMenu( tr("Center") );
                contextCenterMenu->addAction( objectsCenterHorizAction );
                contextCenterMenu->addAction( objectsCenterVertAction );
                contextCenterMenu->addAction( objectsCenterAction );
                contextMenu->addAction( objectsPutTextOnShapeAction );
                contextMenu->addSeparator();
                contextMenu->addAction( contextCutAction );
                contextMenu->addAction( contextCopyAction );
                contextMenu->addAction( contextPasteAction );
                contextMenu->addAction( contextDeleteAction );

                noSelectionContextMenu = new QMenu();
                noSelectionContextMenu->addAction( contextPasteAction );
        }


        ///
        /// Create Tool Bars
        ///
        void MainWindow::createToolBars()
        {
                auto style = model::Settings::toolbarButtonStyle();

                /* Quick access */
                fileToolBar = addToolBar( tr("Quick Access Toolbar") );
                fileToolBar->setObjectName( "fileToolBar" );
                fileToolBar->setToolButtonStyle( style );
                fileToolBar->addAction( fileNewAction );
                fileToolBar->addAction( fileOpenAction );
                fileToolBar->addAction( fileSaveAction );
                fileToolBar->addSeparator();
                fileToolBar->addAction( editUndoAction );
                fileToolBar->addAction( editRedoAction );

                /* Editor: tools and clipboard */
                editorToolBar = new QToolBar( tr("&Editor") );
                editorToolBar->setObjectName( "editorToolBar" );
                editorToolBar->setToolButtonStyle( style );
                editorToolBar->addAction( objectsArrowModeAction );
                editorToolBar->addAction( objectsMeasureModeAction );
                editorToolBar->addSeparator();
                editorToolBar->addAction( objectsCreateTextAction );
                editorToolBar->addAction( objectsCreateBoxAction );
                editorToolBar->addAction( objectsCreateLineAction );
                editorToolBar->addAction( objectsCreateEllipseAction );
                editorToolBar->addAction( objectsCreateImageAction );
                editorToolBar->addAction( objectsCreateBarcodeAction );
                editorToolBar->addSeparator();
                editorToolBar->addAction( editCutAction );
                editorToolBar->addAction( editCopyAction );
                editorToolBar->addAction( editPasteAction );
                editorToolBar->addAction( editDeleteAction );
                addToolBar( Qt::LeftToolBarArea, editorToolBar );

                /* View: grid/markup/rulers, zoom and units */
                viewToolBar = new QToolBar( tr("View && Zoom") );
                viewToolBar->setObjectName( "viewToolBar" );
                viewToolBar->setToolButtonStyle( style );
                viewToolBar->addAction( viewGridAction );
                viewToolBar->addAction( viewMarkupAction );
                viewToolBar->addAction( viewRulersAction );
                viewToolBar->addSeparator();
                viewToolBar->addAction( viewZoomOutAction );

                mZoomCombo = new QComboBox();
                mZoomCombo->setEditable( true );
                mZoomCombo->setInsertPolicy( QComboBox::NoInsert );
                mZoomCombo->setToolTip( tr("Zoom level (type a value and press Enter)") );
                mZoomCombo->setSizeAdjustPolicy( QComboBox::AdjustToContents );
                for ( double z : LabelEditor::standardZoomLevels() )
                {
                        mZoomCombo->addItem( QString( "%1%" ).arg( 100*z, 0, 'f', 0 ), z );
                }
                mZoomCombo->addItem( tr("Fit"), -1.0 );
                mZoomCombo->setMinimumContentsLength( 5 );
                connect( mZoomCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onZoomComboActivated );
                connect( mZoomCombo->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::onZoomComboActivated );
                viewToolBar->addWidget( mZoomCombo );

                viewToolBar->addAction( viewZoomInAction );
                viewToolBar->addAction( viewZoom1To1Action );
                viewToolBar->addAction( viewZoomToFitAction );
                viewToolBar->addSeparator();

                auto* unitsLabel = new QLabel( tr(" Units: ") );
                viewToolBar->addWidget( unitsLabel );
                mUnitsCombo = new QComboBox();
                mUnitsCombo->setToolTip( tr("Measurement units used throughout gLabels Plus") );
                mUnitsCombo->addItem( tr("Inches (in)"),      "in" );
                mUnitsCombo->addItem( tr("Millimeters (mm)"), "mm" );
                mUnitsCombo->addItem( tr("Centimeters (cm)"), "cm" );
                mUnitsCombo->addItem( tr("Points (pt)"),      "pt" );
                mUnitsCombo->addItem( tr("Picas (pc)"),       "pc" );
                connect( mUnitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onUnitsComboChanged );
                viewToolBar->addWidget( mUnitsCombo );
                syncUnitsCombo();

                viewToolBar->addWidget( new QLabel( tr("  Page: ") ) );
                mPageButton = new QToolButton();
                mPageButton->setObjectName( "pageButton" );
                mPageButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
                mPageButton->setAutoRaise( false );
                connect( mPageButton, &QToolButton::clicked, this, &MainWindow::onPageButtonClicked );
                viewToolBar->addWidget( mPageButton );
                syncPageButton();
                addToolBar( Qt::TopToolBarArea, viewToolBar );

                /* Arrange: alignment, order, rotation */
                addToolBarBreak( Qt::TopToolBarArea );
                arrangeToolBar = new QToolBar( tr("Arrange") );
                arrangeToolBar->setObjectName( "arrangeToolBar" );
                arrangeToolBar->setToolButtonStyle( style );

                arrangeToolBar->addWidget( new QLabel( tr(" Align to: ") ) );
                mAlignRefCombo = new QComboBox();
                mAlignRefCombo->setToolTip( tr("Reference used by the alignment buttons") );
                mAlignRefCombo->addItem( tr("Selection"),          int(model::Model::ALIGN_TO_SELECTION) );
                mAlignRefCombo->addItem( tr("Last Selected"),      int(model::Model::ALIGN_TO_KEY_OBJECT) );
                mAlignRefCombo->addItem( tr("Label"),              int(model::Model::ALIGN_TO_LABEL) );
                mAlignRefCombo->setItemData( 0, tr("Align to the outer edges of all selected objects"), Qt::ToolTipRole );
                mAlignRefCombo->setItemData( 1, tr("Align to the object you selected last (Ctrl+click to add objects)"), Qt::ToolTipRole );
                mAlignRefCombo->setItemData( 2, tr("Align to the edges or center of the label"), Qt::ToolTipRole );
                connect( mAlignRefCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        onAlignReferenceChanged( mAlignRefCombo->itemData( i ).toInt() );
                } );
                arrangeToolBar->addWidget( mAlignRefCombo );

                arrangeToolBar->addAction( objectsAlignLeftAction );
                arrangeToolBar->addAction( objectsAlignHCenterAction );
                arrangeToolBar->addAction( objectsAlignRightAction );
                arrangeToolBar->addAction( objectsAlignTopAction );
                arrangeToolBar->addAction( objectsAlignVCenterAction );
                arrangeToolBar->addAction( objectsAlignBottomAction );

                // Label orientation toggle: a small icon-only button (checked = vertical)
                mOrientationButton = new QToolButton( this );
                mOrientationButton->setObjectName( "orientationToggle" );
                mOrientationButton->setCheckable( true );
                mOrientationButton->setAutoRaise( true );
                mOrientationButton->setToolButtonStyle( Qt::ToolButtonIconOnly );
                mOrientationButton->setIcon( QIcon::fromTheme( "glabels-label-orientation-horiz" ) );
                mOrientationButton->setIconSize( arrangeToolBar->iconSize() );
                mOrientationButton->setAccessibleName( tr("Label orientation") );
                connect( arrangeToolBar, &QToolBar::iconSizeChanged, mOrientationButton, &QToolButton::setIconSize );
                connect( mOrientationButton, &QToolButton::clicked, this, &MainWindow::onOrientationButtonClicked );
                mOrientationAction = arrangeToolBar->addWidget( mOrientationButton );

                arrangeToolBar->addSeparator();
                arrangeToolBar->addAction( objectsCenterHorizAction );
                arrangeToolBar->addAction( objectsCenterVertAction );
                arrangeToolBar->addAction( objectsCenterAction );
                arrangeToolBar->addSeparator();
                arrangeToolBar->addAction( objectsOrderRaiseAction );
                arrangeToolBar->addAction( objectsOrderRaiseOneAction );
                arrangeToolBar->addAction( objectsOrderLowerOneAction );
                arrangeToolBar->addAction( objectsOrderLowerAction );
                arrangeToolBar->addSeparator();
                arrangeToolBar->addAction( objectsXformRotateLeftAction );
                arrangeToolBar->addAction( objectsXformRotateRightAction );
                arrangeToolBar->addAction( objectsXformFlipHorizAction );
                arrangeToolBar->addAction( objectsXformFlipVertAction );

                mRotationSpin = new QDoubleSpinBox();
                mRotationSpin->setRange( 0.0, 359.9 );
                mRotationSpin->setDecimals( 1 );
                mRotationSpin->setSingleStep( 15.0 );
                mRotationSpin->setWrapping( true );
                mRotationSpin->setSuffix( QString::fromUtf8( "\u00B0" ) );
                mRotationSpin->setKeyboardTracking( false );
                mRotationSpin->setToolTip( tr("Rotation angle of the selected object(s), about their center") );
                connect( mRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onRotationSpinChanged );
                arrangeToolBar->addWidget( mRotationSpin );
                addToolBar( Qt::TopToolBarArea, arrangeToolBar );

                /* Merge preview */
                mergeToolBar = new QToolBar( tr("Live Data Preview") );
                mergeToolBar->setObjectName( "mergeToolBar" );
                mergeToolBar->setToolButtonStyle( style );
                mergeToolBar->addAction( dataConnectAction );
                mergeToolBar->addSeparator();
                {
                        // Segmented "Show Placeholders | Preview Live Data" switch
                        auto* placeholders = new QToolButton();
                        placeholders->setDefaultAction( mergePlaceholdersAction );
                        placeholders->setToolButtonStyle( Qt::ToolButtonTextOnly );
                        placeholders->setObjectName( "segmentLeft" );
                        auto* live = new QToolButton();
                        live->setDefaultAction( mergePreviewAction );
                        live->setToolButtonStyle( Qt::ToolButtonTextOnly );
                        live->setObjectName( "segmentRight" );
                        auto* segment = new QWidget();
                        segment->setObjectName( "previewSegment" );
                        auto* segLayout = new QHBoxLayout( segment );
                        segLayout->setContentsMargins( 2, 0, 2, 0 );
                        segLayout->setSpacing( 0 );
                        segLayout->addWidget( placeholders );
                        segLayout->addWidget( live );
                        segment->setStyleSheet(
                                "#previewSegment QToolButton { padding: 3px 10px; border: 1px solid palette(mid); }"
                                "#previewSegment QToolButton#segmentLeft { border-top-left-radius: 5px; border-bottom-left-radius: 5px; }"
                                "#previewSegment QToolButton#segmentRight { border-left: none;"
                                "  border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"
                                "#previewSegment QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
                                "  border-color: palette(highlight); }"
                                "#previewSegment QToolButton:disabled { color: palette(placeholder-text); }" );
                        mergeToolBar->addWidget( segment );
                }
                mergeToolBar->addSeparator();
                mergeToolBar->addAction( mergeFirstAction );
                {
                        auto* prev = new QToolButton();
                        prev->setDefaultAction( mergePreviousAction );
                        prev->setToolButtonStyle( Qt::ToolButtonTextOnly );
                        mergeToolBar->addWidget( prev );
                }
                mMergeRecordSpin = new QSpinBox();
                mMergeRecordSpin->setToolTip( tr("Data record shown on the label") );
                mMergeRecordSpin->setPrefix( tr("Record ") );
                mMergeRecordSpin->setKeyboardTracking( false );
                connect( mMergeRecordSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onMergeRecordSpinChanged );
                mergeToolBar->addWidget( mMergeRecordSpin );
                mMergeRecordCountLabel = new QLabel();
                mergeToolBar->addWidget( mMergeRecordCountLabel );
                {
                        auto* next = new QToolButton();
                        next->setDefaultAction( mergeNextAction );
                        next->setToolButtonStyle( Qt::ToolButtonTextOnly );
                        mergeToolBar->addWidget( next );
                }
                mergeToolBar->addAction( mergeLastAction );
                addToolBar( Qt::TopToolBarArea, mergeToolBar );

                for ( auto* tb : { fileToolBar, editorToolBar, viewToolBar, arrangeToolBar, mergeToolBar } )
                {
                        // Every toolbar can be dragged (by its handle) to any edge of the window
                        tb->setMovable( true );
                        tb->setFloatable( true );
                        tb->setAllowedAreas( Qt::AllToolBarAreas );
                        tb->toggleViewAction()->setVisible( false ); // we manage visibility ourselves
                        tb->installEventFilter( this );
                }
        }


        ///
        /// Create dock panels
        ///
        void MainWindow::createDocks()
        {
                // Object properties
                auto* propertiesScroll = new QScrollArea();
                propertiesScroll->setWidgetResizable( true );
                propertiesScroll->setFrameShape( QFrame::NoFrame );
                propertiesScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                mObjectEditor->setMinimumWidth( 300 );
                mObjectEditor->setMaximumWidth( QWIDGETSIZE_MAX );
                propertiesScroll->setWidget( mObjectEditor );
                // Never let the dock get narrower than the editor, so nothing is cut off on the right
                propertiesScroll->setMinimumWidth( qMax( 300, mObjectEditor->minimumSizeHint().width() ) +
                                                   style()->pixelMetric( QStyle::PM_ScrollBarExtent ) + 4 );

                mPropertiesDock = new QDockWidget( tr("Object Properties"), this );
                mPropertiesDock->setObjectName( "propertiesDock" );
                mPropertiesDock->setWidget( propertiesScroll );
                mPropertiesDock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
                mPropertiesDock->setFeatures( QDockWidget::DockWidgetClosable |
                                              QDockWidget::DockWidgetMovable |
                                              QDockWidget::DockWidgetFloatable );
                addDockWidget( Qt::RightDockWidgetArea, mPropertiesDock );

                // Object list
                mObjectListView = new ObjectListView();
                // Toolbar at the top of the Objects pane; each action is enabled
                // according to the current selection
                mObjectListView->addToolBarActions( { objectsOrderRaiseAction, objectsOrderRaiseOneAction,
                                                      objectsOrderLowerOneAction, objectsOrderLowerAction,
                                                      nullptr,
                                                      mObjectListView->groupAction(),
                                                      mObjectListView->ungroupAction(),
                                                      nullptr,
                                                      mObjectListView->renameAction(),
                                                      mObjectListView->lockAction(),
                                                      mObjectListView->duplicateAction(),
                                                      editDeleteAction } );

                // Duplicate (Ctrl+D) goes in the Edit menu, right after Paste
                editMenu->insertAction( editDeleteAction, mObjectListView->duplicateAction() );

                // Layer groups are also reachable from the Objects menu (Ctrl+G / Ctrl+Shift+G)
                objectsMenu->addSeparator();
                objectsMenu->addAction( mObjectListView->groupAction() );
                objectsMenu->addAction( mObjectListView->ungroupAction() );

                // Right-click menu for the object list: same object actions as the
                // canvas menu (paste here is the regular paste, not paste-at-cursor)
                auto* objectListMenu = new QMenu( this );
                objectListMenu->addAction( objectsEditTextAction );
                objectListMenu->addSeparator();
                objectListMenu->addMenu( contextOrderMenu );
                objectListMenu->addMenu( contextXformMenu );
                objectListMenu->addMenu( contextAlignMenu );
                objectListMenu->addMenu( contextCenterMenu );
                objectListMenu->addSeparator();
                objectListMenu->addAction( editCutAction );
                objectListMenu->addAction( editCopyAction );
                objectListMenu->addAction( editPasteAction );
                objectListMenu->addAction( editDeleteAction );
                mObjectListView->setContextMenu( objectListMenu );

                mObjectListDock = new QDockWidget( tr("Objects"), this );
                mObjectListDock->setObjectName( "objectListDock" );
                mObjectListDock->setWidget( mObjectListView );
                mObjectListDock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
                mObjectListDock->setFeatures( QDockWidget::DockWidgetClosable |
                                              QDockWidget::DockWidgetMovable |
                                              QDockWidget::DockWidgetFloatable );
                addDockWidget( Qt::RightDockWidgetArea, mObjectListDock );
                splitDockWidget( mPropertiesDock, mObjectListDock, Qt::Vertical );
                resizeDocks( { mPropertiesDock, mObjectListDock }, { 3, 1 }, Qt::Vertical );
                resizeDocks( { mPropertiesDock }, { 400 }, Qt::Horizontal );

                // Variables (data source) panel, on the left of the canvas
                mVariablesPanel = new VariablesPanel();
                connect( mVariablesPanel, &VariablesPanel::connectRequested, this, &MainWindow::dataConnectSource );
                connect( mVariablesPanel, &VariablesPanel::addVariableRequested, this, &MainWindow::onAddVariableRequested );
                mVariablesDock = new QDockWidget( tr("Variables"), this );
                mVariablesDock->setObjectName( "variablesDock" );
                mVariablesDock->setWidget( mVariablesPanel );
                mVariablesDock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
                mVariablesDock->setFeatures( QDockWidget::DockWidgetClosable |
                                             QDockWidget::DockWidgetMovable |
                                             QDockWidget::DockWidgetFloatable );
                addDockWidget( Qt::LeftDockWidgetArea, mVariablesDock );
                resizeDocks( { mVariablesDock }, { 230 }, Qt::Horizontal );

                for ( auto* dock : { mPropertiesDock, mObjectListDock, mVariablesDock } )
                {
                        dock->toggleViewAction()->setVisible( false );
                        dock->installEventFilter( this );
                }
        }


        ///
        /// Assign ids to actions (for user-customizable shortcuts) and apply saved shortcuts
        ///
        void MainWindow::registerShortcuts()
        {
                const QList<QPair<QAction*,QString>> table = {
                        { fileNewAction, "file.new" }, { fileNewFromSampleAction, "file.newFromSample" }, { fileNewProjectAction, "file.newProject" }, { fileOpenAction, "file.open" },
                        { fileSaveAction, "file.save" }, { fileSaveAsAction, "file.saveAs" }, { fileSaveAsTemplateAction, "file.saveAsTemplate" },
                        { fileShowEditorPageAction, "file.showEditor" }, { fileShowPropertiesPageAction, "file.showProperties" },
                        { fileShowMergePageAction, "file.showMerge" }, { fileShowVariablesPageAction, "file.showVariables" },
                        { fileShowPrintPageAction, "file.showPrint" }, { fileTemplateDesignerAction, "file.templateDesigner" },
                        { fileCloseAction, "file.close" }, { fileExitAction, "file.exit" },
                        { editUndoAction, "edit.undo" }, { editRedoAction, "edit.redo" },
                        { editCutAction, "edit.cut" }, { editCopyAction, "edit.copy" },
                        { editPasteAction, "edit.paste" }, { editDeleteAction, "edit.delete" }, { mObjectListView->duplicateAction(), "edit.duplicate" }, { editDeleteQuickAction, "edit.deleteQuick" },
                        { editSelectAllAction, "edit.selectAll" }, { editUnSelectAllAction, "edit.unselectAll" },
                        { editPageSetupAction, "edit.pageSetup" }, { editShortcutsAction, "edit.shortcuts" }, { editPreferencesAction, "edit.preferences" },
                        { viewGridAction, "view.grid" }, { viewMarkupAction, "view.markup" },
                        { viewRulersAction, "view.rulers" }, { viewPropertiesUnitsAction, "view.propertiesUnits" },
                        { viewPropertiesDockAction, "view.propertiesPanel" }, { viewObjectListDockAction, "view.objectsPanel" },
                        { viewZoomInAction, "view.zoomIn" }, { viewZoomOutAction, "view.zoomOut" },
                        { viewZoom1To1Action, "view.zoom1to1" }, { viewZoomToFitAction, "view.zoomToFit" },
                        { mergePreviewAction, "merge.preview" }, { mergeFirstAction, "merge.first" },
                        { mergePreviousAction, "merge.previous" }, { mergeNextAction, "merge.next" },
                        { mergeLastAction, "merge.last" }, { mergePlaceholdersAction, "merge.placeholders" },
                        { dataConnectAction, "merge.connect" }, { dataReloadAction, "merge.reload" },
                        { viewVariablesDockAction, "view.variablesPanel" },
                        { objectsArrowModeAction, "objects.select" }, { objectsMeasureModeAction, "objects.measure" }, { objectsCreateTextAction, "objects.createText" },
                        { objectsCreateBoxAction, "objects.createBox" }, { objectsCreateLineAction, "objects.createLine" },
                        { objectsCreateEllipseAction, "objects.createEllipse" }, { objectsCreateImageAction, "objects.createImage" },
                        { objectsCreateBarcodeAction, "objects.createBarcode" }, { objectsEditTextAction, "objects.editText" },
                        { objectsOrderRaiseAction, "objects.bringToFront" }, { objectsOrderLowerAction, "objects.sendToBack" },
                        { objectsOrderRaiseOneAction, "objects.bringForward" }, { objectsOrderLowerOneAction, "objects.sendBackward" },
                        { objectsXformRotateLeftAction, "objects.rotateLeft" }, { objectsXformRotateRightAction, "objects.rotateRight" },
                        { objectsXformFlipHorizAction, "objects.flipHoriz" }, { objectsXformFlipVertAction, "objects.flipVert" },
                        { objectsAlignLeftAction, "objects.alignLeft" }, { objectsAlignHCenterAction, "objects.alignHCenter" },
                        { objectsAlignRightAction, "objects.alignRight" }, { objectsAlignTopAction, "objects.alignTop" },
                        { objectsAlignVCenterAction, "objects.alignVCenter" }, { objectsAlignBottomAction, "objects.alignBottom" },
                        { objectsCenterHorizAction, "objects.centerHoriz" }, { objectsCenterVertAction, "objects.centerVert" },
                        { objectsCenterAction, "objects.center" },
                        { objectsPutTextOnShapeAction, "objects.putTextOnShape" },
                        { toolsPrintEstimatorAction, "tools.printEstimator" },
                        { toolsInsertSymbolAction, "tools.insertSymbol" },
                        { helpContentsAction, "help.contents" }, { helpReportBugAction, "help.reportBug" },
                        { helpWhatsNewAction, "help.whatsNew" }, { helpAboutAction, "help.about" },
                };

                QList<QAction*> helpActions;
                for ( auto& entry : table )
                {
                        helpActions.append( entry.first );
                }
                HelpWindow::setShortcutActions( helpActions );

                for ( auto& entry : table )
                {
                        ShortcutsDialog::registerAction( entry.first, entry.second );
                        addAction( entry.first ); // keep shortcuts active even if toolbar/menu hidden
                        connect( entry.first, &QAction::changed, this, &MainWindow::updateShortcutToolTips );
                }
                updateShortcutToolTips();
        }


        ///
        /// Show each command's keyboard shortcut(s) in its tooltip, e.g. "Text (T)"
        ///
        void MainWindow::updateShortcutToolTips()
        {
                QHash<QAction*,QStringList> keys;
                for ( auto* action : actions() )
                {
                        if ( action->objectName().isEmpty() )
                        {
                                continue;
                        }
                        auto* target = qobject_cast<QAction*>( action->property( "quickKeyFor" ).value<QObject*>() );
                        if ( !target )
                        {
                                target = action;
                        }
                        for ( const auto& seq : action->shortcuts() )
                        {
                                if ( !seq.isEmpty() )
                                {
                                        keys[target] << seq.toString( QKeySequence::NativeText );
                                }
                        }
                }

                for ( auto* action : actions() )
                {
                        if ( action->objectName().isEmpty() || action->property( "quickKeyFor" ).isValid() )
                        {
                                continue;
                        }
                        QString tip = ShortcutsDialog::cleanText( action );
                        const QStringList& list = keys.value( action );
                        if ( !list.isEmpty() )
                        {
                                tip += QString( "  (%1)" ).arg( list.join( ", " ) );
                        }
                        if ( action->toolTip() != tip )
                        {
                                action->setToolTip( tip ); // emits changed(); the second pass is a no-op
                        }
                }
        }


        ///
        /// Apply sidebar (table of contents) style from settings
        ///
        void MainWindow::applySidebarStyle()
        {
                static const char* appliedProp = "appliedSidebarStyle";
                int iconSize = model::Settings::sidebarIconSize();
                QString styleKey = QString( "%1/%2" ).arg( int(model::Settings::sidebarStyle()) ).arg( iconSize );
                QString previousKey = mContents->property( appliedProp ).toString();
                mContents->setProperty( appliedProp, styleKey );
                mContents->setIconSize( QSize( iconSize, iconSize ) );

                Qt::ToolButtonStyle style = Qt::ToolButtonTextUnderIcon;
                switch ( model::Settings::sidebarStyle() )
                {
                case model::Settings::SIDEBAR_ICONS_ONLY: style = Qt::ToolButtonIconOnly; break;
                case model::Settings::SIDEBAR_TEXT_ONLY:  style = Qt::ToolButtonTextOnly; break;
                default:                                  style = Qt::ToolButtonTextUnderIcon; break;
                }

                // Page switcher at the bottom-left: a row of equal-width cells, icon over caption
                mContents->setContentsMargins( 4, 2, 4, 2 );
                if ( mContents->layout() )
                {
                        mContents->layout()->setSpacing( 2 );
                        mContents->layout()->setContentsMargins( 0, 0, 0, 0 );
                }

                const QList<QToolButton*> buttons = { mWelcomeButton, mEditorButton,
                                                      mMergeButton, mVariablesButton, mPrintButton };
                int cellWidth = 0;
                for ( auto* button : buttons )
                {
                        button->setToolButtonStyle( style );
                        button->setIconSize( QSize( iconSize, iconSize ) );
                        button->setFont( font() );
                        button->setAutoRaise( true );
                        button->setToolTip( button->text() );
                        button->setMinimumWidth( 0 );
                        cellWidth = std::max( cellWidth, button->sizeHint().width() );
                }
                if ( style != Qt::ToolButtonIconOnly )
                {
                        cellWidth = std::max( cellWidth, iconSize + 40 );
                }
                for ( auto* button : buttons )
                {
                        button->setMinimumWidth( cellWidth );
                }

                auto tbStyle = model::Settings::toolbarButtonStyle();
                for ( auto* tb : { fileToolBar, editorToolBar, viewToolBar, arrangeToolBar, mergeToolBar } )
                {
                        tb->setToolButtonStyle( tbStyle );
                }

                if ( !previousKey.isEmpty() && previousKey != styleKey )
                {
                        mContents->updateGeometry();
                }

        }


        ///
        /// Sidebar context menu
        ///
        void MainWindow::onSidebarContextMenu( const QPoint& pos )
        {
                QMenu menu;

                auto* styleGroup = new QActionGroup( &menu );
                auto addStyle = [&]( const QString& text, model::Settings::SidebarStyle st ) {
                        auto* a = menu.addAction( text );
                        a->setCheckable( true );
                        a->setChecked( model::Settings::sidebarStyle() == st );
                        styleGroup->addAction( a );
                        connect( a, &QAction::triggered, this, [st]{ model::Settings::setSidebarStyle( st ); } );
                };
                addStyle( tr("Icons and Text"), model::Settings::SIDEBAR_ICONS_AND_TEXT );
                addStyle( tr("Icons Only"),     model::Settings::SIDEBAR_ICONS_ONLY );
                addStyle( tr("Text Only"),      model::Settings::SIDEBAR_TEXT_ONLY );

                menu.addSeparator();

                auto* sizeGroup = new QActionGroup( &menu );
                auto addSize = [&]( const QString& text, int size ) {
                        auto* a = menu.addAction( text );
                        a->setCheckable( true );
                        a->setChecked( model::Settings::sidebarIconSize() == size );
                        sizeGroup->addAction( a );
                        connect( a, &QAction::triggered, this, [size]{ model::Settings::setSidebarIconSize( size ); } );
                };
                addSize( tr("Small Icons"),  16 );
                addSize( tr("Medium Icons"), 22 );
                addSize( tr("Large Icons"),  32 );
                addSize( tr("Extra Large Icons"), 48 );

                menu.exec( mContents->mapToGlobal( pos ) );
        }


        ///
        /// Toolbar/dock context menu
        ///
        QMenu* MainWindow::createPopupMenu()
        {
                auto* menu = new QMenu( this );
                menu->addAction( viewFileToolBarAction );
                menu->addAction( viewEditorToolBarAction );
                menu->addAction( viewViewToolBarAction );
                menu->addAction( viewArrangeToolBarAction );
                menu->addAction( viewMergeToolBarAction );
                menu->addSeparator();
                menu->addAction( viewPropertiesDockAction );
                menu->addAction( viewObjectListDockAction );
                menu->addAction( viewVariablesDockAction );
                return menu;
        }


        ///
        /// Event filter: track dock widgets closed by their title-bar button
        ///
        bool MainWindow::eventFilter( QObject* watched, QEvent* event )
        {
                if ( event->type() == QEvent::Close )
                {
                        if ( watched == mPropertiesDock )
                        {
                                viewPropertiesDockAction->setChecked( false );
                        }
                        else if ( watched == mObjectListDock )
                        {
                                viewObjectListDockAction->setChecked( false );
                        }
                        else if ( watched == mVariablesDock )
                        {
                                viewVariablesDockAction->setChecked( false );
                        }
                }
                return QMainWindow::eventFilter( watched, event );
        }


        ///
        /// Show toolbars and panels according to user preference and current page
        ///
        void MainWindow::updateChromeVisibility()
        {
                bool isEditor = mModel && mEditorButton && mEditorButton->isChecked();

                fileToolBar->setVisible( viewFileToolBarAction->isChecked() );
                editorToolBar->setVisible( isEditor && viewEditorToolBarAction->isChecked() );
                viewToolBar->setVisible( isEditor && viewViewToolBarAction->isChecked() );
                arrangeToolBar->setVisible( isEditor && viewArrangeToolBarAction->isChecked() );
                mergeToolBar->setVisible( isEditor && viewMergeToolBarAction->isChecked() );

                if ( mPropertiesDock )
                {
                        mPropertiesDock->setVisible( isEditor && viewPropertiesDockAction->isChecked() );
                        mObjectListDock->setVisible( isEditor && viewObjectListDockAction->isChecked() );
                        mVariablesDock->setVisible( isEditor && viewVariablesDockAction->isChecked() );
                }

                bool rulers = viewRulersAction->isChecked();
                if ( mHRuler )
                {
                        mHRuler->setVisible( rulers );
                        mVRuler->setVisible( rulers );
                        mRulerCorner->setVisible( rulers );
                }
        }


        ///
        /// Create Status Bar
        ///
        void MainWindow::createStatusBar()
        {
                cursorInfoLabel = new QLabel();
                cursorInfoLabel->setMinimumWidth( 160 );
                cursorInfoLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                cursorInfoLabel->setToolTip( tr("Pointer position on the label") );
                statusBar()->addPermanentWidget( cursorInfoLabel );
        }


        ///
        /// Create Welcome Page
        ///
        QWidget* MainWindow::createWelcomePage()
        {
                mWelcomeView = new StartupView( this );

                return mWelcomeView;
        }


        ///
        /// Create Properties Page
        ///
        QWidget* MainWindow::createPropertiesPage()
        {
                mPropertiesView = new PropertiesView();

                // The Properties page itself is retired (its view moves into the Print
                // page); keep an empty placeholder so page indices stay unchanged.
                return new QWidget();
        }


        ///
        /// Create Editor Page
        ///
        QWidget* MainWindow::createEditorPage()
        {
                QWidget* page = new QWidget;

                mLabelEditorScrollArea = new QScrollArea();
                mLabelEditorScrollArea->setWidgetResizable( true );
                mLabelEditorScrollArea->setMinimumSize( 320, 240 );

                mLabelEditor = new LabelEditor( mLabelEditorScrollArea );
                mObjectEditor = new ObjectEditor();

                mLabelEditorScrollArea->setWidget( mLabelEditor );

                // Rulers
                mHRuler = new Ruler( Qt::Horizontal, mLabelEditor );
                mVRuler = new Ruler( Qt::Vertical, mLabelEditor );

                auto* cornerButton = new QToolButton();
                cornerButton->setAutoRaise( true );
                cornerButton->setPopupMode( QToolButton::InstantPopup );
                cornerButton->setFixedSize( mVRuler->sizeHint().width(), mHRuler->sizeHint().height() );
                cornerButton->setToolTip( tr("Ruler units (click to change)") );
                QFont cornerFont = cornerButton->font();
                cornerFont.setPixelSize( 9 );
                cornerButton->setFont( cornerFont );
                cornerButton->setText( model::Settings::units().toIdString() );
                auto* unitsMenu = new QMenu( cornerButton );
                for ( auto id : { "in", "mm", "cm", "pt", "pc" } )
                {
                        auto* a = unitsMenu->addAction( id );
                        connect( a, &QAction::triggered, this, [id]{ model::Settings::setUnits( model::Units( QString(id) ) ); } );
                }
                cornerButton->setMenu( unitsMenu );
                connect( model::Settings::instance(), &model::Settings::changed, cornerButton, [cornerButton]{
                        cornerButton->setText( model::Settings::units().toIdString() );
                } );
                mRulerCorner = cornerButton;

                auto updateRulers = [this]{ mHRuler->update(); mVRuler->update(); };
                connect( mLabelEditor, &LabelEditor::viewChanged, this, updateRulers );
                connect( mLabelEditorScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, updateRulers );
                connect( mLabelEditorScrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, updateRulers );
                connect( mLabelEditor, &LabelEditor::pointerMoved, mHRuler, &Ruler::setPointer );
                connect( mLabelEditor, &LabelEditor::pointerMoved, mVRuler, &Ruler::setPointer );
                connect( mLabelEditor, &LabelEditor::pointerExited, mHRuler, &Ruler::clearPointer );
                connect( mLabelEditor, &LabelEditor::pointerExited, mVRuler, &Ruler::clearPointer );

                auto* grid = new QGridLayout;
                grid->setContentsMargins( 0, 0, 0, 0 );
                grid->setSpacing( 0 );
                grid->addWidget( mRulerCorner, 0, 0 );
                grid->addWidget( mHRuler, 0, 1 );
                grid->addWidget( mVRuler, 1, 0 );
                grid->addWidget( mLabelEditorScrollArea, 1, 1 );
                grid->setRowStretch( 1, 1 );
                grid->setColumnStretch( 1, 1 );

                page->setLayout( grid );

                return page;
        }


        ///
        /// Create Merge Page
        ///
        QWidget* MainWindow::createMergePage()
        {
                mMergeView = new MergeView();
                connect( mMergeView, &MergeView::connectRequested, this, &MainWindow::dataConnectSource );

                return mMergeView;
        }


        ///
        /// Create Variables Page
        ///
        QWidget* MainWindow::createVariablesPage()
        {
                mVariablesView = new VariablesView();

                return mVariablesView;
        }


        ///
        /// Create Print Page
        ///
        QWidget* MainWindow::createPrintPage()
        {
                mPrintView = new PrintView();

                // Product details (formerly the Properties page) sit beside the print controls
                mPropertiesView->showAsProductPane();

                auto* page = new QWidget();
                auto* layout = new QHBoxLayout( page );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 0 );
                layout->addWidget( mPrintView, 1 );
                auto* separator = new QFrame();
                separator->setFrameShape( QFrame::VLine );
                separator->setFrameShadow( QFrame::Sunken );
                layout->addWidget( separator );
                layout->addWidget( mPropertiesView, 0 );

                return page;
        }


        ///
        /// Manage enabled/visibility state of actions
        ///
        void MainWindow::manageActions()
        {
                // Do we have a model?
                bool hasModel      = mModel != nullptr;

                // Which page is currently active?
                bool isWelcomePage    = mWelcomeButton->isChecked() || ( mRecentButton && mRecentButton->isChecked() );
                bool isEditorPage     = mEditorButton->isChecked();
                bool isPropertiesPage = mPropertiesButton->isChecked();
                bool isMergePage      = mMergeButton->isChecked();
                bool isVariablesPage  = mVariablesButton->isChecked();
                bool isPrintPage      = mPrintButton->isChecked();

                // What is the current selection state?
                bool hasSelection = hasModel && !mModel->isSelectionEmpty();
                bool hasMultiSelection = hasSelection && !mModel->isSelectionAtomic();
                bool canPaste = hasModel && mModel->canPaste();

                // Toggle visibility of TOC buttons based on welcome mode
                // (Welcome and Recent are offered while no label is open)
                mWelcomeAction->setVisible( !hasModel );
                if ( mRecentAction )
                {
                        mRecentAction->setVisible( !hasModel );
                }
                mEditorAction->setVisible( hasModel );
                mPropertiesAction->setVisible( false );
                mOrientationAction->setVisible( hasModel );
                mMergeAction->setVisible( hasModel );
                mVariablesAction->setVisible( hasModel );
                mPrintAction->setVisible( hasModel );

                // Recent file actions
                QStringList recentFileList = model::Settings::recentFileList();
                for ( int i = 0; i < recentFileList.size() && i < fileRecentActionList.size(); i++ )
                {
                        QString baseName = QFileInfo( recentFileList.at(i) ).completeBaseName();
                        fileRecentActionList.at(i)->setText(baseName);
                        fileRecentActionList.at(i)->setData(recentFileList.at(i));
                        fileRecentActionList.at(i)->setVisible( true );
                }
                for ( int i = int( recentFileList.size() ); i < fileRecentActionList.size(); i++ )
                {
                        fileRecentActionList.at(i)->setVisible( false );
                }

                // File actions
                fileNewAction->setEnabled( true );
                fileOpenAction->setEnabled( true );
                fileOpenRecentAction->setEnabled( !recentFileList.isEmpty() );
                fileSaveAction->setEnabled( hasModel && mModel->isModified() );
                fileSaveAsAction->setEnabled( hasModel );
                fileSaveAsTemplateAction->setEnabled( hasModel );
                fileShowEditorPageAction->setEnabled( !isWelcomePage && !isEditorPage );
                fileShowPropertiesPageAction->setEnabled( !isWelcomePage && !isPropertiesPage );
                fileShowMergePageAction->setEnabled( !isWelcomePage && !isMergePage );
                fileShowVariablesPageAction->setEnabled( !isWelcomePage && !isVariablesPage );
                fileShowPrintPageAction->setEnabled( !isWelcomePage && !isPrintPage );
                fileTemplateDesignerAction->setEnabled( true );
                fileCloseAction->setEnabled( true );
                fileExitAction->setEnabled( true );

                // Edit actions
                if ( hasModel )
                {
                        if ( mUndoRedoModel->canUndo() )
                        {
                                editUndoAction->setEnabled( true );
                                // TRANSLATORS
                                //:   %1 is the action name/description (e.g. move, delete, ...)  to undo.
                                editUndoAction->setText( QString( tr("Undo %1") ).arg( mUndoRedoModel->undoDescription() ) );
                        }
                        else
                        {
                                editUndoAction->setEnabled( false );
                                editUndoAction->setText( tr("Undo") );
                        }
                        if ( mUndoRedoModel->canRedo() )
                        {
                                editRedoAction->setEnabled( true );
                                // TRANSLATORS
                                //:   %1 is the action name/description (e.g. move, delete, ...) to redo.
                                editRedoAction->setText( QString( tr("Redo %1") ).arg( mUndoRedoModel->redoDescription() ) );
                        }
                        else
                        {
                                editRedoAction->setEnabled( false );
                                editRedoAction->setText( tr("Redo") );
                        }
                }
                else
                {
                        editUndoAction->setEnabled( false );
                        editUndoAction->setText( tr("Undo") );
                        editRedoAction->setEnabled( false );
                        editRedoAction->setText( tr("Redo") );
                }
                editCutAction->setEnabled( isEditorPage && hasSelection );
                editCopyAction->setEnabled( isEditorPage && hasSelection );
                editPasteAction->setEnabled( isEditorPage && canPaste );
                editDeleteAction->setEnabled( isEditorPage && hasSelection );
                editSelectAllAction->setEnabled( isEditorPage );
                editUnSelectAllAction->setEnabled( isEditorPage && hasSelection );
                editPageSetupAction->setEnabled( true );
                editPreferencesAction->setEnabled( true );
                editShortcutsAction->setEnabled( true );

                // View actions
                viewFileToolBarAction->setEnabled( true );
                viewEditorToolBarAction->setEnabled( true );
                viewGridAction->setEnabled( isEditorPage );
                viewMarkupAction->setEnabled( isEditorPage );
                viewRulersAction->setEnabled( isEditorPage );
                mZoomCombo->setEnabled( isEditorPage );

                int nRecords = (hasModel && mLabelEditor) ? mLabelEditor->nPreviewRecords() : 0;
                int iRecord  = (hasModel && mLabelEditor) ? mLabelEditor->previewRecord() : -1;
                bool previewing = iRecord >= 0;
                mergePreviewAction->setEnabled( isEditorPage && nRecords > 0 );
                mergePlaceholdersAction->setEnabled( isEditorPage && nRecords > 0 );
                mergePlaceholdersAction->setChecked( !previewing );
                dataConnectAction->setEnabled( hasModel );
                dataReloadAction->setEnabled( hasModel && mModel->merge() && !mModel->merge()->source().isEmpty() );
                viewVariablesDockAction->setEnabled( isEditorPage );
                mergeFirstAction->setEnabled( isEditorPage && previewing && iRecord > 0 );
                mergePreviousAction->setEnabled( isEditorPage && previewing && iRecord > 0 );
                mergeNextAction->setEnabled( isEditorPage && previewing && iRecord < nRecords-1 );
                mergeLastAction->setEnabled( isEditorPage && previewing && iRecord < nRecords-1 );
                mMergeRecordSpin->setEnabled( isEditorPage && previewing );
                viewZoomInAction->setEnabled( isEditorPage );
                viewZoomOutAction->setEnabled( isEditorPage );
                viewZoom1To1Action->setEnabled( isEditorPage );
                viewZoomToFitAction->setEnabled( isEditorPage );

                // Object actions
                objectsArrowModeAction->setEnabled( isEditorPage );
                objectsMeasureModeAction->setEnabled( isEditorPage );
                objectsCreateMenu->setEnabled( isEditorPage );
                objectsCreateTextAction->setEnabled( isEditorPage );
                objectsCreateLineAction->setEnabled( isEditorPage );
                objectsCreateBoxAction->setEnabled( isEditorPage );
                objectsCreateEllipseAction->setEnabled( isEditorPage );
                objectsCreateImageAction->setEnabled( isEditorPage );
                objectsCreateBarcodeAction->setEnabled( isEditorPage );
                objectsOrderMenu->setEnabled( isEditorPage && hasSelection );
                objectsOrderRaiseAction->setEnabled( isEditorPage && hasSelection );
                objectsOrderLowerAction->setEnabled( isEditorPage && hasSelection );
                objectsOrderRaiseOneAction->setEnabled( isEditorPage && hasSelection );
                objectsOrderLowerOneAction->setEnabled( isEditorPage && hasSelection );
                {
                        bool canEditText = false;
                        if ( hasModel && mModel->isSelectionAtomic() )
                        {
                                auto* object = mModel->getFirstSelectedObject();
                                canEditText = dynamic_cast<model::ModelTextObject*>( object ) ||
                                              dynamic_cast<model::ModelBarcodeObject*>( object );
                        }
                        objectsEditTextAction->setEnabled( isEditorPage && canEditText );
                }
                mRotationSpin->setEnabled( isEditorPage && hasSelection );
                objectsXformMenu->setEnabled( isEditorPage && hasSelection );
                objectsXformRotateLeftAction->setEnabled( isEditorPage && hasSelection );
                objectsXformRotateRightAction->setEnabled( isEditorPage && hasSelection );
                objectsXformFlipHorizAction->setEnabled( isEditorPage && hasSelection );
                objectsXformFlipVertAction->setEnabled( isEditorPage && hasSelection );
                bool canAlign = isEditorPage && hasModel && mModel->canAlignSelection();
                Q_UNUSED( hasMultiSelection );
                objectsAlignMenu->setEnabled( isEditorPage && hasSelection );
                objectsAlignLeftAction->setEnabled( canAlign );
                objectsAlignRightAction->setEnabled( canAlign );
                objectsAlignHCenterAction->setEnabled( canAlign );
                objectsAlignTopAction->setEnabled( canAlign );
                objectsAlignBottomAction->setEnabled( canAlign );
                objectsAlignVCenterAction->setEnabled( canAlign );
                objectsCenterMenu->setEnabled( isEditorPage && hasSelection );
                objectsCenterHorizAction->setEnabled( isEditorPage && hasSelection );
                objectsCenterVertAction->setEnabled( isEditorPage && hasSelection );
                objectsCenterAction->setEnabled( isEditorPage && hasSelection );
                toolsInsertSymbolAction->setEnabled( hasModel );
                objectsPutTextOnShapeAction->setEnabled( isEditorPage && hasModel && textAndShapeSelected( mModel.get() ) );

                // Help actions
                helpContentsAction->setEnabled( true );
                helpReportBugAction->setEnabled( true );
                helpAboutAction->setEnabled( true );

                // Special context actions
                contextCutAction->setEnabled( isEditorPage && hasSelection );
                contextCopyAction->setEnabled( isEditorPage && hasSelection );
                contextPasteAction->setEnabled( isEditorPage && canPaste );
                contextDeleteAction->setEnabled( isEditorPage && hasSelection );
        }


        ///
        /// Set window title
        ///
        void MainWindow::setTitle()
        {
                if ( !mModel )
                {
                        setWindowTitle( "gLabels Plus" );
                }
                else
                {
                        if ( mModel->isModified() )
                        {
                                setWindowTitle( "gLabels Plus - " + mModel->shortName() + " " + tr("(modified)") );
                        }
                        else
                        {
                                setWindowTitle( "gLabels Plus - " + mModel->shortName()  );
                        }
                }
        }


        ///
        /// Read MainWindow Settings
        ///
        void MainWindow::readSettings()
        {
                QSettings settings;

                settings.beginGroup( "MainWindow" );
                bool showFileToolBar    = settings.value( "showFileToolBar",    true ).toBool();
                bool showEditorToolBar  = settings.value( "showEditorToolBar",
                                                          settings.value( "showEditToolBar", true ) ).toBool();
                bool showViewToolBar    = settings.value( "showViewToolBar",    true ).toBool();
                bool showArrangeToolBar = settings.value( "showArrangeToolBar", true ).toBool();
                bool showMergeToolBar   = settings.value( "showMergeToolBar",   true ).toBool();
                bool showPropertiesDock = settings.value( "showPropertiesDock", true ).toBool();
                bool showObjectListDock = settings.value( "showObjectListDock", true ).toBool();
                bool showVariablesDock = settings.value( "showVariablesDock", true ).toBool();
                QByteArray geometry     = settings.value( "geometry" ).toByteArray();
                QByteArray state        = settings.value( "state" ).toByteArray();
                settings.endGroup();

                // Windows always open maximized, so the saved geometry is not restored: on X11 a stale
                // "normal" size could win over the maximize request and leave a small window.
                Q_UNUSED( geometry );
                if ( !state.isEmpty() )
                {
                        restoreState( state, STATE_VERSION );
                }

                mSyncing = true;
                viewFileToolBarAction    ->setChecked( showFileToolBar );
                viewEditorToolBarAction  ->setChecked( showEditorToolBar );
                viewViewToolBarAction    ->setChecked( showViewToolBar );
                viewArrangeToolBarAction ->setChecked( showArrangeToolBar );
                viewMergeToolBarAction   ->setChecked( showMergeToolBar );
                viewPropertiesDockAction ->setChecked( showPropertiesDock );
                viewObjectListDockAction ->setChecked( showObjectListDock );
                viewVariablesDockAction  ->setChecked( showVariablesDock );
                viewGridAction           ->setChecked( model::Settings::showGrid() );
                viewMarkupAction         ->setChecked( model::Settings::showMarkup() );
                viewRulersAction         ->setChecked( model::Settings::showRulers() );
                viewPropertiesUnitsAction->setChecked( model::Settings::propertiesShowUnits() );
                mSyncing = false;

                mLabelEditor  ->setGridVisible(   model::Settings::showGrid() );
                mLabelEditor  ->setMarkupVisible( model::Settings::showMarkup() );

                int ref = model::Settings::alignReference();
                for ( auto* a : alignRefGroup->actions() )
                {
                        a->setChecked( a->data().toInt() == ref );
                }
                mAlignRefCombo->setCurrentIndex( mAlignRefCombo->findData( ref ) );

                updateChromeVisibility();
        }


        ///
        /// Write MainWindow Settings
        ///
        void MainWindow::writeSettings()
        {
                QSettings settings;

                settings.beginGroup( "MainWindow" );
                settings.setValue( "showFileToolBar",    viewFileToolBarAction->isChecked() );
                settings.setValue( "showEditorToolBar",  viewEditorToolBarAction->isChecked() );
                settings.setValue( "showViewToolBar",    viewViewToolBarAction->isChecked() );
                settings.setValue( "showArrangeToolBar", viewArrangeToolBarAction->isChecked() );
                settings.setValue( "showMergeToolBar",   viewMergeToolBarAction->isChecked() );
                settings.setValue( "showPropertiesDock", viewPropertiesDockAction->isChecked() );
                settings.setValue( "showObjectListDock", viewObjectListDockAction->isChecked() );
                settings.setValue( "showVariablesDock",  viewVariablesDockAction->isChecked() );
                settings.setValue( "geometry", saveGeometry() );
                settings.setValue( "state",    saveState( STATE_VERSION ) );
                settings.endGroup();
        }


        ///
        /// Is it ok to close window?
        ///
        bool MainWindow::isOkToClose()
        {
                bool ok = true;

                if ( !this->isEmpty() )
                {
                        if ( mModel->isModified() )
                        {
                                QString msg = tr("Save changes to label \"%1\" before closing?").arg( mModel->shortName() );
                                QString info = tr("Your changes will be lost if you don't save them.");

                                int ret = QMessageBox::warning( this,
                                                                tr( "Save project?" ),
                                                                "<b>" + msg + "</b><p>" + info + "</p>",
                                                                (QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel),
                                                                QMessageBox::Save );

                                switch (ret) {
                                case QMessageBox::Save:
                                        // Save was clicked
                                        ok = File::save( this );
                                        break;
                                case QMessageBox::Discard:
                                        // Don't Save was clicked
                                        ok = true;
                                        break;
                                case QMessageBox::Cancel:
                                        // Cancel was clicked
                                        ok = false;
                                        break;
                                default:
                                        // should never be reached
                                        ok = false;
                                        break;
                                }
                        }
                }

                return ok;
        }


        ///
        /// Toolbar orientation toggle: flip between horizontal and vertical
        ///
        void MainWindow::onOrientationButtonClicked()
        {
                if ( !mModel )
                {
                        return;
                }
                auto* frame = mModel->tmplate().frame();
                if ( !frame || frame->w() == frame->h() )
                {
                        return;
                }
                mUndoRedoModel->checkpoint( tr("Product Rotate") );
                mModel->setRotate( !mModel->rotate() );
                updateOrientationButton();
        }


        ///
        /// Show the current orientation on the toolbar toggle
        ///
        void MainWindow::updateOrientationButton()
        {
                if ( !mOrientationButton || !mModel )
                {
                        return;
                }
                auto* frame = mModel->tmplate().frame();
                bool square = !frame || frame->w() == frame->h();
                bool vertical = !square && ( frame->w() > frame->h() ? mModel->rotate() : !mModel->rotate() );

                mOrientationButton->setEnabled( !square );
                mOrientationButton->setChecked( vertical );
                mOrientationButton->setIcon( QIcon::fromTheme( vertical ? "glabels-label-orientation-vert"
                                                                        : "glabels-label-orientation-horiz" ) );
                mOrientationButton->setToolTip( square ? tr("Page orientation: this label is round or square")
                                                       : tr("Page orientation: %1. Click to switch to %2.")
                                                         .arg( vertical ? tr("vertical") : tr("horizontal"),
                                                               vertical ? tr("horizontal") : tr("vertical") ) );
        }


        ///
        /// Change page
        ///
        void MainWindow::changePage( bool checked )
        {
                if ( checked )
                {
                        if ( mWelcomeButton->isChecked() )
                        {
                                mPages->setCurrentIndex( WELCOME_PAGE_INDEX );
                        }
                        else if ( mRecentButton && mRecentButton->isChecked() )
                        {
                                mPages->setCurrentIndex( RECENT_PAGE_INDEX );
                        }
                        else if ( mEditorButton->isChecked() )
                        {
                                mPages->setCurrentIndex( EDITOR_PAGE_INDEX );
                        }
                        else if ( mPropertiesButton->isChecked() )
                        {
                                mPages->setCurrentIndex( PROPERTIES_PAGE_INDEX );
                        }
                        else if ( mMergeButton->isChecked() )
                        {
                                mPages->setCurrentIndex( MERGE_PAGE_INDEX );
                        }
                        else if ( mVariablesButton->isChecked() )
                        {
                                mPages->setCurrentIndex( VARIABLES_PAGE_INDEX );
                        }
                        else if ( mPrintButton->isChecked() )
                        {
                                mPages->setCurrentIndex( PRINT_PAGE_INDEX );
                        }

                        if ( !mEditorButton->isChecked() && mLabelEditor->isTextEditActive() )
                        {
                                mLabelEditor->endTextEdit( true );
                        }

                        manageActions();
                        updateChromeVisibility();
                }
        }


        ///
        /// Clipboard contents changed
        ///
        void MainWindow::clipboardChanged()
        {
                manageActions();
        }


        ///
        /// File->New Action
        ///
        void MainWindow::fileNew()
        {
                File::newLabel( this );
        }


        ///
        /// File->New from Sample: the sample library on the welcome screen (in this
        /// window if no label is open, otherwise in a new window)
        ///
        void MainWindow::fileNewFromSample()
        {
                MainWindow* target = this;
                if ( mModel )
                {
                        target = new MainWindow();
                        target->show();
                }
                target->showWelcomePage();
                target->mWelcomeView->showSamples();
                target->activateWindow();
        }


        ///
        /// File->New Project Action: create a project and show it on the welcome screen
        /// (in this window if it's empty, otherwise in a new window)
        ///
        void MainWindow::fileNewProject()
        {
                QString project = Projects::askNewProject( this );
                if ( project.isEmpty() )
                {
                        return;
                }

                MainWindow* target = this;
                if ( !isEmpty() )
                {
                        target = new MainWindow();
                        target->show();
                }
                target->mWelcomeView->selectProject( project );
                target->activateWindow();
        }


        ///
        /// File->Open Action
        ///
        void MainWindow::fileOpen()
        {
                File::open( this );
        }


        ///
        /// File->OpenRecent Action
        ///
        void MainWindow::fileOpenRecent()
        {
                QAction* action = qobject_cast<QAction*>( sender() );
                if ( action )
                {
                        QString fileName = action->data().toString();
                        File::open( fileName, this );
                }
        }


        ///
        /// File->Save Action
        ///
        void MainWindow::fileSave()
        {
                File::save( this );
        }


        ///
        /// File->Save As Action
        ///
        void MainWindow::fileSaveAs()
        {
                File::saveAs( this );
        }


        ///
        /// File->Show Editor Page
        ///
        void MainWindow::fileShowEditorPage()
        {
                mEditorButton->setChecked( true );
        }


        ///
        /// File->Show Properties Page
        ///
        void MainWindow::fileShowPropertiesPage()
        {
                mPrintButton->setChecked( true );   // the product pane is part of the Print page
        }


        ///
        /// File->Show Merge Page
        ///
        void MainWindow::fileShowMergePage()
        {
                mMergeButton->setChecked( true );
        }


        ///
        /// File->Show Variables Page
        ///
        void MainWindow::fileShowVariablesPage()
        {
                mVariablesButton->setChecked( true );
        }


        ///
        /// File->Show Print Page
        ///
        void MainWindow::fileShowPrintPage()
        {
                mPrintButton->setChecked( true );
        }


        ///
        /// Show the Print page
        ///
        void MainWindow::showPrintPage()
        {
                if ( mModel )
                {
                        mPrintButton->setChecked( true );
                }
        }


        ///
        /// File->Open Recent...: the Recent labels screen.  It is only offered while
        /// no label is open, so with a label open it appears in a new window.
        ///
        void MainWindow::fileShowRecentScreen()
        {
                if ( !mModel )
                {
                        showRecentPage();
                        return;
                }
                auto* window = new MainWindow();
                window->showRecentPage();
                window->show();
        }


        ///
        /// Show the Recent labels page (no label open)
        ///
        void MainWindow::showRecentPage()
        {
                if ( !mModel )
                {
                        mRecentButton->setChecked( true );
                        mRecentView->reload();
                }
        }


        ///
        /// Show the Welcome page (no label open)
        ///
        void MainWindow::showWelcomePage()
        {
                if ( !mModel )
                {
                        mWelcomeButton->setChecked( true );
                }
        }


        ///
        /// File->Template Designer Action
        ///
        void MainWindow::fileTemplateDesigner()
        {
                File::templateDesigner( this );
        }


        ///
        /// File->Close Action
        ///
        void MainWindow::fileClose()
        {
                File::close( this );
        }


        ///
        /// File->Exit Action
        ///
        void MainWindow::fileExit()
        {
                File::exit();
        }


        ///
        /// Edit->Undo Action
        ///
        void MainWindow::editUndo()
        {
                mUndoRedoModel->undo();
        }


        ///
        /// Edit->Redo Action
        ///
        void MainWindow::editRedo()
        {
                mUndoRedoModel->redo();
        }


        ///
        /// Edit->Cut Action
        ///
        void MainWindow::editCut()
        {
                mUndoRedoModel->checkpoint( tr("Cut") );
                mModel->cutSelection();
        }


        ///
        /// Edit->Copy Action
        ///
        void MainWindow::editCopy()
        {
                // Non-destructive -- do not checkpoint.
                mModel->copySelection();
        }


        ///
        /// Edit->Paste Action
        ///
        void MainWindow::editPaste()
        {
                mUndoRedoModel->checkpoint( tr("Paste") );
                mModel->paste( model::Point() );
        }


        ///
        /// Edit->Paste Action (from context menu)
        ///
        void MainWindow::editContextPaste()
        {
                // Extract original context menu click location
                auto *action = qobject_cast<QAction *>(sender());
                auto p = action->data().value<model::Point>();

                mUndoRedoModel->checkpoint( tr("Paste") );
                mModel->paste( p );
        }


        ///
        /// Edit->Delete Action
        ///
        void MainWindow::editDelete()
        {
                mUndoRedoModel->checkpoint( tr("Delete") );
                mModel->deleteSelection();
        }


        ///
        /// Edit->Select All Action
        ///
        void MainWindow::editSelectAll()
        {
                mModel->selectAll();
        }


        ///
        /// Edit->Unselect All Action
        ///
        void MainWindow::editUnSelectAll()
        {
                mModel->unselectAll();
        }


        ///
        /// Edit->Preferences Action
        ///
        void MainWindow::editPreferences()
        {
                PreferencesDialog dialog( this );
                dialog.exec();
        }


        ///
        /// View->File Tool Bar Toggle Action
        ///
        void MainWindow::viewFileToolBar( bool state )
        {
                updateChromeVisibility();
        }


        ///
        /// View->Objects Tool Bar Toggle Action
        ///
        void MainWindow::viewEditorToolBar( bool state )
        {
                updateChromeVisibility();
        }


        ///
        /// View->Toolbars (other) Toggle Action
        ///
        void MainWindow::viewToolBarToggled( bool state )
        {
                updateChromeVisibility();
        }


        ///
        /// View->Panels Toggle Action
        ///
        void MainWindow::viewDockToggled( bool state )
        {
                updateChromeVisibility();
        }


        ///
        /// View->Rulers Toggle Action
        ///
        void MainWindow::viewRulers( bool state )
        {
                if ( !mSyncing )
                {
                        model::Settings::setShowRulers( state );
                }
                updateChromeVisibility();
        }


        ///
        /// View->Units in Properties Toggle Action
        ///
        void MainWindow::viewPropertiesUnits( bool state )
        {
                if ( !mSyncing )
                {
                        model::Settings::setPropertiesShowUnits( state );
                }
        }


        ///
        /// Zoom combo: user picked or typed a zoom level
        ///
        void MainWindow::onZoomComboActivated()
        {
                if ( !mModel )
                {
                        return;
                }

                int i = mZoomCombo->currentIndex();
                QString text = mZoomCombo->currentText().trimmed();

                if ( i >= 0 && mZoomCombo->itemText( i ) == text )
                {
                        double z = mZoomCombo->itemData( i ).toDouble();
                        if ( z < 0 )
                        {
                                mLabelEditor->zoomToFit();
                        }
                        else
                        {
                                mLabelEditor->setZoom( z );
                        }
                }
                else
                {
                        text.remove( '%' );
                        bool ok = false;
                        double percent = QLocale().toDouble( text, &ok );
                        if ( !ok )
                        {
                                percent = text.toDouble( &ok );
                        }
                        if ( ok && percent > 0 )
                        {
                                mLabelEditor->setZoom( percent / 100.0 );
                        }
                        else
                        {
                                onZoomChanged(); // restore display
                        }
                }

                mLabelEditor->setFocus();
        }


        ///
        /// Units combo changed
        ///
        void MainWindow::onUnitsComboChanged( int index )
        {
                if ( mSyncing || index < 0 )
                {
                        return;
                }
                model::Settings::setUnits( model::Units( mUnitsCombo->itemData( index ).toString() ) );
        }


        ///
        /// Page button clicked: choose a page (product) for a new label, or change
        /// the current label's page while leaving every object exactly where it is
        ///
        void MainWindow::onPageButtonClicked()
        {
                if ( !mModel )
                {
                        File::newLabel( this );
                        return;
                }

                SelectProductDialog dialog( this );
                dialog.exec();

                auto tmplate = dialog.tmplate();
                if ( tmplate.isNull() || tmplate == mModel->tmplate() )
                {
                        return;
                }

                // Only the template is swapped: object positions, sizes and the
                // label rotation are left untouched. Undoable in one step.
                mUndoRedoModel->checkpoint( tr("Change Page") );
                mModel->setTmplate( tmplate );
                model::Settings::addToRecentTemplateList( tmplate.name() );

                statusBar()->showMessage( tr("Page changed to %1").arg( tmplate.name() ), 4000 );
        }


        ///
        /// Sync page button with current model's template
        ///
        void MainWindow::syncPageButton()
        {
                if ( !mPageButton )
                {
                        return;
                }

                if ( !mModel || mModel->tmplate().isNull() )
                {
                        mPageButton->setText( tr("Choose page...") + QString::fromUtf8( "  \u25BE" ) );
                        mPageButton->setToolTip( tr("Choose a page (product template) for a new label") );
                        return;
                }

                const auto& tmplate = mModel->tmplate();
                auto units = model::Settings::units();

                QString name = mPageButton->fontMetrics().elidedText( tmplate.name(), Qt::ElideRight, 220 );
                mPageButton->setText( name + QString::fromUtf8( "  \u25BE" ) );

                QString tip = QString( "<b>%1</b>" ).arg( tmplate.name().toHtmlEscaped() );
                if ( !tmplate.description().isEmpty() )
                {
                        tip += "<br>" + tmplate.description().toHtmlEscaped();
                }
                if ( auto* frame = tmplate.frame() )
                {
                        tip += "<br>" + tr("Label size: %1").arg( frame->sizeDescription( units ).toHtmlEscaped() );
                }
                tip += "<br>" + tr("Paper: %1").arg( tmplate.paperDescription( units ).toHtmlEscaped() );
                tip += "<br><br><i>" + tr("Click to change the page. Objects keep their positions.") + "</i>";
                mPageButton->setToolTip( tip );
        }


        ///
        /// Sync units combo with settings
        ///
        void MainWindow::syncUnitsCombo()
        {
                mSyncing = true;
                int i = mUnitsCombo->findData( model::Settings::units().toIdString() );
                if ( i >= 0 )
                {
                        mUnitsCombo->setCurrentIndex( i );
                }
                mSyncing = false;
        }


        ///
        /// Alignment reference changed
        ///
        void MainWindow::onAlignReferenceChanged( int ref )
        {
                model::Settings::setAlignReference( ref );
        }


        ///
        /// Rotation spin changed
        ///
        void MainWindow::onRotationSpinChanged( double value )
        {
                if ( mSyncing || !mModel || mModel->isSelectionEmpty() )
                {
                        return;
                }
                mUndoRedoModel->checkpoint( tr("Rotate") );
                mModel->setSelectionRotation( value );
        }


        ///
        /// Sync rotation spin with current selection
        ///
        void MainWindow::syncRotationSpin()
        {
                if ( !mRotationSpin )
                {
                        return;
                }
                mSyncing = true;
                double theta = 0;
                if ( mModel )
                {
                        if ( auto* object = mModel->getFirstSelectedObject() )
                        {
                                theta = object->rotation();
                        }
                }
                if ( std::fabs( mRotationSpin->value() - theta ) > 0.05 )
                {
                        mRotationSpin->setValue( theta );
                }
                mSyncing = false;
        }


        ///
        /// Edit->Keyboard Shortcuts Action
        ///
        void MainWindow::editShortcuts()
        {
                QList<QAction*> actions;
                for ( auto* action : this->actions() )
                {
                        if ( !action->objectName().isEmpty() )
                        {
                                actions << action;
                        }
                }

                ShortcutsDialog dialog( actions, this );
                if ( dialog.exec() == QDialog::Accepted )
                {
                        // Apply to all other open windows
                        for ( auto* widget : QApplication::topLevelWidgets() )
                        {
                                if ( auto* window = qobject_cast<MainWindow*>( widget ) )
                                {
                                        if ( window != this )
                                        {
                                                ShortcutsDialog::applySaved( window->actions() );
                                        }
                                }
                        }
                }
        }


        ///
        /// View->Grid Toggle Action
        ///
        void MainWindow::viewGrid( bool state )
        {
                mLabelEditor->setGridVisible( state );
                if ( !mSyncing )
                {
                        model::Settings::setShowGrid( state );
                }
        }


        ///
        /// View->Markup Toggle Action
        ///
        void MainWindow::viewMarkup( bool state )
        {
                mLabelEditor->setMarkupVisible( state );
                if ( !mSyncing )
                {
                        model::Settings::setShowMarkup( state );
                }
        }


        ///
        /// View->Zoom In Action
        ///
        void MainWindow::viewZoomIn()
        {
                mLabelEditor->zoomIn();
        }


        ///
        /// View->Zoom Out Action
        ///
        void MainWindow::viewZoomOut()
        {
                mLabelEditor->zoomOut();
        }


        ///
        /// View->Zoom 1:1 Action
        ///
        void MainWindow::viewZoom1To1()
        {
                mLabelEditor->zoom1To1();
        }


        ///
        /// View->Zoom To Fit Action
        ///
        void MainWindow::viewZoomToFit()
        {
                mLabelEditor->zoomToFit();
        }


        ///
        /// Objects->Arrow Mode Action
        ///
        void MainWindow::objectsArrowMode()
        {
                mLabelEditor->arrowMode();
        }


        ///
        /// Objects->Measure Mode Action
        ///
        void MainWindow::objectsMeasureMode()
        {
                mLabelEditor->measureMode();
        }


        ///
        /// Objects->Create Text Mode Action
        ///
        void MainWindow::objectsCreateText()
        {
                mUndoRedoModel->checkpoint( tr("Create Text") );
                mLabelEditor->createTextMode();
        }


        ///
        /// Objects->Create Box Mode Action
        ///
        void MainWindow::objectsCreateBox()
        {
                mUndoRedoModel->checkpoint( tr("Create Box") );
                mLabelEditor->createBoxMode();
        }


        ///
        /// Objects->Create Line Mode Action
        ///
        void MainWindow::objectsCreateLine()
        {
                mUndoRedoModel->checkpoint( tr("Create Line") );
                mLabelEditor->createLineMode();
        }


        ///
        /// Objects->Create Ellipse Mode Action
        ///
        void MainWindow::objectsCreateEllipse()
        {
                mUndoRedoModel->checkpoint( tr("Create Ellipse") );
                mLabelEditor->createEllipseMode();
        }


        ///
        /// Objects->Create Image Mode Action
        ///
        void MainWindow::objectsCreateImage()
        {
                mUndoRedoModel->checkpoint( tr("Create Image") );
                mLabelEditor->createImageMode();
        }


        ///
        /// Objects->Create Barcode Mode Action
        ///
        void MainWindow::objectsCreateBarcode()
        {
                mUndoRedoModel->checkpoint( tr("Create Barcode") );
                mLabelEditor->createBarcodeMode();
        }


        ///
        /// Objects->Order->Bring To Front Action
        ///
        void MainWindow::objectsOrderRaise()
        {
                mUndoRedoModel->checkpoint( tr("Bring To Front") );
                mModel->raiseSelectionToTop();
        }


        ///
        /// Objects->Order->Send To Back Action
        ///
        void MainWindow::objectsOrderLower()
        {
                mUndoRedoModel->checkpoint( tr("Send To Back") );
                mModel->lowerSelectionToBottom();
        }


        ///
        /// Objects->Order->Bring Forward Action
        ///
        void MainWindow::objectsOrderRaiseOne()
        {
                mUndoRedoModel->checkpoint( tr("Bring Forward") );
                mModel->raiseSelection();
        }


        ///
        /// Objects->Order->Send Backward Action
        ///
        void MainWindow::objectsOrderLowerOne()
        {
                mUndoRedoModel->checkpoint( tr("Send Backward") );
                mModel->lowerSelection();
        }


        ///
        /// Objects->Edit Text In Place Action
        ///
        void MainWindow::objectsEditText()
        {
                if ( mModel && mModel->isSelectionAtomic() )
                {
                        mLabelEditor->beginTextEdit( mModel->getFirstSelectedObject() );
                }
        }


        ///
        /// Merge preview toggled
        ///
        void MainWindow::mergePreviewToggled( bool state )
        {
                if ( mSyncing )
                {
                        return;
                }
                int current = mLabelEditor->previewRecord();
                mLabelEditor->setPreviewRecord( state ? qMax( 0, current ) : -1 );
        }


        void MainWindow::mergePreviewFirst()
        {
                mLabelEditor->setPreviewRecord( 0 );
        }


        void MainWindow::mergePreviewPrevious()
        {
                mLabelEditor->setPreviewRecord( qMax( 0, mLabelEditor->previewRecord() - 1 ) );
        }


        void MainWindow::mergePreviewNext()
        {
                mLabelEditor->setPreviewRecord( mLabelEditor->previewRecord() + 1 );
        }


        void MainWindow::mergePreviewLast()
        {
                mLabelEditor->setPreviewRecord( mLabelEditor->nPreviewRecords() - 1 );
        }


        void MainWindow::onMergeRecordSpinChanged( int value )
        {
                if ( mSyncing )
                {
                        return;
                }
                mLabelEditor->setPreviewRecord( value - 1 );
        }


        ///
        /// Preview record changed (or merge source changed)
        ///
        void MainWindow::onPreviewRecordChanged()
        {
                int n = mLabelEditor->nPreviewRecords();
                int i = mLabelEditor->previewRecord();

                mSyncing = true;
                mergePreviewAction->setChecked( i >= 0 );
                mMergeRecordSpin->setRange( n > 0 ? 1 : 0, qMax( n, 0 ) );
                mMergeRecordSpin->setValue( i + 1 );
                mMergeRecordCountLabel->setText( n > 0 ? tr(" of %1 ").arg( n ) : tr(" (no data) ") );
                mSyncing = false;

                manageActions();
        }


        ///
        /// Data->Show Placeholders
        ///
        void MainWindow::dataShowPlaceholders()
        {
                mLabelEditor->setPreviewRecord( -1 );
                onPreviewRecordChanged();
        }


        ///
        /// Data->Connect Data Source
        ///
        void MainWindow::dataConnectSource()
        {
                if ( !mModel )
                {
                        return;
                }

                ConnectDataSourceDialog dialog( mModel.get(), this );
                if ( dialog.exec() != QDialog::Accepted )
                {
                        return;
                }

                merge::Merge* newMerge = dialog.takeMerge();
                if ( !newMerge )
                {
                        return;
                }
                QMap<QString,QString> renames = dialog.renames();

                mUndoRedoModel->checkpoint( tr("Connect Data Source") );
                mModel->setMerge( newMerge );

                // Two-step rename (old -> temporary -> new) so that swapped names don't collide
                QMap<QString,QString> temporary;
                for ( auto it = renames.constBegin(); it != renames.constEnd(); ++it )
                {
                        QString tmp = "__glabels_rename_" + it.key();
                        mModel->renameVariableReferences( it.key(), tmp );
                        temporary[tmp] = it.value();
                }
                for ( auto it = temporary.constBegin(); it != temporary.constEnd(); ++it )
                {
                        mModel->renameVariableReferences( it.key(), it.value() );
                }

                // Show the variables next to the label
                if ( !mEditorButton->isChecked() )
                {
                        mEditorButton->setChecked( true );
                }
                viewVariablesDockAction->setChecked( true );
                updateChromeVisibility();
                mVariablesDock->raise();

                statusBar()->showMessage( tr("Imported %n variable(s) from %1", "", int( newMerge->keys().size() ))
                                          .arg( QFileInfo( newMerge->source() ).fileName() ), 6000 );
        }


        ///
        /// Data->Reload Data
        ///
        void MainWindow::dataReload()
        {
                if ( mModel && mModel->merge() )
                {
                        mModel->merge()->reloadSource();
                }
        }


        ///
        /// Add an object bound to a variable (from the Variables panel)
        ///
        void MainWindow::onAddVariableRequested( const QString& name, const QString& type )
        {
                if ( !mModel )
                {
                        return;
                }
                if ( !mEditorButton->isChecked() )
                {
                        mEditorButton->setChecked( true );
                }
                mLabelEditor->addVariableObject( name, type );
        }


        ///
        /// Objects->Rotate/Flip->Rotate Left Action
        ///
        void MainWindow::objectsXformRotateLeft()
        {
                mUndoRedoModel->checkpoint( tr("Rotate Left") );
                mModel->rotateSelectionLeft();
        }


        ///
        /// Objects->Rotate/Flip->Rotate Right Action
        ///
        void MainWindow::objectsXformRotateRight()
        {
                mUndoRedoModel->checkpoint( tr("Rotate Right") );
                mModel->rotateSelectionRight();
        }


        ///
        /// Objects->Rotate/Flip->Flip Horizontally Action
        ///
        void MainWindow::objectsXformFlipHoriz()
        {
                mUndoRedoModel->checkpoint( tr("Flip Horizontally") );
                mModel->flipSelectionHoriz();
        }


        ///
        /// Objects->Rotate/Flip->Flip Vertically Action
        ///
        void MainWindow::objectsXformFlipVert()
        {
                mUndoRedoModel->checkpoint( tr("Flip Vertically") );
                mModel->flipSelectionVert();
        }


        ///
        /// Objects->Align->Left Action
        ///
        void MainWindow::objectsAlignLeft()
        {
                mUndoRedoModel->checkpoint( tr("Align Left") );
                mModel->alignSelectionLeft();
        }


        ///
        /// Objects->Align->Center Horizontally Action
        ///
        void MainWindow::objectsAlignHCenter()
        {
                mUndoRedoModel->checkpoint( tr("Align Center") );
                mModel->alignSelectionHCenter();
        }


        ///
        /// Objects->Align->Right Action
        ///
        void MainWindow::objectsAlignRight()
        {
                mUndoRedoModel->checkpoint( tr("Align Right") );
                mModel->alignSelectionRight();
        }


        ///
        /// Objects->Align->Top Action
        ///
        void MainWindow::objectsAlignTop()
        {
                mUndoRedoModel->checkpoint( tr("Align Top") );
                mModel->alignSelectionTop();
        }


        ///
        /// Objects->Align->Center Vertically Action
        ///
        void MainWindow::objectsAlignVCenter()
        {
                mUndoRedoModel->checkpoint( tr("Align Middle") );
                mModel->alignSelectionVCenter();
        }


        ///
        /// Objects->Align->Bottom Action
        ///
        void MainWindow::objectsAlignBottom()
        {
                mUndoRedoModel->checkpoint( tr("Align Bottom") );
                mModel->alignSelectionBottom();
        }


        ///
        /// Objects->Center->Horizontally Action
        ///
        void MainWindow::objectsCenterHoriz()
        {
                mUndoRedoModel->checkpoint( tr("Center Horizontally") );
                mModel->centerSelectionHoriz();
        }


        ///
        /// Objects->Put Text on Shape Action
        ///
        /// Fits the selected text object to the selected circle, ellipse, box or
        /// line: the text takes the shape's position, size and rotation and
        /// follows its outline, and the two are grouped so they move together.
        ///
        void MainWindow::objectsPutTextOnShape()
        {
                model::ModelTextObject* text  = nullptr;
                model::ModelObject*     shape = nullptr;
                if ( !findTextAndShape( mModel.get(), text, shape ) )
                {
                        return;
                }

                mUndoRedoModel->checkpoint( tr("Put Text on Shape") );

                if ( dynamic_cast<model::ModelLineObject*>( shape ) )
                {
                        // Straight text standing on the line, reading left to right
                        QPointF start( shape->x0().pt(), shape->y0().pt() );
                        QPointF v = shape->matrix().map( QPointF( shape->w().pt(), shape->h().pt() ) );
                        if ( v.x() < 0 )
                        {
                                start += v;     // keep the text the right way up
                                v = -v;
                        }
                        double length = std::hypot( v.x(), v.y() );
                        double angle  = qRadiansToDegrees( std::atan2( v.y(), v.x() ) );
                        double h      = text->fontSize() * 1.4 + 6;      // one line plus the text margins

                        QTransform m;
                        m.rotate( angle );
                        QPointF corner = m.map( QPointF( 0, h ) );       // bottom-left corner sits on the line start

                        text->setTextPath( model::TextPath::Straight );
                        text->setTextWrapMode( QTextOption::NoWrap );
                        text->setTextVAlign( Qt::AlignBottom );
                        text->setMatrix( m );
                        text->setSize( model::Distance::pt( qMax( 1.0, length ) ), model::Distance::pt( h ) );
                        text->setPosition( model::Distance::pt( start.x() - corner.x() ),
                                           model::Distance::pt( start.y() - corner.y() ) );
                }
                else
                {
                        model::TextPath path = text->textPath();
                        if ( dynamic_cast<model::ModelBoxObject*>( shape ) )
                        {
                                path = model::TextPath::Rectangle;
                        }
                        else if ( path != model::TextPath::ArcTop && path != model::TextPath::ArcBottom &&
                                  path != model::TextPath::Circle )
                        {
                                path = model::TextPath::ArcTop;
                        }

                        text->setTextPath( path );
                        text->setMatrix( shape->matrix() );
                        text->setSize( shape->w(), shape->h() );
                        text->setPosition( shape->x0(), shape->y0() );
                }
                text->setTextHAlign( Qt::AlignHCenter );

                // Group them so they move together (join the shape's group if it has one)
                QString group = shape->group();
                if ( group.isEmpty() )
                {
                        QSet<QString> used;
                        for ( auto* object : mModel->objectList() )
                        {
                                used.insert( object->group() );
                        }
                        group = tr("Text on shape");
                        for ( int n = 2; used.contains( group ); n++ )
                        {
                                group = tr("Text on shape %1").arg( n );
                        }
                }
                mModel->groupObjects( { shape, text }, group );
        }


        ///
        /// Objects->Center Action
        ///
        void MainWindow::objectsCenter()
        {
                mUndoRedoModel->checkpoint( tr("Center") );
                mModel->centerSelection();
        }


        ///
        /// Objects->Center->Vertically Action
        ///
        void MainWindow::objectsCenterVert()
        {
                mUndoRedoModel->checkpoint( tr("Center Vertically") );
                mModel->centerSelectionVert();
        }


        ///
        /// Help->Contents Action
        ///
        ///
        /// Tools > Print Sheet Estimator (for the open label's product, if any)
        ///
        ///
        /// Tools->Insert Symbol Action: show (or raise) the non-modal symbol picker
        ///
        void MainWindow::toolsInsertSymbol()
        {
                if ( !mSymbolsDialog )
                {
                        mSymbolsDialog = new SymbolsDialog( this );
                        connect( mSymbolsDialog, &SymbolsDialog::insertRequested, this, &MainWindow::onInsertSymbol );
                }
                mSymbolsDialog->show();
                mSymbolsDialog->raise();
                mSymbolsDialog->activateWindow();
        }


        ///
        /// Add a symbol from the picker to the label, centered, as an embedded SVG image
        ///
        void MainWindow::onInsertSymbol( const QString& id, const QString& name, const QByteArray& svg, double size )
        {
                if ( !mModel )
                {
                        return;
                }
                if ( mPages->currentIndex() != EDITOR_PAGE_INDEX )
                {
                        fileShowEditorPage();
                }

                // Center on the label; step each new symbol down and right so they don't hide each other
                double step = 3 * ( mSymbolInsertCount++ % 6 ) / 25.4 * 72.0;   // 3 mm
                double x = qMax( 0.0, ( mModel->w().pt() - size ) / 2 ) + step;
                double y = qMax( 0.0, ( mModel->h().pt() - size ) / 2 ) + step;

                mUndoRedoModel->checkpoint( tr("Insert Symbol") );

                auto* object = new model::ModelImageObject( model::Distance::pt( x ), model::Distance::pt( y ),
                                                            model::Distance::pt( size ), model::Distance::pt( size ),
                                                            true, "symbol-" + id + ".svg", svg );
                object->setName( name );
                mModel->addObject( object );
                mModel->unselectAll();
                mModel->selectObject( object );
        }


        void MainWindow::toolsPrintEstimator()
        {
                PrintEstimatorWindow::showWindow( mModel ? mModel->tmplate() : model::Template() );
        }


        void MainWindow::helpContents()
        {
                // Context-sensitive: open the guide at the page you are working on
                QString topic;
                switch ( mPages->currentIndex() )
                {
                case WELCOME_PAGE_INDEX:    topic = "index";      break;
                case EDITOR_PAGE_INDEX:     topic = "objects";    break;
                case PROPERTIES_PAGE_INDEX: topic = "products";   break;
                case MERGE_PAGE_INDEX:      topic = "merge";      break;
                case VARIABLES_PAGE_INDEX:  topic = "variables";  break;
                case PRINT_PAGE_INDEX:      topic = "printing";   break;
                default:                    break;
                }
                Help::displayContents( this, topic );
        }


        ///
        /// Help->Report Bug Action
        ///
        void MainWindow::helpReportBug()
        {
                Help::displayReportBug( this );
        }


        ///
        /// Help->What's New Action
        ///
        void MainWindow::helpWhatsNew()
        {
                WhatsNewDialog dialog( this );
                dialog.exec();
        }


        ///
        /// Help->About Action
        ///
        void MainWindow::helpAbout()
        {
                Help::displayAbout( this );
        }


        ///
        /// Context Menu Activation
        ///
        void MainWindow::onContextMenuActivate( model::Point p )
        {
                // Save click location for potential paste action
                QVariant variant;
                variant.setValue( p );
                contextPasteAction->setData( variant );

                if ( mModel->isSelectionEmpty() )
                {
                        noSelectionContextMenu->popup( QCursor::pos() );
                }
                else
                {
                        contextMenu->popup( QCursor::pos() );
                }
        }


        ///
        /// Zoom changed: update Zoom Information in Status Bar
        ///
        void MainWindow::onZoomChanged()
        {
                QString zoomText = QString( "%1%" ).arg( 100*mLabelEditor->zoom(), 0, 'f', 0 );
                if ( mZoomCombo && !mZoomCombo->lineEdit()->hasFocus() )
                {
                        mZoomCombo->setEditText( zoomText );
                }

                viewZoomInAction->setEnabled( !mLabelEditor->isZoomMax() );
                viewZoomOutAction->setEnabled( !mLabelEditor->isZoomMin() );
        }


        ///
        /// Pointer moved: update Cursor Information in Status Bar
        ///
        void MainWindow::onPointerMoved( model::Point p )
        {
                auto units = model::Settings::units();
                int digits = units.resolutionDigits();
                cursorInfoLabel->setText( QString( "X: %1  Y: %2 %3" )
                                          .arg( p.x().inUnits(units), 0, 'f', digits )
                                          .arg( p.y().inUnits(units), 0, 'f', digits )
                                          .arg( units.toIdString() ) );
        }


        ///
        /// Pointer exited view: update Zoom Information in Status Bar (Clears information)
        ///
        void MainWindow::onPointerExit()
        {
                cursorInfoLabel->setText( "" );
        }


        ///
        /// Name changed handler
        ///
        void MainWindow::onNameChanged()
        {
                setTitle();
        }


        ///
        /// Modified changed handler
        ///
        void MainWindow::onModifiedChanged()
        {
                manageActions();
                setTitle();
        }


        ///
        /// Selection changed handler
        ///
        void MainWindow::onSelectionChanged()
        {
                manageActions();
                syncRotationSpin();
        }


        ///
        /// Label changed handler
        ///
        void MainWindow::onLabelChanged()
        {
                syncRotationSpin();
                syncPageButton();
        }


        ///
        /// Undo/Redo changed handler
        ///
        void MainWindow::onUndoRedoChanged()
        {
                manageActions();
        }


        ///
        /// Settings changed handler
        ///
        void MainWindow::onSettingsChanged()
        {
                mSyncing = true;
                viewGridAction->setChecked( model::Settings::showGrid() );
                viewMarkupAction->setChecked( model::Settings::showMarkup() );
                viewRulersAction->setChecked( model::Settings::showRulers() );
                viewPropertiesUnitsAction->setChecked( model::Settings::propertiesShowUnits() );

                int ref = model::Settings::alignReference();
                for ( auto* a : alignRefGroup->actions() )
                {
                        a->setChecked( a->data().toInt() == ref );
                }
                mAlignRefCombo->setCurrentIndex( mAlignRefCombo->findData( ref ) );
                mSyncing = false;
                syncPageButton();

                mLabelEditor->setGridVisible( model::Settings::showGrid() );
                mLabelEditor->setMarkupVisible( model::Settings::showMarkup() );

                if ( mModel )
                {
                        mModel->setAlignReference( model::Model::AlignReference( ref ) );
                }

                syncUnitsCombo();
                applySidebarStyle();
                updateChromeVisibility();
                manageActions();
        }

} // namespace glabels
