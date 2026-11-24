//  RecentView.cpp
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

#include "RecentView.hpp"

#include "File.hpp"
#include "MainWindow.hpp"
#include "Projects.hpp"

#include "model/Frame.hpp"
#include "model/Model.hpp"
#include "model/Settings.hpp"
#include "model/Variables.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>


namespace
{
        enum Roles
        {
                PathRole = Qt::UserRole + 1,
                ProjectRole,
                ModifiedRole,
                ExistsRole,
                ThumbRole
        };

        const QSize cardSize( 232, 244 );
        const QSize thumbSize( 204, 140 );
        const int   cardMargin = 8;


        struct RecentText
        {
                Q_DECLARE_TR_FUNCTIONS( RecentView )

        public:
                static QString relativeTime( const QDateTime& when )
                {
                        if ( !when.isValid() )
                        {
                                return QString();
                        }
                        qint64 secs = when.secsTo( QDateTime::currentDateTime() );
                        int minutes = int( secs/60 );
                        int hours   = int( secs/3600 );
                        int days    = int( secs/86400 );
                        if ( minutes < 1 )  return tr("Edited just now");
                        if ( hours < 1 )    return tr("Edited %n minute(s) ago", "", minutes);
                        if ( days < 1 )     return tr("Edited %n hour(s) ago", "", hours);
                        if ( days < 2 )     return tr("Edited yesterday");
                        if ( days < 7 )     return tr("Edited %n day(s) ago", "", days);
                        return tr("Edited %1").arg( QLocale().toString( when.date(), QLocale::ShortFormat ) );
                }
        };


        ///
        /// Card delegate: thumbnail, name, project and time, with hover and selection states
        ///
        class CardDelegate : public QStyledItemDelegate
        {
        public:
                using QStyledItemDelegate::QStyledItemDelegate;

                QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& ) const override
                {
                        return cardSize;
                }

                void paint( QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
                {
                        p->save();
                        p->setRenderHint( QPainter::Antialiasing );

                        // The view's own palette is made transparent by the page style sheet,
                        // so take colors from the application palette
                        const QPalette pal = QApplication::palette();
                        bool hover    = option.state & QStyle::State_MouseOver;
                        bool selected = option.state & QStyle::State_Selected;
                        bool exists   = index.data( ExistsRole ).toBool();
                        bool dark     = pal.color( QPalette::Window ).lightness() < 128;

                        QRectF card = QRectF( option.rect ).adjusted( cardMargin, cardMargin, -cardMargin, -cardMargin );
                        if ( hover && exists )
                        {
                                card.translate( 0, -2 );   // gentle lift
                        }

                        // Shadow
                        QColor shadow( 0, 0, 0, hover ? ( dark ? 120 : 45 ) : ( dark ? 60 : 18 ) );
                        p->setPen( Qt::NoPen );
                        p->setBrush( shadow );
                        p->drawRoundedRect( card.translated( 0, hover ? 4 : 2 ), 10, 10 );

                        // Card
                        QColor border = pal.color( QPalette::Mid );
                        if ( hover )    border = pal.color( QPalette::Highlight );
                        if ( selected ) border = pal.color( QPalette::Highlight );
                        p->setPen( QPen( border, selected ? 2.0 : 1.0 ) );
                        p->setBrush( pal.color( hover ? QPalette::AlternateBase : QPalette::Base ) );
                        p->drawRoundedRect( card, 10, 10 );

                        // Thumbnail well
                        QRectF well( card.left() + 10, card.top() + 10, card.width() - 20, thumbSize.height() );
                        p->setPen( Qt::NoPen );
                        QColor wellBase = pal.color( QPalette::Base );
                        QColor wellInk  = pal.color( QPalette::Text );
                        double k = dark ? 0.08 : 0.05;
                        p->setBrush( QColor::fromRgbF( wellBase.redF()   * (1-k) + wellInk.redF()   * k,
                                                       wellBase.greenF() * (1-k) + wellInk.greenF() * k,
                                                       wellBase.blueF()  * (1-k) + wellInk.blueF()  * k ) );
                        p->drawRoundedRect( well, 6, 6 );

                        QImage thumb = index.data( ThumbRole ).value<QImage>();
                        if ( !thumb.isNull() )
                        {
                                QSizeF ts = thumb.size() / thumb.devicePixelRatio();
                                QPointF tl( well.center().x() - ts.width()/2, well.center().y() - ts.height()/2 );
                                if ( !exists )
                                {
                                        p->setOpacity( 0.35 );
                                }
                                p->drawImage( QRectF( tl, ts ), thumb );
                                p->setOpacity( 1.0 );
                        }
                        else
                        {
                                QIcon icon = QIcon::fromTheme( "glabels" );
                                QRect ir( 0, 0, 40, 40 );
                                ir.moveCenter( well.center().toPoint() );
                                p->setOpacity( 0.4 );
                                icon.paint( p, ir );
                                p->setOpacity( 1.0 );
                        }

                        if ( !exists )
                        {
                                QFont f = option.font;
                                f.setBold( true );
                                p->setFont( f );
                                p->setPen( QColor( 192, 57, 43 ) );
                                p->drawText( well, Qt::AlignCenter, QCoreApplication::translate( "RecentView", "File not found" ) );
                        }

                        // Name
                        double x = card.left() + 12;
                        double w = card.width() - 24;
                        double y = well.bottom() + 10;

                        QFont nameFont = option.font;
                        nameFont.setBold( true );
                        nameFont.setPointSizeF( nameFont.pointSizeF() * 1.05 );
                        QFontMetricsF nfm( nameFont );
                        p->setFont( nameFont );
                        p->setPen( pal.color( exists ? QPalette::Active : QPalette::Disabled, QPalette::Text ) );
                        p->drawText( QRectF( x, y, w, nfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                     nfm.elidedText( index.data( Qt::DisplayRole ).toString(), Qt::ElideMiddle, w ) );
                        y += nfm.height() + 4;

                        // Project pill
                        QFont smallFont = option.font;
                        smallFont.setPointSizeF( smallFont.pointSizeF() * 0.9 );
                        QFontMetricsF sfm( smallFont );
                        p->setFont( smallFont );

                        QString project = index.data( ProjectRole ).toString();
                        QString projectText = project.isEmpty()
                                ? QCoreApplication::translate( "RecentView", "No project" )
                                : project;
                        double pillW = qMin( w, sfm.horizontalAdvance( projectText ) + 18 );
                        QRectF pill( x, y, pillW, sfm.height() + 4 );
                        QColor accent = pal.color( QPalette::Highlight );
                        QColor pillBg = accent;
                        pillBg.setAlpha( project.isEmpty() ? 0 : ( dark ? 70 : 34 ) );
                        p->setBrush( pillBg );
                        p->setPen( project.isEmpty() ? QPen( pal.color( QPalette::Mid ), 1.0 ) : Qt::NoPen );
                        p->drawRoundedRect( pill, pill.height()/2, pill.height()/2 );
                        p->setPen( project.isEmpty() ? pal.color( QPalette::PlaceholderText )
                                                     : ( dark ? accent.lighter( 160 ) : accent.darker( 130 ) ) );
                        p->drawText( pill.adjusted( 9, 0, -9, 0 ), Qt::AlignVCenter | Qt::AlignLeft,
                                     sfm.elidedText( projectText, Qt::ElideRight, pill.width() - 18 ) );
                        y += pill.height() + 4;

                        // Modified time
                        p->setPen( pal.color( QPalette::PlaceholderText ) );
                        p->drawText( QRectF( x, y, w, sfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                     RecentText::relativeTime( index.data( ModifiedRole ).toDateTime() ) );

                        p->restore();
                }
        };
}


namespace glabels
{

        ///
        /// Constructor
        ///
        RecentView::RecentView( MainWindow* window )
                : QWidget( window ), mWindow( window )
        {
                setObjectName( "recentView" );

                auto* outer = new QHBoxLayout( this );
                outer->setContentsMargins( 32, 28, 32, 20 );
                outer->addStretch( 1 );

                auto* column = new QWidget();
                column->setMaximumWidth( 1240 );
                column->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
                outer->addWidget( column, 100 );
                outer->addStretch( 1 );

                auto* layout = new QVBoxLayout( column );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 14 );

                //
                // Header
                //
                auto* header = new QHBoxLayout();
                header->setSpacing( 10 );
                auto* titles = new QVBoxLayout();
                titles->setSpacing( 2 );
                auto* title = new QLabel( tr("Recent Labels") );
                title->setObjectName( "recentTitle" );
                mCountLabel = new QLabel();
                mCountLabel->setObjectName( "recentMuted" );
                titles->addWidget( title );
                titles->addWidget( mCountLabel );
                header->addLayout( titles, 1 );

                mSearch = new QLineEdit();
                mSearch->setPlaceholderText( tr("Search labels and projects") );
                mSearch->setClearButtonEnabled( true );
                mSearch->setMinimumWidth( 220 );
                mSearch->setMaximumWidth( 320 );
                connect( mSearch, &QLineEdit::textChanged, this, &RecentView::onFilterChanged );
                header->addWidget( mSearch, 0, Qt::AlignVCenter );

                auto* openButton = new QPushButton( QIcon::fromTheme( "glabels-file-open" ), tr("Open...") );
                connect( openButton, &QPushButton::clicked, this, [this]{ File::open( mWindow ); } );
                header->addWidget( openButton, 0, Qt::AlignVCenter );

                auto* newButton = new QPushButton( QIcon::fromTheme( "glabels-file-new" ), tr("New Label...") );
                newButton->setObjectName( "recentPrimary" );
                connect( newButton, &QPushButton::clicked, this, [this]{ File::newLabel( mWindow ); } );
                header->addWidget( newButton, 0, Qt::AlignVCenter );

                layout->addLayout( header );

                auto* hint = new QLabel( tr("Click a label to open it in the editor. Double-click to go straight to printing.") );
                hint->setObjectName( "recentMuted" );
                layout->addWidget( hint );

                //
                // Grid
                //
                mGrid = new QListWidget();
                mGrid->setObjectName( "recentGrid" );
                mGrid->setViewMode( QListView::IconMode );
                mGrid->setResizeMode( QListView::Adjust );
                mGrid->setMovement( QListView::Static );
                mGrid->setFlow( QListView::LeftToRight );
                mGrid->setWrapping( true );
                mGrid->setUniformItemSizes( true );
                mGrid->setGridSize( cardSize );
                mGrid->setSpacing( 0 );
                mGrid->setFrameShape( QFrame::NoFrame );
                mGrid->setMouseTracking( true );
                mGrid->setSelectionMode( QAbstractItemView::SingleSelection );
                mGrid->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
                mGrid->setItemDelegate( new CardDelegate( mGrid ) );
                mGrid->setContextMenuPolicy( Qt::CustomContextMenu );
                mGrid->viewport()->setCursor( Qt::PointingHandCursor );
                mGrid->viewport()->setAttribute( Qt::WA_Hover );
                mGrid->installEventFilter( this );
                connect( mGrid, &QListWidget::itemClicked, this, &RecentView::onItemClicked );
                connect( mGrid, &QListWidget::itemDoubleClicked, this, &RecentView::onItemDoubleClicked );
                connect( mGrid, &QWidget::customContextMenuRequested, this, &RecentView::onContextMenu );

                auto* empty = new QLabel( tr("No recent labels yet.\n\nLabels you open or save will appear here.") );
                empty->setObjectName( "recentEmpty" );
                empty->setAlignment( Qt::AlignCenter );

                mStack = new QStackedWidget();
                mStack->addWidget( mGrid );
                mStack->addWidget( empty );
                layout->addWidget( mStack, 1 );

                setStyleSheet(
                        "#recentView { background: palette(base); }"
                        "#recentTitle { font-size: 20pt; font-weight: 600; }"
                        "#recentMuted { color: palette(placeholder-text); }"
                        "#recentGrid { background: transparent; }"
                        "#recentEmpty { color: palette(placeholder-text); border: 1px dashed palette(mid);"
                        "  border-radius: 10px; padding: 24px; font-size: 11pt; }"
                        "QPushButton { padding: 6px 14px; border-radius: 6px; }"
                        "QPushButton#recentPrimary { background: palette(highlight); color: palette(highlighted-text);"
                        "  border: 1px solid palette(highlight); font-weight: 600; }" );
                setAutoFillBackground( true );
                setAttribute( Qt::WA_StyledBackground );

                // Single click is delayed so that a double click doesn't open the label twice
                mClickTimer = new QTimer( this );
                mClickTimer->setSingleShot( true );
                mClickTimer->setInterval( QApplication::doubleClickInterval() );
                connect( mClickTimer, &QTimer::timeout, this, &RecentView::onClickTimeout );

                // Reload only when the recent list itself changes (settings change for
                // many other reasons, e.g. while thumbnails are being rendered)
                connect( model::Settings::instance(), &model::Settings::changed, this, [this]{
                        QStringList files = model::Settings::recentFileList();
                        if ( files == mLoadedFiles )
                        {
                                return;
                        }
                        if ( isVisible() ) reload(); else mNeedsReload = true;
                } );
        }


        ///
        /// Reload when shown, if anything changed meanwhile
        ///
        void RecentView::showEvent( QShowEvent* event )
        {
                QWidget::showEvent( event );
                reload();   // cheap: thumbnails come from the disk cache unless a label changed
        }


        ///
        /// (Re)load the grid from the recent file list
        ///
        void RecentView::reload()
        {
                mNeedsReload = false;
                mClickTimer->stop();
                mPendingItem = nullptr;
                mGrid->clear();
                mThumbnailQueue.clear();

                QStringList files = model::Settings::recentFileList();
                mLoadedFiles = files;
                for ( const auto& fileName : files )
                {
                        QFileInfo info( fileName );
                        auto* item = new QListWidgetItem( info.completeBaseName() );
                        item->setData( PathRole, info.absoluteFilePath() );
                        item->setData( ProjectRole, Projects::projectOf( info.absoluteFilePath() ) );
                        item->setData( ModifiedRole, info.exists() ? info.lastModified() : QDateTime() );
                        item->setData( ExistsRole, info.exists() );
                        item->setToolTip( QDir::toNativeSeparators( info.absoluteFilePath() ) );
                        mGrid->addItem( item );
                        if ( info.exists() )
                        {
                                mThumbnailQueue << info.absoluteFilePath();
                        }
                }

                mStack->setCurrentIndex( files.isEmpty() ? 1 : 0 );
                mCountLabel->setText( tr("%n label(s)", "", int( files.size() )) );
                onFilterChanged( mSearch->text() );

                QTimer::singleShot( 0, this, &RecentView::loadNextThumbnail );
        }


        ///
        /// Load thumbnails one at a time so the screen stays responsive
        ///
        void RecentView::loadNextThumbnail()
        {
                if ( mThumbnailQueue.isEmpty() )
                {
                        return;
                }
                QString fileName = mThumbnailQueue.takeFirst();

                qreal dpr = devicePixelRatioF();
                QImage image = thumbnail( fileName, thumbSize * dpr );
                image.setDevicePixelRatio( dpr );

                for ( int i = 0; i < mGrid->count(); i++ )
                {
                        auto* item = mGrid->item( i );
                        if ( item->data( PathRole ).toString() == fileName )
                        {
                                item->setData( ThumbRole, image );
                        }
                }

                if ( !mThumbnailQueue.isEmpty() )
                {
                        QTimer::singleShot( 0, this, &RecentView::loadNextThumbnail );
                }
        }


        ///
        /// Thumbnail of a label file, cached on disk
        ///
        QImage RecentView::thumbnail( const QString& fileName, const QSize& maxSize )
        {
                QFileInfo info( fileName );
                if ( !info.exists() )
                {
                        return QImage();
                }

                QString cacheDir = QStandardPaths::writableLocation( QStandardPaths::CacheLocation ) + "/thumbnails";
                QByteArray key = QCryptographicHash::hash( ( info.absoluteFilePath() + "|" +
                                                             QString::number( maxSize.width() ) + "x" +
                                                             QString::number( maxSize.height() ) ).toUtf8(),
                                                           QCryptographicHash::Sha1 ).toHex();
                QString cacheFile = cacheDir + "/" + QString::fromLatin1( key ) + ".png";

                QFileInfo cacheInfo( cacheFile );
                if ( cacheInfo.exists() && cacheInfo.lastModified() >= info.lastModified() )
                {
                        QImage cached( cacheFile );
                        if ( !cached.isNull() )
                        {
                                return cached;
                        }
                }

                std::unique_ptr<model::Model> label( model::XmlLabelParser::readFile( fileName ) );
                if ( !label || !label->frame() )
                {
                        return QImage();
                }

                double w = label->w().pt();
                double h = label->h().pt();
                if ( w <= 0 || h <= 0 )
                {
                        return QImage();
                }
                double scale = std::min( ( maxSize.width() - 8 ) / w, ( maxSize.height() - 8 ) / h );
                QSize size( int( w*scale ) + 4, int( h*scale ) + 4 );

                QImage image( size, QImage::Format_ARGB32_Premultiplied );
                image.fill( Qt::transparent );

                QPainter painter( &image );
                painter.setRenderHint( QPainter::Antialiasing );
                painter.setRenderHint( QPainter::SmoothPixmapTransform );
                painter.translate( 2, 2 );
                painter.scale( scale, scale );

                // Label shape
                painter.save();
                if ( label->rotate() )
                {
                        painter.rotate( -90 );
                        painter.translate( -label->frame()->w().pt(), 0 );
                }
                QPainterPath path = label->frame()->path();
                painter.setPen( QPen( QColor( 0, 0, 0, 60 ), 1.0/scale ) );
                painter.setBrush( Qt::white );
                painter.drawPath( path );
                painter.restore();

                // Objects, filled with the first selected record when there is data
                painter.save();
                QPainterPath clip = path;
                if ( label->rotate() )
                {
                        QTransform t;
                        t.rotate( -90 );
                        t.translate( -label->frame()->w().pt(), 0 );
                        clip = t.map( path );
                }
                painter.setClipPath( clip );
                model::Variables variables( label->constVariables() );
                QList<merge::Record> records = label->merge() ? label->merge()->selectedRecords() : QList<merge::Record>();
                if ( !records.isEmpty() )
                {
                        label->draw( &painter, false, records.first(), variables );
                }
                else
                {
                        label->draw( &painter, false, merge::Record(), variables );
                }
                painter.restore();
                painter.end();

                QDir().mkpath( cacheDir );
                image.save( cacheFile, "PNG" );

                return image;
        }


        ///
        /// Filter cards by name or project
        ///
        void RecentView::onFilterChanged( const QString& text )
        {
                QString t = text.trimmed();
                int visible = 0;
                for ( int i = 0; i < mGrid->count(); i++ )
                {
                        auto* item = mGrid->item( i );
                        bool match = t.isEmpty() ||
                                item->text().contains( t, Qt::CaseInsensitive ) ||
                                item->data( ProjectRole ).toString().contains( t, Qt::CaseInsensitive );
                        item->setHidden( !match );
                        if ( match ) visible++;
                }
                if ( !t.isEmpty() )
                {
                        mCountLabel->setText( tr("%1 of %n label(s)", "", mGrid->count()).arg( visible ) );
                }
                else
                {
                        mCountLabel->setText( tr("%n label(s)", "", mGrid->count()) );
                }
        }


        ///
        /// Single click: open (after the double-click interval)
        ///
        void RecentView::onItemClicked( QListWidgetItem* item )
        {
                mPendingItem = item;
                mClickTimer->start();
        }


        void RecentView::onClickTimeout()
        {
                QListWidgetItem* item = mPendingItem;
                mPendingItem = nullptr;
                if ( item )
                {
                        openItem( item, false );
                }
        }


        ///
        /// Double click: open on the Print page
        ///
        void RecentView::onItemDoubleClicked( QListWidgetItem* item )
        {
                mClickTimer->stop();
                mPendingItem = nullptr;
                openItem( item, true );
        }


        ///
        /// Open the label of an item
        ///
        void RecentView::openItem( QListWidgetItem* item, bool print )
        {
                if ( !item )
                {
                        return;
                }
                QString fileName = item->data( PathRole ).toString();
                if ( !QFileInfo::exists( fileName ) )
                {
                        model::Settings::removeFromRecentFileList( fileName );
                        return;
                }

                MainWindow* window = File::open( fileName, mWindow );
                if ( window && print )
                {
                        window->showPrintPage();
                }
        }


        ///
        /// Keyboard: Enter opens, Ctrl+P prints
        ///
        bool RecentView::eventFilter( QObject* watched, QEvent* event )
        {
                if ( watched == mGrid && event->type() == QEvent::KeyPress )
                {
                        auto* key = static_cast<QKeyEvent*>( event );
                        if ( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter )
                        {
                                openItem( mGrid->currentItem(), false );
                                return true;
                        }
                        if ( key->matches( QKeySequence::Print ) )
                        {
                                openItem( mGrid->currentItem(), true );
                                return true;
                        }
                        if ( key->matches( QKeySequence::Delete ) && mGrid->currentItem() )
                        {
                                model::Settings::removeFromRecentFileList( mGrid->currentItem()->data( PathRole ).toString() );
                                return true;
                        }
                }
                return QWidget::eventFilter( watched, event );
        }


        ///
        /// Context menu
        ///
        void RecentView::onContextMenu( const QPoint& pos )
        {
                QListWidgetItem* item = mGrid->itemAt( pos );
                if ( !item )
                {
                        return;
                }
                mClickTimer->stop();
                mPendingItem = nullptr;

                QString fileName = item->data( PathRole ).toString();
                bool exists = item->data( ExistsRole ).toBool();

                QMenu menu;
                auto* open = menu.addAction( QIcon::fromTheme( "glabels-file-open" ), tr("Open"), this,
                                             [this, item]{ openItem( item, false ); } );
                auto* print = menu.addAction( QIcon::fromTheme( "glabels-print" ), tr("Print..."), this,
                                              [this, item]{ openItem( item, true ); } );
                auto* folder = menu.addAction( tr("Open Containing Folder"), this, [fileName]{
                        QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( fileName ).absolutePath() ) );
                } );
                open->setEnabled( exists );
                print->setEnabled( exists );
                folder->setEnabled( QFileInfo( QFileInfo( fileName ).absolutePath() ).exists() );
                menu.addSeparator();
                menu.addAction( tr("Remove from Recent"), this, [fileName]{
                        model::Settings::removeFromRecentFileList( fileName );
                } );
                menu.exec( mGrid->viewport()->mapToGlobal( pos ) );
        }

}
