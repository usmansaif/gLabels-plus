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


class QLabel;


namespace glabels
{

        ///
        /// About Dialog Widget
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
                // Slots
                /////////////////////////////////
        private slots:
                void onLicenseButtonClicked();
                void onWebsiteButtonClicked();


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                QWidget* createCard( const QString& title, QWidget* body );
                QWidget* createPerson( const QString& role, const QString& name,
                                       const QString& email, const QString& website = QString() );
                void     applyStyle();
                void     changeEvent( QEvent* event ) override;


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QLabel*  mTitleLabel{ nullptr };
                bool     mStyling{ false };

        };

}


#endif // AboutDialog_hpp
