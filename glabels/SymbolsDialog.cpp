//  SymbolsDialog.cpp
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


#include "SymbolsDialog.hpp"

#include "CustomSymbols.hpp"

#include "model/Distance.hpp"
#include "model/Settings.hpp"
#include "model/Units.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QVBoxLayout>


namespace
{
        const int    iconPx         = 64;
        const double defaultSizePts = 10.0 / 25.4 * 72.0;    // 10 mm


        ///
        /// The symbol on a white tile, so black symbols show in dark mode too
        ///
        QPixmap symbolTile( QSvgRenderer& renderer, int px, qreal dpr )
        {
                QPixmap pixmap( QSize( px, px ) * dpr );
                pixmap.setDevicePixelRatio( dpr );
                pixmap.fill( Qt::transparent );

                QPainter painter( &pixmap );
                painter.setRenderHint( QPainter::Antialiasing );
                QPainterPath tile;
                tile.addRoundedRect( QRectF( 0.5, 0.5, px - 1, px - 1 ), 8, 8 );
                painter.fillPath( tile, Qt::white );
                painter.setPen( QPen( QColor( 0, 0, 0, 40 ), 1 ) );
                painter.drawPath( tile );

                if ( renderer.isValid() )
                {
                        // Keep the drawing's proportions inside the tile
                        QRectF box = renderer.viewBoxF();
                        if ( box.isEmpty() )
                        {
                                box = QRectF( QPointF( 0, 0 ), renderer.defaultSize() );
                        }
                        QRectF area( px * 0.1875, px * 0.1875, px * 0.625, px * 0.625 );
                        if ( !box.isEmpty() )
                        {
                                QSizeF size = box.size().scaled( area.size(), Qt::KeepAspectRatio );
                                area = QRectF( area.center() - QPointF( size.width() / 2, size.height() / 2 ), size );
                        }
                        renderer.render( &painter, area );
                }
                painter.end();

                return pixmap;
        }


        QIcon symbolIcon( const QString& path, qreal dpr )
        {
                QSvgRenderer renderer( path );
                return QIcon( symbolTile( renderer, iconPx, dpr ) );
        }


        bool isSvgFile( const QUrl& url )
        {
                return url.isLocalFile() && url.toLocalFile().endsWith( ".svg", Qt::CaseInsensitive );
        }


        /// "my-company_logo" becomes "My company logo"
        QString nameFromFile( const QString& fileName )
        {
                QString name = QFileInfo( fileName ).completeBaseName();
                name.replace( '_', ' ' ).replace( '-', ' ' );
                name = name.simplified().left( glabels::CustomSymbols::maxNameLength );
                if ( !name.isEmpty() )
                {
                        name[0] = name[0].toUpper();
                }
                return name;
        }


        ///
        /// Add Your Own Symbol: choose an SVG file, see whether it passes the
        /// checks and how it looks, then name it
        ///
        class AddSymbolDialog : public QDialog
        {
        public:
                AddSymbolDialog( const QStringList& categories, QWidget* parent )
                        : QDialog( parent )
                {
                        using glabels::SymbolsDialog;
                        using namespace glabels::CustomSymbols;

                        setWindowTitle( SymbolsDialog::tr("Add Your Own Symbol") );

                        mFile = new QLineEdit;
                        mFile->setReadOnly( true );
                        mFile->setPlaceholderText( SymbolsDialog::tr("No file chosen") );
                        auto* browse = new QPushButton( SymbolsDialog::tr("&Choose SVG File...") );
                        auto* fileRow = new QHBoxLayout;
                        fileRow->addWidget( mFile, 1 );
                        fileRow->addWidget( browse );

                        mPreview = new QLabel;
                        mPreview->setFixedSize( 128, 128 );
                        mPreview->setAlignment( Qt::AlignCenter );

                        mName = new QLineEdit;
                        mName->setMaxLength( maxNameLength );

                        mCategory = new QComboBox;
                        mCategory->setEditable( true );
                        mCategory->addItems( categories );
                        mCategory->lineEdit()->setMaxLength( maxCategoryLen );
                        mCategory->setCurrentText( categories.value( 0 ) );

                        mKeywords = new QLineEdit;
                        mKeywords->setMaxLength( maxKeywordsLen );
                        mKeywords->setPlaceholderText( SymbolsDialog::tr("Words to find it by, e.g. logo brand") );

                        auto* form = new QFormLayout;
                        form->addRow( SymbolsDialog::tr("File:"), fileRow );
                        form->addRow( SymbolsDialog::tr("Preview:"), mPreview );
                        form->addRow( SymbolsDialog::tr("&Name:"), mName );
                        form->addRow( SymbolsDialog::tr("&Category:"), mCategory );
                        form->addRow( SymbolsDialog::tr("&Keywords:"), mKeywords );

                        mMessage = new QLabel( SymbolsDialog::tr("Choose an SVG drawing. Solid black shapes print "
                                                                 "best on thermal label printers.") );
                        mMessage->setWordWrap( true );
                        mMessage->setForegroundRole( QPalette::PlaceholderText );

                        auto* buttons = new QDialogButtonBox( QDialogButtonBox::Cancel );
                        mAddButton = buttons->addButton( SymbolsDialog::tr("&Add Symbol"), QDialogButtonBox::AcceptRole );
                        mAddButton->setDefault( true );
                        mAddButton->setEnabled( false );

                        auto* layout = new QVBoxLayout( this );
                        layout->addLayout( form );
                        layout->addWidget( mMessage );
                        layout->addStretch( 1 );
                        layout->addWidget( buttons );

                        connect( browse, &QPushButton::clicked, this, [this]() { chooseFile(); } );
                        connect( mName, &QLineEdit::textChanged, this, [this]() { updateButton(); } );
                        connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
                        connect( mAddButton, &QPushButton::clicked, this, [this]() { save(); } );

                        showPreview( QByteArray() );
                        resize( 480, sizeHint().height() );
                }


                /// Load fileName as the drawing to add
                void setFile( const QString& fileName )
                {
                        using glabels::SymbolsDialog;

                        mData.clear();
                        mFile->setText( QDir::toNativeSeparators( fileName ) );
                        QSettings().setValue( "symbols/customFolder", QFileInfo( fileName ).absolutePath() );

                        QString problem;
                        QFileInfo info( fileName );
                        QFile file( fileName );
                        if ( info.size() > glabels::CustomSymbols::maxFileBytes )
                        {
                                problem = SymbolsDialog::tr("The file is larger than %1 MB. Simplify the drawing or "
                                                            "save it as a plain SVG.")
                                                  .arg( glabels::CustomSymbols::maxFileBytes / ( 1024 * 1024 ) );
                        }
                        else if ( !file.open( QFile::ReadOnly ) )
                        {
                                problem = SymbolsDialog::tr("Could not open the file: %1").arg( file.errorString() );
                        }
                        else
                        {
                                QByteArray data = file.readAll();
                                problem = glabels::CustomSymbols::validate( data );
                                if ( problem.isEmpty() )
                                {
                                        mData = data;
                                }
                        }

                        showPreview( mData );
                        if ( problem.isEmpty() )
                        {
                                if ( mName->text().trimmed().isEmpty() )
                                {
                                        mName->setText( nameFromFile( fileName ) );
                                }
                                showMessage( SymbolsDialog::tr("The drawing passed all checks. Name it and press Add Symbol."),
                                             false );
                                mName->setFocus();
                                mName->selectAll();
                        }
                        else
                        {
                                showMessage( problem, true );
                        }
                        updateButton();
                }


                glabels::CustomSymbols::Entry added() const { return mAdded; }


        private:
                void chooseFile()
                {
                        using glabels::SymbolsDialog;
                        QString folder = QSettings().value( "symbols/customFolder",
                                                            QStandardPaths::writableLocation( QStandardPaths::PicturesLocation ) )
                                                 .toString();
                        QString fileName = QFileDialog::getOpenFileName( this, SymbolsDialog::tr("Choose SVG File"), folder,
                                                                         SymbolsDialog::tr("SVG drawings (*.svg)") );
                        if ( !fileName.isEmpty() )
                        {
                                setFile( fileName );
                        }
                }


                void save()
                {
                        QString error;
                        if ( glabels::CustomSymbols::add( mData, mName->text(), mCategory->currentText(), mKeywords->text(),
                                                          &mAdded, &error ) )
                        {
                                accept();
                        }
                        else
                        {
                                showMessage( error, true );
                        }
                }


                void showPreview( const QByteArray& data )
                {
                        QSvgRenderer renderer( data );
                        mPreview->setPixmap( symbolTile( renderer, 128, devicePixelRatioF() ) );
                }


                void showMessage( const QString& text, bool error )
                {
                        mMessage->setText( text );
                        if ( error )
                        {
                                QPalette p = mMessage->palette();
                                p.setColor( QPalette::WindowText, QColor( 0xc0, 0x39, 0x2b ) );
                                mMessage->setPalette( p );
                                mMessage->setForegroundRole( QPalette::WindowText );
                        }
                        else
                        {
                                mMessage->setPalette( QPalette() );
                                mMessage->setForegroundRole( QPalette::WindowText );
                        }
                }


                void updateButton()
                {
                        mAddButton->setEnabled( !mData.isEmpty() && !mName->text().trimmed().isEmpty() );
                }


                QLineEdit*   mFile;
                QLabel*      mPreview;
                QLineEdit*   mName;
                QComboBox*   mCategory;
                QLineEdit*   mKeywords;
                QLabel*      mMessage;
                QPushButton* mAddButton;
                QByteArray   mData;
                glabels::CustomSymbols::Entry mAdded;
        };
}


namespace glabels
{

        QList<SymbolsDialog::Symbol> SymbolsDialog::symbols()
        {
                QList<Symbol> list;
                QFile index( ":/symbols/symbols.json" );
                if ( index.open( QFile::ReadOnly ) )
                {
                        const QJsonArray array = QJsonDocument::fromJson( index.readAll() ).array();
                        for ( const auto& value : array )
                        {
                                QJsonObject o = value.toObject();
                                Symbol s;
                                s.id       = o.value( "id" ).toString();
                                s.name     = QCoreApplication::translate( "Symbols", o.value( "name" ).toString().toUtf8() );
                                s.category = QCoreApplication::translate( "Symbols", o.value( "category" ).toString().toUtf8() );
                                s.keywords = o.value( "keywords" ).toString();
                                s.path     = ":/symbols/" + s.id + ".svg";
                                list << s;
                        }
                }
                return list;
        }


        QList<SymbolsDialog::Symbol> SymbolsDialog::allSymbols()
        {
                QList<Symbol> list = symbols();
                for ( const auto& e : CustomSymbols::load() )
                {
                        Symbol s;
                        s.id       = e.id;
                        s.name     = e.name;
                        s.category = e.category;
                        s.keywords = e.keywords;
                        s.path     = e.path;
                        s.custom   = true;
                        list << s;
                }
                return list;
        }


        SymbolsDialog::SymbolsDialog( QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( tr("Insert Symbol") );
                setModal( false );
                setSizeGripEnabled( true );
                setAcceptDrops( true );

                // Search and category filter
                mSearch = new QLineEdit;
                mSearch->setPlaceholderText( tr("Search symbols (fragile, phone, veg...)") );
                mSearch->setClearButtonEnabled( true );

                mCategory = new QComboBox;

                auto* filterRow = new QHBoxLayout;
                filterRow->addWidget( mSearch, 1 );
                filterRow->addWidget( mCategory );

                // Symbol grid
                mList = new QListWidget;
                mList->setViewMode( QListView::IconMode );
                mList->setIconSize( QSize( iconPx, iconPx ) );
                mList->setGridSize( QSize( iconPx + 50, iconPx + 52 ) );
                mList->setTextElideMode( Qt::ElideNone );            // long names wrap onto two lines
                mList->setResizeMode( QListView::Adjust );
                mList->setMovement( QListView::Static );
                mList->setWordWrap( true );
                mList->setSelectionMode( QAbstractItemView::SingleSelection );
                mList->setContextMenuPolicy( Qt::CustomContextMenu );

                // Size, in the user's units
                model::Units units = model::Settings::units();
                mSize = new QDoubleSpinBox;
                mSize->setDecimals( units.resolutionDigits() );
                mSize->setSingleStep( units.resolution() * 10 );
                mSize->setSuffix( " " + units.toTrName() );
                mSize->setRange( model::Distance::mm( 2 ).inUnits( units ), model::Distance::mm( 150 ).inUnits( units ) );
                QSettings settings;
                double sizePts = settings.value( "symbols/size", defaultSizePts ).toDouble();
                mSize->setValue( model::Distance::pt( sizePts ).inUnits( units ) );
                mSize->setToolTip( tr("Width and height of the inserted symbol. You can resize it on the label afterwards.") );

                mStatus = new QLabel( tr("Solid black symbols that print sharply on thermal label printers.") );
                mStatus->setWordWrap( true );
                mStatus->setForegroundRole( QPalette::PlaceholderText );

                auto* sizeRow = new QHBoxLayout;
                sizeRow->addWidget( new QLabel( tr("Size:") ) );
                sizeRow->addWidget( mSize );
                sizeRow->addSpacing( 12 );
                sizeRow->addWidget( mStatus, 1 );

                // Buttons: Insert keeps the dialog open; Close closes it
                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close );
                auto* addButton = buttons->addButton( tr("Add Your &Own..."), QDialogButtonBox::ActionRole );
                addButton->setToolTip( tr("Add an SVG drawing of your own, such as a logo. It stays on this computer "
                                          "for all your labels. You can also drop .svg files on this window.") );
                mRemoveButton = buttons->addButton( tr("&Remove"), QDialogButtonBox::ActionRole );
                mRemoveButton->setToolTip( tr("Remove the selected symbol of your own") );
                mRemoveButton->setEnabled( false );
                mInsertButton = buttons->addButton( tr("&Insert"), QDialogButtonBox::AcceptRole );
                mInsertButton->setDefault( true );
                mInsertButton->setEnabled( false );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );
                connect( mInsertButton, &QPushButton::clicked, this, &SymbolsDialog::onInsert );
                connect( addButton, &QPushButton::clicked, this, &SymbolsDialog::onAddCustom );
                connect( mRemoveButton, &QPushButton::clicked, this, &SymbolsDialog::onRemoveCustom );

                auto* layout = new QVBoxLayout( this );
                layout->addLayout( filterRow );
                layout->addWidget( mList, 1 );
                layout->addLayout( sizeRow );
                layout->addWidget( buttons );

                connect( mSearch, &QLineEdit::textChanged, this, &SymbolsDialog::onFilterChanged );
                connect( mCategory, QOverload<int>::of( &QComboBox::currentIndexChanged ),
                         this, &SymbolsDialog::onFilterChanged );
                connect( mList, &QListWidget::itemSelectionChanged, this, &SymbolsDialog::onSelectionChanged );
                connect( mList, &QListWidget::itemActivated, this, &SymbolsDialog::onInsert );
                connect( mList, &QListWidget::customContextMenuRequested, this, [this]( const QPoint& pos ) {
                        QListWidgetItem* item = mList->itemAt( pos );
                        if ( item )
                        {
                                item->setSelected( true );
                        }
                        const Symbol* s = selectedSymbol();
                        QMenu menu( this );
                        if ( s )
                        {
                                menu.addAction( tr("&Insert"), this, &SymbolsDialog::onInsert );
                                if ( s->custom )
                                {
                                        menu.addAction( tr("&Remove"), this, &SymbolsDialog::onRemoveCustom );
                                }
                                menu.addSeparator();
                        }
                        menu.addAction( tr("Add Your &Own..."), this, &SymbolsDialog::onAddCustom );
                        menu.exec( mList->viewport()->mapToGlobal( pos ) );
                } );
                connect( mSize, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), this, [units]( double value ) {
                        QSettings().setValue( "symbols/size", model::Distance( value, units ).pt() );
                } );

                // Fill the grid, showing the last category
                populate( QString(), settings.value( "symbols/category" ).toString() );
                connect( mCategory, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]() {
                        QSettings().setValue( "symbols/category", mCategory->currentData().toString() );
                } );

                resize( 660, 580 );
                mSearch->setFocus();
        }


        ///
        /// (Re)build the category list and grid from the built-in and custom
        /// symbols; select selectId if given, and show showCategory if given
        ///
        void SymbolsDialog::populate( const QString& selectId, const QString& showCategory )
        {
                const QString category = showCategory.isNull() ? mCategory->currentData().toString() : showCategory;

                mSymbols = allSymbols();

                {
                        QSignalBlocker blockCategory( mCategory );
                        mCategory->clear();
                        mCategory->addItem( tr("All symbols"), QString() );
                        QStringList categories;
                        for ( const auto& s : mSymbols )
                        {
                                if ( !categories.contains( s.category ) )
                                {
                                        categories << s.category;
                                        mCategory->addItem( s.category, s.category );
                                }
                        }
                        int categoryIndex = mCategory->findData( category );
                        mCategory->setCurrentIndex( categoryIndex < 0 ? 0 : categoryIndex );
                }

                QSignalBlocker blockList( mList );
                mList->clear();
                qreal dpr = devicePixelRatioF();
                for ( int i = 0; i < mSymbols.size(); i++ )
                {
                        const Symbol& s = mSymbols[i];
                        auto* item = new QListWidgetItem( symbolIcon( s.path, dpr ), s.name );
                        item->setData( Qt::UserRole, i );
                        item->setToolTip( s.custom ? tr("%1 · %2 (your own symbol)").arg( s.name, s.category )
                                                   : QString( "%1 · %2" ).arg( s.name, s.category ) );
                        item->setTextAlignment( Qt::AlignHCenter | Qt::AlignTop );
                        mList->addItem( item );
                        if ( !selectId.isEmpty() && s.id == selectId )
                        {
                                item->setSelected( true );
                                mList->setCurrentItem( item );
                        }
                }
                blockList.unblock();

                onFilterChanged();
                if ( mList->currentItem() && mList->currentItem()->isSelected() )
                {
                        mList->scrollToItem( mList->currentItem() );
                }
        }


        const SymbolsDialog::Symbol* SymbolsDialog::selectedSymbol() const
        {
                const auto selected = mList->selectedItems();
                if ( selected.isEmpty() )
                {
                        return nullptr;
                }
                int index = selected.first()->data( Qt::UserRole ).toInt();
                return ( index >= 0 && index < mSymbols.size() ) ? &mSymbols[index] : nullptr;
        }


        void SymbolsDialog::onFilterChanged()
        {
                const QString category = mCategory->currentData().toString();
                const QStringList words = mSearch->text().simplified().split( ' ', Qt::SkipEmptyParts );

                for ( int i = 0; i < mList->count(); i++ )
                {
                        QListWidgetItem* item = mList->item( i );
                        const Symbol& s = mSymbols[ item->data( Qt::UserRole ).toInt() ];

                        bool show = category.isEmpty() || s.category == category;
                        const QString haystack = s.name + " " + s.category + " " + s.keywords + " " + s.id;
                        for ( const QString& word : words )
                        {
                                show = show && haystack.contains( word, Qt::CaseInsensitive );
                        }
                        item->setHidden( !show );
                        if ( !show && item->isSelected() )
                        {
                                item->setSelected( false );
                        }
                }
                onSelectionChanged();
        }


        void SymbolsDialog::onSelectionChanged()
        {
                const Symbol* s = selectedSymbol();
                mInsertButton->setEnabled( s != nullptr );
                mRemoveButton->setEnabled( s && s->custom );
        }


        void SymbolsDialog::onInsert()
        {
                const Symbol* selected = selectedSymbol();
                if ( !selected )
                {
                        return;
                }
                const Symbol s = *selected;

                QFile file( s.path );
                if ( !file.open( QFile::ReadOnly ) )
                {
                        mStatus->setText( tr("Could not read \"%1\": %2").arg( s.name, file.errorString() ) );
                        return;
                }
                QByteArray svg = file.readAll();

                // A custom symbol's file could have been changed outside the program since it was added
                if ( s.custom )
                {
                        QString problem = CustomSymbols::validate( svg );
                        if ( !problem.isEmpty() )
                        {
                                mStatus->setText( tr("\"%1\" can no longer be used: %2").arg( s.name, problem ) );
                                return;
                        }
                }

                double size = model::Distance( mSize->value(), model::Settings::units() ).pt();
                emit insertRequested( s.id, s.name, svg, size );

                mStatus->setText( tr("Inserted \"%1\". Choose another symbol, or close this window.").arg( s.name ) );
        }


        void SymbolsDialog::onAddCustom()
        {
                addCustomFromFile( QString() );
        }


        void SymbolsDialog::addCustomFromFile( const QString& fileName )
        {
                // Offer the categories already used for custom symbols, the default first
                QStringList categories;
                for ( auto it = mSymbols.crbegin(); it != mSymbols.crend(); ++it )     // last used first
                {
                        if ( it->custom )
                        {
                                categories << it->category;
                                break;
                        }
                }
                for ( const auto& s : mSymbols )
                {
                        if ( s.custom && !categories.contains( s.category ) )
                        {
                                categories << s.category;
                        }
                }
                if ( !categories.contains( tr("My Symbols") ) )
                {
                        categories << tr("My Symbols");
                }

                AddSymbolDialog dialog( categories, this );
                if ( !fileName.isEmpty() )
                {
                        dialog.setFile( fileName );
                }
                if ( dialog.exec() != QDialog::Accepted )
                {
                        return;
                }

                const CustomSymbols::Entry added = dialog.added();
                mSearch->clear();
                populate( added.id, added.category );
                mStatus->setText( tr("Added \"%1\" to %2. It will be here every time you open gLabels.")
                                  .arg( added.name, added.category ) );
        }


        void SymbolsDialog::onRemoveCustom()
        {
                const Symbol* selected = selectedSymbol();
                if ( !selected || !selected->custom )
                {
                        return;
                }
                const Symbol s = *selected;

                auto answer = QMessageBox::question( this, tr("Remove Symbol"),
                                                     tr("Remove \"%1\" from your symbols?\n\n"
                                                        "Labels that already use it are not changed.").arg( s.name ),
                                                     QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel );
                if ( answer != QMessageBox::Yes )
                {
                        return;
                }

                QString error;
                if ( !CustomSymbols::remove( s.id, &error ) )
                {
                        QMessageBox::warning( this, tr("Remove Symbol"), error );
                        return;
                }
                populate( QString() );
                mStatus->setText( tr("Removed \"%1\".").arg( s.name ) );
        }


        void SymbolsDialog::dragEnterEvent( QDragEnterEvent* event )
        {
                const QMimeData* mime = event->mimeData();
                if ( mime->hasUrls() && !mime->urls().isEmpty() && isSvgFile( mime->urls().first() ) )
                {
                        event->acceptProposedAction();
                }
        }


        void SymbolsDialog::dropEvent( QDropEvent* event )
        {
                const auto urls = event->mimeData()->urls();
                event->acceptProposedAction();
                for ( const QUrl& url : urls )
                {
                        if ( isSvgFile( url ) )
                        {
                                addCustomFromFile( url.toLocalFile() );
                        }
                }
        }

}
