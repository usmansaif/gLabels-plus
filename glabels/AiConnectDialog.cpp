//  AiConnectDialog.cpp
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


#include "AiConnectDialog.hpp"

#include "AiBridge.hpp"
#include "ClaudeHost.hpp"
#include "MainWindow.hpp"
#include "McpServer.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVBoxLayout>


namespace
{
        const QString serverName = "glabels-plus";

        const char* examplePrompt =
                "Using gLabels Plus, design a 100 × 150 mm shipping label with my company name at the top, "
                "a From and To address block, and a Code 128 barcode for the order number. Show me a preview.";


        QLabel* wrapLabel( const QString& text = QString() )
        {
                auto* label = new QLabel( text );
                label->setWordWrap( true );
                label->setTextFormat( Qt::RichText );
                label->setOpenExternalLinks( true );
                return label;
        }
}


namespace glabels
{

        QPointer<AiConnectDialog> AiConnectDialog::mInstance;


        void AiConnectDialog::showDialog( QWidget* parent )
        {
                if ( !mInstance )
                {
                        mInstance = new AiConnectDialog( parent );
                        mInstance->setAttribute( Qt::WA_DeleteOnClose );
                }
                mInstance->show();
                mInstance->raise();
                mInstance->activateWindow();
                mInstance->refresh();
        }


        QString AiConnectDialog::findClaudeCode()
        {
                return ClaudeHost::find();
        }


        QString AiConnectDialog::claudeDesktopConfigPath()
        {
#if defined(Q_OS_WIN)
                QString base = qEnvironmentVariable( "APPDATA" );
#elif defined(Q_OS_MACOS)
                QString base = QDir::homePath() + "/Library/Application Support";
#else
                QString base = qEnvironmentVariable( "XDG_CONFIG_HOME", QDir::homePath() + "/.config" );
#endif
                return QDir( base ).filePath( "Claude/claude_desktop_config.json" );
        }


        AiConnectDialog::AiConnectDialog( QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( tr("Connect Claude") );

                auto* intro = wrapLabel( tr("<b>Design labels by chatting with Claude.</b> Ask for a label in your own words "
                                            "and Claude builds it here: it picks the label stock, lays out text, barcodes "
                                            "and symbols, checks a preview and saves the file. Every change shows in the "
                                            "editor at once, and you can undo it with Ctrl+Z.") );

                mEnabled = new QCheckBox( tr("&Let Claude work with my labels") );
                mEnabled->setToolTip( tr("When this is off, Claude cannot see or change anything in gLabels Plus.") );
                mBridgeStatus = wrapLabel();
                mBridgeStatus->setForegroundRole( QPalette::PlaceholderText );

                // Claude Code
                auto* codeBox = new QGroupBox( tr("Claude Code (terminal)") );
                mCodeStatus = wrapLabel();
                mCodeConnect = new QPushButton( tr("Co&nnect") );
                mCodeDisconnect = new QPushButton( tr("&Disconnect") );
                mCodeHint = wrapLabel();
                auto* openChat = new QPushButton( tr("Open &Chat Panel") );
                openChat->setToolTip( tr("Chat with Claude in a panel next to your label (Ctrl+Shift+K)") );
                connect( openChat, &QPushButton::clicked, this, [this]() {
                        if ( auto* window = qobject_cast<MainWindow*>( parentWidget() ) )
                        {
                                window->showClaudeChat();
                                close();
                        }
                } );
                auto* codeButtons = new QHBoxLayout;
                codeButtons->addWidget( mCodeStatus, 1 );
                codeButtons->addWidget( mCodeConnect );
                codeButtons->addWidget( mCodeDisconnect );
                auto* codeLayout = new QVBoxLayout( codeBox );
                codeLayout->addLayout( codeButtons );
                codeLayout->addWidget( mCodeHint );
                codeLayout->addWidget( openChat, 0, Qt::AlignLeft );

                // Claude Desktop
                auto* desktopBox = new QGroupBox( tr("Claude Desktop app") );
                mDesktopStatus = wrapLabel();
                mDesktopConnect = new QPushButton( tr("Conn&ect") );
                mDesktopDisconnect = new QPushButton( tr("Disconnec&t") );
                auto* desktopButtons = new QHBoxLayout;
                desktopButtons->addWidget( mDesktopStatus, 1 );
                desktopButtons->addWidget( mDesktopConnect );
                desktopButtons->addWidget( mDesktopDisconnect );
                auto* desktopLayout = new QVBoxLayout( desktopBox );
                desktopLayout->addLayout( desktopButtons );

                // Example
                auto* example = wrapLabel( tr("<b>Try asking:</b> <i>%1</i>").arg( QString( examplePrompt ).toHtmlEscaped() ) );
                auto* copyExample = new QPushButton( tr("Cop&y Example") );
                connect( copyExample, &QPushButton::clicked, this, [copyExample]() {
                        QApplication::clipboard()->setText( examplePrompt );
                        copyExample->setText( tr("Copied") );
                } );
                auto* exampleRow = new QHBoxLayout;
                exampleRow->addWidget( example, 1 );
                exampleRow->addWidget( copyExample, 0, Qt::AlignTop );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( intro );
                layout->addSpacing( 6 );
                layout->addWidget( mEnabled );
                layout->addWidget( mBridgeStatus );
                layout->addWidget( codeBox );
                layout->addWidget( desktopBox );
                layout->addLayout( exampleRow );
                layout->addStretch( 1 );
                layout->addWidget( buttons );

                mEnabled->setChecked( AiBridge::isEnabled() );
                connect( mEnabled, &QCheckBox::toggled, this, &AiConnectDialog::onEnabledToggled );
                connect( mCodeConnect, &QPushButton::clicked, this, &AiConnectDialog::connectClaudeCode );
                connect( mCodeDisconnect, &QPushButton::clicked, this, &AiConnectDialog::disconnectClaudeCode );
                connect( mDesktopConnect, &QPushButton::clicked, this, &AiConnectDialog::connectDesktop );
                connect( mDesktopDisconnect, &QPushButton::clicked, this, &AiConnectDialog::disconnectDesktop );
                connect( AiBridge::instance(), &AiBridge::clientCountChanged, this, [this]() { refresh(); } );

                resize( 560, sizeHint().height() );
        }


        void AiConnectDialog::refresh()
        {
                auto* bridge = AiBridge::instance();
                if ( !AiBridge::isEnabled() )
                {
                        mBridgeStatus->setText( tr("Off. Connecting Claude below turns it on.") );
                }
                else if ( !bridge->isListening() )
                {
                        mBridgeStatus->setText( tr("On, served by another gLabels Plus window that is already open.") );
                }
                else if ( bridge->clientCount() > 0 )
                {
                        mBridgeStatus->setText( tr("On. Claude is connected now.") );
                }
                else
                {
                        mBridgeStatus->setText( tr("On. Waiting for Claude to connect.") );
                }

                refreshClaudeCode();
                refreshDesktop();
        }


        void AiConnectDialog::onEnabledToggled( bool on )
        {
                AiBridge::setEnabled( on );
                refresh();
        }


        ///////////////////////////////////////////////////////////////////////////
        // Claude Code
        ///////////////////////////////////////////////////////////////////////////

        void AiConnectDialog::runClaude( const QStringList& args, std::function<void( int, const QString& )> done )
        {
                auto* process = new QProcess( this );
                process->setProcessChannelMode( QProcess::MergedChannels );

                QString program;
                QStringList arguments = args;
                ClaudeHost::prepare( mClaudePath, QString(), &program, &arguments );
                connect( process, &QProcess::finished, this, [process, done]( int code, QProcess::ExitStatus status ) {
                        QString output = QString::fromUtf8( process->readAll() ).trimmed();
                        done( status == QProcess::NormalExit ? code : -1, output );
                        process->deleteLater();
                } );
                connect( process, &QProcess::errorOccurred, this, [process, done]( QProcess::ProcessError error ) {
                        if ( error == QProcess::FailedToStart )
                        {
                                done( -1, process->errorString() );
                                process->deleteLater();
                        }
                } );
                process->start( program, arguments );
        }


        void AiConnectDialog::refreshClaudeCode()
        {
                if ( mCodeBusy )
                {
                        return;
                }

                mClaudePath = findClaudeCode();
                if ( mClaudePath.isEmpty() )
                {
                        mCodeStatus->setText( tr("Claude Code is not installed on this computer.") );
                        mCodeHint->setText( tr("Install installs it from <a href=\"https://claude.com/claude-code\">claude.com/claude-code</a> "
                                               "and connects it. Then run <code>claude</code> once in a terminal to sign in "
                                               "with your Claude account.") );
                        mCodeConnect->setText( tr("&Install") );
                        mCodeConnect->setEnabled( true );
                        mCodeDisconnect->setVisible( false );
                        return;
                }

                mCodeBusy = true;
                mCodeStatus->setText( tr("Checking...") );
                mCodeConnect->setEnabled( false );
                mCodeDisconnect->setVisible( false );
                runClaude( { "mcp", "get", serverName }, [this]( int code, const QString& output ) {
                        mCodeBusy = false;
                        bool connected = code == 0;
                        bool current = output.contains( ClaudeHost::mcpProgram() ) && output.contains( ClaudeHost::mcpArguments().join( ' ' ) );

                        if ( connected && current )
                        {
                                mCodeStatus->setText( tr("<b>Connected.</b>") );
                                mCodeHint->setText( tr("Chat in the panel next to your label, or run <code>claude</code> in a "
                                                       "terminal. Claude asks before using a gLabels tool the first time "
                                                       "in the terminal.") );
                                mCodeConnect->setText( tr("Reco&nnect") );
                        }
                        else if ( connected )
                        {
                                mCodeStatus->setText( tr("Connected to a different copy of gLabels Plus.") );
                                mCodeHint->setText( tr("Press Reconnect to use this copy.") );
                                mCodeConnect->setText( tr("Reco&nnect") );
                        }
                        else
                        {
                                mCodeStatus->setText( tr("Claude Code is installed but not connected.") );
                                mCodeHint->setText( tr("Connect adds gLabels Plus to Claude Code for all your projects.") );
                                mCodeConnect->setText( tr("Co&nnect") );
                        }
                        mCodeConnect->setEnabled( true );
                        mCodeDisconnect->setVisible( connected );
                } );
        }


        void AiConnectDialog::installClaudeCode()
        {
                mCodeBusy = true;
                mCodeConnect->setEnabled( false );
                mCodeStatus->setText( tr("Installing Claude Code...") );
                mCodeHint->setText( tr("This downloads Claude Code and can take a minute.") );
                ClaudeHost::install( this, [this]( bool ok, const QString& output ) {
                        mCodeBusy = false;
                        mClaudePath = findClaudeCode();
                        if ( !ok || mClaudePath.isEmpty() )
                        {
                                QMessageBox::warning( this, tr("Install Claude Code"),
                                                      tr("Claude Code could not be installed.\n\n%1\n\n"
                                                         "You can install it yourself from claude.com/claude-code.")
                                                      .arg( output.right( 1500 ) ) );
                                refresh();
                                return;
                        }
                        connectClaudeCode();
                } );
        }


        void AiConnectDialog::connectClaudeCode()
        {
                if ( mCodeBusy )
                {
                        return;
                }
                if ( mClaudePath.isEmpty() )
                {
                        installClaudeCode();
                        return;
                }
                AiBridge::setEnabled( true );
                mEnabled->setChecked( true );

                mCodeBusy = true;
                mCodeConnect->setEnabled( false );
                mCodeStatus->setText( tr("Connecting...") );

                // Replace any earlier entry, so it points at this copy of the program
                runClaude( { "mcp", "remove", serverName, "--scope", "user" }, [this]( int, const QString& ) {
                        runClaude( QStringList{ "mcp", "add", "--scope", "user", serverName, "--", ClaudeHost::mcpProgram() }
                                   + ClaudeHost::mcpArguments(),
                                   [this]( int code, const QString& output ) {
                                           mCodeBusy = false;
                                           if ( code != 0 )
                                           {
                                                   QMessageBox::warning( this, tr("Connect Claude Code"),
                                                                         tr("Claude Code could not add gLabels Plus:\n\n%1").arg( output ) );
                                           }
                                           else if ( auto* window = qobject_cast<MainWindow*>( parentWidget() ) )
                                           {
                                                   window->showClaudeChat();      // ready to talk
                                           }
                                           refresh();
                                   } );
                } );
        }


        void AiConnectDialog::disconnectClaudeCode()
        {
                if ( mClaudePath.isEmpty() || mCodeBusy )
                {
                        return;
                }
                mCodeBusy = true;
                mCodeStatus->setText( tr("Disconnecting...") );
                runClaude( { "mcp", "remove", serverName, "--scope", "user" }, [this]( int code, const QString& output ) {
                        mCodeBusy = false;
                        if ( code != 0 )
                        {
                                QMessageBox::warning( this, tr("Disconnect Claude Code"),
                                                      tr("Claude Code could not remove gLabels Plus:\n\n%1").arg( output ) );
                        }
                        refresh();
                } );
        }


        ///////////////////////////////////////////////////////////////////////////
        // Claude Desktop
        ///////////////////////////////////////////////////////////////////////////

        namespace
        {
                /// Read the config; false (with a message) if it exists but isn't a JSON object
                bool readDesktopConfig( const QString& path, QJsonObject* config, QString* error )
                {
                        QFile file( path );
                        if ( !file.exists() )
                        {
                                *config = QJsonObject();
                                return true;
                        }
                        if ( !file.open( QFile::ReadOnly ) )
                        {
                                *error = file.errorString();
                                return false;
                        }
                        QByteArray data = file.readAll();
                        if ( data.trimmed().isEmpty() )
                        {
                                *config = QJsonObject();
                                return true;
                        }
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson( data, &parseError );
                        if ( parseError.error != QJsonParseError::NoError || !doc.isObject() )
                        {
                                *error = AiConnectDialog::tr("%1 is not valid JSON (%2). Fix or remove it, then try again.")
                                                 .arg( QDir::toNativeSeparators( path ), parseError.errorString() );
                                return false;
                        }
                        *config = doc.object();
                        return true;
                }


                bool writeDesktopConfig( const QString& path, const QJsonObject& config, QString* error )
                {
                        if ( !QDir().mkpath( QFileInfo( path ).absolutePath() ) )
                        {
                                *error = AiConnectDialog::tr("Could not create the folder for %1.").arg( QDir::toNativeSeparators( path ) );
                                return false;
                        }
                        if ( QFile::exists( path ) )
                        {
                                QFile::remove( path + ".bak" );
                                QFile::copy( path, path + ".bak" );
                        }
                        QSaveFile file( path );
                        if ( !file.open( QFile::WriteOnly ) )
                        {
                                *error = file.errorString();
                                return false;
                        }
                        file.write( QJsonDocument( config ).toJson( QJsonDocument::Indented ) );
                        if ( !file.commit() )
                        {
                                *error = file.errorString();
                                return false;
                        }
                        return true;
                }
        }


        void AiConnectDialog::refreshDesktop()
        {
                const QString path = claudeDesktopConfigPath();
                bool installed = QFileInfo( QFileInfo( path ).absolutePath() ).isDir();

                QJsonObject config;
                QString error;
                if ( !readDesktopConfig( path, &config, &error ) )
                {
                        mDesktopStatus->setText( error );
                        mDesktopConnect->setEnabled( false );
                        mDesktopDisconnect->setVisible( false );
                        return;
                }

                QJsonObject entry = config.value( "mcpServers" ).toObject().value( serverName ).toObject();
                bool connected = !entry.isEmpty();
                bool current = entry.value( "command" ).toString() == ClaudeHost::mcpProgram()
                               && entry.value( "args" ).toVariant().toStringList() == ClaudeHost::mcpArguments();

                if ( connected && current )
                {
                        mDesktopStatus->setText( tr("<b>Connected.</b> Restart Claude Desktop if it was open when you connected.") );
                        mDesktopConnect->setText( tr("Reconn&ect") );
                }
                else if ( connected )
                {
                        mDesktopStatus->setText( tr("Connected to a different copy of gLabels Plus. Press Reconnect to use this copy.") );
                        mDesktopConnect->setText( tr("Reconn&ect") );
                }
                else if ( installed )
                {
                        mDesktopStatus->setText( tr("Claude Desktop is installed but not connected.") );
                        mDesktopConnect->setText( tr("Conn&ect") );
                }
                else
                {
                        mDesktopStatus->setText( tr("Claude Desktop was not found. Get it from "
                                                    "<a href=\"https://claude.ai/download\">claude.ai/download</a>.") );
                        mDesktopConnect->setText( tr("Conn&ect") );
                }
                mDesktopConnect->setEnabled( installed || connected );
                mDesktopDisconnect->setVisible( connected );
        }


        void AiConnectDialog::connectDesktop()
        {
                const QString path = claudeDesktopConfigPath();
                QJsonObject config;
                QString error;
                if ( !readDesktopConfig( path, &config, &error ) )
                {
                        QMessageBox::warning( this, tr("Connect Claude Desktop"), error );
                        return;
                }

                QJsonObject servers = config.value( "mcpServers" ).toObject();
                servers.insert( serverName, QJsonObject{ { "command", ClaudeHost::mcpProgram() },
                                                         { "args", QJsonArray::fromStringList( ClaudeHost::mcpArguments() ) } } );
                config.insert( "mcpServers", servers );

                if ( !writeDesktopConfig( path, config, &error ) )
                {
                        QMessageBox::warning( this, tr("Connect Claude Desktop"),
                                              tr("Could not update %1:\n\n%2").arg( QDir::toNativeSeparators( path ), error ) );
                        return;
                }

                AiBridge::setEnabled( true );
                mEnabled->setChecked( true );
                refresh();
                QMessageBox::information( this, tr("Connect Claude Desktop"),
                                          tr("gLabels Plus was added to Claude Desktop.\n\n"
                                             "Quit Claude Desktop completely and open it again to start using it.") );
        }


        void AiConnectDialog::disconnectDesktop()
        {
                const QString path = claudeDesktopConfigPath();
                QJsonObject config;
                QString error;
                if ( !readDesktopConfig( path, &config, &error ) )
                {
                        QMessageBox::warning( this, tr("Disconnect Claude Desktop"), error );
                        return;
                }
                QJsonObject servers = config.value( "mcpServers" ).toObject();
                servers.remove( serverName );
                config.insert( "mcpServers", servers );
                if ( !writeDesktopConfig( path, config, &error ) )
                {
                        QMessageBox::warning( this, tr("Disconnect Claude Desktop"), error );
                        return;
                }
                refresh();
        }

}
