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


#include "StartupWidgets.hpp"

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QWidget>


class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTimer;
class QToolButton;
class QVBoxLayout;


namespace glabels
{

        // Forward references
        class MainWindow;


        ///
        /// Startup (Welcome) View Widget
        ///
        /// Three panes: navigation (start actions, recent labels and projects) on the
        /// left, quick start tiles and the label list in the middle, and details of
        /// the selected label on the right.
        ///
        class StartupView : public QWidget
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                StartupView( MainWindow* window );

                /// Show a project's labels (e.g. after File > New Project)
                void selectProject( const QString& project );


                /////////////////////////////////
                // Events
                /////////////////////////////////
        protected:
                void changeEvent( QEvent* event ) override;
                void resizeEvent( QResizeEvent* event ) override;
                bool eventFilter( QObject* watched, QEvent* event ) override;


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onSettingsChanged();
                void onAppearanceChanged();
                void onCurrentItemChanged();
                void onListContextMenu( const QPoint& pos );
                void onCreateProject();
                void onNewLabelInProject();
                void onOpenProjectFolder();
                void onProjectsChangedOnDisk();
                void loadNextThumbnail();
                void updateInspectorDetails();


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                enum Mode { RecentMode, ProjectMode };

                QWidget* createTopBar();
                QWidget* createSidebar();
                QWidget* createCenter();
                QWidget* createInspector();
                QWidget* createStatusBar();
                QLabel*  createSectionLabel( const QString& text );

                void setMode( Mode mode, const QString& project = QString() );
                void loadProjects( const QString& select = QString() );
                void loadList();
                void applyFilter();
                void updateInspector();
                void updateStyle();
                void watchProjectFolders();

                QString selectedPath() const;
                void openSelected( bool print );
                void showProjectMenu( const QString& project, const QPoint& globalPos );

                void renameProject( const QString& project );
                void trashProject( const QString& project );
                void deleteProject( const QString& project );
                void renameLabel( const QString& filePath );
                void duplicateLabel( const QString& filePath );
                void trashLabel( const QString& filePath );
                void deleteLabel( const QString& filePath );


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                MainWindow*     mWindow;
                startup::Theme  mTheme;
                bool            mUpdatingStyle{ false };

                Mode            mMode{ RecentMode };
                QString         mProject;           // project shown in the project tab

                // Top bar
                QLabel*         mAppIcon;
                QLabel*         mVersionPill;
                QLineEdit*      mSearch;

                // Sidebar
                QWidget*                    mSidebar;
                startup::NavButton*         mRecentNav;
                QToolButton*                mNewProjectButton;
                QWidget*                    mProjectNavBox;
                QVBoxLayout*                mProjectNavLayout;
                QList<startup::NavButton*>  mProjectNavs;
                QLabel*                     mNoProjectsLabel;
                QLabel*                     mLinksLabel;

                // Center
                QToolButton*    mRecentTab;
                QToolButton*    mProjectTab;
                QWidget*        mProjectActions;
                QLabel*         mProjectNote;
                QStackedWidget* mListStack;
                QListWidget*    mList;
                QLabel*         mEmptyLabel;

                // Inspector
                QWidget*               mInspector;
                startup::PreviewWell*  mPreview;
                QLabel*                mInspTitle;
                QLabel*                mInspSub;
                QLabel*                mInspProject;
                QLabel*                mInspSize;
                QLabel*                mInspModified;
                QLabel*                mInspPath;
                QWidget*               mInspDetails;
                QToolButton*           mOpenButton;
                QToolButton*           mPrintButton;
                QToolButton*           mShowFolderButton;
                QTimer*                mInspectorTimer;

                // Status bar
                startup::KeyHints*     mHints;
                QLabel*                mStatusLabel;

                // Thumbnails and file watching
                QHash<QString, QImage> mThumbs;
                QHash<QString, QDateTime> mThumbTimes;   // file time each thumbnail was drawn from
                QStringList            mRecentFiles;     // recent list last shown
                QStringList            mThumbQueue;
                QFileSystemWatcher*    mWatcher;
                QTimer*                mReloadTimer;
        };

}


#endif // StartupView_hpp
