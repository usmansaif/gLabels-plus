//  HelpWindow.hpp
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

#ifndef HelpWindow_hpp
#define HelpWindow_hpp


#include <QAction>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QUrl>


class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;


namespace glabels
{

        ///
        /// User guide window (F1): a separate, independent window with a table of
        /// contents, full-text search, find-in-page, history and printing.
        ///
        class HelpWindow : public QMainWindow
        {
                Q_OBJECT

        public:
                /// Show (or raise) the single help window, optionally at a topic ("merge", "printing#copies")
                static void showTopic( const QString& topic = QString() );

                /// Actions whose current shortcuts are listed on the "Keyboard shortcuts" page
                static void setShortcutActions( const QList<QAction*>& actions );

        protected:
                void changeEvent( QEvent* event ) override;
                void closeEvent( QCloseEvent* event ) override;

        private:
                HelpWindow();

                struct Topic
                {
                        QString name;    // file name without .html
                        QString title;
                        QString group;
                };

                void      buildUi();
                void      buildContents();
                void      applyStyle();
                void      openTopic( const QString& topic );
                QString   pageHtml( const QString& name ) const;
                QString   shortcutsTableHtml() const;
                QString   plainText( const QString& name ) const;
                void      selectTocItem( const QString& name );

                void      onSourceChanged( const QUrl& url );
                void      onAnchorClicked( const QUrl& url );
                void      onTocItemClicked( QTreeWidgetItem* item );
                void      onSearchChanged( const QString& text );
                void      onFindChanged();
                void      findNext( bool backward );
                void      showFindBar();
                void      hideFindBar();
                void      printPage();
                void      zoom( int steps );

                static QPointer<HelpWindow> sInstance;
                static QList<QPointer<QAction>> sShortcutActions;

                QList<Topic>                       mTopics;
                QHash<QString, QTreeWidgetItem*>   mTocItems;
                mutable QHash<QString, QString>    mPlainCache;

                QLineEdit*      mSearchEdit;
                QStackedWidget* mNavStack;
                QTreeWidget*    mToc;
                QListWidget*    mResults;
                QTextBrowser*   mBrowser;

                QWidget*        mFindBar;
                QLineEdit*      mFindEdit;
                QLabel*         mFindStatus;

                QAction*        mBackAction;
                QAction*        mForwardAction;

                int             mZoom{ 0 };
                bool            mUpdatingStyle{ false };
        };

}


#endif // HelpWindow_hpp
