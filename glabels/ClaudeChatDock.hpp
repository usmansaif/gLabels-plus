//  ClaudeChatDock.hpp
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

#ifndef ClaudeChatDock_hpp
#define ClaudeChatDock_hpp


#include <QDockWidget>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QTimer>

class QLabel;
class QPlainTextEdit;
class QStackedWidget;
class QTextBrowser;
class QToolButton;
class QWidget;


namespace glabels
{
        class MainWindow;


        ///
        /// View > Panels > Claude Chat
        ///
        /// A chat with Claude in a panel that docks to any side of the window (or
        /// floats).  It runs Claude Code in the background ("claude -p" with
        /// stream-json input and output), gives it the gLabels Plus tools (see
        /// McpServer and AiBridge) and only read access to files, and shows the
        /// conversation as chat messages while Claude edits the label next to it.
        ///
        class ClaudeChatDock : public QDockWidget
        {
                Q_OBJECT

        public:
                explicit ClaudeChatDock( MainWindow* window );
                ~ClaudeChatDock() override;

                /// Put the cursor in the message box
                void focusInput();

        protected:
                void showEvent( QShowEvent* event ) override;
                bool eventFilter( QObject* watched, QEvent* event ) override;

        private:
                void checkInstalled();
                void send( const QString& text );
                void stop();
                void newChat();
                bool startProcess();
                void onStdout();
                void onFinished( int exitCode, QProcess::ExitStatus status );
                void handleEvent( const QJsonObject& event );
                void setBusy( bool busy );
                void moveTo( Qt::DockWidgetArea area );

                // Transcript
                void appendUser( const QString& text );
                void appendClaudeText( const QString& delta );
                void appendTool( const QString& toolName );
                void appendNote( const QString& text, bool error );
                void renderStream();
                void endStream();
                void scrollToEndIfFollowing( bool following );
                bool isFollowing() const;
                void updateEmptyState();

                MainWindow*     mWindow;

                QStackedWidget* mPages;
                QLabel*         mStatus;
                QToolButton*    mStopButton;
                QToolButton*    mNewButton;
                QTextBrowser*   mTranscript;
                QWidget*        mSuggestions;
                QPlainTextEdit* mInput;
                QToolButton*    mSendButton;
                QLabel*         mMissingText;

                QString         mClaudePath;
                QProcess*       mProcess = nullptr;
                QByteArray      mStdoutBuffer;
                QString         mStderrTail;
                QString         mSessionId;
                QString         mPromptFile;
                QString         mMcpFile;
                bool            mBusy = false;
                bool            mStopping = false;
                bool            mHasMessages = false;

                // The Claude text being streamed into the transcript
                int             mStreamStart = -1;
                QString         mStreamText;
                bool            mTurnHeaderShown = false;
                QTimer          mRenderTimer;
                QTimer          mDotsTimer;
                int             mDots = 0;
                QSet<QString>   mFailedToolIds;
        };

}


#endif // ClaudeChatDock_hpp
