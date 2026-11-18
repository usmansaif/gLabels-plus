//  WhatsNewDialog.hpp
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

#ifndef WhatsNewDialog_hpp
#define WhatsNewDialog_hpp


#include <QDialog>


class QTextBrowser;


namespace glabels
{

        ///
        /// "What's New" Dialog: bullet list of recently added features
        ///
        class WhatsNewDialog : public QDialog
        {
                Q_OBJECT

        public:
                WhatsNewDialog( QWidget* parent = nullptr );

        protected:
                void changeEvent( QEvent* event ) override;

        private:
                void updateContent();

                QTextBrowser* mBrowser;
        };

}


#endif // WhatsNewDialog_hpp
