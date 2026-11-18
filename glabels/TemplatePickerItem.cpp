//  TemplatePickerItem.cpp
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


#include "TemplatePickerItem.hpp"

#include "MiniPreviewPixmap.hpp"

#include "model/Settings.hpp"

#include <QDebug>
#include <QIcon>
#include <QRegularExpression>


namespace glabels
{

        ///
        /// Constructor
        ///
        TemplatePickerItem::TemplatePickerItem( const model::Template& tmplate,
                                                QListView::ViewMode    mode )
                : QStandardItem()
        {
                mTmplate = tmplate;
                mFavorite = model::Settings::isFavoriteTemplate( tmplate.name() );

                // Build search text: names, description and sizes in several unit systems
                auto frame = tmplate.frame();
                auto fmt = []( double v ) { return QString::number( v, 'f', 3 ).remove( QRegularExpression( "\\.?0+$" ) ); };
                double wIn = frame->w().in(), hIn = frame->h().in();
                double wMm = frame->w().mm(), hMm = frame->h().mm();
                QStringList parts = {
                        tmplate.name(), tmplate.brand(), tmplate.part(), tmplate.equivPart(),
                        tmplate.description(),
                        frame->sizeDescription( model::Units::in() ),
                        frame->sizeDescription( model::Units::mm() ),
                        fmt(wIn) + "x" + fmt(hIn), fmt(hIn) + "x" + fmt(wIn),
                        fmt(wMm) + "x" + fmt(hMm), fmt(hMm) + "x" + fmt(wMm),
                        QString::number( qRound(wMm) ) + "x" + QString::number( qRound(hMm) ),
                        QString::number( qRound(hMm) ) + "x" + QString::number( qRound(wMm) ),
                        frame->layoutDescription(),
                        tmplate.paperDescription( model::Units::mm() ),
                };
                mSearchText = parts.join( ' ' ).toLower();
                mSearchText.replace( QChar( 0x00D7 ), 'x' ); // multiplication sign

                setIcon( QIcon( MiniPreviewPixmap( tmplate, SIZE, SIZE ) ) );
                setMode( mode );

                setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
        }


        ///
        /// Configure for given View Mode
        ///
        void TemplatePickerItem::setMode( QListView::ViewMode mode )
        {
                auto frame = mTmplate.frame();
                mMode = mode;

                QString star = mFavorite ? QString( QChar( 0x2605 ) ) + " " : QString();

                switch ( mode )
                {

                case QListView::IconMode:
                        setText( star + mTmplate.name() );
                        break;

                case QListView::ListMode:
                        setText( "<b>" + star + mTmplate.name() + "</b><br/>" +
                                 mTmplate.description() + "<br/>" +
                                 frame->sizeDescription( model::Settings::units() ) + "<br/>" +
                                 frame->layoutDescription() );
                        break;

                default:
                        qWarning() << "TemplatePickerItem: unknown mode!";
                        break;

                }
        }


        ///
        /// Search text
        ///
        const QString& TemplatePickerItem::searchText() const
        {
                return mSearchText;
        }


        ///
        /// Favorite flag
        ///
        bool TemplatePickerItem::isFavorite() const
        {
                return mFavorite;
        }


        void TemplatePickerItem::setFavorite( bool favorite )
        {
                if ( mFavorite != favorite )
                {
                        mFavorite = favorite;
                        model::Settings::setFavoriteTemplate( mTmplate.name(), favorite );
                        setMode( mMode );
                }
        }


        ///
        /// Template Property Getter
        ///
        model::Template TemplatePickerItem::tmplate() const
        {
                return mTmplate;
        }


} // namespace glabels
