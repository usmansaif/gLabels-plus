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
#include <QPointer>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>


namespace
{
        enum Columns { NAME_COLUMN = 0, LOCK_COLUMN = 1 };
        const int RowHeight = 24;
        const int ObjectIdRole = Qt::UserRole + 1;
        const int GroupRole    = Qt::UserRole + 2;
}


namespace glabels
{

        ObjectListView::ObjectListView( QWidget* parent )
                : QWidget(parent)
        {
                //
                // Actions owned by the panel (shared editor actions are added by MainWindow)
                //
                mLockAction = new QAction( QIcon::fromTheme( "glabels-lock" ), tr("&Lock"), this );
                mLockAction->setToolTip( tr("Lock or unlock the selected objects") );
                connect( mLockAction, &QAction::triggered, this, &ObjectListView::toggleLock );

                mRenameAction = new QAction( QIcon::fromTheme( "glabels-edit" ), tr("&Rename"), this );
                mRenameAction->setToolTip( tr("Rename the selected object or group (F2)") );
                mRenameAction->setShortcut( QKeySequence( Qt::Key_F2 ) );
                mRenameAction->setShortcutContext( Qt::WidgetWithChildrenShortcut );
                connect( mRenameAction, &QAction::triggered, this, &ObjectListView::renameCurrent );

                mDuplicateAction = new QAction( QIcon::fromTheme( "glabels-edit-copy" ), tr("D&uplicate"), this );
                mDuplicateAction->setToolTip( tr("Duplicate the selected objects (Ctrl+D)") );
                mDuplicateAction->setStatusTip( tr("Duplicate the selected objects") );
                mDuplicateAction->setShortcut( QKeySequence( tr("Ctrl+D") ) );
                connect( mDuplicateAction, &QAction::triggered, this, &ObjectListView::duplicateSelection );

                mGroupAction = new QAction( QIcon::fromTheme( "glabels-group" ), tr("&Group"), this );
                mGroupAction->setToolTip( tr("Put the selected objects into a layer group (Ctrl+G)") );
                mGroupAction->setShortcut( QKeySequence( tr("Ctrl+G") ) );
                connect( mGroupAction, &QAction::triggered, this, &ObjectListView::groupSelection );

                mUngroupAction = new QAction( QIcon::fromTheme( "glabels-ungroup" ), tr("U&ngroup"), this );
                mUngroupAction->setToolTip( tr("Remove the selected objects from their group (Ctrl+Shift+G)") );
                mUngroupAction->setShortcut( QKeySequence( tr("Ctrl+Shift+G") ) );
                connect( mUngroupAction, &QAction::triggered, this, &ObjectListView::ungroupSelection );

                //
                // Toolbar (top) and layer tree
                //
                mToolBar = new QToolBar();
                mToolBar->setIconSize( QSize( 16, 16 ) );

                mTree = new QTreeWidget();
                mTree->setColumnCount( 2 );
                mTree->setHeaderLabels( { tr("Object"), QString() } );
                mTree->headerItem()->setIcon( LOCK_COLUMN, QIcon::fromTheme( "glabels-lock" ) );
                mTree->headerItem()->setToolTip( LOCK_COLUMN, tr("Locked objects cannot be selected or moved on the canvas") );
                mTree->setRootIsDecorated( true );
                mTree->setIndentation( 14 );
                mTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
                mTree->setUniformRowHeights( true );
                mTree->header()->setStretchLastSection( false );
                mTree->header()->setSectionResizeMode( NAME_COLUMN, QHeaderView::Stretch );
                mTree->header()->setSectionResizeMode( LOCK_COLUMN, QHeaderView::ResizeToContents );
                // Editing is started explicitly so only the name column can be edited
                mTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
                mTree->setToolTip( tr("Objects from top-most to bottom-most.\n"
                                      "Double-click a name (or press F2) to rename it.\n"
                                      "Click the lock column to lock or unlock an object.") );
                mTree->addAction( mRenameAction );

                auto* layout = new QVBoxLayout();
                layout->setContentsMargins( 2, 2, 2, 2 );
                layout->setSpacing( 2 );
                layout->addWidget( mToolBar );
                layout->addWidget( mTree );
                setLayout( layout );

                connect( mTree, &QTreeWidget::itemSelectionChanged, this, &ObjectListView::onItemSelectionChanged );
                connect( mTree, &QTreeWidget::itemClicked, this, &ObjectListView::onItemClicked );
                connect( mTree, &QTreeWidget::itemChanged, this, &ObjectListView::onItemChanged );
                connect( mTree, &QTreeWidget::itemDoubleClicked, this, [this]( QTreeWidgetItem* item, int column )
                {
                        if ( column == NAME_COLUMN )
                        {
                                mTree->editItem( item, NAME_COLUMN );
                        }
                } );
                connect( mTree, &QTreeWidget::itemExpanded, this, [this]( QTreeWidgetItem* item )
                {
                        if ( !mBlocked && isGroupItem( item ) )
                        {
                                mCollapsedGroups.remove( item->data( NAME_COLUMN, GroupRole ).toString() );
                        }
                } );
                connect( mTree, &QTreeWidget::itemCollapsed, this, [this]( QTreeWidgetItem* item )
                {
                        if ( !mBlocked && isGroupItem( item ) )
                        {
                                mCollapsedGroups.insert( item->data( NAME_COLUMN, GroupRole ).toString() );
                        }
                } );

                mTree->setContextMenuPolicy( Qt::CustomContextMenu );
                connect( mTree, &QWidget::customContextMenuRequested, this, &ObjectListView::onContextMenuRequested );

                updateActions();
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


        ///
        /// Editor actions shown at the end of the right-click menu
        ///
        void ObjectListView::setContextMenu( QMenu* menu )
        {
                mContextMenu = menu;
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


        bool ObjectListView::isGroupItem( const QTreeWidgetItem* item )
        {
                return item && item->data( NAME_COLUMN, GroupRole ).isValid();
        }


        model::ModelObject* ObjectListView::objectFor( QTreeWidgetItem* item ) const
        {
                if ( !item || !mModel || isGroupItem( item ) )
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


        ///
        /// Objects behind tree items (a group item stands for all its members),
        /// without duplicates, in stacking order
        ///
        QList<model::ModelObject*> ObjectListView::objectsFor( const QList<QTreeWidgetItem*>& items ) const
        {
                QSet<model::ModelObject*> wanted;
                for ( auto* item : items )
                {
                        if ( isGroupItem( item ) )
                        {
                                for ( int i = 0; i < item->childCount(); i++ )
                                {
                                        if ( auto* object = objectFor( item->child( i ) ) )
                                        {
                                                wanted.insert( object );
                                        }
                                }
                        }
                        else if ( auto* object = objectFor( item ) )
                        {
                                wanted.insert( object );
                        }
                }

                QList<model::ModelObject*> result;
                if ( mModel )
                {
                        for ( auto* object : mModel->objectList() )
                        {
                                if ( wanted.contains( object ) )
                                {
                                        result << object;
                                }
                        }
                }
                return result;
        }


        QList<model::ModelObject*> ObjectListView::selectedObjects() const
        {
                return objectsFor( mTree->selectedItems() );
        }


        ///
        /// Rebuild the tree.  Consecutive objects (in stacking order) that share a
        /// group name are shown under one collapsible group row.
        ///
        void ObjectListView::rebuild()
        {
                mBlocked = true;
                mTree->clear();

                if ( mModel )
                {
                        QTreeWidgetItem* groupItem = nullptr;

                        const auto& list = mModel->objectList();
                        for ( int i = list.size() - 1; i >= 0; i-- )
                        {
                                auto* object = list[i];

                                auto* item = new QTreeWidgetItem();
                                item->setText( NAME_COLUMN, object->name().isEmpty() ? describe( object ) : object->name() );
                                item->setToolTip( NAME_COLUMN, describe( object ) );
                                item->setFlags( item->flags() | Qt::ItemIsEditable );
                                item->setSizeHint( NAME_COLUMN, QSize( 0, RowHeight ) );
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

                                const QString group = object->group();
                                if ( group.isEmpty() )
                                {
                                        groupItem = nullptr;
                                        mTree->addTopLevelItem( item );
                                }
                                else
                                {
                                        if ( !groupItem || groupItem->data( NAME_COLUMN, GroupRole ).toString() != group )
                                        {
                                                groupItem = new QTreeWidgetItem();
                                                groupItem->setText( NAME_COLUMN, group );
                                                groupItem->setToolTip( NAME_COLUMN, tr("Layer group") );
                                                groupItem->setIcon( NAME_COLUMN, QIcon::fromTheme( "glabels-group" ) );
                                                groupItem->setData( NAME_COLUMN, GroupRole, group );
                                                groupItem->setFlags( groupItem->flags() | Qt::ItemIsEditable );
                                                groupItem->setSizeHint( NAME_COLUMN, QSize( 0, RowHeight ) );
                                                QFont f = groupItem->font( NAME_COLUMN );
                                                f.setBold( true );
                                                groupItem->setFont( NAME_COLUMN, f );
                                                mTree->addTopLevelItem( groupItem );
                                        }
                                        groupItem->addChild( item );
                                }
                        }

                        // Group rows: lock state of their members, remembered expansion
                        for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
                        {
                                auto* item = mTree->topLevelItem( i );
                                if ( !isGroupItem( item ) )
                                {
                                        continue;
                                }
                                bool allLocked = true;
                                for ( auto* object : objectsFor( { item } ) )
                                {
                                        allLocked = allLocked && object->isLocked();
                                }
                                item->setIcon( LOCK_COLUMN, QIcon::fromTheme( allLocked ? "glabels-lock" : "glabels-unlock" ) );
                                item->setToolTip( LOCK_COLUMN, allLocked ? tr("Group locked (click to unlock)")
                                                                         : tr("Click to lock the whole group") );
                                item->setExpanded( !mCollapsedGroups.contains( item->data( NAME_COLUMN, GroupRole ).toString() ) );
                        }
                }

                mBlocked = false;
                syncSelectionFromModel();
        }


        ///
        /// Mirror the editor's selection.  A group row is selected when all of
        /// its members are.
        ///
        void ObjectListView::syncSelectionFromModel()
        {
                if ( mBlocked || !mModel )
                {
                        return;
                }

                mBlocked = true;
                for ( QTreeWidgetItemIterator it( mTree ); *it; ++it )
                {
                        auto* item = *it;
                        if ( isGroupItem( item ) )
                        {
                                continue;
                        }
                        auto* object = objectFor( item );
                        bool selected = object && object->isSelected();
                        item->setSelected( selected );
                        if ( selected )
                        {
                                mTree->scrollToItem( item );
                        }
                }
                for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
                {
                        auto* item = mTree->topLevelItem( i );
                        if ( isGroupItem( item ) )
                        {
                                bool all = item->childCount() > 0;
                                for ( int c = 0; c < item->childCount(); c++ )
                                {
                                        all = all && item->child( c )->isSelected();
                                }
                                item->setSelected( all );
                        }
                }
                mBlocked = false;
                updateActions();
        }


        void ObjectListView::onItemSelectionChanged()
        {
                if ( mBlocked || !mModel )
                {
                        return;
                }

                mBlocked = true;
                mModel->unselectAll();
                for ( auto* object : selectedObjects() )
                {
                        mModel->selectObject( object );
                }
                mBlocked = false;
                updateActions();
        }


        ///
        /// Enable each toolbar action according to what is selected
        ///
        void ObjectListView::updateActions()
        {
                auto items = mTree->selectedItems();
                auto objects = selectedObjects();

                bool any = !objects.isEmpty();
                bool allLocked = any;
                bool anyGrouped = false;
                for ( auto* object : objects )
                {
                        allLocked = allLocked && object->isLocked();
                        anyGrouped = anyGrouped || !object->group().isEmpty();
                }

                mLockAction->setEnabled( any );
                mLockAction->setText( allLocked ? tr("&Unlock") : tr("&Lock") );
                mLockAction->setIcon( QIcon::fromTheme( allLocked ? "glabels-unlock" : "glabels-lock" ) );
                mLockAction->setToolTip( allLocked ? tr("Unlock the selected objects") : tr("Lock the selected objects") );

                mRenameAction->setEnabled( items.size() == 1 );
                mDuplicateAction->setEnabled( any );
                mGroupAction->setEnabled( any );
                mUngroupAction->setEnabled( anyGrouped );
        }


        void ObjectListView::onItemClicked( QTreeWidgetItem* item, int column )
        {
                if ( column != LOCK_COLUMN || !mModel )
                {
                        return;
                }

                auto objects = objectsFor( { item } );
                if ( objects.isEmpty() )
                {
                        return;
                }
                bool allLocked = true;
                for ( auto* object : objects )
                {
                        allLocked = allLocked && object->isLocked();
                }
                setLocked( objects, !allLocked );
        }


        void ObjectListView::setLocked( const QList<model::ModelObject*>& objects, bool lock )
        {
                if ( mUndoRedoModel )
                {
                        mUndoRedoModel->checkpoint( lock ? tr("Lock") : tr("Unlock") );
                }
                for ( auto* object : objects )
                {
                        mModel->setObjectLocked( object, lock );
                }
        }


        void ObjectListView::toggleLock()
        {
                auto objects = selectedObjects();
                if ( objects.isEmpty() || !mModel )
                {
                        return;
                }
                bool allLocked = true;
                for ( auto* object : objects )
                {
                        allLocked = allLocked && object->isLocked();
                }
                setLocked( objects, !allLocked );
        }


        ///
        /// Right-click: select the row (unless already selected) and show the
        /// panel actions followed by the editor's object actions
        ///
        void ObjectListView::onContextMenuRequested( const QPoint& pos )
        {
                QTreeWidgetItem* item = mTree->itemAt( pos );
                if ( !item || !mModel )
                {
                        return;
                }

                if ( !item->isSelected() )
                {
                        mTree->setCurrentItem( item );   // replaces the selection, syncs the model
                }
                updateActions();

                QMenu menu( this );
                menu.addAction( mRenameAction );
                menu.addAction( mLockAction );
                menu.addAction( mDuplicateAction );
                menu.addSeparator();
                menu.addAction( mGroupAction );
                menu.addAction( mUngroupAction );

                if ( mContextMenu )
                {
                        menu.addSeparator();
                        menu.addActions( mContextMenu->actions() );
                }

                menu.exec( mTree->viewport()->mapToGlobal( pos ) );
        }


        ///
        /// A name was edited in place.  For an object it becomes the object's
        /// name (empty = automatic name); for a group row it renames the group.
        /// Applied after the edit finishes, because it rebuilds the list.
        ///
        void ObjectListView::onItemChanged( QTreeWidgetItem* item, int column )
        {
                if ( mBlocked || !mModel || column != NAME_COLUMN )
                {
                        return;
                }

                QString text = item->text( NAME_COLUMN ).trimmed();

                if ( isGroupItem( item ) )
                {
                        QString oldName = item->data( NAME_COLUMN, GroupRole ).toString();
                        if ( text.isEmpty() || text == oldName )
                        {
                                QTimer::singleShot( 0, this, &ObjectListView::rebuild );   // restore the name
                                return;
                        }

                        QList<QPointer<model::ModelObject>> members;
                        for ( auto* object : objectsFor( { item } ) )
                        {
                                members << object;
                        }
                        bool collapsed = mCollapsedGroups.contains( oldName );
                        QTimer::singleShot( 0, this, [this, members, text, collapsed]()
                        {
                                if ( !mModel )
                                {
                                        return;
                                }
                                if ( mUndoRedoModel )
                                {
                                        mUndoRedoModel->checkpoint( tr("Rename Group") );
                                }
                                if ( collapsed )
                                {
                                        mCollapsedGroups.insert( text );
                                }
                                for ( const auto& object : members )
                                {
                                        if ( object )
                                        {
                                                object->setGroup( text );
                                        }
                                }
                        } );
                        return;
                }

                auto* object = objectFor( item );
                if ( !object )
                {
                        return;
                }
                if ( object->name().isEmpty() && text == describe( object ) )
                {
                        return;   // automatic name left unchanged
                }
                if ( text == object->name() )
                {
                        return;
                }

                QPointer<model::ModelObject> target( object );
                QTimer::singleShot( 0, this, [this, target, text]()
                {
                        if ( !target || !mModel )
                        {
                                return;
                        }
                        if ( mUndoRedoModel )
                        {
                                mUndoRedoModel->checkpoint( tr("Rename") );
                        }
                        target->setName( text );
                } );
        }


        ///
        /// Start renaming the selected object or group
        ///
        void ObjectListView::renameCurrent()
        {
                QTreeWidgetItem* item = mTree->currentItem();
                if ( !item || !item->isSelected() )
                {
                        auto selected = mTree->selectedItems();
                        item = selected.isEmpty() ? nullptr : selected.first();
                }
                if ( item )
                {
                        mTree->editItem( item, NAME_COLUMN );
                }
        }


        ///
        /// Duplicate the selected objects, slightly offset, and select the copies.
        /// Copies of grouped objects go into a "<group> copy" group.
        ///
        void ObjectListView::duplicateSelection()
        {
                auto originals = selectedObjects();
                if ( originals.isEmpty() || !mModel )
                {
                        return;
                }

                if ( mUndoRedoModel )
                {
                        mUndoRedoModel->checkpoint( tr("Duplicate") );
                }

                const auto offset = model::Distance::mm( 2 );
                mModel->unselectAll();
                for ( auto* original : originals )
                {
                        auto* copy = original->clone();
                        copy->setLocked( false );
                        copy->setX0( original->x0() + offset );
                        copy->setY0( original->y0() + offset );
                        if ( !original->name().isEmpty() )
                        {
                                copy->setName( tr("%1 copy").arg( original->name() ) );
                        }
                        if ( !original->group().isEmpty() )
                        {
                                copy->setGroup( tr("%1 copy").arg( original->group() ) );
                        }
                        mModel->addObject( copy );
                        mModel->selectObject( copy );
                }
        }


        ///
        /// "Group 1", "Group 2", ... : the first one not already used
        ///
        QString ObjectListView::nextGroupName() const
        {
                QSet<QString> used;
                if ( mModel )
                {
                        for ( auto* object : mModel->objectList() )
                        {
                                used.insert( object->group() );
                        }
                }
                for ( int n = 1; ; n++ )
                {
                        QString name = tr("Group %1").arg( n );
                        if ( !used.contains( name ) )
                        {
                                return name;
                        }
                }
        }


        void ObjectListView::groupSelection()
        {
                auto objects = selectedObjects();
                if ( objects.isEmpty() || !mModel )
                {
                        return;
                }

                if ( mUndoRedoModel )
                {
                        mUndoRedoModel->checkpoint( tr("Group") );
                }
                mModel->groupObjects( objects, nextGroupName() );
        }


        void ObjectListView::ungroupSelection()
        {
                auto objects = selectedObjects();
                if ( objects.isEmpty() || !mModel )
                {
                        return;
                }

                if ( mUndoRedoModel )
                {
                        mUndoRedoModel->checkpoint( tr("Ungroup") );
                }
                mModel->ungroupObjects( objects );
        }

}
