//  SplashScreen.cpp
//
//  Copyright (C) 2026  Usman Saif <usmansaif.com>
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


#include "SplashScreen.hpp"

#include "model/Version.hpp"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QScreen>
#include <QThread>

#include <algorithm>
#include <cmath>


namespace
{
        // Layout (logical pixels)
        // Design is laid out on a 640x380 grid and scaled to suit the screen:
        // about 46% of the available width, at most 55% of the available height
        const qreal  WIDTH_FRACTION  = 0.46;
        const qreal  HEIGHT_FRACTION = 0.55;
        const qreal  MIN_SCALE       = 0.75;
        const qreal  MAX_SCALE       = 2.4;
        const int    W            = 640;
        const int    H            = 380;
        const int    MARGIN       = 40;
        const int    BAR_Y        = H - 78;
        const int    BAR_H        = 4;

        // Palette
        const QColor BG_TOP       ( 0x0B, 0x12, 0x20 );
        const QColor BG_BOTTOM    ( 0x17, 0x25, 0x54 );
        const QColor ACCENT       ( 0x3B, 0x82, 0xF6 );
        const QColor ACCENT_2     ( 0x8B, 0x5C, 0xF6 );
        const QColor ACCENT_LIGHT ( 0x60, 0xA5, 0xFA );
        const QColor TEXT_MAIN    ( 0xF8, 0xFA, 0xFC );
        const QColor TEXT_MUTED   ( 0x94, 0xA3, 0xB8 );
        const QColor TEXT_SOFT    ( 0xCB, 0xD5, 0xE1 );

        const QString PRODUCT_NAME_1 = "gLabel";
        const QString PRODUCT_NAME_2 = " Plus";
        const QString MAINTAINER     = "Usman Saif";
        const QString MAINTAINER_SITE = "usmansaif.com";

        QFont makeFont( int pixelSize, QFont::Weight weight, qreal spacing = 0 )
        {
                QFont f = QApplication::font();
                f.setPixelSize( pixelSize );
                f.setWeight( weight );
                if ( spacing != 0 )
                {
                        f.setLetterSpacing( QFont::AbsoluteSpacing, spacing );
                }
                return f;
        }
}


namespace glabels
{

        ///
        /// Constructor
        ///
        SplashScreen::SplashScreen()
        {
                // QSplashScreen opens on the screen under the mouse cursor
                QScreen* screen = QGuiApplication::screenAt( QCursor::pos() );
                if ( !screen )
                {
                        screen = QGuiApplication::primaryScreen();
                }
                const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
                mScale = scaleForScreen( screen );

                setPixmap( renderBackground( QSize( W, H ), dpr, mScale ) );
                setCursor( Qt::BusyCursor );

                mMessage = tr( "Starting..." );
                mClock.start();

                mTimer.setInterval( 16 );
                connect( &mTimer, &QTimer::timeout, this, &SplashScreen::onTick );
                mTimer.start();
        }


        ///
        /// Set progress target and status message
        ///
        void SplashScreen::setProgress( int percent, const QString& message )
        {
                mTarget  = std::clamp( percent, 0, 100 );
                mMessage = message;
                pump();
        }


        ///
        /// Animate to completion, honoring a minimum visible time
        ///
        void SplashScreen::finishLoading( int minVisibleMs )
        {
                static const char* stages[] = {
                        QT_TR_NOOP( "Preparing workspace..." ),
                        QT_TR_NOOP( "Loading fonts and resources..." ),
                        QT_TR_NOOP( "Preparing label editor..." ),
                        QT_TR_NOOP( "Setting up print engine..." ),
                        QT_TR_NOOP( "Finalizing..." ),
                };
                const int nStages = int( sizeof(stages) / sizeof(stages[0]) );

                // Spread the remaining progress evenly over the remaining time
                const qint64 start     = mClock.elapsed();
                const qint64 remaining = std::max<qint64>( 600, minVisibleMs - start );
                const double from      = mTarget;

                while ( true )
                {
                        double t = std::min( 1.0, double( mClock.elapsed() - start ) / remaining );
                        if ( t >= 1.0 && mShown >= 99.5 )
                        {
                                break;
                        }
                        mTarget = from + ( 100.0 - from ) * t;

                        int stage = std::min( nStages - 1, int( t * nStages ) );
                        mMessage  = ( t >= 1.0 ) ? tr( "Ready" ) : tr( stages[stage] );

                        pump();
                        QThread::msleep( 8 );
                }

                mTimer.stop();
                setCursor( Qt::ArrowCursor );
        }


        ///
        /// Animation tick: ease displayed progress toward target
        ///
        void SplashScreen::onTick()
        {
                mShown += ( mTarget - mShown ) * 0.12;
                if ( std::abs( mTarget - mShown ) < 0.05 )
                {
                        mShown = mTarget;
                }
                mPhase = mClock.elapsed();
                update();
        }


        ///
        /// Process pending events so the splash keeps animating during init
        ///
        void SplashScreen::pump()
        {
                QApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
        }


        ///
        /// Ignore clicks: QSplashScreen would otherwise hide on any click
        ///
        void SplashScreen::mousePressEvent( QMouseEvent* )
        {
        }


        ///
        /// Scale for the design grid, from the screen's available (logical) size
        ///
        qreal SplashScreen::scaleForScreen( const QScreen* screen )
        {
                if ( !screen )
                {
                        return 1.4;
                }
                const QRect avail = screen->availableGeometry();
                const qreal s = std::min( avail.width()  * WIDTH_FRACTION  / W,
                                          avail.height() * HEIGHT_FRACTION / H );
                return std::clamp( s, MIN_SCALE, MAX_SCALE );
        }


        ///
        /// Static artwork: background, decoration, branding, footer
        ///
        QPixmap SplashScreen::renderBackground( const QSize& size, qreal dpr, qreal scale )
        {
                QPixmap pixmap( ( QSizeF( size ) * scale * dpr ).toSize() );
                pixmap.setDevicePixelRatio( dpr );

                QPainter p( &pixmap );
                p.scale( scale, scale );
                p.setRenderHint( QPainter::Antialiasing );
                p.setRenderHint( QPainter::TextAntialiasing );
                p.setRenderHint( QPainter::SmoothPixmapTransform );

                const QRectF r( 0, 0, size.width(), size.height() );

                // Base gradient
                QLinearGradient bg( r.topLeft(), r.bottomRight() );
                bg.setColorAt( 0.0, BG_TOP );
                bg.setColorAt( 1.0, BG_BOTTOM );
                p.fillRect( r, bg );

                // Soft accent glows
                QRadialGradient glow1( QPointF( 120, 60 ), 320 );
                glow1.setColorAt( 0.0, QColor( 59, 130, 246, 70 ) );
                glow1.setColorAt( 1.0, QColor( 59, 130, 246, 0 ) );
                p.fillRect( r, glow1 );

                QRadialGradient glow2( QPointF( W - 60, H - 40 ), 300 );
                glow2.setColorAt( 0.0, QColor( 139, 92, 246, 60 ) );
                glow2.setColorAt( 1.0, QColor( 139, 92, 246, 0 ) );
                p.fillRect( r, glow2 );

                // Decorative label sheet, tilted and bleeding off the right edge
                p.save();
                p.translate( W - 150, 40 );
                p.rotate( 14 );
                const QRectF sheet( 0, 0, 230, 300 );
                p.setPen( QPen( QColor( 255, 255, 255, 40 ), 1.2 ) );
                p.setBrush( QColor( 255, 255, 255, 14 ) );
                p.drawRoundedRect( sheet, 8, 8 );

                const int cols = 2, rows = 5;
                const qreal pad = 14, gap = 10;
                const qreal lw = ( sheet.width()  - 2*pad - ( cols-1 )*gap ) / cols;
                const qreal lh = ( sheet.height() - 2*pad - ( rows-1 )*gap ) / rows;
                for ( int row = 0; row < rows; row++ )
                {
                        for ( int col = 0; col < cols; col++ )
                        {
                                QRectF lr( pad + col*( lw+gap ), pad + row*( lh+gap ), lw, lh );
                                bool highlight = ( row == 1 && col == 0 );
                                if ( highlight )
                                {
                                        QLinearGradient lg( lr.topLeft(), lr.bottomRight() );
                                        lg.setColorAt( 0, QColor( 59, 130, 246, 150 ) );
                                        lg.setColorAt( 1, QColor( 139, 92, 246, 150 ) );
                                        p.setPen( QPen( ACCENT_LIGHT, 1.2 ) );
                                        p.setBrush( lg );
                                }
                                else
                                {
                                        p.setPen( QPen( QColor( 255, 255, 255, 55 ), 1 ) );
                                        p.setBrush( QColor( 255, 255, 255, 10 ) );
                                }
                                p.drawRoundedRect( lr, 5, 5 );

                                // Faux text lines on each label
                                p.setPen( Qt::NoPen );
                                p.setBrush( QColor( 255, 255, 255, highlight ? 170 : 45 ) );
                                p.drawRoundedRect( QRectF( lr.x()+10, lr.y()+12, lw*0.55, 4 ), 2, 2 );
                                p.drawRoundedRect( QRectF( lr.x()+10, lr.y()+22, lw*0.38, 3 ), 1.5, 1.5 );
                        }
                }
                p.restore();

                // Fade the decoration under the text column so it never competes
                QLinearGradient fade( QPointF( 0, 0 ), QPointF( W, 0 ) );
                fade.setColorAt( 0.00, QColor( 11, 18, 32, 230 ) );
                fade.setColorAt( 0.50, QColor( 11, 18, 32, 120 ) );
                fade.setColorAt( 0.72, QColor( 11, 18, 32, 0 ) );
                p.fillRect( r, fade );

                // ...and fade it out above the loading row as well
                QLinearGradient vfade( QPointF( 0, BAR_Y - 70 ), QPointF( 0, BAR_Y - 20 ) );
                vfade.setColorAt( 0.0, QColor( 20, 32, 72, 0 ) );
                vfade.setColorAt( 1.0, QColor( 20, 32, 72, 255 ) );
                p.fillRect( QRectF( 0, BAR_Y - 70, W, H - ( BAR_Y - 70 ) ), vfade );

                // Logo
                QPixmap logo( ":/images/glabels-logo.png" );
                const int logoSize = 64;
                p.drawPixmap( QRect( MARGIN, 44, logoSize, logoSize ),
                              logo.scaled( QSize( logoSize, logoSize ) * dpr * scale,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

                // Product name
                QFont titleFont = makeFont( 44, QFont::Bold, -0.5 );
                p.setFont( titleFont );
                QFontMetrics tfm( titleFont );
                const int titleBase = 170;
                p.setPen( TEXT_MAIN );
                p.drawText( MARGIN, titleBase, PRODUCT_NAME_1 );
                int x2 = MARGIN + tfm.horizontalAdvance( PRODUCT_NAME_1 );
                QLinearGradient tg( x2, 0, x2 + tfm.horizontalAdvance( PRODUCT_NAME_2 ), 0 );
                tg.setColorAt( 0, ACCENT_LIGHT );
                tg.setColorAt( 1, QColor( 0xA7, 0x8B, 0xFA ) );
                p.setPen( QPen( QBrush( tg ), 1 ) );
                p.drawText( x2, titleBase, PRODUCT_NAME_2 );

                // Tagline
                p.setFont( makeFont( 14, QFont::Normal, 0.3 ) );
                p.setPen( TEXT_MUTED );
                p.drawText( MARGIN, titleBase + 30, SplashScreen::tr( "Professional Label & Business Card Designer" ) );

                // Version pill
                QFont pillFont = makeFont( 11, QFont::DemiBold, 0.6 );
                p.setFont( pillFont );
                QString ver = SplashScreen::tr( "VERSION %1" ).arg( glabels::model::Version::STRING );
                QFontMetrics pfm( pillFont );
                QRectF pill( MARGIN, titleBase + 48, pfm.horizontalAdvance( ver ) + 20, 22 );
                p.setPen( QPen( QColor( 96, 165, 250, 140 ), 1 ) );
                p.setBrush( QColor( 59, 130, 246, 40 ) );
                p.drawRoundedRect( pill, 11, 11 );
                p.setPen( ACCENT_LIGHT );
                p.drawText( pill, Qt::AlignCenter, ver );

                // Footer divider
                const int footerY = H - 42;
                p.setPen( QPen( QColor( 255, 255, 255, 28 ), 1 ) );
                p.drawLine( QPointF( 0, footerY ), QPointF( W, footerY ) );
                p.fillRect( QRectF( 0, footerY, W, H - footerY ), QColor( 0, 0, 0, 60 ) );

                // Footer: maintainer (left), upstream credit (right)
                QFont fLabel = makeFont( 12, QFont::Normal );
                QFont fName  = makeFont( 12, QFont::DemiBold );
                const int fy = footerY + 26;
                int fx = MARGIN;

                p.setFont( fLabel );
                p.setPen( TEXT_MUTED );
                QString lbl = SplashScreen::tr( "Maintainer:" ) + " ";
                p.drawText( fx, fy, lbl );
                fx += QFontMetrics( fLabel ).horizontalAdvance( lbl );

                p.setFont( fName );
                p.setPen( TEXT_MAIN );
                p.drawText( fx, fy, MAINTAINER );
                fx += QFontMetrics( fName ).horizontalAdvance( MAINTAINER );

                p.setFont( fLabel );
                p.setPen( TEXT_MUTED );
                QString sep = "  ·  ";
                p.drawText( fx, fy, sep );
                fx += QFontMetrics( fLabel ).horizontalAdvance( sep );
                p.setPen( ACCENT_LIGHT );
                p.drawText( fx, fy, MAINTAINER_SITE );

                p.setPen( QColor( 148, 163, 184, 170 ) );
                p.setFont( makeFont( 11, QFont::Normal ) );
                p.drawText( QRectF( 0, footerY, W - MARGIN, H - footerY ),
                            Qt::AlignRight | Qt::AlignVCenter,
                            SplashScreen::tr( "Based on gLabels · GPL v3" ) );

                // Hairline border
                p.setPen( QPen( QColor( 255, 255, 255, 36 ), 1 ) );
                p.setBrush( Qt::NoBrush );
                p.drawRect( r.adjusted( 0.5, 0.5, -0.5, -0.5 ) );

                return pixmap;
        }


        ///
        /// Dynamic content: status text, percentage and progress bar
        ///
        void SplashScreen::drawContents( QPainter* p )
        {
                p->setRenderHint( QPainter::Antialiasing );
                p->setRenderHint( QPainter::TextAntialiasing );
                p->scale( mScale, mScale );

                const QRectF track( MARGIN, BAR_Y, W - 2*MARGIN, BAR_H );

                // Status message (left) and percent (right)
                p->setFont( makeFont( 12, QFont::Normal ) );
                p->setPen( TEXT_SOFT );
                QRectF textRow( MARGIN, BAR_Y - 24, track.width(), 18 );
                QString msg = QFontMetrics( p->font() ).elidedText( mMessage, Qt::ElideRight,
                                                                   int( textRow.width() - 60 ) );
                p->drawText( textRow, Qt::AlignLeft | Qt::AlignVCenter, msg );

                p->setFont( makeFont( 12, QFont::DemiBold ) );
                p->setPen( TEXT_MAIN );
                p->drawText( textRow, Qt::AlignRight | Qt::AlignVCenter,
                             QString( "%1%" ).arg( int( std::round( mShown ) ) ) );

                // Track
                p->setPen( Qt::NoPen );
                p->setBrush( QColor( 255, 255, 255, 28 ) );
                p->drawRoundedRect( track, BAR_H/2.0, BAR_H/2.0 );

                // Fill
                qreal fillW = track.width() * mShown / 100.0;
                if ( fillW > 0.5 )
                {
                        QRectF fill( track.x(), track.y(), fillW, BAR_H );
                        QLinearGradient fg( track.topLeft(), track.topRight() );
                        fg.setColorAt( 0, ACCENT );
                        fg.setColorAt( 1, ACCENT_2 );

                        QPainterPath clip;
                        clip.addRoundedRect( fill, BAR_H/2.0, BAR_H/2.0 );
                        p->save();
                        p->setClipPath( clip );
                        p->fillRect( fill, fg );

                        // Moving shimmer highlight
                        const qreal sw = 90;
                        qreal sx = track.x() - sw
                                   + std::fmod( mPhase * 0.35, track.width() + sw );
                        QLinearGradient sg( sx, 0, sx + sw, 0 );
                        sg.setColorAt( 0.0, QColor( 255, 255, 255, 0 ) );
                        sg.setColorAt( 0.5, QColor( 255, 255, 255, 150 ) );
                        sg.setColorAt( 1.0, QColor( 255, 255, 255, 0 ) );
                        p->fillRect( QRectF( sx, fill.y(), sw, BAR_H ), sg );
                        p->restore();

                        // Glow at the leading edge
                        QRadialGradient tip( QPointF( fill.right(), fill.center().y() ), 10 );
                        tip.setColorAt( 0, QColor( 167, 139, 250, 160 ) );
                        tip.setColorAt( 1, QColor( 167, 139, 250, 0 ) );
                        p->setBrush( tip );
                        p->drawEllipse( QPointF( fill.right(), fill.center().y() ), 10, 10 );
                }
        }

}
