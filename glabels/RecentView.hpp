//  RecentView.hpp
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

#ifndef RecentView_hpp
#define RecentView_hpp


#include <QImage>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTimer;


namespace glabels
{

        class MainWindow;


        ///
        /// Recent labels: a grid of label thumbnails shown when no label is open
        ///
        /// Single click opens the label in the editor, double click opens it on
        /// the Print page.
        ///
        class RecentView : public QWidget
        {
                Q_OBJECT

        public:
                RecentView( MainWindow* window );

                /// Render (or fetch from the disk cache) a thumbnail of a label file
                static QImage thumbnail( const QString& fileName, const QSize& maxSize );

        signals:
                void homeRequested();

        public slots:
                void reload();

        protected:
                bool eventFilter( QObject* watched, QEvent* event ) override;
                void showEvent( QShowEvent* event ) override;

        private slots:
                void onItemClicked( QListWidgetItem* item );
                void onItemDoubleClicked( QListWidgetItem* item );
                void onClickTimeout();
                void onContextMenu( const QPoint& pos );
                void onFilterChanged( const QString& text );
                void loadNextThumbnail();

        private:
                void openItem( QListWidgetItem* item, bool print );

                MainWindow*     mWindow;
                QListWidget*    mGrid;
                QLineEdit*      mSearch;
                QLabel*         mCountLabel;
                QStackedWidget* mStack;
                QTimer*         mClickTimer;
                QListWidgetItem* mPendingItem{ nullptr };
                QStringList     mThumbnailQueue;
                QStringList     mLoadedFiles;   // recent list currently shown
                bool            mNeedsReload{ true };
        };

}


#endif // RecentView_hpp
