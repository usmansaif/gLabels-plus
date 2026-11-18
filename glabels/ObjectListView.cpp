//  ObjectListView.cpp
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

#include "ObjectListView.hpp"

#include "UndoRedoModel.hpp"

#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelTextObject.hpp"

#include <QHeaderView>
#include <QVBoxLayout>


namespace
{
        enum Columns { NAME_COLUMN = 0, LOCK_COLUMN = 1 };
        const int ObjectIdRole = Qt::UserRole + 1;
}


namespace glabels
{

        ObjectListView::ObjectListView( QWidget* parent )
                : QWidget(parent)
        {
                mTree = new QTreeWidget();
                mTree->setColumnCount( 2 );
                mTree->setHeaderLabels( { tr("Object"), QString() } );
                mTree->headerItem()->setIcon( LOCK_COLUMN, QIcon::fromTheme( "glabels-lock" ) );
                mTree->headerItem()->setToolTip( LOCK_COLUMN, tr("Locked objects cannot be selected or moved on the canvas") );
                mTree->setRootIsDecorated( false );
                mTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
                mTree->setUniformRowHeights( true );
                mTree->header()->setStretchLastSection( false );
                mTree->header()->setSectionResizeMode( NAME_COLUMN, QHeaderView::Stretch );
                mTree->header()->setSectionResizeMode( LOCK_COLUMN, QHeaderView::ResizeToContents );
                mTree->setToolTip( tr("Objects from top-most to bottom-most.\nClick the lock column to lock or unlock an object.") );

                mToolBar = new QToolBar();
                mToolBar->setIconSize( QSize( 16, 16 ) );

                auto* layout = new QVBoxLayout();
                layout->setContentsMargins( 2, 2, 2, 2 );
                layout->setSpacing( 2 );
                layout->addWidget( mTree );
                layout->addWidget( mToolBar );
                setLayout( layout );

                connect( mTree, &QTreeWidget::itemSelectionChanged, this, &ObjectListView::onItemSelectionChanged );
                connect( mTree, &QTreeWidget::itemClicked, this, &ObjectListView::onItemClicked );

                setEnabled( false );
        }


        void ObjectListView::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                mModel = model;
                mUndoRedoModel = undoRedoModel;

                if ( mModel )
                {
                        connect( mModel, &model::Model::changed, this, &ObjectListView::rebuild );
                        connect( mModel, &model::Model::selectionChanged, this, &ObjectListView::syncSelectionFromModel );
                }

                setEnabled( mModel != nullptr );
                rebuild();
        }


        void ObjectListView::addToolBarActions( const QList<QAction*>& actions )
        {
                for ( auto* action : actions )
                {
                        if ( action )
                        {
                                mToolBar->addAction( action );
                        }
                        else
                        {
                                mToolBar->addSeparator();
                        }
                }
        }


        QString ObjectListView::describe( const model::ModelObject* object )
        {
                auto snippet = []( QString text ) {
                        text = text.section( '\n', 0, 0 ).simplified();
                        if ( text.size() > 28 )
                        {
                                text = text.left( 27 ) + QChar( 0x2026 );
                        }
                        return text;
                };

                if ( dynamic_cast<const model::ModelTextObject*>( object ) )
                {
                        QString t = snippet( object->text() );
                        return t.isEmpty() ? tr("Text") : tr("Text: %1").arg( t );
                }
                if ( dynamic_cast<const model::ModelBarcodeObject*>( object ) )
                {
                        QString t = snippet( object->bcData() );
                        return t.isEmpty() ? tr("Barcode") : tr("Barcode: %1").arg( t );
                }
                if ( dynamic_cast<const model::ModelImageObject*>( object ) )
                {
                        return tr("Image");
                }
                if ( dynamic_cast<const model::ModelLineObject*>( object ) )
                {
                        return tr("Line");
                }
                if ( dynamic_cast<const model::ModelEllipseObject*>( object ) )
                {
                        return tr("Ellipse");
                }
                if ( dynamic_cast<const model::ModelBoxObject*>( object ) )
                {
                        return tr("Box");
                }
                return tr("Object");
        }


        QIcon ObjectListView::iconFor( const model::ModelObject* object )
        {
                if ( dynamic_cast<const model::ModelTextObject*>( object ) )    return QIcon::fromTheme( "glabels-text" );
                if ( dynamic_cast<const model::ModelBarcodeObject*>( object ) ) return QIcon::fromTheme( "glabels-barcode" );
                if ( dynamic_cast<const model::ModelImageObject*>( object ) )   return QIcon::fromTheme( "glabels-image" );
                if ( dynamic_cast<const model::ModelLineObject*>( object ) )    return QIcon::fromTheme( "glabels-line" );
                if ( dynamic_cast<const model::ModelEllipseObject*>( object ) ) return QIcon::fromTheme( "glabels-ellipse" );
                return QIcon::fromTheme( "glabels-box" );
        }


        model::ModelObject* ObjectListView::objectFor( QTreeWidgetItem* item ) const
        {
                if ( !item || !mModel )
                {
                        return nullptr;
                }
                quintptr id = item->data( NAME_COLUMN, ObjectIdRole ).value<quintptr>();
                for ( auto* object : mModel->objectList() )
                {
                        if ( reinterpret_cast<quintptr>( object ) == id )
                        {
                                return object;
                        }
                }
                return nullptr;
        }


        void ObjectListView::rebuild()
        {
                mBlocked = true;
                mTree->clear();

                if ( mModel )
                {
                        const auto& list = mModel->objectList();
                        for ( int i = list.size() - 1; i >= 0; i-- )
                        {
                                auto* object = list[i];
                                auto* item = new QTreeWidgetItem();
                                item->setText( NAME_COLUMN, describe( object ) );
                                item->setIcon( NAME_COLUMN, iconFor( object ) );
                                item->setData( NAME_COLUMN, ObjectIdRole, QVariant::fromValue( reinterpret_cast<quintptr>( object ) ) );
                                item->setIcon( LOCK_COLUMN, QIcon::fromTheme( object->isLocked() ? "glabels-lock" : "glabels-unlock" ) );
                                item->setToolTip( LOCK_COLUMN, object->isLocked() ? tr("Locked (click to unlock)") : tr("Unlocked (click to lock)") );
                                if ( object->isLocked() )
                                {
                                        QFont f = item->font( NAME_COLUMN );
                                        f.setItalic( true );
                                        item->setFont( NAME_COLUMN, f );
                                        item->setForeground( NAME_COLUMN, palette().brush( QPalette::Disabled, QPalette::Text ) );
                                }
                                mTree->addTopLevelItem( item );
                                item->setSelected( object->isSelected() );
                        }
                }

                mBlocked = false;
        }


        void ObjectListView::syncSelectionFromModel()
        {
                if ( mBlocked || !mModel )
                {
                        return;
                }

                mBlocked = true;
                for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
                {
                        auto* item = mTree->topLevelItem( i );
                        auto* object = objectFor( item );
                        item->setSelected( object && object->isSelected() );
                        if ( object && object->isSelected() )
                        {
                                mTree->scrollToItem( item );
                        }
                }
                mBlocked = false;
        }


        void ObjectListView::onItemSelectionChanged()
        {
                if ( mBlocked || !mModel )
                {
                        return;
                }

                mBlocked = true;
                mModel->unselectAll();
                for ( auto* item : mTree->selectedItems() )
                {
                        if ( auto* object = objectFor( item ) )
                        {
                                mModel->selectObject( object );
                        }
                }
                mBlocked = false;
        }


        void ObjectListView::onItemClicked( QTreeWidgetItem* item, int column )
        {
                if ( column != LOCK_COLUMN || !mModel )
                {
                        return;
                }

                if ( auto* object = objectFor( item ) )
                {
                        bool lock = !object->isLocked();
                        if ( mUndoRedoModel )
                        {
                                mUndoRedoModel->checkpoint( lock ? tr("Lock") : tr("Unlock") );
                        }
                        mModel->setObjectLocked( object, lock );
                }
        }

}
