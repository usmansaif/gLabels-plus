//  PreferencesDialog.cpp
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


#include "PreferencesDialog.hpp"

#include "AppearanceModel.hpp"
#include "DialogUtil.hpp"
#include "ShortcutsDialog.hpp"

#include "model/Distance.hpp"
#include "model/Model.hpp"
#include "model/Settings.hpp"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QStandardPaths>


namespace
{
        const char* CARD_STYLE =
                "QGroupBox#card {"
                "  font-weight: bold;"
                "  border: 1px solid palette(mid);"
                "  border-radius: 6px;"
                "  margin-top: 1.1em;"
                "  padding: 10px 10px 8px 10px;"
                "}"
                "QGroupBox#card::title {"
                "  subcontrol-origin: margin;"
                "  left: 10px;"
                "  padding: 0 4px;"
                "}"
                "QGroupBox#card QLabel, QGroupBox#card QCheckBox, QGroupBox#card QRadioButton {"
                "  font-weight: normal;"
                "}";
}


namespace glabels
{

        ///
        /// Constructor
        ///
        PreferencesDialog::PreferencesDialog( QWidget *parent )
                : QDialog(parent)
        {
                setWindowTitle( tr("gLabels Plus - Preferences") );
                setObjectName( "PreferencesDialog" );
                DialogUtil::makeResizableWindow( this );
                setStyleSheet( CARD_STYLE );
                resize( 820, 560 );

                // Left side: search + categories
                mSearchEdit = new QLineEdit();
                mSearchEdit->setObjectName( "searchEdit" );
                mSearchEdit->setPlaceholderText( tr("Find a setting...") );
                mSearchEdit->setClearButtonEnabled( true );
                mSearchEdit->setToolTip( tr("Type a word such as \"grid\", \"dark\" or \"mm\" to find the right page") );

                mCategoryList = new QListWidget();
                mCategoryList->setIconSize( QSize( 22, 22 ) );
                mCategoryList->setSpacing( 2 );
                mCategoryList->setUniformItemSizes( true );
                mCategoryList->setFrameShape( QFrame::NoFrame );
                mCategoryList->setStyleSheet( "QListWidget::item { padding: 6px 4px; }" );

                auto* left = new QWidget();
                auto* leftLayout = new QVBoxLayout( left );
                leftLayout->setContentsMargins( 0, 0, 0, 0 );
                leftLayout->addWidget( mSearchEdit );
                leftLayout->addWidget( mCategoryList, 1 );
                left->setMinimumWidth( 170 );

                // Right side: pages
                mPages = new QStackedWidget();

                auto* splitter = new QSplitter( Qt::Horizontal );
                splitter->setChildrenCollapsible( false );
                splitter->addWidget( left );
                splitter->addWidget( mPages );
                splitter->setStretchFactor( 0, 0 );
                splitter->setStretchFactor( 1, 1 );
                splitter->setSizes( { 200, 620 } );

                // Bottom buttons
                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close | QDialogButtonBox::RestoreDefaults );
                buttons->button( QDialogButtonBox::RestoreDefaults )->setToolTip( tr("Reset all preferences to their original values") );
                auto* note = new QLabel( tr("Changes are applied immediately.") );
                note->setForegroundRole( QPalette::PlaceholderText );

                auto* bottom = new QHBoxLayout();
                bottom->addWidget( note, 1 );
                bottom->addWidget( buttons );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( splitter, 1 );
                layout->addLayout( bottom );

                // Build pages
                buildGeneralPage();
                buildUnitsPage();
                buildEditorPage();
                buildAppearancePage();
                buildLayoutPage();
                buildPrintingPage();
                buildKeyboardPage();

                loadFromSettings();

                connect( mCategoryList, &QListWidget::currentRowChanged, mPages, &QStackedWidget::setCurrentIndex );
                connect( mSearchEdit, &QLineEdit::textChanged, this, &PreferencesDialog::onSearchTextChanged );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
                connect( buttons->button( QDialogButtonBox::Close ), &QPushButton::clicked, this, &QDialog::accept );
                connect( buttons->button( QDialogButtonBox::RestoreDefaults ), &QPushButton::clicked,
                         this, &PreferencesDialog::onRestoreDefaultsClicked );
                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );

                mCategoryList->setCurrentRow( 0 );
        }


        ///
        /// Add a page with a header; returns the page's content widget
        ///
        QWidget* PreferencesDialog::addPage( const QString& iconName, const QString& title,
                                             const QString& description, const QStringList& keywords )
        {
                auto* item = new QListWidgetItem( QIcon::fromTheme( iconName ), title );
                item->setToolTip( description );
                mCategoryList->addItem( item );
                mPageKeywords.append( keywords + QStringList{ title, description } );

                auto* content = new QWidget();
                auto* contentLayout = new QVBoxLayout( content );
                contentLayout->setContentsMargins( 12, 4, 12, 12 );
                contentLayout->setSpacing( 10 );

                auto* titleLabel = new QLabel( title );
                QFont f = titleLabel->font();
                f.setPointSizeF( f.pointSizeF() * 1.5 );
                f.setBold( true );
                titleLabel->setFont( f );
                contentLayout->addWidget( titleLabel );

                auto* descLabel = new QLabel( description );
                descLabel->setWordWrap( true );
                descLabel->setForegroundRole( QPalette::PlaceholderText );
                contentLayout->addWidget( descLabel );

                auto* scroll = new QScrollArea();
                scroll->setWidgetResizable( true );
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidget( content );
                mPages->addWidget( scroll );

                return content;
        }


        ///
        /// Add a titled "card" (group box) to a page
        ///
        QGroupBox* PreferencesDialog::addCard( QWidget* page, const QString& title )
        {
                auto* card = new QGroupBox( title );
                card->setObjectName( "card" );
                auto* cardLayout = new QVBoxLayout( card );
                cardLayout->setSpacing( 6 );
                static_cast<QVBoxLayout*>( page->layout() )->addWidget( card );
                return card;
        }


        ///
        /// Add a small explanatory text line
        ///
        QLabel* PreferencesDialog::addHelp( QLayout* layout, const QString& text )
        {
                auto* label = new QLabel( text );
                label->setWordWrap( true );
                label->setForegroundRole( QPalette::PlaceholderText );
                QFont f = label->font();
                f.setPointSizeF( f.pointSizeF() * 0.92 );
                label->setFont( f );
                layout->addWidget( label );
                return label;
        }


        ///
        /// Add radio button with optional help text indented below it
        ///
        QRadioButton* PreferencesDialog::addRadio( QLayout* layout, QButtonGroup* group,
                                                   const QString& text, const QString& help )
        {
                auto* radio = new QRadioButton( text );
                group->addButton( radio );
                layout->addWidget( radio );
                if ( !help.isEmpty() )
                {
                        radio->setToolTip( help );
                        auto* helpLayout = new QHBoxLayout();
                        helpLayout->setContentsMargins( 24, 0, 0, 4 );
                        addHelp( helpLayout, help );
                        static_cast<QBoxLayout*>( layout )->addLayout( helpLayout );
                }
                return radio;
        }


        ///
        /// General page
        ///
        void PreferencesDialog::buildGeneralPage()
        {
                auto* page = addPage( "glabels-general", tr("General"),
                                      tr("Where gLabels Plus starts looking for files, and your recent files list."),
                                      { "startup", "folder", "directory", "home", "documents", "recent", "files", "open" } );

                auto* card = addCard( page, tr("Startup folder") );
                auto* l = card->layout();
                addHelp( l, tr("The folder shown first when you open or save a project.") );

                mStartupGroup = new QButtonGroup( this );
                mStartupParentRadio    = addRadio( l, mStartupGroup, tr("Folder gLabels Plus was started from"),
                                                   tr("Inherit the working folder of the program or shortcut that launched gLabels Plus.") );
                mStartupHomeRadio      = addRadio( l, mStartupGroup, tr("My home folder") );
                mStartupDocumentsRadio = addRadio( l, mStartupGroup, tr("My Documents folder") );
                mStartupOtherRadio     = addRadio( l, mStartupGroup, tr("Another folder:") );

                mStartupOtherEdit = new QLineEdit();
                mStartupOtherEdit->setReadOnly( true );
                mStartupBrowseButton = new QPushButton( tr("Browse...") );
                auto* row = new QHBoxLayout();
                row->setContentsMargins( 24, 0, 0, 0 );
                row->addWidget( mStartupOtherEdit, 1 );
                row->addWidget( mStartupBrowseButton );
                static_cast<QBoxLayout*>( l )->addLayout( row );

                connect( mStartupGroup, &QButtonGroup::buttonClicked, this, &PreferencesDialog::onStartupLocationChanged );
                connect( mStartupBrowseButton, &QPushButton::clicked, this, &PreferencesDialog::onStartupOtherBrowseButtonClicked );

                auto* recentCard = addCard( page, tr("Recent files") );
                addHelp( recentCard->layout(), tr("gLabels Plus remembers the last %1 projects you opened (File > Open Recent).")
                                               .arg( model::Settings::maxRecentFiles() ) );
                mClearRecentButton = new QPushButton( tr("Clear Recent Files List") );
                auto* recentRow = new QHBoxLayout();
                recentRow->addWidget( mClearRecentButton );
                recentRow->addStretch( 1 );
                static_cast<QBoxLayout*>( recentCard->layout() )->addLayout( recentRow );
                connect( mClearRecentButton, &QPushButton::clicked, this, &PreferencesDialog::onClearRecentFilesClicked );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Units page
        ///
        void PreferencesDialog::buildUnitsPage()
        {
                auto* page = addPage( "glabels-rulers", tr("Units"),
                                      tr("Choose how sizes and positions are measured and displayed everywhere in gLabels Plus."),
                                      { "units", "inches", "millimeters", "mm", "cm", "centimeters", "points", "picas",
                                        "measurement", "paper", "a4", "letter", "iso", "us" } );

                auto* card = addCard( page, tr("Measurement units") );
                auto* l = card->layout();

                mUnitsGroup = new QButtonGroup( this );
                struct U { model::Units::Enum e; QString text; QString help; };
                const QList<U> units = {
                        { model::Units::MM, tr("Millimeters (mm)"), tr("Metric. Common for A4 sheets and most of the world.") },
                        { model::Units::CM, tr("Centimeters (cm)"), tr("Metric, in larger steps.") },
                        { model::Units::IN, tr("Inches (in)"),      tr("Imperial. Common for US Letter sheets.") },
                        { model::Units::PT, tr("Points (pt)"),      tr("Typographic unit: 72 points = 1 inch.") },
                        { model::Units::PC, tr("Picas (pc)"),       tr("Typographic unit: 6 picas = 1 inch.") },
                };
                for ( auto& u : units )
                {
                        auto* radio = addRadio( l, mUnitsGroup, u.text, u.help );
                        mUnitsGroup->setId( radio, int(u.e) );
                }

                mUnitsExampleLabel = new QLabel();
                mUnitsExampleLabel->setWordWrap( true );
                l->addWidget( mUnitsExampleLabel );

                connect( mUnitsGroup, &QButtonGroup::idClicked, this, &PreferencesDialog::onUnitsChanged );

                auto* dispCard = addCard( page, tr("Display") );
                mPropertiesUnitsCheck = new QCheckBox( tr("Show unit names next to values in the Object Properties panel") );
                dispCard->layout()->addWidget( mPropertiesUnitsCheck );
                addHelp( dispCard->layout(), tr("Example: \"25.40 mm\" instead of \"25.40\". Turn off for a more compact panel.") );
                connect( mPropertiesUnitsCheck, &QCheckBox::toggled, this, []( bool v ) {
                        model::Settings::setPropertiesShowUnits( v );
                } );

                auto* paperCard = addCard( page, tr("Paper") );
                auto* form = new QFormLayout();
                mPageFamilyCombo = new QComboBox();
                mPageFamilyCombo->addItem( tr("ISO (A4, A5, ...)"),         int(model::Settings::ISO) );
                mPageFamilyCombo->addItem( tr("US (Letter, Legal, ...)"),   int(model::Settings::US) );
                form->addRow( tr("Preferred paper sizes:"), mPageFamilyCombo );
                static_cast<QBoxLayout*>( paperCard->layout() )->addLayout( form );
                addHelp( paperCard->layout(), tr("Used as the default when designing your own product templates.") );
                connect( mPageFamilyCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        model::Settings::setPreferedPageSizeFamily(
                                model::Settings::PageSizeFamily( mPageFamilyCombo->itemData( i ).toInt() ) );
                } );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Editor page
        ///
        void PreferencesDialog::buildEditorPage()
        {
                auto* page = addPage( "glabels-edit", tr("Editor"),
                                      tr("How the label canvas looks and behaves while you design."),
                                      { "grid", "spacing", "origin", "markup", "rulers", "align", "alignment", "canvas", "snap" } );

                auto* viewCard = addCard( page, tr("Canvas guides") );
                mShowGridCheck   = new QCheckBox( tr("Show grid") );
                mShowMarkupCheck = new QCheckBox( tr("Show markup (safe area and margin lines)") );
                mShowRulersCheck = new QCheckBox( tr("Show rulers") );
                viewCard->layout()->addWidget( mShowGridCheck );
                viewCard->layout()->addWidget( mShowMarkupCheck );
                viewCard->layout()->addWidget( mShowRulersCheck );
                addHelp( viewCard->layout(), tr("You can also toggle these from the View menu or the View toolbar.") );
                connect( mShowGridCheck,   &QCheckBox::toggled, this, []( bool v ) { model::Settings::setShowGrid( v ); } );
                connect( mShowMarkupCheck, &QCheckBox::toggled, this, []( bool v ) { model::Settings::setShowMarkup( v ); } );
                connect( mShowRulersCheck, &QCheckBox::toggled, this, []( bool v ) { model::Settings::setShowRulers( v ); } );

                auto* gridCard = addCard( page, tr("Grid") );
                auto* form = new QFormLayout();
                mGridSpacingSpin = new QDoubleSpinBox();
                mGridSpacingSpin->setMaximum( 1000 );
                auto* resetButton = new QPushButton( tr("Default") );
                resetButton->setToolTip( tr("Restore the default grid spacing") );
                auto* spacingRow = new QHBoxLayout();
                spacingRow->addWidget( mGridSpacingSpin );
                spacingRow->addWidget( resetButton );
                spacingRow->addStretch( 1 );
                form->addRow( tr("Spacing:"), spacingRow );
                static_cast<QBoxLayout*>( gridCard->layout() )->addLayout( form );

                auto* originLabel = new QLabel( tr("Grid starts at:") );
                gridCard->layout()->addWidget( originLabel );
                mGridOriginGroup = new QButtonGroup( this );
                auto* tl = addRadio( gridCard->layout(), mGridOriginGroup, tr("Top-left corner of the label") );
                auto* c  = addRadio( gridCard->layout(), mGridOriginGroup, tr("Center of the label"),
                                     tr("Round, elliptical and CD labels always use the center.") );
                mGridOriginGroup->setId( tl, int(model::Settings::ORIGIN_TL) );
                mGridOriginGroup->setId( c,  int(model::Settings::ORIGIN_CENTER) );

                connect( mGridSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         this, &PreferencesDialog::onGridSpacingSpinChanged );
                connect( resetButton, &QPushButton::clicked, this, &PreferencesDialog::onGridSpacingResetButtonClicked );
                connect( mGridOriginGroup, &QButtonGroup::idClicked, this, &PreferencesDialog::onGridOriginChanged );

                auto* alignCard = addCard( page, tr("Alignment") );
                auto* alignForm = new QFormLayout();
                mAlignRefCombo = new QComboBox();
                mAlignRefCombo->addItem( tr("The whole selection"),      int(model::Model::ALIGN_TO_SELECTION) );
                mAlignRefCombo->addItem( tr("The last selected object"), int(model::Model::ALIGN_TO_KEY_OBJECT) );
                mAlignRefCombo->addItem( tr("The label"),                int(model::Model::ALIGN_TO_LABEL) );
                alignForm->addRow( tr("Align objects relative to:"), mAlignRefCombo );
                static_cast<QBoxLayout*>( alignCard->layout() )->addLayout( alignForm );
                addHelp( alignCard->layout(), tr("\"The label\" lets you align even a single object to an edge or the center of the label.") );
                connect( mAlignRefCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        model::Settings::setAlignReference( mAlignRefCombo->itemData( i ).toInt() );
                } );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Appearance page
        ///
        void PreferencesDialog::buildAppearancePage()
        {
                auto* page = addPage( "glabels-appearance", tr("Appearance"),
                                      tr("Light or dark colors, and the style of icons."),
                                      { "theme", "dark", "light", "color", "colour", "scheme", "icons", "flat", "vector", "system" } );

                auto* schemeCard = addCard( page, tr("Color scheme") );
                mColorSchemeGroup = new QButtonGroup( this );
                auto* light = addRadio( schemeCard->layout(), mColorSchemeGroup, tr("Light"),
                                        tr("Dark text on a light background.") );
                auto* dark  = addRadio( schemeCard->layout(), mColorSchemeGroup, tr("Dark"),
                                        tr("Light text on a dark background; easier on the eyes in dim rooms.") );
                mSystemSchemeRadio = addRadio( schemeCard->layout(), mColorSchemeGroup, tr("Same as my system"),
                                               tr("Follow the operating system's light/dark setting automatically.") );
                mColorSchemeGroup->setId( light, int(model::Settings::LIGHT_COLOR_SCHEME) );
                mColorSchemeGroup->setId( dark,  int(model::Settings::DARK_COLOR_SCHEME) );
                mColorSchemeGroup->setId( mSystemSchemeRadio, int(model::Settings::SYSTEM_COLOR_SCHEME) );
                if ( !AppearanceModel::supportsSystemColorScheme() )
                {
                        mSystemSchemeRadio->setEnabled( false );
                        mSystemSchemeRadio->setToolTip( tr("Requires a newer version of Qt.") );
                }

                auto* iconCard = addCard( page, tr("Icon style") );
                mIconStyleGroup = new QButtonGroup( this );
                auto* flat   = addRadio( iconCard->layout(), mIconStyleGroup, tr("Flat (colorful)") );
                auto* vector = addRadio( iconCard->layout(), mIconStyleGroup, tr("Vector (monochrome outlines)") );
                mIconStyleGroup->setId( flat,   int(model::Settings::FLAT_ICON_STYLE) );
                mIconStyleGroup->setId( vector, int(model::Settings::VECTOR_ICON_STYLE) );

                connect( mColorSchemeGroup, &QButtonGroup::idClicked, this, &PreferencesDialog::onAppearanceChanged );
                connect( mIconStyleGroup,   &QButtonGroup::idClicked, this, &PreferencesDialog::onAppearanceChanged );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Window layout page
        ///
        void PreferencesDialog::buildLayoutPage()
        {
                auto* page = addPage( "glabels-view-grid", tr("Window Layout"),
                                      tr("Make more room for your label by adjusting the page buttons and toolbars."),
                                      { "sidebar", "toolbar", "buttons", "text", "icons", "size", "small", "large", "layout", "panel" } );

                auto* sidebarCard = addCard( page, tr("Page buttons") );
                auto* form = new QFormLayout();
                mSidebarStyleCombo = new QComboBox();
                mSidebarStyleCombo->addItem( tr("Icons and text"), int(model::Settings::SIDEBAR_ICONS_AND_TEXT) );
                mSidebarStyleCombo->addItem( tr("Icons only"),     int(model::Settings::SIDEBAR_ICONS_ONLY) );
                mSidebarStyleCombo->addItem( tr("Text only"),      int(model::Settings::SIDEBAR_TEXT_ONLY) );
                mSidebarIconSizeCombo = new QComboBox();
                mSidebarIconSizeCombo->addItem( tr("Small"),  16 );
                mSidebarIconSizeCombo->addItem( tr("Medium"), 22 );
                mSidebarIconSizeCombo->addItem( tr("Large"),  32 );
                mSidebarIconSizeCombo->addItem( tr("Extra large"), 48 );
                form->addRow( tr("Show:"), mSidebarStyleCombo );
                form->addRow( tr("Icon size:"), mSidebarIconSizeCombo );
                static_cast<QBoxLayout*>( sidebarCard->layout() )->addLayout( form );
                addHelp( sidebarCard->layout(), tr("The page buttons (Edit, Properties, Merge, Variables, Print) are at the bottom left. "
                                                   "Right-click them for these options. Toolbars can be dragged to any edge of the window.") );
                connect( mSidebarStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        model::Settings::setSidebarStyle(
                                model::Settings::SidebarStyle( mSidebarStyleCombo->itemData( i ).toInt() ) );
                } );
                connect( mSidebarIconSizeCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        model::Settings::setSidebarIconSize( mSidebarIconSizeCombo->itemData( i ).toInt() );
                } );

                auto* toolbarCard = addCard( page, tr("Toolbars") );
                auto* tform = new QFormLayout();
                mToolbarStyleCombo = new QComboBox();
                mToolbarStyleCombo->addItem( tr("Icons only"),              int(Qt::ToolButtonIconOnly) );
                mToolbarStyleCombo->addItem( tr("Icons with text beside"),  int(Qt::ToolButtonTextBesideIcon) );
                mToolbarStyleCombo->addItem( tr("Icons with text below"),   int(Qt::ToolButtonTextUnderIcon) );
                mToolbarStyleCombo->addItem( tr("Text only"),               int(Qt::ToolButtonTextOnly) );
                tform->addRow( tr("Toolbar buttons:"), mToolbarStyleCombo );
                static_cast<QBoxLayout*>( toolbarCard->layout() )->addLayout( tform );
                addHelp( toolbarCard->layout(), tr("Showing text makes buttons easier to recognize while you learn gLabels Plus. "
                                                   "Choose which toolbars and panels are visible from View > Toolbars and View > Panels.") );
                connect( mToolbarStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this]( int i ) {
                        model::Settings::setToolbarButtonStyle( Qt::ToolButtonStyle( mToolbarStyleCombo->itemData( i ).toInt() ) );
                } );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Printing page
        ///
        void PreferencesDialog::buildPrintingPage()
        {
                auto* page = addPage( "glabels-print", tr("Printing"),
                                      tr("Default options for the Print page."),
                                      { "print", "outlines", "crop", "marks", "printer", "pdf" } );

                auto* card = addCard( page, tr("Print options") );
                mPrintOutlinesCheck  = new QCheckBox( tr("Print label outlines") );
                mPrintCropMarksCheck = new QCheckBox( tr("Print crop marks") );
                card->layout()->addWidget( mPrintOutlinesCheck );
                addHelp( card->layout(), tr("Draws the edge of every label. Useful for test prints on plain paper.") );
                card->layout()->addWidget( mPrintCropMarksCheck );
                addHelp( card->layout(), tr("Draws small marks at the page edges showing where to cut.") );
                connect( mPrintOutlinesCheck,  &QCheckBox::toggled, this, []( bool v ) { model::Settings::setPrintOutlines( v ); } );
                connect( mPrintCropMarksCheck, &QCheckBox::toggled, this, []( bool v ) { model::Settings::setPrintCropMarks( v ); } );

                auto* tipCard = addCard( page, tr("Tip") );
                addHelp( tipCard->layout(), tr("On the Print page you can click a label in the preview to choose where printing "
                                               "starts, which is handy for partially used sheets. Shift+click sets the last label.") );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Keyboard page
        ///
        void PreferencesDialog::buildKeyboardPage()
        {
                auto* page = addPage( "glabels-keyboard", tr("Keyboard"),
                                      tr("Customize keyboard shortcuts for menu commands."),
                                      { "keyboard", "shortcuts", "keys", "hotkeys", "accelerators" } );

                auto* card = addCard( page, tr("Keyboard shortcuts") );
                addHelp( card->layout(), tr("Assign your own key combinations to commands, or restore the defaults.") );
                auto* button = new QPushButton( QIcon::fromTheme( "glabels-keyboard" ), tr("Customize Shortcuts...") );
                auto* row = new QHBoxLayout();
                row->addWidget( button );
                row->addStretch( 1 );
                static_cast<QBoxLayout*>( card->layout() )->addLayout( row );
                connect( button, &QPushButton::clicked, this, &PreferencesDialog::onShortcutsButtonClicked );

                auto* tipsCard = addCard( page, tr("Handy keys in the editor") );
                addHelp( tipsCard->layout(),
                         tr("Arrow keys: nudge selected objects (hold Shift for bigger steps)\n"
                            "F2 or Enter: edit the selected text or barcode in place\n"
                            "Double-click: edit text or barcode in place\n"
                            "Esc: deselect all\n"
                            "Ctrl+] / Ctrl+[: bring forward / send backward") );

                static_cast<QVBoxLayout*>( page->layout() )->addStretch( 1 );
        }


        ///
        /// Load all controls from current settings
        ///
        void PreferencesDialog::loadFromSettings()
        {
                mBlocked = true;

                // Units
                auto units = model::Settings::units();
                if ( auto* b = mUnitsGroup->button( int(units.toEnum()) ) )
                {
                        b->setChecked( true );
                }
                mPropertiesUnitsCheck->setChecked( model::Settings::propertiesShowUnits() );
                mPageFamilyCombo->setCurrentIndex( mPageFamilyCombo->findData( int(model::Settings::preferedPageSizeFamily()) ) );
                updateUnitsExample();

                // Editor
                mShowGridCheck->setChecked( model::Settings::showGrid() );
                mShowMarkupCheck->setChecked( model::Settings::showMarkup() );
                mShowRulersCheck->setChecked( model::Settings::showRulers() );
                if ( auto* b = mGridOriginGroup->button( int(model::Settings::gridOrigin()) ) )
                {
                        b->setChecked( true );
                }
                auto gridSpacing = model::Settings::gridSpacing();
                mGridSpacingSpin->blockSignals( true );
                mGridSpacingSpin->setDecimals( units.resolutionDigits() );
                mGridSpacingSpin->setSingleStep( units.resolution() );
                mGridSpacingSpin->setMinimum( units.resolution() );
                mGridSpacingSpin->setSuffix( " " + units.toIdString() );
                mGridSpacingSpin->setValue( gridSpacing.inUnits( units ) );
                mGridSpacingSpin->blockSignals( false );
                mAlignRefCombo->setCurrentIndex( mAlignRefCombo->findData( model::Settings::alignReference() ) );

                // Appearance
                if ( auto* b = mColorSchemeGroup->button( int(model::Settings::colorScheme()) ) )
                {
                        b->setChecked( true );
                }
                if ( auto* b = mIconStyleGroup->button( int(model::Settings::iconStyle()) ) )
                {
                        b->setChecked( true );
                }

                // Layout
                mSidebarStyleCombo->setCurrentIndex( mSidebarStyleCombo->findData( int(model::Settings::sidebarStyle()) ) );
                int iconSize = model::Settings::sidebarIconSize();
                int iconIndex = mSidebarIconSizeCombo->findData( iconSize );
                if ( iconIndex < 0 )
                {
                        mSidebarIconSizeCombo->addItem( tr("Custom (%1 px)").arg( iconSize ), iconSize );
                        iconIndex = mSidebarIconSizeCombo->count() - 1;
                }
                mSidebarIconSizeCombo->setCurrentIndex( iconIndex );
                mToolbarStyleCombo->setCurrentIndex( mToolbarStyleCombo->findData( int(model::Settings::toolbarButtonStyle()) ) );

                // Printing
                mPrintOutlinesCheck->setChecked( model::Settings::printOutlines() );
                mPrintCropMarksCheck->setChecked( model::Settings::printCropMarks() );

                // General
                auto path = model::Settings::startupPath();
                if ( path.isEmpty() )
                {
                        mStartupParentRadio->setChecked( true );
                }
                else if ( path == QStandardPaths::writableLocation( QStandardPaths::HomeLocation ) )
                {
                        mStartupHomeRadio->setChecked( true );
                }
                else if ( path == QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) )
                {
                        mStartupDocumentsRadio->setChecked( true );
                }
                else
                {
                        mStartupOtherRadio->setChecked( true );
                }
                mStartupOtherEdit->setText( path.isEmpty() ? QStandardPaths::writableLocation( QStandardPaths::HomeLocation )
                                                           : path );
                mStartupOtherEdit->setEnabled( mStartupOtherRadio->isChecked() );
                mStartupBrowseButton->setEnabled( mStartupOtherRadio->isChecked() );
                mClearRecentButton->setEnabled( !model::Settings::recentFileList().isEmpty() );

                mBlocked = false;
        }


        ///
        /// Show a concrete example in the chosen units
        ///
        void PreferencesDialog::updateUnitsExample()
        {
                auto units = model::Settings::units();
                auto w = model::Distance::in( 2.625 );
                auto h = model::Distance::in( 1.0 );
                int digits = units.resolutionDigits();
                mUnitsExampleLabel->setText(
                        tr("Example: a common 2.625 × 1 inch address label is shown as <b>%1 × %2 %3</b>.")
                        .arg( w.inUnits( units ), 0, 'f', digits )
                        .arg( h.inUnits( units ), 0, 'f', digits )
                        .arg( units.toIdString() ) );
        }


        ///
        /// Units changed
        ///
        void PreferencesDialog::onUnitsChanged()
        {
                if ( mBlocked )
                {
                        return;
                }
                model::Settings::setUnits( model::Units( model::Units::Enum( mUnitsGroup->checkedId() ) ) );
        }


        ///
        /// Grid origin changed
        ///
        void PreferencesDialog::onGridOriginChanged()
        {
                if ( mBlocked )
                {
                        return;
                }
                model::Settings::setGridOrigin( model::Settings::GridOrigin( mGridOriginGroup->checkedId() ) );
        }


        ///
        /// Grid spacing changed
        ///
        void PreferencesDialog::onGridSpacingSpinChanged()
        {
                if ( mBlocked )
                {
                        return;
                }
                auto units = model::Settings::units();
                model::Settings::setGridSpacing( model::Distance( mGridSpacingSpin->value(), units ) );
        }


        ///
        /// Grid spacing reset
        ///
        void PreferencesDialog::onGridSpacingResetButtonClicked()
        {
                model::Settings::resetGridSpacing();
        }


        ///
        /// Color scheme / icon style changed
        ///
        void PreferencesDialog::onAppearanceChanged()
        {
                if ( mBlocked )
                {
                        return;
                }

                int scheme = mColorSchemeGroup->checkedId();
#if QT_VERSION < QT_VERSION_CHECK(6,8,0)
                if ( scheme == int(model::Settings::SYSTEM_COLOR_SCHEME) )
                {
                        scheme = int(model::Settings::LIGHT_COLOR_SCHEME);
                }
#endif
                if ( scheme != int(model::Settings::colorScheme()) )
                {
                        model::Settings::setColorScheme( model::Settings::ColorScheme( scheme ) );
                }

                int style = mIconStyleGroup->checkedId();
                if ( style != int(model::Settings::iconStyle()) )
                {
                        model::Settings::setIconStyle( model::Settings::IconStyle( style ) );
                }
        }


        ///
        /// Startup location changed
        ///
        void PreferencesDialog::onStartupLocationChanged()
        {
                if ( mBlocked )
                {
                        return;
                }

                QString path;
                if ( mStartupHomeRadio->isChecked() )
                {
                        path = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
                }
                else if ( mStartupDocumentsRadio->isChecked() )
                {
                        path = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                }
                else if ( mStartupOtherRadio->isChecked() )
                {
                        path = mStartupOtherEdit->text();
                }

                mStartupOtherEdit->setEnabled( mStartupOtherRadio->isChecked() );
                mStartupBrowseButton->setEnabled( mStartupOtherRadio->isChecked() );

                model::Settings::setStartupPath( path );
                if ( !path.isEmpty() )
                {
                        QDir::setCurrent( path );
                }
        }


        ///
        /// Browse for startup folder
        ///
        void PreferencesDialog::onStartupOtherBrowseButtonClicked()
        {
                auto path = QFileDialog::getExistingDirectory( this,
                                                               tr("Select startup directory"),
                                                               mStartupOtherEdit->text(),
                                                               QFileDialog::ShowDirsOnly );
                if ( !path.isEmpty() )
                {
                        mStartupOtherEdit->setText( path );
                        mStartupOtherRadio->setChecked( true );
                        onStartupLocationChanged();
                }
        }


        ///
        /// Clear recent files
        ///
        void PreferencesDialog::onClearRecentFilesClicked()
        {
                for ( const auto& file : model::Settings::recentFileList() )
                {
                        model::Settings::removeFromRecentFileList( file );
                }
                mClearRecentButton->setEnabled( false );
        }


        ///
        /// Open keyboard shortcuts editor
        ///
        void PreferencesDialog::onShortcutsButtonClicked()
        {
                QList<QAction*> actions;
                if ( parentWidget() )
                {
                        for ( auto* action : parentWidget()->window()->actions() )
                        {
                                if ( !action->objectName().isEmpty() )
                                {
                                        actions << action;
                                }
                        }
                }
                if ( actions.isEmpty() )
                {
                        QMessageBox::information( this, tr("Keyboard Shortcuts"),
                                                  tr("Open a project window first, then use Edit > Keyboard Shortcuts.") );
                        return;
                }

                ShortcutsDialog dialog( actions, this );
                dialog.exec();
        }


        ///
        /// Restore defaults
        ///
        void PreferencesDialog::onRestoreDefaultsClicked()
        {
                auto answer = QMessageBox::question( this, tr("Restore Defaults"),
                                                     tr("Reset all preferences to their original values?\n\n"
                                                        "Your recent files, favorites and keyboard shortcuts are kept.") );
                if ( answer == QMessageBox::Yes )
                {
                        model::Settings::resetToDefaults();
                        loadFromSettings();
                }
        }


        ///
        /// Filter categories by search text
        ///
        void PreferencesDialog::onSearchTextChanged( const QString& text )
        {
                int firstVisible = -1;
                for ( int i = 0; i < mCategoryList->count(); i++ )
                {
                        bool match = text.trimmed().isEmpty();
                        if ( !match )
                        {
                                for ( const auto& keyword : mPageKeywords[i] )
                                {
                                        if ( keyword.contains( text.trimmed(), Qt::CaseInsensitive ) )
                                        {
                                                match = true;
                                                break;
                                        }
                                }
                        }
                        mCategoryList->item( i )->setHidden( !match );
                        if ( match && firstVisible < 0 )
                        {
                                firstVisible = i;
                        }
                }

                auto* current = mCategoryList->currentItem();
                if ( firstVisible >= 0 && ( !current || current->isHidden() ) )
                {
                        mCategoryList->setCurrentRow( firstVisible );
                }
        }


        ///
        /// Settings changed elsewhere
        ///
        void PreferencesDialog::onSettingsChanged()
        {
                if ( !mBlocked )
                {
                        loadFromSettings();
                }
        }


} // namespace glabels
