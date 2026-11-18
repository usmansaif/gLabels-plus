//  TemplatePicker.cpp
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


#include "TemplatePicker.hpp"

#include "MiniPreviewPixmap.hpp"
#include "TemplatePickerItem.hpp"

#include "model/Settings.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QRegularExpression>
#include <QPainter>
#include <QStyledItemDelegate>

#include <algorithm>
#include <cmath>


namespace
{

        //
        // Custom item delegate to render text as HTML in List View
        //
        // Based on solutions at
        // https://stackoverflow.com/questions/1956542/how-to-make-item-view-render-rich-html-text-in-qt/1956781#1956781
        //
        class HtmlDelegate : public QStyledItemDelegate
        {
        protected:
                void paint ( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
                QSize sizeHint ( const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
        };


        void HtmlDelegate::paint( QPainter*                   painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex&          index) const
        {
                auto opt = option;
                initStyleOption( &opt, index );

                QStyle *style = opt.widget? opt.widget->style() : QApplication::style();

                QTextDocument doc;
                doc.setHtml( opt.text );

                /// Painting everything other than text
                opt.text = QString();
                style->drawControl( QStyle::CE_ItemViewItem, &opt, painter );

                QAbstractTextDocumentLayout::PaintContext ctx;

                // Highlighting text if item is selected
                if ( opt.state & QStyle::State_Selected )
                {
                        ctx.palette.setColor( QPalette::Text, opt.palette.color( QPalette::Active, QPalette::HighlightedText ) );
                }
                else
                {
                        ctx.palette.setColor( QPalette::Text, opt.palette.color( QPalette::Active, QPalette::Text ) );
                }

                QRect textRect = style->subElementRect( QStyle::SE_ItemViewItemText, &opt );
                painter->save();
                painter->translate( textRect.topLeft() );
                painter->setClipRect( textRect.translated( -textRect.topLeft() ) );
                doc.documentLayout()->draw( painter, ctx );
                painter->restore();
        }


        QSize HtmlDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
        {
                auto opt = option;
                initStyleOption( &opt, index );

                QTextDocument doc;
                doc.setHtml( opt.text );
                doc.setTextWidth( opt.rect.width() );
                return QSize( doc.idealWidth(), std::max( doc.size().height(), qreal(glabels::TemplatePickerItem::SIZE) ) );
        }

}


namespace glabels
{

        ///
        /// Constructor
        ///
        TemplatePicker::TemplatePicker( QWidget* parent )
                : QListView(parent)
        {
                mModel = new QStandardItemModel( this );
                setModel( mModel );

                setResizeMode( QListView::Adjust );
                setUniformItemSizes( true );
                setWordWrap( true );
                setIconSize( QSize( TemplatePickerItem::SIZE, TemplatePickerItem::SIZE ) );
                setSpacing( 24 );

                setMode( model::Settings::templatePickerMode() );

                connect( selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                         this, SLOT(onSelectionChanged()) );

                setContextMenuPolicy( Qt::CustomContextMenu );
                connect( this, &QWidget::customContextMenuRequested, this, &TemplatePicker::onContextMenu );
        }


        ///
        /// Context menu: toggle favorite
        ///
        void TemplatePicker::onContextMenu( const QPoint& pos )
        {
                auto index = indexAt( pos );
                if ( !index.isValid() )
                {
                        return;
                }
                auto* tItem = dynamic_cast<TemplatePickerItem*>( mModel->item( index.row(), 0 ) );
                if ( !tItem )
                {
                        return;
                }

                QMenu menu;
                QAction* action = menu.addAction( QIcon::fromTheme( "glabels-favorite" ),
                                                  tItem->isFavorite() ? tr("Remove from Favorites") : tr("Add to Favorites") );
                if ( menu.exec( viewport()->mapToGlobal( pos ) ) == action )
                {
                        tItem->setFavorite( !tItem->isFavorite() );
                        emit favoritesChanged();
                }
        }


        ///
        /// Fuzzy matching of search string against item search text
        ///
        /// Every whitespace-separated token must match.  A token matches if it is a
        /// substring of the search text, or (for tokens of 3+ characters) if its
        /// characters appear in order within a single word of the search text.
        /// Sizes may be typed as "2x4", "2 x 4", "50x25" etc.
        ///
        bool TemplatePicker::matches( const QString& searchText, const QString& searchString )
        {
                QString q = searchString.toLower();
                q.replace( QChar( 0x00D7 ), 'x' );
                q.replace( QRegularExpression( "(\\d)\\s*(x|by)\\s*(\\d)" ), "\\1x\\3" );
                q.remove( '"' );

                const QStringList tokens = q.split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
                if ( tokens.isEmpty() )
                {
                        return true;
                }

                static thread_local QString cachedText;
                static thread_local QStringList cachedWords;
                if ( cachedText != searchText )
                {
                        cachedText  = searchText;
                        cachedWords = searchText.split( QRegularExpression( "[\\s,;()/]+" ), Qt::SkipEmptyParts );
                }

                static const QRegularExpression sizeRe( "^(\\d+(?:\\.\\d+)?)x(\\d+(?:\\.\\d+)?)$" );
                static const QRegularExpression alphaRe( "^[^\\d\\W]+$", QRegularExpression::UseUnicodePropertiesOption );

                for ( const QString& token : tokens )
                {
                        // Size, e.g. "2x4" or "50x25": compare numerically against the template's
                        // own "WxH" words (inches and millimeters), allowing small rounding differences.
                        auto sizeMatch = sizeRe.match( token );
                        if ( sizeMatch.hasMatch() )
                        {
                                double a = sizeMatch.captured( 1 ).toDouble();
                                double b = sizeMatch.captured( 2 ).toDouble();
                                bool found = false;
                                for ( const QString& word : cachedWords )
                                {
                                        auto m = sizeRe.match( word );
                                        if ( m.hasMatch() )
                                        {
                                                double wa = m.captured( 1 ).toDouble();
                                                double wb = m.captured( 2 ).toDouble();
                                                auto close = []( double x, double y ) {
                                                        return std::fabs( x - y ) <= std::max( 0.02 * y, 0.06 );
                                                };
                                                if ( close( a, wa ) && close( b, wb ) )
                                                {
                                                        found = true;
                                                        break;
                                                }
                                        }
                                }
                                if ( !found )
                                {
                                        return false;
                                }
                                continue;
                        }

                        if ( searchText.contains( token ) )
                        {
                                continue;
                        }

                        // Fuzzy (in-order characters within one word) only for alphabetic words
                        bool found = false;
                        if ( token.size() >= 3 && alphaRe.match( token ).hasMatch() )
                        {
                                for ( const QString& word : cachedWords )
                                {
                                        if ( word.isEmpty() || word[0] != token[0] )
                                        {
                                                continue;
                                        }
                                        int j = 0;
                                        for ( int k = 0; k < word.size() && j < token.size(); k++ )
                                        {
                                                if ( word[k] == token[j] )
                                                {
                                                        j++;
                                                }
                                        }
                                        if ( j == token.size() )
                                        {
                                                found = true;
                                                break;
                                        }
                                }
                        }
                        if ( !found )
                        {
                                return false;
                        }
                }
                return true;
        }


        ///
        /// Set List of Templates to Pick From
        ///
        void TemplatePicker::setTemplates( const QList<model::Template>& tmplates )
        {
                mModel->clear();
                auto mode = model::Settings::templatePickerMode();
                for ( auto& tmplate : tmplates )
                {
                        mModel->appendRow( new TemplatePickerItem( tmplate, mode ) );
                }
        }


        ///
        /// Set View Mode
        ///
        void TemplatePicker::setMode( QListView::ViewMode mode )
        {
                model::Settings::setTemplatePickerMode( mode );
                setViewMode( mode );

                for ( int i = 0; i < mModel->rowCount(); i++ )
                {
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( i, 0 ) ) )
                        {
                                tItem->setMode( mode );
                        }
                }

                switch ( mode )
                {

                case QListView::IconMode:
                        setItemDelegate( new QStyledItemDelegate() ); // Use default delegate
                        setViewMode( QListView::IconMode );
                        setSpacing( 24 );
                        break;

                case QListView::ListMode:
                        setItemDelegate( new HtmlDelegate() );
                        setViewMode( QListView::ListMode );
                        setSpacing( 8 );
                        break;

                default:
                        qWarning() << "TemplatePicker: unknown mode!";
                        break;

                }

                auto selection = selectedIndexes();
                if ( selection.size() )
                {
                        scrollTo( selection[0], QAbstractItemView::PositionAtCenter );
                }
        }


        ///
        /// Get current View Mode
        ///
        QListView::ViewMode TemplatePicker::mode() const
        {
                return model::Settings::templatePickerMode();
        }


        ///
        /// Apply Filter to Narrow Template Choices by search criteria
        ///
        void TemplatePicker::applyFilter( const QString& searchString,
                                          bool isoMask, bool usMask, bool otherMask,
                                          bool anyCategory, const QStringList& categoryIds )
        {
                for ( int i = 0; i < mModel->rowCount(); i++ )
                {
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( i, 0 ) ) )
                        {
                                bool textMask = matches( tItem->searchText(), searchString );

                                bool sizeMask =
                                        (isoMask   && tItem->tmplate().isSizeIso())   ||
                                        (usMask    && tItem->tmplate().isSizeUs())    ||
                                        (otherMask && tItem->tmplate().isSizeOther());

                                bool categoryMask;
                                if ( anyCategory )
                                {
                                        categoryMask = true;
                                }
                                else
                                {
                                        categoryMask = false;
                                        for ( QString id : categoryIds )
                                        {
                                                categoryMask = categoryMask || tItem->tmplate().hasCategory( id );
                                        }
                                }


                                if (  textMask && sizeMask && categoryMask )
                                {
                                        setRowHidden( i, false );
                                }
                                else
                                {
                                        setRowHidden( i, true );
                                        selectionModel()->select( tItem->index(), QItemSelectionModel::Deselect );
                                }
                        }
                }

                auto selection = selectedIndexes();
                if ( selection.size() )
                {
                        scrollTo( selection[0], QAbstractItemView::PositionAtCenter );
                }
        }


        ///
        /// Apply Filter to Narrow Template Choices by a list of names
        ///
        void TemplatePicker::applyFilter( const QStringList& names )
        {
                for ( int i = 0; i < mModel->rowCount(); i++ )
                {
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( i, 0 ) ) )
                        {
                                bool match = false;
                                for ( QString name : names )
                                {
                                        if ( tItem->tmplate().name() == name )
                                        {
                                                match = true;
                                                break;
                                        }
                                }

                                if (  match )
                                {
                                        setRowHidden( i, false );
                                }
                                else
                                {
                                        setRowHidden( i, true );
                                        selectionModel()->select( tItem->index(), QItemSelectionModel::Deselect );
                                }
                        }
                }

                auto selection = selectedIndexes();
                if ( selection.size() )
                {
                        scrollTo( selection[0], QAbstractItemView::PositionAtCenter );
                }
        }


        ///
        /// Get Currently Selected Template
        ///
        int TemplatePicker::visibleCount() const
        {
                int n = 0;
                for ( int i = 0; i < mModel->rowCount(); i++ )
                {
                        if ( !isRowHidden( i ) )
                        {
                                n++;
                        }
                }
                return n;
        }


        int TemplatePicker::totalCount() const
        {
                return mModel->rowCount();
        }


        bool TemplatePicker::selectFirstVisible()
        {
                for ( int i = 0; i < mModel->rowCount(); i++ )
                {
                        if ( !isRowHidden( i ) )
                        {
                                auto index = mModel->index( i, 0 );
                                setCurrentIndex( index );
                                selectionModel()->select( index, QItemSelectionModel::ClearAndSelect );
                                scrollTo( index );
                                return true;
                        }
                }
                return false;
        }


        bool TemplatePicker::isSelectedFavorite() const
        {
                auto selectionList = selectedIndexes();
                if ( selectionList.size() )
                {
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( selectionList[0].row(), 0 ) ) )
                        {
                                return tItem->isFavorite();
                        }
                }
                return false;
        }


        void TemplatePicker::setSelectedFavorite( bool favorite )
        {
                auto selectionList = selectedIndexes();
                if ( selectionList.size() )
                {
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( selectionList[0].row(), 0 ) ) )
                        {
                                if ( tItem->isFavorite() != favorite )
                                {
                                        tItem->setFavorite( favorite );
                                        emit favoritesChanged();
                                }
                        }
                }
        }


        model::Template TemplatePicker::selectedTemplate() const
        {
                auto selectionList = selectedIndexes();
                if ( selectionList.size() )
                {
                        int i = selectionList[0].row();
                        if ( auto* tItem = dynamic_cast<TemplatePickerItem *>( mModel->item( i, 0 ) ) )
                        {
                                return tItem->tmplate();
                        }
                }

                return model::Template();
        }


        ///
        /// Handle selection changed from selection model
        ///
        void TemplatePicker::onSelectionChanged()
        {
                emit selectionChanged();
        }


} // namespace glabels
