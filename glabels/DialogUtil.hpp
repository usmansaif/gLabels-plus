//  DialogUtil.hpp
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

#ifndef DialogUtil_hpp
#define DialogUtil_hpp


#include <QDialog>
#include <QLayout>


namespace glabels::DialogUtil
{

        ///
        /// Give a dialog normal window decorations (minimize, maximize, close)
        /// and make sure it can be freely resized.
        ///
        /// Window managers (e.g. GNOME/Mutter) never offer minimize/maximize for
        /// dialog-type windows or windows that are transient for a parent.  With
        /// topLevel = true the dialog becomes an ordinary, independent window
        /// (still application-modal when exec()'d), so all buttons are shown.
        ///
        inline void makeResizableWindow( QDialog* dialog, bool topLevel = false )
        {
                Qt::WindowFlags flags = dialog->windowFlags();
                flags &= ~Qt::WindowType_Mask;
                flags |= Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                         Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint;
                flags &= ~Qt::WindowContextHelpButtonHint;

                if ( topLevel && dialog->parentWidget() )
                {
                        dialog->setParent( nullptr, flags );
                        dialog->setWindowModality( Qt::ApplicationModal );
                }
                else
                {
                        dialog->setWindowFlags( flags );
                }

                dialog->setSizeGripEnabled( true );
                if ( dialog->layout() )
                {
                        dialog->layout()->setSizeConstraint( QLayout::SetDefaultConstraint );
                }
                dialog->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
        }

}


#endif // DialogUtil_hpp
