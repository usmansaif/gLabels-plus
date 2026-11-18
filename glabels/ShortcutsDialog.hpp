//  ShortcutsDialog.hpp
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

#ifndef ShortcutsDialog_hpp
#define ShortcutsDialog_hpp


#include <QAction>
#include <QDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>


namespace glabels
{

        ///
        /// Keyboard Shortcuts Editor Dialog
        ///
        /// Actions must have a unique objectName and a "defaultShortcut" property.
        /// Custom shortcuts are stored in QSettings under the "Shortcuts" group.
        ///
        class ShortcutsDialog : public QDialog
        {
                Q_OBJECT

        public:
                ShortcutsDialog( const QList<QAction*>& actions, QWidget* parent = nullptr );

                /// Register an action: remembers its default shortcut and applies any saved override
                static void registerAction( QAction* action, const QString& id );

                /// Re-apply saved shortcuts to a list of registered actions
                static void applySaved( const QList<QAction*>& actions );

        private slots:
                void onCurrentItemChanged();
                void onKeySequenceChanged();
                void onClearClicked();
                void onResetClicked();
                void onResetAllClicked();
                void onFilterChanged( const QString& text );
                void onAccepted();

        private:
                QTreeWidgetItem* itemForAction( QAction* action ) const;
                void refreshItem( QTreeWidgetItem* item );
                void updateConflictLabel();
                static QString cleanText( const QAction* action );

                QList<QAction*>               mActions;
                QHash<QAction*, QKeySequence> mPending;

                QLineEdit*        mFilterEdit;
                QTreeWidget*      mTree;
                QKeySequenceEdit* mSequenceEdit;
                QPushButton*      mClearButton;
                QPushButton*      mResetButton;
                QLabel*           mConflictLabel;
                bool              mBlocked{ false };
        };

}


#endif // ShortcutsDialog_hpp
