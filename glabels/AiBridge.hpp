//  AiBridge.hpp
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

#ifndef AiBridge_hpp
#define AiBridge_hpp


#include <QJsonObject>
#include <QObject>
#include <QPointer>


class QLocalServer;
class QLocalSocket;


namespace glabels
{
        class MainWindow;


        ///
        /// Lets an AI assistant (Claude Code, Claude Desktop) work with the labels
        /// open in this program.
        ///
        /// The assistant starts "glabels-qt --mcp" (see McpServer), which speaks MCP
        /// on stdin/stdout and forwards each tool call here over a local socket that
        /// only this user can open.  Calls run on the GUI thread against the open
        /// windows, so every change shows in the editor at once and can be undone.
        ///
        /// The socket is only opened while AI access is turned on
        /// (Tools > Connect AI, setting "ai/enabled").
        ///
        class AiBridge : public QObject
        {
                Q_OBJECT

        public:
                static AiBridge* instance();

                /// Name of the local socket for the current user
                static QString socketName();

                static bool isEnabled();
                static void setEnabled( bool enabled );

                /// Open or close the socket to match isEnabled()
                void update();
                bool isListening() const;

                /// The id tools use for the label in window (see get_status)
                static int documentId( MainWindow* window );

                /// Number of assistants connected right now
                int clientCount() const;

        signals:
                void clientCountChanged( int count );

        private:
                AiBridge();

                void onNewConnection();
                void onReadyRead( QLocalSocket* socket );

                QJsonObject call( const QString& tool, const QJsonObject& args );

                // Tools
                QJsonObject toolGetStatus( const QJsonObject& args );
                QJsonObject toolGetFormatGuide( const QJsonObject& args );
                QJsonObject toolGetLabel( const QJsonObject& args );
                QJsonObject toolSetLabelObjects( const QJsonObject& args );
                QJsonObject toolAddObjects( const QJsonObject& args );
                QJsonObject toolDeleteObjects( const QJsonObject& args );
                QJsonObject toolRenderPreview( const QJsonObject& args );
                QJsonObject toolListTemplates( const QJsonObject& args );
                QJsonObject toolNewLabel( const QJsonObject& args );
                QJsonObject toolSetTemplate( const QJsonObject& args );
                QJsonObject toolListSymbols( const QJsonObject& args );
                QJsonObject toolInsertSymbol( const QJsonObject& args );
                QJsonObject toolAddImage( const QJsonObject& args );
                QJsonObject toolSaveLabel( const QJsonObject& args );
                QJsonObject toolOpenLabel( const QJsonObject& args );
                QJsonObject toolExportPdf( const QJsonObject& args );
                QJsonObject toolUndo( const QJsonObject& args );

                /// The window a tool acts on: the "document" argument, or the
                /// window the user worked in last.  Sets *error if there is none.
                MainWindow* target( const QJsonObject& args, QString* error, bool needModel = true );

                void noteActivity( MainWindow* window, const QString& text );

                QLocalServer*              mServer = nullptr;
                QList<QLocalSocket*>       mClients;
                QPointer<MainWindow>       mLastWindow;
        };

}


#endif // AiBridge_hpp
