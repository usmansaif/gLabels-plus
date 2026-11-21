//  StartupWidgets.cpp
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

#include "StartupWidgets.hpp"

#include "model/Frame.hpp"
#include "model/Model.hpp"
#include "model/Variables.hpp"
#include "model/XmlLabelParser.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QSvgRenderer>

#include <algorithm>
#include <memory>


namespace glabels
{
        namespace startup
        {

                namespace
                {
                        QColor mix( const QColor& a, const QColor& b, double t )
                        {
                                return QColor::fromRgbF( a.redF()   + ( b.redF()   - a.redF()   ) * t,
                                                         a.greenF() + ( b.greenF() - a.greenF() ) * t,
                                                         a.blueF()  + ( b.blueF()  - a.blueF()  ) * t );
                        }

                        QColor withAlpha( QColor c, int alpha )
                        {
                                c.setAlpha( alpha );
                                return c;
                        }

                        QFont scaledFont( const QFont& base, double factor, int weight = -1 )
                        {
                                QFont f = base;
                                f.setPointSizeF( base.pointSizeF() * factor );
                                if ( weight >= 0 )
                                {
                                        f.setWeight( QFont::Weight( weight ) );
                                }
                                return f;
                        }

                        QFont monoFont( const QFont& base, double factor )
                        {
                                QFont f = QFontDatabase::systemFont( QFontDatabase::FixedFont );
                                f.setPointSizeF( base.pointSizeF() * factor );
                                return f;
                        }

                        /// A keyboard key cap; returns its width
                        double drawKey( QPainter* p, const QPointF& topLeft, double height, const QString& key,
                                        const QFont& font, const QColor& fg, const QColor& bg, const QColor& border )
                        {
                                QFontMetricsF fm( font );
                                double w = fm.horizontalAdvance( key ) + 12;
                                QRectF r( topLeft, QSizeF( w, height ) );
                                p->setPen( Qt::NoPen );
                                p->setBrush( border );
                                p->drawRoundedRect( r, 5, 5 );
                                p->setBrush( bg );
                                p->drawRoundedRect( r.adjusted( 1, 1, -1, -2 ), 4, 4 );
                                p->setFont( font );
                                p->setPen( fg );
                                p->drawText( r.adjusted( 0, 0, 0, -1 ), Qt::AlignCenter, key );
                                return w;
                        }

                        double keyWidth( const QString& key, const QFont& font )
                        {
                                return QFontMetricsF( font ).horizontalAdvance( key ) + 12;
                        }
                }


                ///
                /// Theme
                ///
                Theme Theme::make( const QPalette& palette, bool dark )
                {
                        // Neutrals come from the application palette (Preferences > Appearance),
                        // so the Welcome screen matches menus, toolbars and dialogs.
                        const QColor window = palette.color( QPalette::Window );
                        const QColor base   = palette.color( QPalette::Base );
                        const QColor ink    = palette.color( QPalette::WindowText );
                        const QColor accent = palette.color( QPalette::Highlight );

                        Theme t;
                        t.dark = dark;
                        if ( dark )
                        {
                                t.bg           = base;
                                t.surface      = window;
                                t.muted        = mix( base, window, 0.5 );
                                t.hover        = mix( window, ink, 0.07 );
                                t.border       = mix( window, ink, 0.10 );
                                t.borderStrong = mix( window, ink, 0.20 );
                                t.text         = ink;
                                t.text2        = mix( window, ink, 0.75 );
                                t.text3        = mix( window, ink, 0.58 );
                                t.primary      = accent.lightness() < 90 ? accent.lighter( 140 ) : accent;
                                t.primaryDark  = t.primary.darker( 115 );
                                t.tint         = mix( t.surface, t.primary, 0.16 );
                                t.tintBorder   = mix( t.surface, t.primary, 0.45 );
                        }
                        else
                        {
                                t.bg           = mix( window, ink, 0.035 );
                                t.surface      = mix( window, Qt::white, 0.7 );
                                t.muted        = window;
                                t.hover        = mix( window, ink, 0.065 );
                                t.border       = mix( window, ink, 0.10 );
                                t.borderStrong = mix( window, ink, 0.18 );
                                t.text         = mix( window, ink, 0.92 );
                                t.text2        = mix( window, ink, 0.70 );
                                t.text3        = mix( window, ink, 0.56 );
                                t.primary      = accent;
                                t.primaryDark  = accent.darker( 115 );
                                t.tint         = mix( t.surface, accent, 0.08 );
                                t.tintBorder   = mix( t.surface, accent, 0.30 );
                        }
                        t.onPrimary = Qt::white;
                        return t;
                }


                ///
                /// Icons
                ///
                namespace icon
                {
#define SVG_OPEN "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' " \
                 "stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>"

                        const char* const NewLabel = SVG_OPEN
                                "<path d='M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z'/>"
                                "<path d='M14 3v5h5M12 11v6M9 14h6'/></svg>";
                        const char* const Open = SVG_OPEN
                                "<path d='M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'/></svg>";
                        const char* const Designer = SVG_OPEN
                                "<rect x='3' y='3' width='18' height='18' rx='2'/><path d='M3 9h18M9 21V9'/></svg>";
                        const char* const Recent = SVG_OPEN
                                "<circle cx='12' cy='12' r='9'/><path d='M12 7v5l3 2'/></svg>";
                        const char* const Folder =
                                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='currentColor' opacity='.85'>"
                                "<path d='M3 6.5A1.5 1.5 0 0 1 4.5 5H9l2 2.2h8.5A1.5 1.5 0 0 1 21 8.7v8.8a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 17.5z'/></svg>";
                        const char* const Plus =
                                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
                                "stroke-width='2.4' stroke-linecap='round'><path d='M12 5v14M5 12h14'/></svg>";
                        const char* const Search =
                                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
                                "stroke-width='2' stroke-linecap='round'><circle cx='11' cy='11' r='7'/><path d='m20 20-3.5-3.5'/></svg>";
                        const char* const Print = SVG_OPEN
                                "<path d='M6 9V3h12v6'/><rect x='3' y='9' width='18' height='8' rx='2'/>"
                                "<path d='M7 14h10v7H7z'/></svg>";
                        const char* const Samples = SVG_OPEN
                                "<rect x='3' y='3' width='7' height='7' rx='1.5'/><rect x='14' y='3' width='7' height='7' rx='1.5'/>"
                                "<rect x='3' y='14' width='7' height='7' rx='1.5'/><rect x='14' y='14' width='7' height='7' rx='1.5'/></svg>";

#undef SVG_OPEN
                }


                QPixmap iconPixmap( const char* svg, const QColor& color, int size, qreal dpr )
                {
                        static QHash<QString, QPixmap> cache;
                        QString key = QString( "%1|%2|%3|%4" ).arg( quintptr( svg ) ).arg( color.name( QColor::HexArgb ) )
                                                              .arg( size ).arg( dpr );
                        auto it = cache.constFind( key );
                        if ( it != cache.constEnd() )
                        {
                                return *it;
                        }

                        QString source = QString::fromLatin1( svg );
                        source.replace( "currentColor", color.name() );
                        QSvgRenderer renderer( source.toUtf8() );

                        int px = qRound( size * dpr );
                        QPixmap pixmap( px, px );
                        pixmap.fill( Qt::transparent );
                        {
                                QPainter p( &pixmap );
                                p.setRenderHint( QPainter::Antialiasing );
                                if ( color.alpha() < 255 )
                                {
                                        p.setOpacity( color.alphaF() );
                                }
                                renderer.render( &p );
                        }
                        pixmap.setDevicePixelRatio( dpr );
                        cache.insert( key, pixmap );
                        return pixmap;
                }


                ///
                /// NavButton
                ///
                NavButton::NavButton( const char* svg, const QString& text, const Theme* theme, QWidget* parent )
                        : QAbstractButton( parent ), mSvg( svg ), mTheme( theme )
                {
                        setText( text );
                        setAccessibleName( text );
                        setAttribute( Qt::WA_Hover );
                        setCursor( Qt::PointingHandCursor );
                        setFocusPolicy( Qt::TabFocus );
                        setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                }


                void NavButton::setTrailing( const QString& text, TrailingStyle style )
                {
                        mTrailing = text;
                        mTrailingStyle = text.isEmpty() ? None : style;
                        updateGeometry();
                        update();
                }


                void NavButton::setActive( bool active )
                {
                        if ( mActive != active )
                        {
                                mActive = active;
                                update();
                        }
                }


                QSize NavButton::sizeHint() const
                {
                        QFontMetrics fm( font() );
                        return QSize( 38 + fm.horizontalAdvance( text() ) + 60, std::max( 34, fm.height() + 16 ) );
                }


                QSize NavButton::minimumSizeHint() const
                {
                        return QSize( 80, sizeHint().height() );
                }


                void NavButton::paintEvent( QPaintEvent* )
                {
                        QPainter p( this );
                        p.setRenderHint( QPainter::Antialiasing );
                        const Theme& t = *mTheme;

                        bool hover = underMouse() && isEnabled();
                        QRectF r = QRectF( rect() ).adjusted( 1, 1, -1, -1 );

                        if ( mActive )
                        {
                                p.setPen( Qt::NoPen );
                                p.setBrush( withAlpha( t.primary, 70 ) );
                                p.drawRoundedRect( r.translated( 0, 1.5 ), 8, 8 );
                                p.setBrush( t.primary );
                                p.drawRoundedRect( r, 8, 8 );
                        }
                        else if ( hover || isDown() )
                        {
                                p.setPen( Qt::NoPen );
                                p.setBrush( t.hover );
                                p.drawRoundedRect( r, 8, 8 );
                        }
                        if ( hasFocus() )
                        {
                                p.setPen( QPen( t.tintBorder, 2 ) );
                                p.setBrush( Qt::NoBrush );
                                p.drawRoundedRect( r.adjusted( 1, 1, -1, -1 ), 7, 7 );
                        }

                        QColor fg = mActive ? t.onPrimary : ( hover ? t.text : t.text2 );

                        // Icon
                        const int iconSize = 18;
                        p.drawPixmap( QPointF( r.left() + 9, r.center().y() - iconSize/2.0 ),
                                      iconPixmap( mSvg, fg, iconSize, devicePixelRatioF() ) );

                        // Trailing key or count
                        double right = r.right() - 9;
                        if ( mTrailingStyle == Key )
                        {
                                QFont kf = monoFont( font(), 0.8 );
                                kf.setWeight( QFont::DemiBold );
                                double h = QFontMetricsF( kf ).height() + 4;
                                double w = keyWidth( mTrailing, kf );
                                p.setOpacity( 0.8 );
                                drawKey( &p, QPointF( right - w, r.center().y() - h/2 ), h, mTrailing, kf,
                                         mActive ? t.onPrimary : t.text3,
                                         mActive ? withAlpha( Qt::white, 40 ) : t.surface,
                                         mActive ? withAlpha( Qt::white, 90 ) : t.borderStrong );
                                p.setOpacity( 1.0 );
                                right -= w + 8;
                        }
                        else if ( mTrailingStyle == Count )
                        {
                                QFont cf = scaledFont( font(), 0.8, QFont::DemiBold );
                                QFontMetricsF cfm( cf );
                                double w = cfm.horizontalAdvance( mTrailing ) + 16;
                                double h = cfm.height() + 2;
                                QRectF pill( right - w, r.center().y() - h/2, w, h );
                                p.setPen( Qt::NoPen );
                                p.setBrush( mActive ? withAlpha( Qt::white, 56 ) : t.border );
                                p.drawRoundedRect( pill, h/2, h/2 );
                                p.setFont( cf );
                                p.setPen( mActive ? t.onPrimary : t.text2 );
                                p.drawText( pill, Qt::AlignCenter, mTrailing );
                                right -= w + 8;
                        }

                        // Text
                        QFont tf = font();
                        tf.setWeight( mActive ? QFont::DemiBold : QFont::Medium );
                        p.setFont( tf );
                        p.setPen( fg );
                        double x = r.left() + 9 + iconSize + 10;
                        QRectF textRect( x, r.top(), std::max( 0.0, right - x ), r.height() );
                        p.drawText( textRect, Qt::AlignLeft | Qt::AlignVCenter,
                                    QFontMetricsF( tf ).elidedText( text(), Qt::ElideRight, textRect.width() ) );
                }


                ///
                /// TileButton
                ///
                TileButton::TileButton( const char* svg, const QString& title, const QString& description,
                                        bool primary, const Theme* theme, QWidget* parent )
                        : QAbstractButton( parent ), mSvg( svg ), mDescription( description ),
                          mPrimary( primary ), mTheme( theme )
                {
                        setText( title );
                        setToolTip( description );
                        setAccessibleName( title );
                        setAccessibleDescription( description );
                        setAttribute( Qt::WA_Hover );
                        setCursor( Qt::PointingHandCursor );
                        setFocusPolicy( Qt::TabFocus );
                        setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                }


                QSize TileButton::sizeHint() const
                {
                        QFontMetrics fm( font() );
                        QFontMetrics dfm( scaledFont( font(), 0.92 ) );
                        int h = 4 + 16 + 40 + 12 + fm.height() + 2 + 2 * dfm.lineSpacing() + 16 + 4;
                        return QSize( 220, h );
                }


                QSize TileButton::minimumSizeHint() const
                {
                        return QSize( 150, sizeHint().height() );
                }


                void TileButton::paintEvent( QPaintEvent* )
                {
                        QPainter p( this );
                        p.setRenderHint( QPainter::Antialiasing );
                        const Theme& t = *mTheme;

                        bool hover = underMouse() && isEnabled() && !isDown();
                        QRectF card = QRectF( rect() ).adjusted( 1, 3, -1, -5 );
                        if ( hover )
                        {
                                card.translate( 0, -2 );
                        }

                        // Shadow
                        p.setPen( Qt::NoPen );
                        if ( hover )
                        {
                                p.setBrush( QColor( 0, 0, 0, t.dark ? 110 : 16 ) );
                                p.drawRoundedRect( card.translated( 0, 5 ).adjusted( 2, 0, -2, 0 ), 12, 12 );
                                p.setBrush( QColor( 0, 0, 0, t.dark ? 70 : 14 ) );
                                p.drawRoundedRect( card.translated( 0, 2 ), 12, 12 );
                        }
                        else
                        {
                                p.setBrush( QColor( 0, 0, 0, t.dark ? 70 : 12 ) );
                                p.drawRoundedRect( card.translated( 0, 1 ), 12, 12 );
                        }

                        // Card
                        QColor border = mPrimary ? ( hover ? t.primaryDark.darker( 110 ) : t.primaryDark )
                                                 : ( hover ? t.primary : t.border );
                        p.setPen( QPen( border, 1 ) );
                        p.setBrush( mPrimary ? ( isDown() ? t.primaryDark : t.primary ) : t.surface );
                        p.drawRoundedRect( card.adjusted( 0.5, 0.5, -0.5, -0.5 ), 12, 12 );

                        if ( hasFocus() )
                        {
                                p.setPen( QPen( mPrimary ? withAlpha( Qt::white, 160 ) : t.tintBorder, 2 ) );
                                p.setBrush( Qt::NoBrush );
                                p.drawRoundedRect( card.adjusted( 2, 2, -2, -2 ), 10, 10 );
                        }

                        // Icon box
                        QRectF box( card.left() + 16, card.top() + 16, 40, 40 );
                        p.setPen( Qt::NoPen );
                        p.setBrush( mPrimary ? withAlpha( Qt::white, 51 ) : ( hover ? t.primary : t.tint ) );
                        p.drawRoundedRect( box, 10, 10 );
                        QColor iconColor = ( mPrimary || hover ) ? t.onPrimary : t.primary;
                        p.drawPixmap( QPointF( box.center().x() - 10, box.center().y() - 10 ),
                                      iconPixmap( mSvg, iconColor, 20, devicePixelRatioF() ) );

                        // Title and description
                        double x = card.left() + 16;
                        double w = card.width() - 32;
                        double y = box.bottom() + 12;

                        QFont tf = font();
                        tf.setWeight( QFont::DemiBold );
                        QFontMetricsF tfm( tf );
                        p.setFont( tf );
                        p.setPen( mPrimary ? t.onPrimary : t.text );
                        p.drawText( QRectF( x, y, w, tfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                    tfm.elidedText( text(), Qt::ElideRight, w ) );
                        y += tfm.height() + 2;

                        QFont df = scaledFont( font(), 0.92 );
                        p.setFont( df );
                        p.setPen( mPrimary ? mix( t.onPrimary, t.primary, 0.18 ) : t.text3 );
                        p.drawText( QRectF( x, y, w, card.bottom() - y - 8 ),
                                    Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, mDescription );
                }


                ///
                /// PreviewWell
                ///
                PreviewWell::PreviewWell( const Theme* theme, QWidget* parent )
                        : QWidget( parent ), mTheme( theme )
                {
                        setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                        setMinimumHeight( 190 );
                }


                void PreviewWell::setImage( const QImage& image, const QString& placeholder )
                {
                        mImage = image;
                        mPlaceholder = placeholder;
                        update();
                }


                QSize PreviewWell::imageArea() const
                {
                        return QSize( std::max( 40, width() - 40 ), std::max( 40, height() - 40 ) );
                }


                QSize PreviewWell::sizeHint() const
                {
                        return QSize( 280, 190 );
                }


                void PreviewWell::paintEvent( QPaintEvent* )
                {
                        QPainter p( this );
                        p.setRenderHint( QPainter::Antialiasing );
                        const Theme& t = *mTheme;

                        QRectF r = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                        QPainterPath clip;
                        clip.addRoundedRect( r, 12, 12 );

                        // Checkerboard
                        p.save();
                        p.setClipPath( clip );
                        QColor b = t.muted;
                        QColor a = mix( t.muted, t.text, t.dark ? 0.05 : 0.035 );
                        p.fillRect( rect(), b );
                        const int sq = 8;
                        for ( int y = 0; y < height(); y += sq )
                        {
                                for ( int x = ( ( y / sq ) % 2 ) * sq; x < width(); x += 2 * sq )
                                {
                                        p.fillRect( x, y, sq, sq, a );
                                }
                        }
                        p.restore();

                        p.setPen( QPen( t.border, 1 ) );
                        p.setBrush( Qt::NoBrush );
                        p.drawRoundedRect( r, 12, 12 );

                        if ( !mImage.isNull() )
                        {
                                QSizeF is = QSizeF( mImage.size() ) / mImage.devicePixelRatio();
                                QRectF ir( QPointF( 0, 0 ), is );
                                ir.moveCenter( QRectF( rect() ).center() );

                                // Soft drop shadow under the sheet
                                p.setPen( Qt::NoPen );
                                for ( int i = 3; i >= 1; i-- )
                                {
                                        p.setBrush( QColor( 0, 0, 0, t.dark ? 40 : 14 ) );
                                        p.drawRoundedRect( ir.adjusted( 2, 2 + i, -2, -2 + 2*i ), 4, 4 );
                                }
                                p.drawImage( ir, mImage );
                        }
                        else if ( !mPlaceholder.isEmpty() )
                        {
                                p.setPen( t.text3 );
                                p.drawText( QRectF( rect() ).adjusted( 16, 16, -16, -16 ),
                                            Qt::AlignCenter | Qt::TextWordWrap, mPlaceholder );
                        }
                }


                ///
                /// KeyHints
                ///
                KeyHints::KeyHints( const QList<Hint>& hints, const Theme* theme, QWidget* parent )
                        : QWidget( parent ), mHints( hints ), mTheme( theme )
                {
                        setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
                }


                QSize KeyHints::sizeHint() const
                {
                        QFont kf = monoFont( font(), 0.8 );
                        QFont tf = scaledFont( font(), 0.9 );
                        QFontMetricsF tfm( tf );
                        double w = 0;
                        for ( const auto& hint : mHints )
                        {
                                for ( const auto& key : hint.first )
                                {
                                        w += keyWidth( key, kf ) + 3;
                                }
                                w += 3 + tfm.horizontalAdvance( hint.second ) + 16;
                        }
                        int h = int( std::max( tfm.height(), QFontMetricsF( kf ).height() + 4 ) ) + 2;
                        return QSize( int( w ), h );
                }


                QSize KeyHints::minimumSizeHint() const
                {
                        return QSize( 0, sizeHint().height() );
                }


                void KeyHints::paintEvent( QPaintEvent* )
                {
                        QPainter p( this );
                        p.setRenderHint( QPainter::Antialiasing );
                        const Theme& t = *mTheme;

                        QFont kf = monoFont( font(), 0.8 );
                        kf.setWeight( QFont::DemiBold );
                        QFont tf = scaledFont( font(), 0.9 );
                        QFontMetricsF tfm( tf );
                        double kh = QFontMetricsF( kf ).height() + 4;
                        double cy = height() / 2.0;

                        double x = 0;
                        for ( const auto& hint : mHints )
                        {
                                double needed = 0;
                                for ( const auto& key : hint.first )
                                {
                                        needed += keyWidth( key, kf ) + 3;
                                }
                                needed += 3 + tfm.horizontalAdvance( hint.second );
                                if ( x + needed > width() )
                                {
                                        break;   // drop hints that don't fit rather than clipping them
                                }

                                for ( const auto& key : hint.first )
                                {
                                        x += drawKey( &p, QPointF( x, cy - kh/2 ), kh, key, kf,
                                                      t.text3, t.surface, t.borderStrong ) + 3;
                                }
                                x += 3;
                                p.setFont( tf );
                                p.setPen( t.text3 );
                                double tw = tfm.horizontalAdvance( hint.second );
                                p.drawText( QRectF( x, 0, tw + 1, height() ), Qt::AlignLeft | Qt::AlignVCenter, hint.second );
                                x += tw + 16;
                        }
                }


                ///
                /// LabelRowDelegate
                ///
                const QSize LabelRowDelegate::ThumbSize( 56, 38 );


                LabelRowDelegate::LabelRowDelegate( const Theme* theme, QObject* parent )
                        : QStyledItemDelegate( parent ), mTheme( theme )
                {
                }


                QSize LabelRowDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& ) const
                {
                        QFontMetrics fm( option.font );
                        return QSize( 300, std::max( ThumbSize.height() + 24, 2 * fm.height() + 26 ) );
                }


                void LabelRowDelegate::paint( QPainter* p, const QStyleOptionViewItem& option,
                                              const QModelIndex& index ) const
                {
                        p->save();
                        p->setRenderHint( QPainter::Antialiasing );
                        p->setRenderHint( QPainter::SmoothPixmapTransform );
                        const Theme& t = *mTheme;

                        QRectF r( option.rect );
                        bool selected = option.state & QStyle::State_Selected;
                        bool hover    = option.state & QStyle::State_MouseOver;
                        bool exists   = index.data( ExistsRole ).toBool();

                        // Background, separator and selection bar
                        if ( selected )
                        {
                                p->fillRect( r, t.tint );
                        }
                        else if ( hover )
                        {
                                p->fillRect( r, mix( t.surface, t.primary, t.dark ? 0.07 : 0.035 ) );
                        }
                        if ( index.row() < index.model()->rowCount() - 1 )
                        {
                                p->fillRect( QRectF( r.left(), r.bottom(), r.width(), 1 ), t.border );
                        }
                        if ( selected )
                        {
                                p->fillRect( QRectF( r.left(), r.top(), 3, r.height() ), t.primary );
                        }

                        // Thumbnail
                        QRectF thumb( r.left() + 16, r.center().y() - ThumbSize.height()/2.0,
                                      ThumbSize.width(), ThumbSize.height() );
                        p->setPen( Qt::NoPen );
                        p->setBrush( QColor( 0, 0, 0, t.dark ? 60 : 10 ) );
                        p->drawRoundedRect( thumb.translated( 0, 1 ), 6, 6 );
                        p->setPen( QPen( t.borderStrong, 1 ) );
                        p->setBrush( t.surface );
                        p->drawRoundedRect( thumb.adjusted( 0.5, 0.5, -0.5, -0.5 ), 6, 6 );

                        if ( !exists )
                        {
                                p->setOpacity( 0.4 );
                        }
                        QImage image = index.data( ThumbRole ).value<QImage>();
                        if ( !image.isNull() )
                        {
                                QSizeF is = QSizeF( image.size() ) / image.devicePixelRatio();
                                QRectF area = thumb.adjusted( 4, 4, -4, -4 );
                                double s = std::min( { 1.0, area.width() / is.width(), area.height() / is.height() } );
                                QRectF ir( QPointF( 0, 0 ), is * s );
                                ir.moveCenter( area.center() );
                                p->drawImage( ir, image );
                        }
                        else
                        {
                                QRect ir( 0, 0, 20, 20 );
                                ir.moveCenter( thumb.center().toPoint() );
                                p->setOpacity( p->opacity() * 0.5 );
                                QIcon::fromTheme( "glabels" ).paint( p, ir );
                        }
                        p->setOpacity( 1.0 );

                        // Modified time pill
                        QFont small = scaledFont( option.font, 0.88 );
                        QFontMetricsF sfm( small );
                        QString when = index.data( WhenRole ).toString();
                        double pillW = sfm.horizontalAdvance( when ) + 22;
                        double pillH = sfm.height() + 6;
                        QRectF pill( r.right() - 16 - pillW, r.center().y() - pillH/2, pillW, pillH );
                        p->setPen( QPen( t.border, 1 ) );
                        p->setBrush( t.muted );
                        p->drawRoundedRect( pill.adjusted( 0.5, 0.5, -0.5, -0.5 ), pillH/2, pillH/2 );
                        p->setFont( small );
                        p->setPen( exists ? t.text2 : QColor( 192, 57, 43 ) );
                        p->drawText( pill, Qt::AlignCenter, when );

                        // Name, project tag and folder
                        double x = thumb.right() + 14;
                        double w = std::max( 0.0, pill.left() - 14 - x );

                        QFont nameFont = option.font;
                        nameFont.setWeight( QFont::DemiBold );
                        QFontMetricsF nfm( nameFont );
                        QFont mono = monoFont( option.font, 0.86 );
                        QFontMetricsF mfm( mono );

                        double blockH = nfm.height() + 2 + mfm.height();
                        double y = r.center().y() - blockH / 2;

                        QString project = index.data( ProjectRole ).toString();
                        QFont tagFont = scaledFont( option.font, 0.8, QFont::DemiBold );
                        QFontMetricsF tfm( tagFont );
                        double tagW = project.isEmpty() ? 0 : std::min( w * 0.5, tfm.horizontalAdvance( project ) + 16 );

                        QString name = nfm.elidedText( index.data( NameRole ).toString(), Qt::ElideMiddle,
                                                       std::max( 0.0, w - ( tagW > 0 ? tagW + 8 : 0 ) ) );
                        p->setFont( nameFont );
                        p->setPen( exists ? t.text : t.text3 );
                        p->drawText( QRectF( x, y, w, nfm.height() ), Qt::AlignLeft | Qt::AlignVCenter, name );

                        if ( tagW > 0 )
                        {
                                double tagH = tfm.height() + 2;
                                QRectF tag( x + nfm.horizontalAdvance( name ) + 8, y + ( nfm.height() - tagH ) / 2, tagW, tagH );
                                p->setPen( Qt::NoPen );
                                p->setBrush( selected ? mix( t.tint, t.primary, 0.18 ) : t.border );
                                p->drawRoundedRect( tag, tagH/2, tagH/2 );
                                p->setFont( tagFont );
                                p->setPen( selected ? t.primaryDark : t.text2 );
                                p->drawText( tag, Qt::AlignCenter, tfm.elidedText( project, Qt::ElideRight, tagW - 12 ) );
                        }

                        y += nfm.height() + 2;
                        p->setFont( mono );
                        p->setPen( t.text3 );
                        p->drawText( QRectF( x, y, w, mfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                     mfm.elidedText( index.data( FolderRole ).toString(), Qt::ElideMiddle, w ) );

                        p->restore();
                }


                ///
                /// Label grid tile
                ///
                const QSize LabelTileDelegate::MaxThumbSize( 160, 112 );

                LabelTileDelegate::LabelTileDelegate( const Theme* theme, QObject* parent )
                        : QStyledItemDelegate( parent ), mTheme( theme )
                {
                }


                QSize LabelTileDelegate::tileExtent() const
                {
                        return mSize == Small ? QSize( 132, 146 ) : QSize( 176, 182 );
                }


                QSize LabelTileDelegate::sizeHint( const QStyleOptionViewItem&, const QModelIndex& ) const
                {
                        return tileExtent();
                }


                void LabelTileDelegate::paint( QPainter* p, const QStyleOptionViewItem& option,
                                               const QModelIndex& index ) const
                {
                        p->save();
                        p->setRenderHint( QPainter::Antialiasing );
                        p->setRenderHint( QPainter::SmoothPixmapTransform );
                        const Theme& t = *mTheme;

                        // Center a fixed-size card in the grid cell
                        QSizeF extent = tileExtent();
                        QRectF r( QPointF( 0, 0 ), extent - QSizeF( 8, 8 ) );
                        r.moveCenter( QRectF( option.rect ).center() );

                        bool selected = option.state & QStyle::State_Selected;
                        bool hover    = option.state & QStyle::State_MouseOver;
                        bool exists   = index.data( LabelRowDelegate::ExistsRole ).toBool();

                        // Card
                        QColor fill = selected ? t.tint : ( hover ? mix( t.surface, t.primary, t.dark ? 0.07 : 0.035 ) : t.surface );
                        p->setPen( QPen( selected ? t.primary : ( hover ? t.borderStrong : t.border ), selected ? 2 : 1 ) );
                        p->setBrush( fill );
                        p->drawRoundedRect( r.adjusted( 1, 1, -1, -1 ), 9, 9 );

                        // Thumbnail well
                        double pad = mSize == Small ? 7 : 8;
                        QFont nameFont = scaledFont( option.font, mSize == Small ? 0.92 : 1.0, QFont::DemiBold );
                        QFont small = scaledFont( option.font, mSize == Small ? 0.8 : 0.86 );
                        QFontMetricsF nfm( nameFont );
                        QFontMetricsF sfm( small );
                        double textH = nfm.height() + 2 + sfm.height();
                        QRectF well( r.left() + pad, r.top() + pad, r.width() - 2*pad, r.height() - 2*pad - textH - 8 );
                        p->setPen( Qt::NoPen );
                        p->setBrush( t.muted );
                        p->drawRoundedRect( well, 6, 6 );

                        if ( !exists )
                        {
                                p->setOpacity( 0.4 );
                        }
                        QImage image = index.data( LabelRowDelegate::ThumbRole ).value<QImage>();
                        if ( !image.isNull() )
                        {
                                QSizeF is = QSizeF( image.size() ) / image.devicePixelRatio();
                                QRectF area = well.adjusted( 6, 6, -6, -6 );
                                double s = std::min( { 1.0, area.width() / is.width(), area.height() / is.height() } );
                                QRectF ir( QPointF( 0, 0 ), is * s );
                                ir.moveCenter( area.center() );
                                p->drawImage( ir, image );
                        }
                        else
                        {
                                QRect ir( 0, 0, 24, 24 );
                                ir.moveCenter( well.center().toPoint() );
                                p->setOpacity( p->opacity() * 0.5 );
                                QIcon::fromTheme( "glabels" ).paint( p, ir );
                        }
                        p->setOpacity( 1.0 );

                        // Name and detail line (project or category, then time or kind)
                        double x = r.left() + pad + 2;
                        double w = r.width() - 2*pad - 4;
                        double y = well.bottom() + 6;
                        p->setFont( nameFont );
                        p->setPen( exists ? t.text : t.text3 );
                        p->drawText( QRectF( x, y, w, nfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                     nfm.elidedText( index.data( LabelRowDelegate::NameRole ).toString(), Qt::ElideRight, w ) );

                        QString project = index.data( LabelRowDelegate::ProjectRole ).toString();
                        QString when    = index.data( LabelRowDelegate::WhenRole ).toString();
                        QString detail  = project.isEmpty() ? when
                                                            : ( when.isEmpty() ? project
                                                                               : project + QString::fromUtf8( " · " ) + when );
                        y += nfm.height() + 2;
                        p->setFont( small );
                        p->setPen( exists ? t.text3 : QColor( 192, 57, 43 ) );
                        p->drawText( QRectF( x, y, w, sfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                                     sfm.elidedText( detail, Qt::ElideRight, w ) );

                        p->restore();
                }


                ///
                /// Label rendering
                ///
                QImage renderLabel( const model::Model& label, const QSize& maxSize )
                {
                        if ( !label.frame() )
                        {
                                return QImage();
                        }
                        double w = label.w().pt();
                        double h = label.h().pt();
                        if ( w <= 0 || h <= 0 )
                        {
                                return QImage();
                        }
                        double scale = std::min( ( maxSize.width() - 4 ) / w, ( maxSize.height() - 4 ) / h );
                        QSize size( int( w*scale ) + 4, int( h*scale ) + 4 );

                        QImage image( size, QImage::Format_ARGB32_Premultiplied );
                        image.fill( Qt::transparent );

                        QPainter painter( &image );
                        painter.setRenderHint( QPainter::Antialiasing );
                        painter.setRenderHint( QPainter::SmoothPixmapTransform );
                        painter.translate( 2, 2 );
                        painter.scale( scale, scale );

                        // Label shape
                        QPainterPath path = label.frame()->path();
                        QTransform shape;
                        if ( label.rotate() )
                        {
                                shape.rotate( -90 );
                                shape.translate( -label.frame()->w().pt(), 0 );
                        }
                        QPainterPath outline = shape.map( path );
                        painter.setPen( QPen( QColor( 0, 0, 0, 60 ), 1.0/scale ) );
                        painter.setBrush( Qt::white );
                        painter.drawPath( outline );

                        // Objects, filled in with the first selected record when there is data
                        painter.save();
                        painter.setClipPath( outline );
                        model::Variables variables( label.constVariables() );
                        QList<merge::Record> records = label.merge() ? label.merge()->selectedRecords()
                                                                     : QList<merge::Record>();
                        label.draw( &painter, false, records.isEmpty() ? merge::Record() : records.first(), variables );
                        painter.restore();
                        painter.end();

                        return image;
                }


                QImage labelThumbnail( const QString& fileName, const QSize& maxSize )
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
                        if ( !label )
                        {
                                return QImage();
                        }
                        QImage image = renderLabel( *label, maxSize );
                        if ( !image.isNull() )
                        {
                                QDir().mkpath( cacheDir );
                                image.save( cacheFile, "PNG" );
                        }
                        return image;
                }

        }
}
