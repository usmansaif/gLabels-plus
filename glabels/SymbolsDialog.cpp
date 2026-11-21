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

#include "model/Distance.hpp"
#include "model/Settings.hpp"
#include "model/Units.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSvgRenderer>
#include <QVBoxLayout>


namespace
{
        const int    iconPx         = 64;
        const double defaultSizePts = 10.0 / 25.4 * 72.0;    // 10 mm


        ///
        /// The symbol on a white tile, so black symbols show in dark mode too
        ///
        QIcon symbolIcon( const QString& path, qreal dpr )
        {
                QPixmap pixmap( QSize( iconPx, iconPx ) * dpr );
                pixmap.setDevicePixelRatio( dpr );
                pixmap.fill( Qt::transparent );

                QPainter painter( &pixmap );
                painter.setRenderHint( QPainter::Antialiasing );
                QPainterPath tile;
                tile.addRoundedRect( QRectF( 0.5, 0.5, iconPx - 1, iconPx - 1 ), 8, 8 );
                painter.fillPath( tile, Qt::white );
                painter.setPen( QPen( QColor( 0, 0, 0, 40 ), 1 ) );
                painter.drawPath( tile );

                QSvgRenderer renderer( path );
                renderer.render( &painter, QRectF( 12, 12, iconPx - 24, iconPx - 24 ) );
                painter.end();

                return QIcon( pixmap );
        }
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
                                list << s;
                        }
                }
                return list;
        }


        SymbolsDialog::SymbolsDialog( QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( tr("Insert Symbol") );
                setModal( false );
                setSizeGripEnabled( true );

                mSymbols = symbols();

                // Search and category filter
                mSearch = new QLineEdit;
                mSearch->setPlaceholderText( tr("Search symbols (fragile, phone, veg...)") );
                mSearch->setClearButtonEnabled( true );

                mCategory = new QComboBox;
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

                qreal dpr = devicePixelRatioF();
                for ( int i = 0; i < mSymbols.size(); i++ )
                {
                        const Symbol& s = mSymbols[i];
                        auto* item = new QListWidgetItem( symbolIcon( ":/symbols/" + s.id + ".svg", dpr ), s.name );
                        item->setData( Qt::UserRole, i );
                        item->setToolTip( QString( "%1 · %2" ).arg( s.name, s.category ) );
                        item->setTextAlignment( Qt::AlignHCenter | Qt::AlignTop );
                        mList->addItem( item );
                }

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
                mInsertButton = buttons->addButton( tr("&Insert"), QDialogButtonBox::AcceptRole );
                mInsertButton->setDefault( true );
                mInsertButton->setEnabled( false );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );
                connect( mInsertButton, &QPushButton::clicked, this, &SymbolsDialog::onInsert );

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
                connect( mSize, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), this, [units]( double value ) {
                        QSettings().setValue( "symbols/size", model::Distance( value, units ).pt() );
                } );

                // Remember the last category
                int categoryIndex = mCategory->findData( settings.value( "symbols/category" ).toString() );
                mCategory->setCurrentIndex( categoryIndex < 0 ? 0 : categoryIndex );
                connect( mCategory, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]() {
                        QSettings().setValue( "symbols/category", mCategory->currentData().toString() );
                } );
                onFilterChanged();

                resize( 660, 580 );
                mSearch->setFocus();
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
                mInsertButton->setEnabled( !mList->selectedItems().isEmpty() );
        }


        void SymbolsDialog::onInsert()
        {
                const auto selected = mList->selectedItems();
                if ( selected.isEmpty() )
                {
                        return;
                }
                const Symbol& s = mSymbols[ selected.first()->data( Qt::UserRole ).toInt() ];

                QFile file( ":/symbols/" + s.id + ".svg" );
                if ( !file.open( QFile::ReadOnly ) )
                {
                        return;
                }

                double size = model::Distance( mSize->value(), model::Settings::units() ).pt();
                emit insertRequested( s.id, s.name, file.readAll(), size );

                mStatus->setText( tr("Inserted \"%1\". Choose another symbol, or close this window.").arg( s.name ) );
        }

}
