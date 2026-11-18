//  ObjectListView.hpp
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

#ifndef ObjectListView_hpp
#define ObjectListView_hpp


#include "model/Model.hpp"

#include <QMenu>
#include <QSet>
#include <QTreeWidget>
#include <QToolBar>
#include <QWidget>


namespace glabels
{

        class UndoRedoModel;


        ///
        /// Object List ("Layers") panel
        ///
        /// Lists label objects from top-most to bottom-most, keeps selection in sync
        /// with the editor, and lets objects be renamed, locked, duplicated and
        /// organized into layer groups.  Actions live in a toolbar at the top of
        /// the panel and are enabled according to the selection.
        ///
        class ObjectListView : public QWidget
        {
                Q_OBJECT

        public:
                ObjectListView( QWidget* parent = nullptr );

                void setModel( model::Model* model, UndoRedoModel* undoRedoModel );
                void addToolBarActions( const QList<QAction*>& actions );
                void setContextMenu( QMenu* menu );

                QAction* lockAction() const { return mLockAction; }
                QAction* renameAction() const { return mRenameAction; }
                QAction* duplicateAction() const { return mDuplicateAction; }
                QAction* groupAction() const { return mGroupAction; }
                QAction* ungroupAction() const { return mUngroupAction; }

        private slots:
                void rebuild();
                void syncSelectionFromModel();
                void onItemSelectionChanged();
                void onItemClicked( QTreeWidgetItem* item, int column );
                void onItemChanged( QTreeWidgetItem* item, int column );
                void onContextMenuRequested( const QPoint& pos );

                void toggleLock();
                void renameCurrent();
                void duplicateSelection();
                void groupSelection();
                void ungroupSelection();

        private:
                static QString describe( const model::ModelObject* object );
                static QIcon iconFor( const model::ModelObject* object );
                static bool isGroupItem( const QTreeWidgetItem* item );

                model::ModelObject* objectFor( QTreeWidgetItem* item ) const;
                QList<model::ModelObject*> objectsFor( const QList<QTreeWidgetItem*>& items ) const;
                QList<model::ModelObject*> selectedObjects() const;
                QString nextGroupName() const;
                void setLocked( const QList<model::ModelObject*>& objects, bool lock );
                void updateActions();

                model::Model*  mModel{ nullptr };
                UndoRedoModel* mUndoRedoModel{ nullptr };
                QTreeWidget*   mTree;
                QToolBar*      mToolBar;
                QMenu*         mContextMenu{ nullptr };

                QAction*       mLockAction;
                QAction*       mRenameAction;
                QAction*       mDuplicateAction;
                QAction*       mGroupAction;
                QAction*       mUngroupAction;

                QSet<QString>  mCollapsedGroups;
                bool           mBlocked{ false };
        };

}


#endif // ObjectListView_hpp
