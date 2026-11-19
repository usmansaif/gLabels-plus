//  ConnectDataSourceDialog.hpp
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

#ifndef ConnectDataSourceDialog_hpp
#define ConnectDataSourceDialog_hpp


#include "merge/Merge.hpp"

#include <QDialog>
#include <QMap>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTableView;
class QTableWidget;
class QToolButton;
class QWidget;


namespace glabels
{

        namespace model { class Model; }

        class DropZone;


        ///
        /// "Connect Data Source" dialog
        ///
        /// Step 1: pick or drop a .csv, .tsv, .txt or .json file (format detected automatically).
        /// Step 2: set up variables (include, rename, type) in the Variable Setup table.
        /// Step 3: choose which records to print in the Preview Records tab.
        ///
        class ConnectDataSourceDialog : public QDialog
        {
                Q_OBJECT

        public:
                ConnectDataSourceDialog( model::Model* model, QWidget* parent = nullptr );
                ~ConnectDataSourceDialog() override;

                /// Open a file right away (e.g. one dropped on the main window)
                void loadFile( const QString& fileName );

                /// The configured data source; the caller takes ownership
                merge::Merge* takeMerge();

                /// Variable renames implied by the new mapping (old name -> new name)
                QMap<QString,QString> renames() const;

        private slots:
                void onBrowse();
                void onAdvancedToggled( bool checked );
                void onFormatChanged();
                void onVariableItemChanged();
                void onTypeChanged();
                void onSelectAllRecords();
                void onSelectNoRecords();
                void updateRecordCount();

        private:
                void reload();
                QString backendId() const;
                void fillVariableTable();
                void applyMapping();
                bool validate();
                void refreshPreview();

                model::Model*                   mModel;
                std::unique_ptr<merge::Merge>   mMerge;
                QString                         mFileName;
                QList<merge::Field>             mFields;
                QMap<QString,QString>           mInitialNames;   // source column -> name before editing
                bool                            mBlocked{ false };

                DropZone*     mDropZone;
                QLabel*       mFileLabel;
                QLabel*       mDetectedLabel;
                QToolButton*  mAdvancedButton;
                QWidget*      mAdvancedPanel;
                QComboBox*    mFormatCombo;
                QCheckBox*    mHeaderCheck;
                QTabWidget*   mTabs;
                QTableWidget* mVariableTable;
                QTableView*   mRecordsView;
                QLabel*       mRecordCountLabel;
                QLabel*       mErrorLabel;
                QPushButton*  mConfirmButton;
        };

}


#endif // ConnectDataSourceDialog_hpp
