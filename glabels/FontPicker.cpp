//  FontPicker.cpp
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

#include "FontPicker.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyledItemDelegate>


namespace
{
        ///
        /// Draws each font name in its own typeface (used by the search popup)
        ///
        class FontPreviewDelegate : public QStyledItemDelegate
        {
        public:
                using QStyledItemDelegate::QStyledItemDelegate;

                void initStyleOption( QStyleOptionViewItem* option, const QModelIndex& index ) const override
                {
                        QStyledItemDelegate::initStyleOption( option, index );
                        QFont font( index.data( Qt::DisplayRole ).toString() );
                        font.setPointSizeF( option->font.pointSizeF() * 1.15 );
                        option->font = font;
                }

                QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override
                {
                        QSize size = QStyledItemDelegate::sizeHint( option, index );
                        return QSize( size.width(), std::max( size.height(), option.fontMetrics.height() * 3 / 2 + 6 ) );
                }
        };
}


namespace glabels
{

        FontPicker::FontPicker( QWidget* parent )
                : QFontComboBox( parent )
        {
                setEditable( true );
                setInsertPolicy( QComboBox::NoInsert );
                setMaxVisibleItems( 16 );
                lineEdit()->setPlaceholderText( tr("Type to search fonts...") );
                lineEdit()->setClearButtonEnabled( false );
                setToolTip( tr("Font family. Type part of a name to search, then press Enter.") );

                mCompleter = new QCompleter( model(), this );
                mCompleter->setCaseSensitivity( Qt::CaseInsensitive );
                mCompleter->setFilterMode( Qt::MatchContains );
                mCompleter->setCompletionMode( QCompleter::PopupCompletion );
                mCompleter->setMaxVisibleItems( 12 );
                mCompleter->popup()->setItemDelegate( new FontPreviewDelegate( mCompleter->popup() ) );
                setCompleter( mCompleter );

                connect( mCompleter, qOverload<const QString&>( &QCompleter::activated ),
                         this, &FontPicker::onCompleterActivated );
                connect( lineEdit(), &QLineEdit::returnPressed, this, &FontPicker::commitTypedText );
        }


        QString FontPicker::family() const
        {
                int index = currentIndex();
                return index >= 0 ? itemText( index ) : currentFont().family();
        }


        void FontPicker::setFamily( const QString& family )
        {
                int index = findText( family, Qt::MatchFixedString );
                if ( index >= 0 )
                {
                        if ( index != currentIndex() )
                        {
                                setCurrentIndex( index );
                        }
                        revertText();
                }
                else
                {
                        // Font not installed on this computer: show the name the label asks for
                        lineEdit()->setText( family );
                }
        }


        void FontPicker::onCompleterActivated( const QString& text )
        {
                int index = findText( text, Qt::MatchFixedString );
                if ( index >= 0 )
                {
                        setCurrentIndex( index );
                }
                revertText();
                lineEdit()->selectAll();
        }


        void FontPicker::commitTypedText()
        {
                QString text = lineEdit()->text().trimmed();

                // Exact name first, then the first font containing the typed text
                int index = findText( text, Qt::MatchFixedString );
                if ( index < 0 && !text.isEmpty() )
                {
                        index = findText( text, Qt::MatchContains );
                }
                if ( index >= 0 && index != currentIndex() )
                {
                        setCurrentIndex( index );
                }
                revertText();
                lineEdit()->selectAll();
        }


        void FontPicker::revertText()
        {
                if ( currentIndex() >= 0 && lineEdit()->text() != itemText( currentIndex() ) )
                {
                        lineEdit()->setText( itemText( currentIndex() ) );
                }
        }


        void FontPicker::focusOutEvent( QFocusEvent* event )
        {
                // Keep half-typed text from sticking: show the selected font again
                if ( !mCompleter->popup()->isVisible() )
                {
                        revertText();
                }
                QFontComboBox::focusOutEvent( event );
        }


        void FontPicker::keyPressEvent( QKeyEvent* event )
        {
                if ( event->key() == Qt::Key_Escape && !mCompleter->popup()->isVisible() )
                {
                        revertText();
                        lineEdit()->selectAll();
                        event->accept();
                        return;
                }
                QFontComboBox::keyPressEvent( event );
        }

}
