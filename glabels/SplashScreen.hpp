//  SplashScreen.hpp
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

#ifndef SplashScreen_hpp
#define SplashScreen_hpp


#include <QElapsedTimer>
#include <QPixmap>
#include <QSplashScreen>
#include <QTimer>

class QScreen;


namespace glabels
{

        ///
        /// Startup splash screen: branding plus an animated loading progress bar
        ///
        class SplashScreen : public QSplashScreen
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                SplashScreen();


                /////////////////////////////////
                // Public methods
                /////////////////////////////////
        public:
                /// Advance the progress bar to percent (0-100) and show a status message.
                /// Pumps the event loop so the splash repaints between blocking init steps.
                void setProgress( int percent, const QString& message );

                /// Keep the splash up for minVisibleMs in total, filling the bar
                /// gradually to 100% over the remaining time.
                /// Call QSplashScreen::finish() afterwards, once the main window is shown.
                void finishLoading( int minVisibleMs = 10000 );


                /////////////////////////////////
                // Event handlers
                /////////////////////////////////
        protected:
                void drawContents( QPainter* painter ) override;
                void mousePressEvent( QMouseEvent* event ) override;


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                static qreal   scaleForScreen( const QScreen* screen );
                static QPixmap renderBackground( const QSize& size, qreal dpr, qreal scale );
                void onTick();
                void pump();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QTimer        mTimer;
                QElapsedTimer mClock;
                double        mShown  { 0.0 };   // displayed progress, eases toward mTarget
                double        mTarget { 0.0 };
                QString       mMessage;
                qreal         mScale  { 1.4 };   // design-grid -> screen scale, set from the screen size
                qint64        mPhase  { 0 };     // shimmer animation phase (ms)
        };

}


#endif // SplashScreen_hpp
