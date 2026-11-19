//  VariablesPanel.hpp
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

#ifndef VariablesPanel_hpp
#define VariablesPanel_hpp


#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class QToolButton;


namespace glabels
{

        namespace model { class Model; }
        class UndoRedoModel;


        ///
        /// Variables panel: the data-source variables of the current label
        ///
        /// Variables can be dragged onto the label (creating a bound text, barcode
        /// or image object), renamed (every reference on the label follows) and
        /// given a data type.
        ///
        class VariablesPanel : public QWidget
        {
                Q_OBJECT

        public:
                /// MIME type of a dragged variable: "<name>\n<type>" (UTF-8)
                static const QString MimeType;

                VariablesPanel( QWidget* parent = nullptr );

                void setModel( model::Model* model, UndoRedoModel* undoRedoModel );

                /// Rename a data variable and every reference to it (one undo step)
                static bool renameVariable( model::Model* model, UndoRedoModel* undoRedoModel,
                                            const QString& oldName, const QString& newName,
                                            QString* error = nullptr );

                /// Change a data variable's type
                static void setVariableType( model::Model* model, UndoRedoModel* undoRedoModel,
                                             const QString& name, const QString& type );

        signals:
                void connectRequested();
                void addVariableRequested( const QString& name, const QString& type );

        private slots:
                void reload();
                void onItemChanged( QListWidgetItem* item );
                void onContextMenu( const QPoint& pos );

        private:
                model::Model*          mModel{ nullptr };
                UndoRedoModel*         mUndoRedoModel{ nullptr };
                bool                   mBlocked{ false };

                QStackedWidget* mStack;
                QListWidget*    mList;
                QLabel*         mSourceLabel;
                QLabel*         mMessageLabel;
                QToolButton*    mConnectButton;
                QToolButton*    mReloadButton;
        };

}


#endif // VariablesPanel_hpp
