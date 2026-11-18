//  StartupView.hpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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

#ifndef StartupView_hpp
#define StartupView_hpp


#include <QWidget>


class QFileSystemWatcher;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTimer;
class QToolButton;


namespace glabels
{

        // Forward references
        class MainWindow;


        ///
        /// Startup (Welcome) View Widget
        ///
        class StartupView : public QWidget
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                StartupView( MainWindow* window );

                /// Select a project in the Projects panel (e.g. after File > New Project)
                void selectProject( const QString& project );


                /////////////////////////////////
                // Events
                /////////////////////////////////
        protected:
                void changeEvent( QEvent* event ) override;


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onNewProjectButtonClicked();
                void onOpenProjectButtonClicked();
                void onTemplateDesignerButtonClicked();
                void onRecentItemActivated( QListWidgetItem* item );
                void onRecentContextMenu( const QPoint& pos );
                void onSettingsChanged();
                void onAppearanceChanged();

                void onCreateProject();
                void onProjectSelectionChanged();
                void onProjectContextMenu( const QPoint& pos );
                void onNewLabelInProject();
                void onOpenProjectFolder();
                void onProjectLabelActivated( QListWidgetItem* item );
                void onProjectLabelContextMenu( const QPoint& pos );
                void onProjectsChangedOnDisk();


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                QToolButton* createActionCard( const QString& iconName,
                                               const QString& title,
                                               const QString& description );
                void loadRecentList();
                QWidget* createProjectsSection();
                void loadProjects( const QString& select = QString() );
                void loadProjectLabels();
                QString currentProject() const;
                void renameProject( const QString& project );
                void trashProject( const QString& project );
                void renameLabel( const QString& filePath );
                void duplicateLabel( const QString& filePath );
                void trashLabel( const QString& filePath );
                void watchProjectFolders();
                void updateStyle();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                MainWindow*     mWindow;

                QLabel*         mLogoLabel;
                QLabel*         mVersionLabel;
                QLabel*         mFooterLabel{ nullptr };
                QLabel*         mBannerLabel{ nullptr };
                QToolButton*    mNewButton;
                QToolButton*    mOpenButton;
                QToolButton*    mDesignerButton;
                QStackedWidget* mRecentStack;
                QListWidget*    mRecentList;
                QLabel*         mRecentEmptyLabel;
                bool            mUpdatingStyle { false };

                // Projects panel
                QStackedWidget*     mProjectsStack;
                QListWidget*        mProjectList;
                QLabel*             mProjectsEmptyLabel;
                QLabel*             mProjectTitle;
                QToolButton*        mNewLabelInProjectButton;
                QToolButton*        mProjectFolderButton;
                QStackedWidget*     mProjectLabelsStack;
                QListWidget*        mProjectLabelList;
                QLabel*             mProjectLabelsEmptyLabel;
                QFileSystemWatcher* mWatcher;
                QTimer*             mReloadTimer;

        };

}


#endif // StartupView_hpp
