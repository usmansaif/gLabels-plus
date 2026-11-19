//  ConnectDataSourceDialog.cpp
//
//  Copyright (C) 2026  gLabels Plus contributors
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

#include "ConnectDataSourceDialog.hpp"

#include "MergeTableModel.hpp"
#include "VariableTypeIcon.hpp"

#include "model/FileUtil.hpp"
#include "model/Model.hpp"

#include "merge/DataSourceUtil.hpp"
#include "merge/Factory.hpp"
#include "merge/Text.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>


namespace
{
        enum VariableColumn { COL_INCLUDE = 0, COL_SOURCE, COL_NAME, COL_TYPE, COL_SAMPLE, N_COLUMNS };

        enum FormatIndex { FMT_AUTO = 0, FMT_COMMA, FMT_TAB, FMT_SEMICOLON, FMT_COLON, FMT_JSON };

        const QStringList supportedSuffixes = { "csv", "tsv", "tab", "txt", "json" };
}


namespace glabels
{

        ///
        /// Drop zone: dashed box that accepts a dropped file or opens a file dialog on click
        ///
        class DropZone : public QFrame
        {
        public:
                std::function<void(const QString&)> onFileDropped;
                std::function<void()>               onBrowseRequested;

                DropZone( QWidget* parent = nullptr ) : QFrame( parent )
                {
                        setObjectName( "dropZone" );
                        setAcceptDrops( true );
                        setCursor( Qt::PointingHandCursor );
                        setMinimumHeight( 132 );

                        auto* layout = new QVBoxLayout( this );
                        layout->setAlignment( Qt::AlignCenter );
                        auto* title = new QLabel( tr("Drop a data file here") );
                        title->setObjectName( "dropZoneTitle" );
                        title->setAlignment( Qt::AlignCenter );
                        auto* hint = new QLabel( tr("CSV, TSV or JSON. The delimiter and encoding are detected automatically.") );
                        hint->setObjectName( "dropZoneHint" );
                        hint->setAlignment( Qt::AlignCenter );
                        hint->setWordWrap( true );
                        hint->setMinimumHeight( hint->fontMetrics().height() * 2 + 4 );
                        auto* browse = new QPushButton( tr("Browse File...") );
                        browse->setCursor( Qt::PointingHandCursor );
                        connect( browse, &QPushButton::clicked, this, [this]{ if ( onBrowseRequested ) onBrowseRequested(); } );

                        layout->addWidget( title );
                        layout->addWidget( hint );
                        layout->addWidget( browse, 0, Qt::AlignCenter );
                        setHighlighted( false );
                }

        protected:
                void dragEnterEvent( QDragEnterEvent* event ) override
                {
                        if ( fileFrom( event->mimeData() ).isEmpty() )
                        {
                                event->ignore();
                                return;
                        }
                        setHighlighted( true );
                        event->acceptProposedAction();
                }

                void dragLeaveEvent( QDragLeaveEvent* event ) override
                {
                        setHighlighted( false );
                        QFrame::dragLeaveEvent( event );
                }

                void dropEvent( QDropEvent* event ) override
                {
                        setHighlighted( false );
                        QString fn = fileFrom( event->mimeData() );
                        if ( !fn.isEmpty() )
                        {
                                event->acceptProposedAction();
                                if ( onFileDropped ) onFileDropped( fn );
                        }
                }

                void mouseReleaseEvent( QMouseEvent* event ) override
                {
                        if ( event->button() == Qt::LeftButton )
                        {
                                if ( onBrowseRequested ) onBrowseRequested();
                        }
                }

        private:
                static QString fileFrom( const QMimeData* data )
                {
                        if ( data && data->hasUrls() && !data->urls().isEmpty() && data->urls().first().isLocalFile() )
                        {
                                return data->urls().first().toLocalFile();
                        }
                        return QString();
                }

                void setHighlighted( bool on )
                {
                        setStyleSheet( QString( "#dropZone { border: 2px dashed %1; border-radius: 10px; background: %2; }"
                                                "#dropZoneTitle { font-weight: 600; font-size: 11pt; }"
                                                "#dropZoneHint { color: palette(placeholder-text); }" )
                                       .arg( on ? "palette(highlight)" : "palette(mid)",
                                             on ? "palette(alternate-base)" : "palette(base)" ) );
                }
        };


        ///
        /// Constructor
        ///
        ConnectDataSourceDialog::ConnectDataSourceDialog( model::Model* model, QWidget* parent )
                : QDialog( parent ), mModel( model )
        {
                setWindowTitle( tr("Connect Data Source") );
                setModal( true );
                resize( 860, 620 );

                auto* layout = new QVBoxLayout( this );
                layout->setSpacing( 10 );

                auto* intro = new QLabel( tr("Import a spreadsheet export to fill your label with data. "
                                             "Each column becomes a variable you can place on the label as {{Variable_Name}}.") );
                intro->setWordWrap( true );
                layout->addWidget( intro );

                //
                // Step 1: file
                //
                mDropZone = new DropZone();
                mDropZone->onFileDropped     = [this]( const QString& fn ) { loadFile( fn ); };
                mDropZone->onBrowseRequested = [this]() { onBrowse(); };
                layout->addWidget( mDropZone );

                auto* fileRow = new QHBoxLayout();
                mFileLabel = new QLabel();
                mFileLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
                QFont bold = mFileLabel->font();
                bold.setBold( true );
                mFileLabel->setFont( bold );
                mDetectedLabel = new QLabel();
                mDetectedLabel->setStyleSheet( "color: palette(placeholder-text);" );
                fileRow->addWidget( mFileLabel );
                fileRow->addWidget( mDetectedLabel, 1 );
                layout->addLayout( fileRow );

                // Advanced Parsing Options (collapsed)
                mAdvancedButton = new QToolButton();
                mAdvancedButton->setText( tr("Advanced Parsing Options") );
                mAdvancedButton->setCheckable( true );
                mAdvancedButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                mAdvancedButton->setArrowType( Qt::RightArrow );
                mAdvancedButton->setAutoRaise( true );
                connect( mAdvancedButton, &QToolButton::toggled, this, &ConnectDataSourceDialog::onAdvancedToggled );
                layout->addWidget( mAdvancedButton );

                mAdvancedPanel = new QWidget();
                auto* advLayout = new QHBoxLayout( mAdvancedPanel );
                advLayout->setContentsMargins( 24, 0, 0, 0 );
                advLayout->addWidget( new QLabel( tr("Format:") ) );
                mFormatCombo = new QComboBox();
                mFormatCombo->addItem( tr("Auto-detect") );
                mFormatCombo->addItem( tr("Comma separated (CSV)") );
                mFormatCombo->addItem( tr("Tab separated (TSV)") );
                mFormatCombo->addItem( tr("Semicolon separated") );
                mFormatCombo->addItem( tr("Colon separated") );
                mFormatCombo->addItem( tr("JSON (array of objects)") );
                advLayout->addWidget( mFormatCombo );
                advLayout->addSpacing( 16 );
                mHeaderCheck = new QCheckBox( tr("First row contains column names") );
                mHeaderCheck->setChecked( true );
                advLayout->addWidget( mHeaderCheck );
                advLayout->addStretch( 1 );
                mAdvancedPanel->setVisible( false );
                connect( mFormatCombo, QOverload<int>::of( &QComboBox::activated ), this, &ConnectDataSourceDialog::onFormatChanged );
                connect( mHeaderCheck, &QCheckBox::toggled, this, &ConnectDataSourceDialog::onFormatChanged );
                layout->addWidget( mAdvancedPanel );

                //
                // Step 2 and 3: variables and records
                //
                mTabs = new QTabWidget();

                mVariableTable = new QTableWidget( 0, N_COLUMNS );
                mVariableTable->setHorizontalHeaderLabels( { tr("Include"), tr("Source Column"), tr("Placeholder Name"),
                                                             tr("Data Type"), tr("Sample Value") } );
                mVariableTable->verticalHeader()->setVisible( false );
                mVariableTable->setSelectionMode( QAbstractItemView::SingleSelection );
                mVariableTable->setSelectionBehavior( QAbstractItemView::SelectRows );
                mVariableTable->setEditTriggers( QAbstractItemView::AllEditTriggers );
                mVariableTable->horizontalHeader()->setSectionResizeMode( COL_INCLUDE, QHeaderView::ResizeToContents );
                mVariableTable->horizontalHeader()->setSectionResizeMode( COL_SOURCE, QHeaderView::Interactive );
                mVariableTable->horizontalHeader()->setSectionResizeMode( COL_NAME, QHeaderView::Interactive );
                mVariableTable->horizontalHeader()->setSectionResizeMode( COL_TYPE, QHeaderView::ResizeToContents );
                mVariableTable->horizontalHeader()->setStretchLastSection( true );
                mVariableTable->setColumnWidth( COL_SOURCE, 170 );
                mVariableTable->setColumnWidth( COL_NAME, 200 );
                mVariableTable->setAlternatingRowColors( true );
                connect( mVariableTable, &QTableWidget::itemChanged, this, &ConnectDataSourceDialog::onVariableItemChanged );

                auto* variablesPage = new QWidget();
                auto* vLayout = new QVBoxLayout( variablesPage );
                auto* vHint = new QLabel( tr("Click a placeholder name to rename it. Names may contain letters, digits and underscores. "
                                             "The data type decides what is created when you drag the variable onto the label.") );
                vHint->setWordWrap( true );
                vHint->setStyleSheet( "color: palette(placeholder-text);" );
                vLayout->addWidget( vHint );
                vLayout->addWidget( mVariableTable, 1 );
                mTabs->addTab( variablesPage, tr("Variable Setup") );

                auto* recordsPage = new QWidget();
                auto* rLayout = new QVBoxLayout( recordsPage );
                auto* rButtons = new QHBoxLayout();
                auto* selectAll = new QPushButton( tr("Select all") );
                auto* selectNone = new QPushButton( tr("Select none") );
                connect( selectAll, &QPushButton::clicked, this, &ConnectDataSourceDialog::onSelectAllRecords );
                connect( selectNone, &QPushButton::clicked, this, &ConnectDataSourceDialog::onSelectNoRecords );
                mRecordCountLabel = new QLabel();
                rButtons->addWidget( selectAll );
                rButtons->addWidget( selectNone );
                rButtons->addStretch( 1 );
                rButtons->addWidget( mRecordCountLabel );
                rLayout->addLayout( rButtons );
                mRecordsView = new QTableView();
                mRecordsView->setAlternatingRowColors( true );
                mRecordsView->setSelectionMode( QAbstractItemView::NoSelection );
                mRecordsView->horizontalHeader()->setStretchLastSection( true );
                rLayout->addWidget( mRecordsView, 1 );
                mTabs->addTab( recordsPage, tr("Preview Records") );

                layout->addWidget( mTabs, 1 );

                //
                // Errors and buttons
                //
                mErrorLabel = new QLabel();
                mErrorLabel->setWordWrap( true );
                mErrorLabel->setStyleSheet( "color: #c0392b;" );
                layout->addWidget( mErrorLabel );

                auto* buttons = new QDialogButtonBox();
                mConfirmButton = buttons->addButton( tr("Confirm && Import Variables"), QDialogButtonBox::AcceptRole );
                mConfirmButton->setDefault( true );
                buttons->addButton( QDialogButtonBox::Cancel );
                connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
                layout->addWidget( buttons );

                //
                // Start from the current data source, if any
                //
                merge::Merge* current = mModel ? mModel->merge() : nullptr;
                if ( current && merge::Factory::idToType( current->id() ) == merge::Factory::FILE &&
                     !current->source().isEmpty() )
                {
                        mFileName = current->source();
                        QString id = current->id();
                        mBlocked = true;
                        if      ( id.startsWith( "Text/Comma" ) )     mFormatCombo->setCurrentIndex( FMT_COMMA );
                        else if ( id.startsWith( "Text/Tab" ) )       mFormatCombo->setCurrentIndex( FMT_TAB );
                        else if ( id.startsWith( "Text/Semicolon" ) ) mFormatCombo->setCurrentIndex( FMT_SEMICOLON );
                        else if ( id.startsWith( "Text/Colon" ) )     mFormatCombo->setCurrentIndex( FMT_COLON );
                        else if ( id == "Json" )                      mFormatCombo->setCurrentIndex( FMT_JSON );
                        else                                          mFormatCombo->setCurrentIndex( FMT_AUTO );
                        mHeaderCheck->setChecked( id == "Json" || id.endsWith( "Line1Keys" ) );
                        mBlocked = false;

                        mMerge.reset( current->clone() );
                        mFields = current->fields();
                        if ( mFields.isEmpty() )
                        {
                                // Identity mapping: keep the names the label already uses
                                for ( const auto& f : merge::DataSourceUtil::defaultFields( current ) )
                                {
                                        merge::Field id = f;
                                        id.name = merge::DataSourceUtil::isValidName( f.source ) ? f.source : f.name;
                                        mFields << id;
                                }
                        }
                        for ( const auto& f : mFields )
                        {
                                mInitialNames[f.source] = f.include ? f.name : QString();
                        }
                        if ( current->fields().isEmpty() )
                        {
                                for ( const auto& col : current->sourceColumns() )
                                {
                                        mInitialNames[col] = col;
                                }
                        }
                        applyMapping();
                        fillVariableTable();
                        refreshPreview();
                }
                else
                {
                        mFileLabel->setText( tr("No file selected") );
                }

                validate();
        }


        ConnectDataSourceDialog::~ConnectDataSourceDialog() = default;


        ///
        /// Browse for a file
        ///
        void ConnectDataSourceDialog::onBrowse()
        {
                QString dir = mFileName.isEmpty()
                        ? QSettings().value( "DataSource/lastDir", mModel ? mModel->dirPath() : QString() ).toString()
                        : QFileInfo( mFileName ).absolutePath();

                QString fileName = QFileDialog::getOpenFileName(
                        this, tr("Select data file"), dir,
                        tr("Data files (*.csv *.tsv *.tab *.txt *.json)") + ";;" +
                        tr("CSV files (*.csv)") + ";;" +
                        tr("TSV files (*.tsv *.tab)") + ";;" +
                        tr("JSON files (*.json)") + ";;" +
                        tr("All files (*)") );

                if ( !fileName.isEmpty() )
                {
                        loadFile( fileName );
                }
        }


        ///
        /// Load a file
        ///
        void ConnectDataSourceDialog::loadFile( const QString& fileName )
        {
                QFileInfo info( fileName );
                if ( !info.isFile() )
                {
                        return;
                }
                QSettings().setValue( "DataSource/lastDir", info.absolutePath() );

                if ( info.suffix().toLower() == "xlsx" || info.suffix().toLower() == "xls" ||
                     info.suffix().toLower() == "ods" )
                {
                        mErrorLabel->setText( tr("Spreadsheet files can't be read directly. "
                                                 "Save the sheet as CSV (File > Save As > CSV) and import that file.") );
                        return;
                }

                mFileName = info.absoluteFilePath();
                mInitialNames.clear();

                // Pick the format from the extension; content decides the delimiter
                mBlocked = true;
                mFormatCombo->setCurrentIndex( info.suffix().toLower() == "json" ? FMT_JSON : FMT_AUTO );
                mHeaderCheck->setChecked( true );
                mBlocked = false;

                mFields.clear();
                reload();
        }


        ///
        /// Advanced options toggled
        ///
        void ConnectDataSourceDialog::onAdvancedToggled( bool checked )
        {
                mAdvancedButton->setArrowType( checked ? Qt::DownArrow : Qt::RightArrow );
                mAdvancedPanel->setVisible( checked );
        }


        ///
        /// Format or header option changed
        ///
        void ConnectDataSourceDialog::onFormatChanged()
        {
                if ( mBlocked )
                {
                        return;
                }
                mHeaderCheck->setEnabled( mFormatCombo->currentIndex() != FMT_JSON );
                mFields.clear();
                reload();
        }


        ///
        /// Backend ID for the current options
        ///
        QString ConnectDataSourceDialog::backendId() const
        {
                bool keys = mHeaderCheck->isChecked();
                switch ( mFormatCombo->currentIndex() )
                {
                case FMT_COMMA:     return keys ? "Text/Comma/Line1Keys"     : "Text/Comma";
                case FMT_TAB:       return keys ? "Text/Tab/Line1Keys"       : "Text/Tab";
                case FMT_SEMICOLON: return keys ? "Text/Semicolon/Line1Keys" : "Text/Semicolon";
                case FMT_COLON:     return keys ? "Text/Colon/Line1Keys"     : "Text/Colon";
                case FMT_JSON:      return "Json";
                default:            return keys ? "Text/Auto/Line1Keys"      : "Text/Auto";
                }
        }


        ///
        /// (Re)read the file with the current options
        ///
        void ConnectDataSourceDialog::reload()
        {
                if ( mFileName.isEmpty() )
                {
                        return;
                }

                mHeaderCheck->setEnabled( mFormatCombo->currentIndex() != FMT_JSON );

                mMerge.reset( merge::Factory::createMerge( backendId() ) );
                mMerge->setSource( mFileName );

                if ( mFields.isEmpty() )
                {
                        mFields = merge::DataSourceUtil::defaultFields( mMerge.get() );
                }

                applyMapping();
                fillVariableTable();
                refreshPreview();
                validate();
        }


        ///
        /// Fill the Variable Setup table from mFields
        ///
        void ConnectDataSourceDialog::fillVariableTable()
        {
                QFileInfo info( mFileName );
                mFileLabel->setText( info.fileName() );
                mFileLabel->setToolTip( QDir::toNativeSeparators( mFileName ) );

                // Detection summary
                QStringList detected;
                if ( auto* text = dynamic_cast<merge::Text*>( mMerge.get() ) )
                {
                        QChar d = text->delimiter();
                        if      ( d == ',' )  detected << tr("comma separated");
                        else if ( d == '\t' ) detected << tr("tab separated");
                        else if ( d == ';' )  detected << tr("semicolon separated");
                        else if ( d == ':' )  detected << tr("colon separated");
                        else if ( d == '|' )  detected << tr("pipe separated");
                        detected << ( text->isLatin1() ? tr("Latin-1") : tr("UTF-8") );
                }
                else if ( mMerge && mMerge->id() == "Json" )
                {
                        detected << tr("JSON");
                }
                int nRecords = mMerge ? mMerge->recordList().size() : 0;
                detected << tr("%n record(s)", "", nRecords);
                detected << tr("%n column(s)", "", int( mFields.size() ));
                mDetectedLabel->setText( "  " + detected.join( "  ·  " ) );

                // Raw samples: first non-empty value per column
                std::unique_ptr<merge::Merge> raw( mMerge ? mMerge->clone() : nullptr );
                if ( raw )
                {
                        raw->setFields( {} );
                }

                mBlocked = true;
                mVariableTable->setRowCount( 0 );
                mVariableTable->setRowCount( int( mFields.size() ) );
                for ( int row = 0; row < mFields.size(); row++ )
                {
                        const auto& f = mFields[row];

                        auto* include = new QTableWidgetItem();
                        include->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable );
                        include->setCheckState( f.include ? Qt::Checked : Qt::Unchecked );
                        mVariableTable->setItem( row, COL_INCLUDE, include );

                        auto* source = new QTableWidgetItem( f.source );
                        source->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
                        mVariableTable->setItem( row, COL_SOURCE, source );

                        auto* name = new QTableWidgetItem( f.name );
                        name->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable );
                        name->setToolTip( tr("Click to rename. Used on the label as {{%1}}").arg( f.name ) );
                        mVariableTable->setItem( row, COL_NAME, name );

                        auto* typeCombo = new QComboBox();
                        for ( const auto& type : VariableType::types() )
                        {
                                typeCombo->addItem( VariableType::icon( type ), VariableType::displayName( type ), type );
                        }
                        typeCombo->setCurrentIndex( std::max( 0, int( VariableType::types().indexOf( f.type ) ) ) );
                        typeCombo->setProperty( "row", row );
                        connect( typeCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
                                 this, &ConnectDataSourceDialog::onTypeChanged );
                        mVariableTable->setCellWidget( row, COL_TYPE, typeCombo );

                        QString sample;
                        if ( raw )
                        {
                                for ( const auto& record : raw->recordList() )
                                {
                                        sample = record.value( f.source );
                                        if ( !sample.trimmed().isEmpty() )
                                        {
                                                break;
                                        }
                                }
                        }
                        auto* sampleItem = new QTableWidgetItem( sample.simplified() );
                        sampleItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
                        sampleItem->setToolTip( sample );
                        QFont italic = sampleItem->font();
                        italic.setItalic( sample.isEmpty() );
                        sampleItem->setFont( italic );
                        if ( sample.isEmpty() )
                        {
                                sampleItem->setText( tr("(empty)") );
                        }
                        mVariableTable->setItem( row, COL_SAMPLE, sampleItem );
                }
                mBlocked = false;
        }


        ///
        /// Variable table edited
        ///
        void ConnectDataSourceDialog::onVariableItemChanged()
        {
                if ( mBlocked )
                {
                        return;
                }

                for ( int row = 0; row < mVariableTable->rowCount() && row < mFields.size(); row++ )
                {
                        mFields[row].include = mVariableTable->item( row, COL_INCLUDE )->checkState() == Qt::Checked;
                        mFields[row].name    = mVariableTable->item( row, COL_NAME )->text().trimmed();
                }

                if ( validate() )
                {
                        applyMapping();
                        refreshPreview();
                }
        }


        ///
        /// Data type changed
        ///
        void ConnectDataSourceDialog::onTypeChanged()
        {
                auto* combo = qobject_cast<QComboBox*>( sender() );
                if ( !combo )
                {
                        return;
                }
                int row = combo->property( "row" ).toInt();
                if ( row >= 0 && row < mFields.size() )
                {
                        mFields[row].type = combo->currentData().toString();
                        applyMapping();
                }
        }


        ///
        /// Push mFields into the working merge (keeps record selection)
        ///
        void ConnectDataSourceDialog::applyMapping()
        {
                if ( mMerge )
                {
                        mMerge->setFields( mFields );
                }
        }


        ///
        /// Validate names; mark problems and enable/disable the confirm button
        ///
        bool ConnectDataSourceDialog::validate()
        {
                QStringList errors;

                if ( !mMerge || mFileName.isEmpty() )
                {
                        mErrorLabel->clear();
                        mConfirmButton->setEnabled( false );
                        return false;
                }

                if ( mMerge->recordList().isEmpty() )
                {
                        errors << tr("No records were found in this file. Check the format under Advanced Parsing Options.");
                }

                QStringList customVariables = mModel ? mModel->variables().keys() : QStringList();
                QMap<QString,int> seen;
                int nIncluded = 0;

                mBlocked = true;
                for ( int row = 0; row < mFields.size() && row < mVariableTable->rowCount(); row++ )
                {
                        const auto& f = mFields[row];
                        auto* item = mVariableTable->item( row, COL_NAME );
                        QString problem;

                        if ( f.include )
                        {
                                nIncluded++;
                                if ( f.name.isEmpty() )
                                {
                                        problem = tr("Name is empty");
                                }
                                else if ( !merge::DataSourceUtil::isValidName( f.name ) )
                                {
                                        problem = tr("Use only letters, digits and underscores");
                                }
                                else if ( seen.contains( f.name.toLower() ) )
                                {
                                        problem = tr("Name is used more than once");
                                }
                                else if ( customVariables.contains( f.name ) )
                                {
                                        problem = tr("A custom variable already has this name");
                                }
                                seen[f.name.toLower()] = row;
                        }

                        if ( problem.isEmpty() )
                        {
                                item->setData( Qt::BackgroundRole, QVariant() );
                                item->setToolTip( tr("Click to rename. Used on the label as {{%1}}").arg( f.name ) );
                        }
                        else
                        {
                                item->setData( Qt::BackgroundRole, QColor( 231, 76, 60, 70 ) );
                                item->setToolTip( problem );
                                errors << QString( "%1: %2" ).arg( f.source, problem );
                        }

                        // Dim excluded rows
                        for ( int col : { COL_SOURCE, COL_NAME, COL_SAMPLE } )
                        {
                                auto* it = mVariableTable->item( row, col );
                                it->setForeground( f.include ? QBrush() : palette().brush( QPalette::Disabled, QPalette::Text ) );
                        }
                        if ( auto* combo = mVariableTable->cellWidget( row, COL_TYPE ) )
                        {
                                combo->setEnabled( f.include );
                        }
                }
                mBlocked = false;

                if ( nIncluded == 0 && !mFields.isEmpty() )
                {
                        errors << tr("Include at least one column.");
                }

                mErrorLabel->setText( errors.join( "\n" ) );
                bool ok = errors.isEmpty();
                mConfirmButton->setEnabled( ok );
                return ok;
        }


        ///
        /// Rebuild the records preview
        ///
        void ConnectDataSourceDialog::refreshPreview()
        {
                auto* old = mRecordsView->model();
                if ( mMerge )
                {
                        auto* tableModel = new MergeTableModel( mMerge.get(), mRecordsView );
                        mRecordsView->setModel( tableModel );
                        mRecordsView->resizeColumnsToContents();
                        connect( mMerge.get(), &merge::Merge::selectionChanged,
                                 this, &ConnectDataSourceDialog::updateRecordCount, Qt::UniqueConnection );
                }
                else
                {
                        mRecordsView->setModel( nullptr );
                }
                delete old;
                updateRecordCount();
        }


        void ConnectDataSourceDialog::onSelectAllRecords()
        {
                if ( mMerge )
                {
                        mMerge->selectAll();
                        mRecordsView->viewport()->update();
                }
        }


        void ConnectDataSourceDialog::onSelectNoRecords()
        {
                if ( mMerge )
                {
                        mMerge->unselectAll();
                        mRecordsView->viewport()->update();
                }
        }


        void ConnectDataSourceDialog::updateRecordCount()
        {
                if ( !mMerge )
                {
                        mRecordCountLabel->clear();
                        return;
                }
                mRecordCountLabel->setText( tr("%1 of %2 records selected for printing")
                                            .arg( mMerge->nSelectedRecords() )
                                            .arg( mMerge->recordList().size() ) );
        }


        ///
        /// Result
        ///
        merge::Merge* ConnectDataSourceDialog::takeMerge()
        {
                if ( mMerge )
                {
                        mMerge->setFields( mFields );
                }
                return mMerge.release();
        }


        ///
        /// Renames implied by the new mapping
        ///
        QMap<QString,QString> ConnectDataSourceDialog::renames() const
        {
                QMap<QString,QString> map;
                for ( const auto& f : mFields )
                {
                        if ( !f.include || !mInitialNames.contains( f.source ) )
                        {
                                continue;
                        }
                        QString oldName = mInitialNames[f.source];
                        if ( !oldName.isEmpty() && oldName != f.name )
                        {
                                map[oldName] = f.name;
                        }
                }
                return map;
        }

}

