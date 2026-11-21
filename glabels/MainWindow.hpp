//  MainWindow.hpp
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

#ifndef MainWindow_hpp
#define MainWindow_hpp


#include "UndoRedoModel.hpp"

#include <model/Model.hpp>

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSplitter>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

#include <memory>


namespace glabels
{

        // Forward References
        class LabelEditor;
        class MergeView;
        class ObjectEditor;
        class ObjectListView;
        class Ruler;
        class PrintView;
        class PropertiesView;
        class RecentView;
        class StartupView;
        class SymbolsDialog;
        class VariablesPanel;
        class VariablesView;


        ///
        /// MainWindow Widget
        ///
        class MainWindow : public QMainWindow
        {
                Q_OBJECT


                /////////////////////////////////////
                // Lifecycle
                /////////////////////////////////////
        public:
                MainWindow();
                virtual ~MainWindow() = default;


                /////////////////////////////////////
                // Public Methods
                /////////////////////////////////////
        public:
                model::Model* model() const;
                void setModel( model::Model* model );
                bool isEmpty() const;
                void closeDocument();

                void showPrintPage();
                void showRecentPage();
                void fileShowRecentScreen();
                void showWelcomePage();


                /////////////////////////////////////
                // Events
                /////////////////////////////////////
        protected:
                void closeEvent( QCloseEvent *event ) override;
                void showEvent( QShowEvent* event ) override;
                bool eventFilter( QObject* watched, QEvent* event ) override;
                QMenu* createPopupMenu() override;


                /////////////////////////////////////
                // Slots
                /////////////////////////////////////
        private slots:
                void changePage( bool checked );

                void clipboardChanged();

                void fileNew();
                void fileNewProject();
                void fileNewFromSample();
                void fileOpen();
                void fileOpenRecent();
                void fileSave();
                void fileSaveAs();
                void fileShowEditorPage();
                void fileShowPropertiesPage();
                void fileShowMergePage();
                void fileShowVariablesPage();
                void fileShowPrintPage();
                void fileTemplateDesigner();
                void fileClose();
                void fileExit();

                void toolsPrintEstimator();

                void editUndo();
                void editRedo();
                void editCut();
                void editCopy();
                void editPaste();
                void editContextPaste();
                void editDelete();
                void editSelectAll();
                void editUnSelectAll();
                void editPreferences();
                void editShortcuts();

                void viewFileToolBar( bool );
                void viewEditorToolBar( bool );
                void viewGrid( bool );
                void viewMarkup( bool );
                void viewRulers( bool );
                void viewPropertiesUnits( bool );
                void viewToolBarToggled( bool );
                void viewDockToggled( bool );
                void onZoomComboActivated();
                void onUnitsComboChanged( int index );
                void onPageButtonClicked();
                void onAlignReferenceChanged( int index );
                void onRotationSpinChanged( double value );
                void onSidebarContextMenu( const QPoint& pos );
                void viewZoomIn();
                void viewZoomOut();
                void viewZoom1To1();
                void viewZoomToFit();

                void objectsArrowMode();
                void objectsMeasureMode();
                void objectsCreateText();
                void objectsCreateBox();
                void objectsCreateLine();
                void objectsCreateEllipse();
                void objectsCreateImage();
                void objectsCreateBarcode();
                void objectsOrderRaise();
                void objectsOrderLower();
                void objectsOrderRaiseOne();
                void objectsOrderLowerOne();
                void objectsEditText();
                void mergePreviewToggled( bool );
                void mergePreviewFirst();
                void mergePreviewPrevious();
                void mergePreviewNext();
                void mergePreviewLast();
                void onMergeRecordSpinChanged( int value );
                void onPreviewRecordChanged();
                void dataConnectSource();
                void dataReload();
                void dataShowPlaceholders();
                void onAddVariableRequested( const QString& name, const QString& type );
                void objectsXformRotateLeft();
                void objectsXformRotateRight();
                void objectsXformFlipHoriz();
                void objectsXformFlipVert();
                void objectsAlignLeft();
                void objectsAlignHCenter();
                void objectsAlignRight();
                void objectsAlignTop();
                void objectsAlignVCenter();
                void objectsAlignBottom();
                void objectsCenter();
                void objectsPutTextOnShape();
                void toolsInsertSymbol();
                void onInsertSymbol( const QString& id, const QString& name, const QByteArray& svg, double size );

                void objectsCenterHoriz();
                void objectsCenterVert();

                void helpContents();
                void helpReportBug();
                void helpWhatsNew();
                void helpAbout();

                void onContextMenuActivate( model::Point );

                void onZoomChanged();
                void onOrientationButtonClicked();
                void updateOrientationButton();
                void onPointerMoved( model::Point );
                void onPointerExit();

                void onNameChanged();
                void onModifiedChanged();
                void onSelectionChanged();
                void onLabelChanged();
                void onUndoRedoChanged();

                void onSettingsChanged();


                /////////////////////////////////////
                // Internal Private Methods
                /////////////////////////////////////
        private:
                void createActions();
                void createMenus();
                void createToolBars();
                void createStatusBar();
                void createDocks();
                void registerShortcuts();
                void updateShortcutToolTips();
                void applySidebarStyle();
                void updateChromeVisibility();
                void syncUnitsCombo();
                void syncPageButton();
                void syncRotationSpin();

                QWidget* createWelcomePage();
                QWidget* createEditorPage();
                QWidget* createPropertiesPage();
                QWidget* createMergePage();
                QWidget* createVariablesPage();
                QWidget* createPrintPage();

                void manageActions();

                void setTitle();

                void readSettings();
                void writeSettings();

                bool isOkToClose();


                /////////////////////////////////////
                // Private Data
                // owned and managed by us
                /////////////////////////////////////
        private:
                std::unique_ptr<model::Model>  mModel;
                std::unique_ptr<UndoRedoModel> mUndoRedoModel;


                /////////////////////////////////////
                // Private Data
                // owned by QMainWindow
                /////////////////////////////////////
                QMenu*    fileMenu;
                QAction*  fileOpenRecentAction;
                QMenu*    editMenu;
                QMenu*    viewMenu;
                QMenu*    viewToolBarsMenu;
                QMenu*    objectsMenu;
                QMenu*    objectsCreateMenu;
                QMenu*    objectsOrderMenu;
                QMenu*    objectsXformMenu;
                QMenu*    objectsAlignMenu;
                QMenu*    objectsCenterMenu;
                QMenu*    toolsMenu;
                QMenu*    helpMenu;

                QMenu*    contextMenu;
                QMenu*    contextOrderMenu;
                QMenu*    contextXformMenu;
                QMenu*    contextAlignMenu;
                QMenu*    contextCenterMenu;
                QMenu*    noSelectionContextMenu;

                QToolBar* fileToolBar;
                QToolBar* editorToolBar;
                QToolBar* viewToolBar;
                QToolBar* arrangeToolBar;
                QToolBar* mergeToolBar;

                QDockWidget*    mPropertiesDock{ nullptr };
                QDockWidget*    mObjectListDock{ nullptr };
                ObjectListView* mObjectListView{ nullptr };
                QDockWidget*    mVariablesDock{ nullptr };
                VariablesPanel* mVariablesPanel{ nullptr };

                // Bump when the default toolbar/dock arrangement changes, so stale layouts are ignored
                static const int STATE_VERSION = 2;
                QWidget*        mRulerCorner{ nullptr };
                Ruler*          mHRuler{ nullptr };
                Ruler*          mVRuler{ nullptr };

                QComboBox*      mZoomCombo;
                QComboBox*      mUnitsCombo;
                QToolButton*    mPageButton{ nullptr };
                QComboBox*      mAlignRefCombo;
                QDoubleSpinBox* mRotationSpin;
                QSpinBox*       mMergeRecordSpin;
                QLabel*         mMergeRecordCountLabel;
                bool            mSyncing{ false };
                bool            mMaximizeChecked{ false };
                bool            mCloseConfirmed{ false };   // save prompt already answered

                QToolBar*            mContents;
                QToolButton*         mWelcomeButton{ nullptr };
                QToolButton*         mEditorButton{ nullptr };
                QToolButton*         mPropertiesButton;
                QToolButton*         mOrientationButton{ nullptr };
                QToolButton*         mMergeButton;
                QToolButton*         mVariablesButton;
                QToolButton*         mPrintButton;
                QToolButton*         mRecentButton{ nullptr };

                QAction*             mWelcomeAction;
                QAction*             mEditorAction;
                QAction*             mPropertiesAction;
                QAction*             mOrientationAction{ nullptr };
                QAction*             mMergeAction;
                QAction*             mVariablesAction;
                QAction*             mPrintAction;
                QAction*             mRecentAction{ nullptr };

                QStackedWidget*      mPages;
                StartupView*         mWelcomeView;
                RecentView*          mRecentView{ nullptr };
                QScrollArea*         mLabelEditorScrollArea;
                LabelEditor*         mLabelEditor;
                ObjectEditor*        mObjectEditor;
                PropertiesView*      mPropertiesView;
                MergeView*           mMergeView;
                VariablesView*       mVariablesView;
                PrintView*           mPrintView;

                QLabel*   cursorInfoLabel;

                QAction*  fileNewAction;
                QAction*  fileNewProjectAction;
                QAction*  fileNewFromSampleAction;
                QAction*  fileSaveAsTemplateAction;
                QAction*  fileOpenAction;
                QAction*  fileSaveAction;
                QAction*  fileSaveAsAction;
                QAction*  fileShowEditorPageAction;
                QAction*  fileShowPropertiesPageAction;
                QAction*  fileShowMergePageAction;
                QAction*  fileShowVariablesPageAction;
                QAction*  fileShowPrintPageAction;
                QAction*  fileTemplateDesignerAction;
                QAction*  fileCloseAction;
                QAction*  fileExitAction;

                QList<QAction*>  fileRecentActionList;

                QAction*  editUndoAction;
                QAction*  editRedoAction;
                QAction*  editCutAction;
                QAction*  editCopyAction;
                QAction*  editPasteAction;
                QAction*  editDeleteAction;
                QAction*  editDeleteQuickAction;
                QAction*  editSelectAllAction;
                QAction*  editUnSelectAllAction;
                QAction*  editPageSetupAction;
                QAction*  editPreferencesAction;
                QAction*  editShortcutsAction;

                QAction*  viewFileToolBarAction;
                QAction*  viewEditorToolBarAction;
                QAction*  viewGridAction;
                QAction*  viewMarkupAction;
                QAction*  viewRulersAction;
                QAction*  viewPropertiesUnitsAction;
                QAction*  viewViewToolBarAction;
                QAction*  viewArrangeToolBarAction;
                QAction*  viewMergeToolBarAction;
                QAction*  viewPropertiesDockAction;
                QAction*  viewObjectListDockAction;
                QAction*  viewZoomInAction;
                QAction*  viewZoomOutAction;
                QAction*  viewZoom1To1Action;
                QAction*  viewZoomToFitAction;

                QAction*  objectsArrowModeAction;
                QAction*  objectsMeasureModeAction;
                QAction*  objectsCreateTextAction;
                QAction*  objectsCreateBoxAction;
                QAction*  objectsCreateLineAction;
                QAction*  objectsCreateEllipseAction;
                QAction*  objectsCreateImageAction;
                QAction*  objectsCreateBarcodeAction;
                QAction*  objectsOrderRaiseAction;
                QAction*  objectsOrderLowerAction;
                QAction*  objectsOrderRaiseOneAction;
                QAction*  objectsOrderLowerOneAction;
                QAction*  objectsEditTextAction;
                QAction*  mergePreviewAction;
                QAction*  mergePlaceholdersAction;
                QAction*  dataConnectAction;
                QAction*  dataReloadAction;
                QAction*  viewVariablesDockAction;
                QMenu*    dataMenu;
                QAction*  mergeFirstAction;
                QAction*  mergePreviousAction;
                QAction*  mergeNextAction;
                QAction*  mergeLastAction;
                QActionGroup* alignRefGroup;
                QAction*  alignRefSelectionAction;
                QAction*  alignRefKeyAction;
                QAction*  alignRefLabelAction;
                QAction*  objectsXformRotateLeftAction;
                QAction*  objectsXformRotateRightAction;
                QAction*  objectsXformFlipHorizAction;
                QAction*  objectsXformFlipVertAction;
                QAction*  objectsAlignLeftAction;
                QAction*  objectsAlignHCenterAction;
                QAction*  objectsAlignRightAction;
                QAction*  objectsAlignTopAction;
                QAction*  objectsAlignVCenterAction;
                QAction*  objectsAlignBottomAction;
                QAction*  objectsCenterAction;
                QAction*  objectsPutTextOnShapeAction;
                QAction*  toolsInsertSymbolAction;
                SymbolsDialog* mSymbolsDialog = nullptr;
                int       mSymbolInsertCount = 0;
                QAction*  objectsCenterHorizAction;
                QAction*  objectsCenterVertAction;

                QAction*  helpContentsAction;
                QAction*  helpReportBugAction;
                QAction*  helpWhatsNewAction;
                QAction*  helpAboutAction;

                QAction*  toolsPrintEstimatorAction;

                QAction*  contextCutAction;
                QAction*  contextCopyAction;
                QAction*  contextPasteAction;
                QAction*  contextDeleteAction;
        };


}


#endif // MainWindow_hpp
