//  AiConnectDialog.hpp
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

#ifndef AiConnectDialog_hpp
#define AiConnectDialog_hpp


#include <QDialog>
#include <QPointer>
#include <QStringList>

#include <functional>

class QCheckBox;
class QLabel;
class QProcess;
class QPushButton;


namespace glabels
{

        ///
        /// Tools > Connect AI > Claude
        ///
        /// Registers gLabels Plus as an MCP server with Claude Code (through the
        /// "claude mcp" command) and with the Claude Desktop app (its
        /// claude_desktop_config.json), and turns AI access on or off.
        ///
        class AiConnectDialog : public QDialog
        {
                Q_OBJECT

        public:
                static void showDialog( QWidget* parent );

                /// The Claude Code program, or empty if it isn't installed
                static QString findClaudeCode();

                /// Claude Desktop's config file (it may not exist yet)
                static QString claudeDesktopConfigPath();

        private:
                explicit AiConnectDialog( QWidget* parent );

                void refresh();
                void refreshClaudeCode();
                void refreshDesktop();
                void onEnabledToggled( bool on );

                void connectClaudeCode();
                void disconnectClaudeCode();
                void connectDesktop();
                void disconnectDesktop();

                /// Run "claude <args>" without blocking; done gets the exit code and output
                void runClaude( const QStringList& args, std::function<void( int, const QString& )> done );

                static QPointer<AiConnectDialog> mInstance;

                QCheckBox*   mEnabled;
                QLabel*      mBridgeStatus;

                QLabel*      mCodeStatus;
                QPushButton* mCodeConnect;
                QPushButton* mCodeDisconnect;
                QLabel*      mCodeHint;
                QString      mClaudePath;
                bool         mCodeBusy = false;

                QLabel*      mDesktopStatus;
                QPushButton* mDesktopConnect;
                QPushButton* mDesktopDisconnect;
        };

}


#endif // AiConnectDialog_hpp
