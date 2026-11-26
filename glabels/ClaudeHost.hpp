//  ClaudeHost.hpp
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

#ifndef ClaudeHost_hpp
#define ClaudeHost_hpp


#include <QString>
#include <QStringList>

#include <functional>

class QObject;
class QProcess;


namespace glabels
{

        ///
        /// Finding, installing and running the Claude Code program.
        ///
        /// Claude Code lives on the user's computer.  Inside a Flatpak sandbox it
        /// can't be seen or started directly, so every command goes through
        /// "flatpak-spawn --host" (which needs --talk-name=org.freedesktop.Flatpak).
        ///
        namespace ClaudeHost
        {
                /// Running inside a Flatpak sandbox
                bool inFlatpak();

                /// The Claude Code program, or empty if it isn't installed
                QString find();

                /// Turn "claude <args>" into the program and arguments to start,
                /// run in workingDir (empty for the current folder)
                void prepare( const QString& claudePath, const QString& workingDir,
                              QString* program, QStringList* args );

                /// Stop a Claude Code process started from prepare()
                void stop( QProcess* process );

                /// Install Claude Code with its official installer, without blocking;
                /// done gets whether it worked and the installer's output
                void install( QObject* parent, std::function<void( bool, const QString& )> done );

                /// The command and arguments Claude runs to start the gLabels Plus
                /// tools ("glabels-qt --mcp", or through "flatpak run")
                QString mcpProgram();
                QStringList mcpArguments();
        }

}


#endif // ClaudeHost_hpp
