//  ClaudeChatDock.cpp
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


#include "ClaudeChatDock.hpp"

#include "AiBridge.hpp"
#include "AiConnectDialog.hpp"
#include "MainWindow.hpp"
#include "McpServer.hpp"

#include "model/Model.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextTable>
#include <QToolButton>
#include <QVBoxLayout>


namespace
{
        const QString toolPrefix = "mcp__glabels-plus__";


        /// What the user sees while Claude uses a tool
        QString toolDescription( const QString& fullName )
        {
                static const QHash<QString, const char*> names = {
                        { "get_status",        QT_TRANSLATE_NOOP( "ClaudeChatDock", "Looking at your open labels" ) },
                        { "get_format_guide",  QT_TRANSLATE_NOOP( "ClaudeChatDock", "Reading the label format" ) },
                        { "get_label",         QT_TRANSLATE_NOOP( "ClaudeChatDock", "Reading the label" ) },
                        { "set_label_objects", QT_TRANSLATE_NOOP( "ClaudeChatDock", "Updating the label" ) },
                        { "add_objects",       QT_TRANSLATE_NOOP( "ClaudeChatDock", "Adding to the label" ) },
                        { "delete_objects",    QT_TRANSLATE_NOOP( "ClaudeChatDock", "Removing objects" ) },
                        { "render_preview",    QT_TRANSLATE_NOOP( "ClaudeChatDock", "Checking a preview" ) },
                        { "list_templates",    QT_TRANSLATE_NOOP( "ClaudeChatDock", "Searching label templates" ) },
                        { "new_label",         QT_TRANSLATE_NOOP( "ClaudeChatDock", "Creating a new label" ) },
                        { "set_template",      QT_TRANSLATE_NOOP( "ClaudeChatDock", "Changing the label stock" ) },
                        { "list_symbols",      QT_TRANSLATE_NOOP( "ClaudeChatDock", "Searching symbols" ) },
                        { "insert_symbol",     QT_TRANSLATE_NOOP( "ClaudeChatDock", "Inserting a symbol" ) },
                        { "add_image",         QT_TRANSLATE_NOOP( "ClaudeChatDock", "Adding an image" ) },
                        { "save_label",        QT_TRANSLATE_NOOP( "ClaudeChatDock", "Saving the label" ) },
                        { "open_label",        QT_TRANSLATE_NOOP( "ClaudeChatDock", "Opening a label" ) },
                        { "export_pdf",        QT_TRANSLATE_NOOP( "ClaudeChatDock", "Exporting a PDF" ) },
                        { "undo",              QT_TRANSLATE_NOOP( "ClaudeChatDock", "Undoing" ) },
                        { "Read",              QT_TRANSLATE_NOOP( "ClaudeChatDock", "Reading a file" ) },
                        { "Glob",              QT_TRANSLATE_NOOP( "ClaudeChatDock", "Looking for files" ) },
                        { "Grep",              QT_TRANSLATE_NOOP( "ClaudeChatDock", "Searching files" ) },
                };
                QString name = fullName.startsWith( toolPrefix ) ? fullName.mid( toolPrefix.size() ) : fullName;
                auto it = names.find( name );
                if ( it != names.end() )
                {
                        return QCoreApplication::translate( "ClaudeChatDock", it.value() );
                }
                return QCoreApplication::translate( "ClaudeChatDock", "Using %1" ).arg( name );
        }


        QString systemPrompt( int documentId )
        {
                return QString(
                        "You are running inside the gLabels Plus label designer, in its chat panel. The user sees "
                        "the label editor right next to this chat, and every change you make with the glabels-plus "
                        "tools appears there immediately (they can undo it with Ctrl+Z).\n"
                        "The user is chatting from the window with document id %1: work on that document unless "
                        "they ask otherwise. If it has no label yet, help them pick a template and create one.\n"
                        "The panel is narrow: keep replies short and friendly, a sentence or two saying what you "
                        "did or asking one clear question. Don't show XML or code unless asked.\n"
                        "Before saying a design is done, check it with render_preview and fix overlaps, text that "
                        "doesn't fit, and anything outside the label.\n"
                        "Sizes are in millimetres unless the user uses other units. Ask before overwriting files." )
                        .arg( documentId );
        }
}


namespace glabels
{

        ClaudeChatDock::ClaudeChatDock( MainWindow* window )
                : QDockWidget( tr("Claude"), window ),
                  mWindow( window )
        {
                setObjectName( "claudeChatDock" );
                setAllowedAreas( Qt::AllDockWidgetAreas );
                setFeatures( QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
                             QDockWidget::DockWidgetFloatable );

                //
                // Chat page
                //
                mStatus = new QLabel;
                mStatus->setForegroundRole( QPalette::PlaceholderText );
                mStatus->setToolTip( tr("Chatting with Claude Code on this computer, signed in with your Claude account") );
                mStatus->setMinimumWidth( 1 );          // never widen the panel

                mStopButton = new QToolButton;
                mStopButton->setText( tr("Stop") );
                mStopButton->setToolTip( tr("Stop Claude (Esc)") );
                mStopButton->setAutoRaise( true );
                mStopButton->setVisible( false );
                connect( mStopButton, &QToolButton::clicked, this, &ClaudeChatDock::stop );

                mNewButton = new QToolButton;
                mNewButton->setText( tr("New Chat") );
                mNewButton->setToolTip( tr("Start a new conversation") );
                mNewButton->setAutoRaise( true );
                connect( mNewButton, &QToolButton::clicked, this, &ClaudeChatDock::newChat );

                auto* moreButton = new QToolButton;
                moreButton->setText( QString::fromUtf8( "⋯" ) );          // ⋯
                moreButton->setToolTip( tr("Panel position and settings") );
                moreButton->setAutoRaise( true );
                moreButton->setPopupMode( QToolButton::InstantPopup );
                auto* moreMenu = new QMenu( moreButton );
                moreMenu->addAction( tr("Dock on the &Right"),  this, [this]() { moveTo( Qt::RightDockWidgetArea ); } );
                moreMenu->addAction( tr("Dock on the &Left"),   this, [this]() { moveTo( Qt::LeftDockWidgetArea ); } );
                moreMenu->addAction( tr("Dock at the &Top"),    this, [this]() { moveTo( Qt::TopDockWidgetArea ); } );
                moreMenu->addAction( tr("Dock at the &Bottom"), this, [this]() { moveTo( Qt::BottomDockWidgetArea ); } );
                moreMenu->addAction( tr("&Float in Its Own Window"), this, [this]() { setFloating( true ); } );
                moreMenu->addSeparator();
                moreMenu->addAction( tr("Connect AI &Settings..."), this, [this]() { AiConnectDialog::showDialog( mWindow ); } );
                moreMenu->addAction( tr("&Hide Panel"), this, [this]() { close(); } );
                moreButton->setMenu( moreMenu );

                auto* header = new QHBoxLayout;
                header->setContentsMargins( 8, 4, 4, 4 );
                header->addWidget( mStatus, 1 );
                header->addWidget( mStopButton );
                header->addWidget( mNewButton );
                header->addWidget( moreButton );

                mTranscript = new QTextBrowser;
                mTranscript->setOpenExternalLinks( true );
                mTranscript->setFrameShape( QFrame::NoFrame );
                mTranscript->document()->setDocumentMargin( 10 );

                // Suggestions, shown until the first message
                mSuggestions = new QWidget;
                auto* suggestionsLayout = new QVBoxLayout( mSuggestions );
                suggestionsLayout->setContentsMargins( 10, 0, 10, 6 );
                suggestionsLayout->setSpacing( 4 );
                auto* intro = new QLabel( tr("<b>Design labels by chatting.</b><br>Describe what you need and Claude "
                                             "builds it in the editor. You can undo anything with Ctrl+Z.") );
                intro->setWordWrap( true );
                suggestionsLayout->addWidget( intro );
                suggestionsLayout->addSpacing( 4 );
                const QStringList examples = {
                        tr("Design a 100 × 150 mm shipping label with a barcode"),
                        tr("Make a price tag for 40 × 30 mm thermal labels"),
                        tr("Check this label for text that doesn't fit"),
                        tr("Add a QR code that links to my website"),
                };
                for ( const QString& example : examples )
                {
                        auto* button = new QPushButton( example );
                        button->setFlat( true );
                        button->setCursor( Qt::PointingHandCursor );
                        button->setStyleSheet( "QPushButton { text-align: left; padding: 4px 6px; }" );
                        connect( button, &QPushButton::clicked, this, [this, example]() { send( example ); } );
                        suggestionsLayout->addWidget( button );
                }

                mInput = new QPlainTextEdit;
                mInput->setPlaceholderText( tr("Ask Claude to design or change your label...") );
                mInput->setTabChangesFocus( true );
                mInput->installEventFilter( this );
                int lineHeight = mInput->fontMetrics().lineSpacing();
                mInput->setFixedHeight( lineHeight * 3 + 14 );
                connect( mInput->document(), &QTextDocument::contentsChanged, this, [this, lineHeight]() {
                        int lines = qBound( 3, int( mInput->document()->size().height() ), 8 );
                        mInput->setFixedHeight( lineHeight * lines + 14 );
                        mSendButton->setEnabled( !mBusy && !mInput->toPlainText().trimmed().isEmpty() );
                } );

                mSendButton = new QToolButton;
                mSendButton->setText( tr("Send") );
                mSendButton->setToolTip( tr("Send (Enter). Shift+Enter starts a new line.") );
                mSendButton->setEnabled( false );
                connect( mSendButton, &QToolButton::clicked, this, [this]() { send( mInput->toPlainText() ); } );

                auto* inputRow = new QHBoxLayout;
                inputRow->setContentsMargins( 8, 4, 8, 8 );
                inputRow->addWidget( mInput, 1 );
                inputRow->addWidget( mSendButton, 0, Qt::AlignBottom );

                auto* chatPage = new QWidget;
                auto* chatLayout = new QVBoxLayout( chatPage );
                chatLayout->setContentsMargins( 0, 0, 0, 0 );
                chatLayout->setSpacing( 0 );
                chatLayout->addLayout( header );
                chatLayout->addWidget( mTranscript, 1 );
                chatLayout->addWidget( mSuggestions );
                chatLayout->addLayout( inputRow );

                //
                // Claude Code missing
                //
                mMissingText = new QLabel;
                mMissingText->setWordWrap( true );
                mMissingText->setOpenExternalLinks( true );
                mMissingText->setAlignment( Qt::AlignCenter );
                auto* checkAgain = new QPushButton( tr("Check Again") );
                connect( checkAgain, &QPushButton::clicked, this, &ClaudeChatDock::checkInstalled );
                auto* missingPage = new QWidget;
                auto* missingLayout = new QVBoxLayout( missingPage );
                missingLayout->addStretch( 1 );
                missingLayout->addWidget( mMissingText );
                missingLayout->addWidget( checkAgain, 0, Qt::AlignCenter );
                missingLayout->addStretch( 2 );

                mPages = new QStackedWidget;
                mPages->addWidget( chatPage );
                mPages->addWidget( missingPage );
                mPages->setMinimumWidth( 260 );
                setWidget( mPages );

                mRenderTimer.setSingleShot( true );
                mRenderTimer.setInterval( 40 );
                connect( &mRenderTimer, &QTimer::timeout, this, &ClaudeChatDock::renderStream );

                mDotsTimer.setInterval( 450 );
                connect( &mDotsTimer, &QTimer::timeout, this, [this]() {
                        mDots = ( mDots + 1 ) % 4;
                        mStatus->setText( tr("Claude is working") + QString( mDots, '.' ) );
                } );

                setBusy( false );
                updateEmptyState();
        }


        ClaudeChatDock::~ClaudeChatDock()
        {
                if ( mProcess )
                {
                        mProcess->disconnect( this );
                        mProcess->kill();
                        mProcess->waitForFinished( 2000 );
                }
                QFile::remove( mPromptFile );
                QFile::remove( mMcpFile );
        }


        void ClaudeChatDock::focusInput()
        {
                mInput->setFocus();
        }


        void ClaudeChatDock::showEvent( QShowEvent* event )
        {
                QDockWidget::showEvent( event );
                checkInstalled();
        }


        bool ClaudeChatDock::eventFilter( QObject* watched, QEvent* event )
        {
                if ( watched == mInput && event->type() == QEvent::KeyPress )
                {
                        auto* key = static_cast<QKeyEvent*>( event );
                        if ( ( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter )
                             && !( key->modifiers() & Qt::ShiftModifier ) )
                        {
                                send( mInput->toPlainText() );
                                return true;
                        }
                        if ( key->key() == Qt::Key_Escape && mBusy )
                        {
                                stop();
                                return true;
                        }
                }
                return QDockWidget::eventFilter( watched, event );
        }


        void ClaudeChatDock::checkInstalled()
        {
                mClaudePath = AiConnectDialog::findClaudeCode();
                if ( mClaudePath.isEmpty() )
                {
                        mMissingText->setText( tr("<p><b>Claude Code is not installed.</b></p>"
                                                  "<p>This panel talks to the Claude Code program on this computer, "
                                                  "signed in with your own Claude account.</p>"
                                                  "<p>Install it from <a href=\"https://claude.com/claude-code\">"
                                                  "claude.com/claude-code</a>, run <code>claude</code> once in a "
                                                  "terminal to sign in, then press Check Again.</p>") );
                        mPages->setCurrentIndex( 1 );
                }
                else
                {
                        mPages->setCurrentIndex( 0 );
                }
        }


        void ClaudeChatDock::moveTo( Qt::DockWidgetArea area )
        {
                setFloating( false );
                mWindow->addDockWidget( area, this );
                show();
        }


        ///////////////////////////////////////////////////////////////////////////
        // Talking to Claude Code
        ///////////////////////////////////////////////////////////////////////////

        void ClaudeChatDock::send( const QString& rawText )
        {
                const QString text = rawText.trimmed();
                if ( text.isEmpty() || mBusy )
                {
                        return;
                }
                if ( mClaudePath.isEmpty() )
                {
                        checkInstalled();
                        if ( mClaudePath.isEmpty() )
                        {
                                return;
                        }
                }

                // Chatting here is how the user lets Claude reach the labels
                if ( !AiBridge::isEnabled() )
                {
                        AiBridge::setEnabled( true );
                }

                appendUser( text );
                mInput->clear();

                if ( !mProcess && !startProcess() )
                {
                        return;
                }

                QJsonObject message{ { "type", "user" },
                                     { "message", QJsonObject{ { "role", "user" }, { "content", text } } } };
                mProcess->write( QJsonDocument( message ).toJson( QJsonDocument::Compact ) + "\n" );
                setBusy( true );
        }


        bool ClaudeChatDock::startProcess()
        {
                // Settings files for this session: our tools, and how to behave in the panel
                QString dir = QStandardPaths::writableLocation( QStandardPaths::AppLocalDataLocation ) + "/claude-chat";
                QDir().mkpath( dir );
                QString stem = QString( "%1/%2-%3" ).arg( dir ).arg( QCoreApplication::applicationPid() )
                                       .arg( AiBridge::documentId( mWindow ) );
                mPromptFile = stem + "-prompt.txt";
                mMcpFile = stem + "-mcp.json";

                QFile prompt( mPromptFile );
                QFile mcp( mMcpFile );
                QJsonObject server{ { "command", mcpCommand() }, { "args", QJsonArray{ "--mcp" } } };
                QJsonObject config{ { "mcpServers", QJsonObject{ { "glabels-plus", server } } } };
                if ( !prompt.open( QFile::WriteOnly ) || !mcp.open( QFile::WriteOnly ) )
                {
                        appendNote( tr("Could not write the chat settings in %1.").arg( QDir::toNativeSeparators( dir ) ), true );
                        return false;
                }
                prompt.write( systemPrompt( AiBridge::documentId( mWindow ) ).toUtf8() );
                mcp.write( QJsonDocument( config ).toJson() );
                prompt.close();
                mcp.close();

                QStringList args = {
                        "-p",
                        "--input-format", "stream-json",
                        "--output-format", "stream-json",
                        "--verbose",
                        "--include-partial-messages",
                        "--strict-mcp-config", "--mcp-config", mMcpFile,
                        "--append-system-prompt-file", mPromptFile,
                        // Read-only file access (for logos, data files), plus the label tools
                        "--tools", "Read,Glob,Grep",
                        "--allowedTools", "mcp__glabels-plus,Read,Glob,Grep",
                };
                if ( !mSessionId.isEmpty() )
                {
                        args << "--resume" << mSessionId;
                }

                QString program = mClaudePath;
#ifdef Q_OS_WIN
                if ( program.endsWith( ".cmd", Qt::CaseInsensitive ) || program.endsWith( ".bat", Qt::CaseInsensitive ) )
                {
                        args.prepend( QDir::toNativeSeparators( program ) );
                        args.prepend( "/c" );
                        program = "cmd.exe";
                }
#endif

                // Work in the label's folder, so "the logo next to my label" makes sense
                QString cwd = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                if ( mWindow->model() && !mWindow->model()->fileName().isEmpty() )
                {
                        cwd = mWindow->model()->dirPath();
                }

                mProcess = new QProcess( this );
                mProcess->setWorkingDirectory( cwd );
                mStdoutBuffer.clear();
                mStderrTail.clear();
                connect( mProcess, &QProcess::readyReadStandardOutput, this, &ClaudeChatDock::onStdout );
                connect( mProcess, &QProcess::readyReadStandardError, this, [this]() {
                        mStderrTail = ( mStderrTail + QString::fromUtf8( mProcess->readAllStandardError() ) ).right( 2000 );
                } );
                connect( mProcess, &QProcess::finished, this, &ClaudeChatDock::onFinished );

                mProcess->start( program, args );
                if ( !mProcess->waitForStarted( 10000 ) )
                {
                        appendNote( tr("Could not start Claude Code (%1): %2").arg( mClaudePath, mProcess->errorString() ), true );
                        mProcess->deleteLater();
                        mProcess = nullptr;
                        return false;
                }
                return true;
        }


        void ClaudeChatDock::stop()
        {
                if ( mProcess && mBusy )
                {
                        // The conversation is kept: the next message resumes it
                        mStopping = true;
                        mProcess->kill();
                }
        }


        void ClaudeChatDock::newChat()
        {
                if ( mProcess )
                {
                        mStopping = true;
                        mProcess->disconnect( this );
                        mProcess->kill();
                        mProcess->waitForFinished( 2000 );
                        mProcess->deleteLater();
                        mProcess = nullptr;
                }
                mStopping = false;
                mSessionId.clear();
                mTranscript->clear();
                mHasMessages = false;
                endStream();
                setBusy( false );
                updateEmptyState();
                mInput->setFocus();
        }


        void ClaudeChatDock::onStdout()
        {
                mStdoutBuffer += mProcess->readAllStandardOutput();
                int newline;
                while ( ( newline = mStdoutBuffer.indexOf( '\n' ) ) >= 0 )
                {
                        QByteArray line = mStdoutBuffer.left( newline );
                        mStdoutBuffer.remove( 0, newline + 1 );
                        QJsonDocument doc = QJsonDocument::fromJson( line );
                        if ( doc.isObject() )
                        {
                                handleEvent( doc.object() );
                        }
                }
        }


        void ClaudeChatDock::handleEvent( const QJsonObject& event )
        {
                const QString type = event.value( "type" ).toString();

                // Work done by sub-agents is not shown
                if ( !event.value( "parent_tool_use_id" ).isNull() && event.contains( "parent_tool_use_id" ) )
                {
                        return;
                }

                if ( type == "system" && event.value( "subtype" ).toString() == "init" )
                {
                        mSessionId = event.value( "session_id" ).toString();
                        for ( const auto& value : event.value( "mcp_servers" ).toArray() )
                        {
                                QJsonObject server = value.toObject();
                                if ( server.value( "name" ).toString() == "glabels-plus"
                                     && server.value( "status" ).toString() == "failed" )
                                {
                                        appendNote( tr("Claude could not reach the gLabels Plus tools."), true );
                                }
                        }
                }
                else if ( type == "stream_event" )
                {
                        const QJsonObject e = event.value( "event" ).toObject();
                        const QString kind = e.value( "type" ).toString();
                        if ( kind == "content_block_start" )
                        {
                                QJsonObject block = e.value( "content_block" ).toObject();
                                if ( block.value( "type" ).toString() == "tool_use" )
                                {
                                        endStream();
                                        appendTool( block.value( "name" ).toString() );
                                }
                        }
                        else if ( kind == "content_block_delta" )
                        {
                                QJsonObject delta = e.value( "delta" ).toObject();
                                if ( delta.value( "type" ).toString() == "text_delta" )
                                {
                                        appendClaudeText( delta.value( "text" ).toString() );
                                }
                        }
                        else if ( kind == "content_block_stop" )
                        {
                                renderStream();
                        }
                }
                else if ( type == "user" )
                {
                        // Tool results: only failures are worth a line
                        for ( const auto& value : event.value( "message" ).toObject().value( "content" ).toArray() )
                        {
                                QJsonObject block = value.toObject();
                                if ( block.value( "type" ).toString() == "tool_result" && block.value( "is_error" ).toBool() )
                                {
                                        appendNote( tr("That step didn't work; Claude will try another way."), false );
                                }
                        }
                }
                else if ( type == "result" )
                {
                        endStream();
                        if ( event.value( "is_error" ).toBool() || event.value( "subtype" ).toString() != "success" )
                        {
                                QString text = event.value( "result" ).toString();
                                if ( text.contains( "login", Qt::CaseInsensitive ) || text.contains( "API key", Qt::CaseInsensitive ) )
                                {
                                        text = tr("Claude Code is not signed in. Open a terminal, run claude once and sign in, "
                                                  "then send your message again.");
                                }
                                else if ( text.isEmpty() )
                                {
                                        text = tr("Claude stopped before finishing (%1).").arg( event.value( "subtype" ).toString() );
                                }
                                appendNote( text, true );
                        }
                        setBusy( false );
                }
        }


        void ClaudeChatDock::onFinished( int exitCode, QProcess::ExitStatus status )
        {
                endStream();
                if ( mStopping )
                {
                        appendNote( tr("Stopped."), false );
                }
                else if ( mBusy || status != QProcess::NormalExit || exitCode != 0 )
                {
                        QString detail = mStderrTail.trimmed().split( '\n' ).last();
                        appendNote( detail.isEmpty() ? tr("Claude Code stopped unexpectedly.")
                                                     : tr("Claude Code stopped: %1").arg( detail ), true );
                }
                mStopping = false;
                mProcess->deleteLater();
                mProcess = nullptr;          // the next message starts it again, resuming the chat
                setBusy( false );
        }


        void ClaudeChatDock::setBusy( bool busy )
        {
                mBusy = busy;
                mStopButton->setVisible( busy );
                mSendButton->setEnabled( !busy && !mInput->toPlainText().trimmed().isEmpty() );
                if ( busy )
                {
                        mDots = 0;
                        mStatus->setText( tr("Claude is working") );
                        mDotsTimer.start();
                }
                else
                {
                        mDotsTimer.stop();
                        mStatus->setText( tr("Ready") );
                }
        }


        ///////////////////////////////////////////////////////////////////////////
        // Transcript
        ///////////////////////////////////////////////////////////////////////////

        bool ClaudeChatDock::isFollowing() const
        {
                auto* bar = mTranscript->verticalScrollBar();
                return bar->value() >= bar->maximum() - 4;
        }


        void ClaudeChatDock::scrollToEndIfFollowing( bool following )
        {
                if ( following )
                {
                        mTranscript->verticalScrollBar()->setValue( mTranscript->verticalScrollBar()->maximum() );
                }
        }


        void ClaudeChatDock::updateEmptyState()
        {
                mSuggestions->setVisible( !mHasMessages );
        }


        void ClaudeChatDock::appendUser( const QString& text )
        {
                endStream();
                mTurnHeaderShown = false;
                mHasMessages = true;
                updateEmptyState();

                QTextCursor cursor( mTranscript->document() );
                cursor.movePosition( QTextCursor::End );
                if ( !mTranscript->document()->isEmpty() )
                {
                        cursor.insertBlock( QTextBlockFormat() );
                }

                QColor bubble = palette().color( QPalette::Highlight );
                bubble.setAlpha( 40 );
                QTextTableFormat format;
                format.setAlignment( Qt::AlignRight );
                format.setBorder( 0 );
                format.setCellPadding( 8 );
                format.setTopMargin( 6 );
                format.setBottomMargin( 2 );
                format.setBackground( bubble );
                QTextTable* table = cursor.insertTable( 1, 1, format );
                QTextCursor cell = table->cellAt( 0, 0 ).firstCursorPosition();
                cell.insertText( text, QTextCharFormat() );

                scrollToEndIfFollowing( true );
        }


        void ClaudeChatDock::appendClaudeText( const QString& delta )
        {
                if ( mStreamStart < 0 )
                {
                        bool following = isFollowing();
                        QTextCursor cursor( mTranscript->document() );
                        cursor.movePosition( QTextCursor::End );
                        cursor.insertBlock( QTextBlockFormat(), QTextCharFormat() );
                        if ( !mTurnHeaderShown )
                        {
                                QTextCharFormat name;
                                name.setFontWeight( QFont::Bold );
                                name.setForeground( QColor( 0xd9, 0x77, 0x57 ) );      // Claude orange
                                cursor.insertText( tr("Claude"), name );
                                cursor.insertBlock( QTextBlockFormat(), QTextCharFormat() );
                                mTurnHeaderShown = true;
                        }
                        mStreamStart = cursor.position();
                        mStreamText.clear();
                        scrollToEndIfFollowing( following );
                }
                mStreamText += delta;
                if ( !mRenderTimer.isActive() )
                {
                        mRenderTimer.start();
                }
        }


        void ClaudeChatDock::renderStream()
        {
                if ( mStreamStart < 0 )
                {
                        return;
                }
                mRenderTimer.stop();
                bool following = isFollowing();

                QTextCursor cursor( mTranscript->document() );
                cursor.setPosition( mStreamStart );
                cursor.movePosition( QTextCursor::End, QTextCursor::KeepAnchor );
                cursor.removeSelectedText();
                cursor.insertMarkdown( mStreamText.trimmed(), QTextDocument::MarkdownDialectGitHub );

                scrollToEndIfFollowing( following );
        }


        void ClaudeChatDock::endStream()
        {
                renderStream();
                mStreamStart = -1;
                mStreamText.clear();
        }


        void ClaudeChatDock::appendTool( const QString& toolName )
        {
                bool following = isFollowing();
                QTextCursor cursor( mTranscript->document() );
                cursor.movePosition( QTextCursor::End );
                cursor.insertBlock( QTextBlockFormat(), QTextCharFormat() );

                QTextCharFormat format;
                format.setForeground( palette().color( QPalette::PlaceholderText ) );
                format.setFontPointSize( QApplication::font().pointSizeF() * 0.92 );
                cursor.insertText( QString::fromUtf8( "◦ " ) + toolDescription( toolName ), format );
                scrollToEndIfFollowing( following );
        }


        void ClaudeChatDock::appendNote( const QString& text, bool error )
        {
                bool following = isFollowing();
                QTextCursor cursor( mTranscript->document() );
                cursor.movePosition( QTextCursor::End );
                cursor.insertBlock( QTextBlockFormat(), QTextCharFormat() );

                QTextCharFormat format;
                format.setForeground( error ? QColor( 0xc0, 0x39, 0x2b ) : palette().color( QPalette::PlaceholderText ) );
                cursor.insertText( text, format );
                mHasMessages = true;
                updateEmptyState();
                scrollToEndIfFollowing( following );
        }

}
