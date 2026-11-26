//  ClaudeHost.cpp
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

#include "ClaudeHost.hpp"

#include "McpServer.hpp"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>


namespace
{
        // Programs started from a desktop menu often don't get the shell's PATH
        QStringList extraDirs()
        {
                const QString home = QDir::homePath();
                QStringList dirs = { home + "/.local/bin", home + "/.claude/local", home + "/.npm-global/bin",
                                     home + "/bin", "/usr/local/bin", "/opt/homebrew/bin" };
#ifdef Q_OS_WIN
                dirs << qEnvironmentVariable( "APPDATA" ) + "/npm";
#endif
                return dirs;
        }


        // The installer, as a shell command for Linux and macOS
        const char* installScript =
                "if command -v curl >/dev/null 2>&1; then curl -fsSL https://claude.ai/install.sh | bash; "
                "elif command -v wget >/dev/null 2>&1; then wget -qO- https://claude.ai/install.sh | bash; "
                "else echo 'curl or wget is needed to install Claude Code.'; exit 1; fi";
}


namespace glabels
{
        namespace ClaudeHost
        {

                bool inFlatpak()
                {
                        static const bool flatpak = QFile::exists( "/.flatpak-info" );
                        return flatpak;
                }


                QString find()
                {
                        if ( inFlatpak() )
                        {
                                // Ask the user's computer, through a login shell so its PATH is complete
                                QString script = "command -v claude 2>/dev/null && exit 0; "
                                                 "for d in";
                                for ( const QString& dir : extraDirs() )
                                {
                                        script += " \"" + dir + "\"";
                                }
                                script += "; do if [ -x \"$d/claude\" ]; then echo \"$d/claude\"; exit 0; fi; done; exit 1";

                                QProcess process;
                                process.start( "flatpak-spawn", { "--host", "sh", "-lc", script } );
                                if ( !process.waitForFinished( 10000 ) || process.exitCode() != 0 )
                                {
                                        process.kill();
                                        return QString();
                                }
                                QString path = QString::fromUtf8( process.readAllStandardOutput() ).trimmed();
                                return path.startsWith( '/' ) ? path.section( '\n', 0, 0 ) : QString();
                        }

                        QString path = QStandardPaths::findExecutable( "claude" );
                        if ( !path.isEmpty() )
                        {
                                return path;
                        }
                        return QStandardPaths::findExecutable( "claude", extraDirs() );
                }


                void prepare( const QString& claudePath, const QString& workingDir,
                              QString* program, QStringList* args )
                {
                        *program = claudePath;
#ifdef Q_OS_WIN
                        // npm installs a .cmd wrapper, which needs the command interpreter
                        if ( claudePath.endsWith( ".cmd", Qt::CaseInsensitive ) || claudePath.endsWith( ".bat", Qt::CaseInsensitive ) )
                        {
                                args->prepend( QDir::toNativeSeparators( claudePath ) );
                                args->prepend( "/c" );
                                *program = "cmd.exe";
                        }
#endif
                        if ( inFlatpak() )
                        {
                                QStringList host = { "--host", "--watch-bus" };
                                if ( !workingDir.isEmpty() )
                                {
                                        host << "--directory=" + workingDir;
                                }
                                host << claudePath;
                                *args = host + *args;
                                *program = "flatpak-spawn";
                        }
                }


                void stop( QProcess* process )
                {
                        // flatpak-spawn passes SIGTERM on to the program, but can't pass SIGKILL
                        if ( inFlatpak() )
                        {
                                process->terminate();
                        }
                        else
                        {
                                process->kill();
                        }
                }


                void install( QObject* parent, std::function<void( bool, const QString& )> done )
                {
                        auto* process = new QProcess( parent );
                        process->setProcessChannelMode( QProcess::MergedChannels );

#ifdef Q_OS_WIN
                        QString program = "powershell.exe";
                        QStringList args = { "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                                             "irm https://claude.ai/install.ps1 | iex" };
#else
                        QString program = "sh";
                        QStringList args = { "-c", installScript };
                        if ( inFlatpak() )
                        {
                                args = QStringList{ "--host", "sh", "-lc", installScript };
                                program = "flatpak-spawn";
                        }
#endif

                        QObject::connect( process, &QProcess::finished, process,
                                          [process, done]( int code, QProcess::ExitStatus status ) {
                                                  QString output = QString::fromUtf8( process->readAll() ).trimmed();
                                                  bool ok = status == QProcess::NormalExit && code == 0 && !find().isEmpty();
                                                  process->deleteLater();
                                                  done( ok, output );
                                          } );
                        QObject::connect( process, &QProcess::errorOccurred, process,
                                          [process, done]( QProcess::ProcessError error ) {
                                                  if ( error == QProcess::FailedToStart )
                                                  {
                                                          QString message = process->errorString();
                                                          process->deleteLater();
                                                          done( false, message );
                                                  }
                                          } );
                        process->start( program, args );
                }


                QString mcpProgram()
                {
                        return inFlatpak() ? QString( "flatpak" ) : mcpCommand();
                }


                QStringList mcpArguments()
                {
                        if ( inFlatpak() )
                        {
                                return { "run", "--command=glabels-qt", qEnvironmentVariable( "FLATPAK_ID" ), "--mcp" };
                        }
                        return { "--mcp" };
                }

        }
}
