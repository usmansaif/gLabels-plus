//  McpServer.hpp
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

#ifndef McpServer_hpp
#define McpServer_hpp


#include <QString>


namespace glabels
{

        ///
        /// "glabels-qt --mcp": a Model Context Protocol server on stdin/stdout for
        /// Claude Code and Claude Desktop.
        ///
        /// It lists the label tools itself and forwards each call to the running
        /// gLabels Plus window (AiBridge), starting the program first if needed.
        /// Runs without a GUI and returns when stdin closes.
        ///
        int runMcpServer( int argc, char** argv );


        /// The command an assistant should run to start this server: the
        /// program's path (the AppImage itself when running from one) plus "--mcp"
        QString mcpCommand();

}


#endif // McpServer_hpp
