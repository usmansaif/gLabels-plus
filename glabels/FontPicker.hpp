//  FontPicker.hpp
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

#ifndef glabels_FontPicker_hpp
#define glabels_FontPicker_hpp


#include <QFontComboBox>


class QCompleter;


namespace glabels
{

        ///
        /// Font family picker with quick search
        ///
        /// A QFontComboBox you can type into: matching fonts (anywhere in the name, any case)
        /// are listed in their own typeface as you type. Pressing Enter or clicking a match
        /// selects it; anything else reverts to the current font.
        ///
        class FontPicker : public QFontComboBox
        {
                Q_OBJECT

        public:
                explicit FontPicker( QWidget* parent = nullptr );

                /// Family of the selected item (never partially typed text)
                QString family() const;

                /// Select a family by name, without emitting a change if it is already selected
                void setFamily( const QString& family );

        protected:
                void focusOutEvent( QFocusEvent* event ) override;
                void keyPressEvent( QKeyEvent* event ) override;

        private:
                void onCompleterActivated( const QString& text );
                void commitTypedText();
                void revertText();

                QCompleter* mCompleter;
        };

}


#endif // glabels_FontPicker_hpp
