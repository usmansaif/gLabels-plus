//  VariablesPanel.cpp
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

#include "VariablesPanel.hpp"

#include "UndoRedoModel.hpp"
#include "VariableTypeIcon.hpp"

#include "model/Model.hpp"

#include "merge/DataSourceUtil.hpp"
#include "merge/Factory.hpp"

#include <QApplication>
#include <QClipboard>
#include <QStyle>
#include <QDrag>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>


namespace
{
        enum Roles { NameRole = Qt::UserRole + 1, TypeRole };


        ///
        /// List widget that drags variables as our own MIME type
        ///
        class VariableListWidget : public QListWidget
        {
        public:
                using QListWidget::QListWidget;

        protected:
                QStringList mimeTypes() const override
                {
                        return { glabels::VariablesPanel::MimeType, "text/plain" };
                }

                QMimeData* mimeData( const QList<QListWidgetItem*>& items ) const override
                {
                        if ( items.isEmpty() )
                        {
                                return nullptr;
                        }
                        auto* data = new QMimeData();
                        QString name = items.first()->data( NameRole ).toString();
                        QString type = items.first()->data( TypeRole ).toString();
                        data->setData( glabels::VariablesPanel::MimeType, ( name + "\n" + type ).toUtf8() );
                        data->setText( "${" + name + "}" );
                        return data;
                }

                void startDrag( Qt::DropActions supportedActions ) override
                {
                        QListWidgetItem* item = currentItem();
                        if ( !item )
                        {
                                return;
                        }
                        auto* drag = new QDrag( this );
                        drag->setMimeData( mimeData( { item } ) );

                        // Drag pixmap: a "pill" with the type icon and {{name}}
                        QString text = "{{" + item->data( NameRole ).toString() + "}}";
                        QFontMetrics fm( font() );
                        int h = fm.height() + 10;
                        int w = fm.horizontalAdvance( text ) + h + 14;
                        qreal dpr = devicePixelRatioF();
                        QPixmap pixmap( QSize( w, h ) * dpr );
                        pixmap.setDevicePixelRatio( dpr );
                        pixmap.fill( Qt::transparent );
                        QPainter p( &pixmap );
                        p.setRenderHint( QPainter::Antialiasing );
                        p.setPen( QColor( 52, 101, 164 ) );
                        p.setBrush( QColor( 230, 238, 250 ) );
                        p.drawRoundedRect( QRectF( 0.5, 0.5, w - 1, h - 1 ), h/2.0, h/2.0 );
                        item->icon().paint( &p, QRect( 6, 5, h - 10, h - 10 ) );
                        p.setPen( QColor( 30, 50, 90 ) );
                        p.drawText( QRect( h, 0, w - h - 6, h ), Qt::AlignVCenter | Qt::AlignLeft, text );
                        p.end();
                        drag->setPixmap( pixmap );
                        drag->setHotSpot( QPoint( h/2, h/2 ) );

                        drag->exec( supportedActions & Qt::CopyAction ? Qt::CopyAction : supportedActions, Qt::CopyAction );
                }
        };


        ///
        /// Mapping of a merge, made explicit (identity mapping when none is set)
        ///
        QList<glabels::merge::Field> explicitFields( const glabels::merge::Merge* merge )
        {
                QList<glabels::merge::Field> fields = merge->fields();
                if ( fields.isEmpty() )
                {
                        for ( const auto& f : glabels::merge::DataSourceUtil::defaultFields( merge ) )
                        {
                                glabels::merge::Field id = f;
                                id.name = f.source;   // keep the names the label already uses
                                fields << id;
                        }
                }
                return fields;
        }
}


namespace glabels
{

        const QString VariablesPanel::MimeType = QStringLiteral( "application/x-glabels-variable" );


        ///
        /// Constructor
        ///
        VariablesPanel::VariablesPanel( QWidget* parent ) : QWidget( parent )
        {
                auto* layout = new QVBoxLayout( this );
                layout->setContentsMargins( 6, 6, 6, 6 );
                layout->setSpacing( 6 );

                // Header: data source and actions
                auto* header = new QHBoxLayout();
                mSourceLabel = new QLabel();
                mSourceLabel->setWordWrap( true );
                mSourceLabel->setStyleSheet( "color: palette(placeholder-text);" );
                mConnectButton = new QToolButton();
                mConnectButton->setIcon( QIcon::fromTheme( "glabels-merge" ) );
                mConnectButton->setToolTip( tr("Connect Data Source...") );
                mConnectButton->setAutoRaise( true );
                connect( mConnectButton, &QToolButton::clicked, this, &VariablesPanel::connectRequested );
                mReloadButton = new QToolButton();
                mReloadButton->setIcon( QIcon::fromTheme( "view-refresh", style()->standardIcon( QStyle::SP_BrowserReload ) ) );
                mReloadButton->setToolTip( tr("Reload data from the file") );
                mReloadButton->setAutoRaise( true );
                connect( mReloadButton, &QToolButton::clicked, this, [this]{
                        if ( mModel && mModel->merge() ) mModel->merge()->reloadSource();
                } );
                header->addWidget( mSourceLabel, 1 );
                header->addWidget( mReloadButton );
                header->addWidget( mConnectButton );
                layout->addLayout( header );

                mStack = new QStackedWidget();

                // Page 0: empty state
                auto* empty = new QWidget();
                auto* emptyLayout = new QVBoxLayout( empty );
                emptyLayout->addStretch( 1 );
                auto* emptyLabel = new QLabel( tr("No data source connected.\n\nImport a CSV, TSV or JSON file to "
                                                  "turn its columns into variables you can drag onto the label.") );
                emptyLabel->setWordWrap( true );
                emptyLabel->setAlignment( Qt::AlignCenter );
                emptyLabel->setStyleSheet( "color: palette(placeholder-text);" );
                auto* emptyButton = new QPushButton( tr("Connect Data Source...") );
                connect( emptyButton, &QPushButton::clicked, this, &VariablesPanel::connectRequested );
                emptyLayout->addWidget( emptyLabel );
                emptyLayout->addSpacing( 8 );
                emptyLayout->addWidget( emptyButton, 0, Qt::AlignCenter );
                emptyLayout->addStretch( 2 );
                mStack->addWidget( empty );

                // Page 1: variable list
                auto* listPage = new QWidget();
                auto* listLayout = new QVBoxLayout( listPage );
                listLayout->setContentsMargins( 0, 0, 0, 0 );
                mList = new VariableListWidget();
                mList->setObjectName( "variablesPanelList" );
                mList->setDragEnabled( true );
                mList->setDragDropMode( QAbstractItemView::DragOnly );
                mList->setDefaultDropAction( Qt::CopyAction );
                mList->setSelectionMode( QAbstractItemView::SingleSelection );
                mList->setEditTriggers( QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked );
                mList->setIconSize( QSize( 20, 20 ) );
                mList->setSpacing( 1 );
                mList->setContextMenuPolicy( Qt::CustomContextMenu );
                mList->setStyleSheet( "#variablesPanelList::item { padding: 4px 2px; }" );
                connect( mList, &QListWidget::itemChanged, this, &VariablesPanel::onItemChanged );
                connect( mList, &QWidget::customContextMenuRequested, this, &VariablesPanel::onContextMenu );
                connect( mList, &QListWidget::itemDoubleClicked, this, [this]( QListWidgetItem* item ) {
                        if ( item ) emit addVariableRequested( item->data( NameRole ).toString(), item->data( TypeRole ).toString() );
                } );
                listLayout->addWidget( mList, 1 );

                auto* hint = new QLabel( tr("Drag a variable onto the label, or double-click to add it. "
                                            "Press F2 to rename.") );
                hint->setWordWrap( true );
                hint->setStyleSheet( "color: palette(placeholder-text); font-size: 9pt;" );
                listLayout->addWidget( hint );
                mStack->addWidget( listPage );

                layout->addWidget( mStack, 1 );

                mMessageLabel = new QLabel();
                mMessageLabel->setWordWrap( true );
                mMessageLabel->setStyleSheet( "color: #c0392b;" );
                mMessageLabel->setVisible( false );
                layout->addWidget( mMessageLabel );

                reload();
        }


        ///
        /// Set model
        ///
        void VariablesPanel::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                if ( mModel )
                {
                        disconnect( mModel, nullptr, this, nullptr );
                }
                mModel = model;
                mUndoRedoModel = undoRedoModel;
                if ( mModel )
                {
                        connect( mModel, &model::Model::mergeChanged, this, &VariablesPanel::reload );
                        connect( mModel, &model::Model::mergeSourceChanged, this, &VariablesPanel::reload );
                        connect( mModel, &model::Model::mergeSelectionChanged, this, &VariablesPanel::reload );
                }
                reload();
        }


        ///
        /// Reload list from the model
        ///
        void VariablesPanel::reload()
        {
                mBlocked = true;
                mList->clear();

                merge::Merge* merge = mModel ? mModel->merge() : nullptr;
                bool connected = merge && merge::Factory::idToType( merge->id() ) == merge::Factory::FILE &&
                                 !merge->source().isEmpty();

                if ( !connected )
                {
                        mSourceLabel->setText( tr("Data source: none") );
                        mReloadButton->setEnabled( false );
                        mConnectButton->setToolTip( tr("Connect Data Source...") );
                        mStack->setCurrentIndex( 0 );
                        mBlocked = false;
                        return;
                }

                mReloadButton->setEnabled( true );
                mConnectButton->setToolTip( tr("Change data source or variables...") );
                QFileInfo info( merge->source() );
                mSourceLabel->setText( QString( "<b>%1</b><br>%2" )
                                       .arg( info.fileName().toHtmlEscaped(),
                                             tr("%1 of %2 records selected").arg( merge->nSelectedRecords() )
                                                                             .arg( merge->recordList().size() ) ) );
                mSourceLabel->setToolTip( merge->source() );

                const auto& records = merge->recordList();
                for ( const auto& name : merge->keys() )
                {
                        merge::Field f = merge->field( name );
                        QString type = f.type.isEmpty() ? merge::DataSourceUtil::TYPE_TEXT : f.type;

                        QString sample;
                        for ( const auto& record : records )
                        {
                                sample = record.value( name );
                                if ( !sample.trimmed().isEmpty() ) break;
                        }

                        auto* item = new QListWidgetItem( VariableType::icon( type ), name );
                        item->setData( NameRole, name );
                        item->setData( TypeRole, type );
                        item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled );
                        item->setToolTip( QString( "<b>{{%1}}</b><br>%2: %3<br>%4: %5<br>%6: %7" )
                                          .arg( name.toHtmlEscaped(),
                                                tr("Type"), VariableType::displayName( type ),
                                                tr("Column"), ( f.source.isEmpty() ? name : f.source ).toHtmlEscaped(),
                                                tr("Sample"), sample.left( 60 ).toHtmlEscaped() ) );
                        mList->addItem( item );
                }

                mStack->setCurrentIndex( 1 );
                mBlocked = false;
        }


        ///
        /// Item renamed in place
        ///
        void VariablesPanel::onItemChanged( QListWidgetItem* item )
        {
                if ( mBlocked || !item || !mModel )
                {
                        return;
                }

                QString oldName = item->data( NameRole ).toString();
                QString newName = item->text().trimmed();
                if ( newName == oldName )
                {
                        return;
                }

                QString error;
                if ( !renameVariable( mModel, mUndoRedoModel, oldName, newName, &error ) )
                {
                        mBlocked = true;
                        item->setText( oldName );
                        mBlocked = false;
                        mMessageLabel->setText( error );
                        mMessageLabel->setVisible( true );
                        return;
                }
                mMessageLabel->setVisible( false );
        }


        ///
        /// Context menu
        ///
        void VariablesPanel::onContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mList->itemAt( pos );
                if ( !item || !mModel )
                {
                        return;
                }
                QString name = item->data( NameRole ).toString();
                QString type = item->data( TypeRole ).toString();

                QMenu menu;
                menu.addAction( tr("Add to Label"), this, [this, name, type]{ emit addVariableRequested( name, type ); } );
                menu.addAction( tr("Rename"), this, [this, item]{ mList->editItem( item ); } );
                auto* typeMenu = menu.addMenu( tr("Data Type") );
                for ( const auto& t : VariableType::types() )
                {
                        auto* action = typeMenu->addAction( VariableType::icon( t ), VariableType::displayName( t ) );
                        action->setCheckable( true );
                        action->setChecked( t == type );
                        connect( action, &QAction::triggered, this, [this, name, t]{
                                setVariableType( mModel, mUndoRedoModel, name, t );
                        } );
                }
                menu.addSeparator();
                menu.addAction( tr("Copy Placeholder"), this, [name]{
                        QApplication::clipboard()->setText( "{{" + name + "}}" );
                } );
                menu.exec( mList->viewport()->mapToGlobal( pos ) );
        }


        ///
        /// Rename variable everywhere
        ///
        bool VariablesPanel::renameVariable( model::Model* model, UndoRedoModel* undoRedoModel,
                                             const QString& oldName, const QString& newName, QString* error )
        {
                if ( !model || !model->merge() )
                {
                        return false;
                }
                merge::Merge* merge = model->merge();

                auto fail = [error]( const QString& message ) {
                        if ( error ) *error = message;
                        return false;
                };

                if ( !merge::DataSourceUtil::isValidName( newName ) )
                {
                        return fail( tr("\"%1\" is not a valid name. Use only letters, digits and underscores.").arg( newName ) );
                }
                for ( const auto& key : merge->keys() )
                {
                        if ( key != oldName && key.compare( newName, Qt::CaseInsensitive ) == 0 )
                        {
                                return fail( tr("A variable named \"%1\" already exists.").arg( key ) );
                        }
                }
                if ( model->variables().contains( newName ) )
                {
                        return fail( tr("A custom variable named \"%1\" already exists.").arg( newName ) );
                }

                QList<merge::Field> fields = explicitFields( merge );
                bool found = false;
                for ( auto& f : fields )
                {
                        if ( f.include && f.name == oldName )
                        {
                                f.name = newName;
                                found = true;
                                break;
                        }
                }
                if ( !found )
                {
                        return fail( tr("Variable \"%1\" was not found.").arg( oldName ) );
                }

                if ( undoRedoModel )
                {
                        undoRedoModel->checkpoint( tr("Rename Variable") );
                }

                merge::Merge* newMerge = merge->clone();
                newMerge->setFields( fields );
                model->setMerge( newMerge );
                model->renameVariableReferences( oldName, newName );
                return true;
        }


        ///
        /// Change type of variable
        ///
        void VariablesPanel::setVariableType( model::Model* model, UndoRedoModel* undoRedoModel,
                                              const QString& name, const QString& type )
        {
                if ( !model || !model->merge() )
                {
                        return;
                }
                QList<merge::Field> fields = explicitFields( model->merge() );
                for ( auto& f : fields )
                {
                        if ( f.include && f.name == name )
                        {
                                if ( f.type == type )
                                {
                                        return;
                                }
                                f.type = type;
                                if ( undoRedoModel )
                                {
                                        undoRedoModel->checkpoint( tr("Variable Type") );
                                }
                                merge::Merge* newMerge = model->merge()->clone();
                                newMerge->setFields( fields );
                                model->setMerge( newMerge );
                                return;
                        }
                }
        }

}
