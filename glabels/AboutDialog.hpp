//  AboutDialog.hpp
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

#ifndef AboutDialog_hpp
#define AboutDialog_hpp


#include <QDialog>
#include <QList>
#include <QPair>


class QButtonGroup;
class QLabel;
class QStackedWidget;


namespace glabels
{

        ///
        /// About Dialog: product header and About, Credits, System and License pages
        ///
        class AboutDialog : public QDialog
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                AboutDialog( QWidget *parent = nullptr );


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                QWidget* createHeader();
                QWidget* createAboutPage();
                QWidget* createCreditsPage();
                QWidget* createSystemPage();
                QWidget* createLicensePage();

                QWidget* createPage( QWidget* content );
                QWidget* createSection( const QString& title );
                QWidget* createLink( const QString& title, const QString& detail, const QString& url );
                QWidget* createPerson( const QString& name, const QString& role, const QString& note,
                                       const QString& email, const QString& website = QString() );

                QList<QPair<QString,QString>> systemInfo() const;
                QString  systemInfoText() const;
                void     copySystemInfo();

                void     applyStyle();
                void     changeEvent( QEvent* event ) override;


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QLabel*         mWordmark{ nullptr };
                QButtonGroup*   mTabs{ nullptr };
                QStackedWidget* mPages{ nullptr };
                QLabel*         mCopiedLabel{ nullptr };
                QList<QLabel*>  mContactLabels;
                bool            mStyling{ false };

        };

}


#endif // AboutDialog_hpp
