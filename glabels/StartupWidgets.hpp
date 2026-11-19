//  StartupWidgets.hpp
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

#ifndef StartupWidgets_hpp
#define StartupWidgets_hpp


#include <QAbstractButton>
#include <QColor>
#include <QImage>
#include <QPair>
#include <QStringList>
#include <QStyledItemDelegate>


namespace glabels
{
        namespace model
        {
                class Model;
        }

        ///
        /// Painted building blocks of the Welcome screen (StartupView)
        ///
        namespace startup
        {

                ///
                /// Color tokens shared by every part of the Welcome screen
                ///
                struct Theme
                {
                        bool   dark{ false };
                        QColor bg, surface, muted, hover, border, borderStrong;
                        QColor text, text2, text3;
                        QColor primary, primaryDark, tint, tintBorder, onPrimary;

                        static Theme make( const QColor& accent, bool dark );
                };


                /// Line icons (24x24 SVG bodies drawn with "currentColor")
                namespace icon
                {
                        extern const char* const NewLabel;
                        extern const char* const Open;
                        extern const char* const Designer;
                        extern const char* const Recent;
                        extern const char* const Folder;
                        extern const char* const Plus;
                        extern const char* const Search;
                        extern const char* const Print;
                        extern const char* const Samples;
                }

                /// Render an icon in a given color at a logical size
                QPixmap iconPixmap( const char* svg, const QColor& color, int size, qreal dpr );


                ///
                /// Sidebar navigation row: icon, text and an optional shortcut key or count
                ///
                class NavButton : public QAbstractButton
                {
                public:
                        enum TrailingStyle { None, Key, Count };

                        NavButton( const char* svg, const QString& text, const Theme* theme, QWidget* parent = nullptr );

                        void setTrailing( const QString& text, TrailingStyle style );
                        void setActive( bool active );
                        bool isActive() const { return mActive; }

                        QSize sizeHint() const override;
                        QSize minimumSizeHint() const override;

                protected:
                        void paintEvent( QPaintEvent* event ) override;

                private:
                        const char*   mSvg;
                        const Theme*  mTheme;
                        QString       mTrailing;
                        TrailingStyle mTrailingStyle{ None };
                        bool          mActive{ false };
                };


                ///
                /// Quick start tile: icon, title and description
                ///
                class TileButton : public QAbstractButton
                {
                public:
                        TileButton( const char* svg, const QString& title, const QString& description,
                                    bool primary, const Theme* theme, QWidget* parent = nullptr );

                        QSize sizeHint() const override;
                        QSize minimumSizeHint() const override;

                protected:
                        void paintEvent( QPaintEvent* event ) override;

                private:
                        const char*  mSvg;
                        QString      mDescription;
                        bool         mPrimary;
                        const Theme* mTheme;
                };


                ///
                /// Checkerboard well showing a label preview (or a placeholder)
                ///
                class PreviewWell : public QWidget
                {
                public:
                        PreviewWell( const Theme* theme, QWidget* parent = nullptr );

                        void setImage( const QImage& image, const QString& placeholder = QString() );
                        QSize imageArea() const;

                        QSize sizeHint() const override;

                protected:
                        void paintEvent( QPaintEvent* event ) override;

                private:
                        const Theme* mTheme;
                        QImage       mImage;
                        QString      mPlaceholder;
                };


                ///
                /// Row of keyboard hints ("[/] Search  [Ctrl N] New label ...")
                ///
                class KeyHints : public QWidget
                {
                public:
                        using Hint = QPair<QStringList, QString>;

                        KeyHints( const QList<Hint>& hints, const Theme* theme, QWidget* parent = nullptr );

                        QSize sizeHint() const override;
                        QSize minimumSizeHint() const override;

                protected:
                        void paintEvent( QPaintEvent* event ) override;

                private:
                        QList<Hint>  mHints;
                        const Theme* mTheme;
                };


                ///
                /// Label list row: thumbnail, name, project tag, folder and modified time
                ///
                class LabelRowDelegate : public QStyledItemDelegate
                {
                public:
                        enum Roles
                        {
                                PathRole = Qt::UserRole + 1,
                                NameRole,
                                FolderRole,
                                ProjectRole,
                                WhenRole,
                                ExistsRole,
                                ThumbRole
                        };

                        static const QSize ThumbSize;

                        LabelRowDelegate( const Theme* theme, QObject* parent = nullptr );

                        QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
                        void paint( QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index ) const override;

                private:
                        const Theme* mTheme;
                };


                ///
                /// Label grid tile: thumbnail on top, name and a detail line below.
                /// Uses the LabelRowDelegate roles.  Small or Medium size.
                ///
                class LabelTileDelegate : public QStyledItemDelegate
                {
                public:
                        enum Size { Small, Medium };

                        /// Largest thumbnail area (Medium); render thumbnails at this size
                        static const QSize MaxThumbSize;

                        LabelTileDelegate( const Theme* theme, QObject* parent = nullptr );

                        void  setTileSize( Size size ) { mSize = size; }
                        Size  tileSize() const { return mSize; }
                        QSize tileExtent() const;

                        QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
                        void paint( QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index ) const override;

                private:
                        const Theme* mTheme;
                        Size         mSize{ Medium };
                };


                /// Draw a label (shape and objects, first merge record) scaled into maxSize pixels
                QImage renderLabel( const model::Model& label, const QSize& maxSize );

                /// Thumbnail of a label file, cached on disk; null if it can't be read
                QImage labelThumbnail( const QString& fileName, const QSize& maxSize );

        }
}


#endif // StartupWidgets_hpp
