//  McpServer.cpp
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


#include "McpServer.hpp"

#include "AiBridge.hpp"

#include "model/Version.hpp"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QSettings>
#include <QThread>

#include <cstdio>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif


namespace
{
        const char* serverInstructions =
                "gLabels Plus is a label designer. These tools work on the labels open in the gLabels Plus "
                "window on the user's computer; every change appears there at once and the user can undo it "
                "with Ctrl+Z.\n"
                "Workflow: get_status to see what is open; list_templates + new_label to start a label on the "
                "right stock (or open_label), or create_template for a size no template has; get_format_guide once before writing XML; get_label to read the "
                "design; set_label_objects or add_objects to change it; render_preview to look at the result "
                "and fix overlaps or overflow; save_label when the user wants it saved.\n"
                "Mail merge: set_merge_data connects a CSV/JSON file, then use ${Field} in text and barcodes and "
                "render_preview with a record to check real values. set_variable makes counters and serial numbers.\n"
                "All tool sizes are in millimetres. Keep about 1.5-2 mm margin inside the label edge. Never "
                "overwrite files without the user's say-so.";


        // Tool definitions shown to the assistant.  Kept here (not in the GUI) so the
        // list is available before the program has started.
        const char* toolsJson = R"JSON([
{
  "name": "get_status",
  "description": "List the labels open in gLabels Plus (document ids, file, template, label size in mm, object count, unsaved changes) and recently used files. Call this first.",
  "inputSchema": { "type": "object", "properties": {} },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "get_format_guide",
  "description": "Reference for the gLabels label XML used by get_label, set_label_objects and add_objects: object elements, attributes, colors, barcode styles, and design tips for thermal printers. Read once before writing XML.",
  "inputSchema": { "type": "object", "properties": {} },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "get_label",
  "description": "Read a label: its size, a numbered list of objects (index, type, position and size in mm) and the full label XML. Embedded image data is left out; keep their src names to keep the images.",
  "inputSchema": { "type": "object", "properties": {
    "document": { "type": "integer", "description": "Document id from get_status. Default: the label the user is working on." }
  } },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "set_label_objects",
  "description": "Replace all objects on the label with the given XML (an <Objects> element or a series of <Object-...> elements), as one undoable step. Use this to redesign or edit: read get_label, change the XML, send it back. Images already in the label keep working through their src names.",
  "inputSchema": { "type": "object", "properties": {
    "objects_xml": { "type": "string", "description": "<Objects rotate=\"false\">...</Objects> or <Object-text ...>...</Object-text><Object-box .../>..." },
    "document": { "type": "integer", "description": "Document id from get_status. Default: the active label." }
  }, "required": ["objects_xml"] },
  "annotations": { "destructiveHint": false }
},
{
  "name": "add_objects",
  "description": "Add objects (one or more <Object-...> elements) on top of the existing ones, as one undoable step.",
  "inputSchema": { "type": "object", "properties": {
    "objects_xml": { "type": "string", "description": "One or more <Object-text>, <Object-box>, <Object-ellipse>, <Object-line>, <Object-barcode> or <Object-image> elements." },
    "document": { "type": "integer" }
  }, "required": ["objects_xml"] }
},
{
  "name": "delete_objects",
  "description": "Delete objects by their index from get_label, as one undoable step.",
  "inputSchema": { "type": "object", "properties": {
    "indices": { "type": "array", "items": { "type": "integer" } },
    "document": { "type": "integer" }
  }, "required": ["indices"] }
},
{
  "name": "render_preview",
  "description": "Render the label to a PNG image so you can see it: check layout, overlaps, text overflow and margins after each change. Merge fields show as ${name}, or with real values when you pass a record.",
  "inputSchema": { "type": "object", "properties": {
    "max_pixels": { "type": "integer", "description": "Longest side in pixels, 200-2000. Default 900." },
    "record": { "type": "integer", "description": "Fill the fields from this merge record (index from get_merge_data) instead of showing ${name}." },
    "document": { "type": "integer" }
  } },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "list_templates",
  "description": "Search the label stock templates (sheets and thermal rolls). Returns exact template names for new_label and set_template, with label size, labels per page and paper.",
  "inputSchema": { "type": "object", "properties": {
    "search": { "type": "string", "description": "Words that must all match, e.g. \"avery 5160\", \"roll 100\", \"a4 address\"." },
    "width_mm": { "type": "number", "description": "With height_mm: only labels of this size (either orientation, within 1 mm)." },
    "height_mm": { "type": "number" },
    "limit": { "type": "integer", "description": "Default 25, at most 100." }
  } },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "new_label",
  "description": "Start a new, empty label on a template, in its own window. Returns the document id and the design size in mm.",
  "inputSchema": { "type": "object", "properties": {
    "template": { "type": "string", "description": "Exact template name from list_templates." },
    "rotate": { "type": "boolean", "description": "Design the label turned 90 degrees. Default: portrait labels are rotated to landscape." }
  }, "required": ["template"] }
},
{
  "name": "set_template",
  "description": "Move the current label to another template (label stock). Objects keep their positions.",
  "inputSchema": { "type": "object", "properties": {
    "template": { "type": "string", "description": "Exact template name from list_templates." },
    "rotate": { "type": "boolean" },
    "document": { "type": "integer" }
  }, "required": ["template"] }
},
{
  "name": "list_symbols",
  "description": "Search the built-in symbols (handling marks, food, care, recycling, contact icons, arrows...) and the user's own symbols.",
  "inputSchema": { "type": "object", "properties": {
    "search": { "type": "string", "description": "e.g. \"fragile\", \"phone\", \"recycle\". Empty lists all." }
  } },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "insert_symbol",
  "description": "Put a symbol from list_symbols on the label as a square image.",
  "inputSchema": { "type": "object", "properties": {
    "id": { "type": "string" },
    "x_mm": { "type": "number", "description": "Left edge. Default: centered." },
    "y_mm": { "type": "number", "description": "Top edge. Default: centered." },
    "size_mm": { "type": "number", "description": "Width and height. Default 10." },
    "document": { "type": "integer" }
  }, "required": ["id"] }
},
{
  "name": "add_image",
  "description": "Embed a picture file (PNG, JPEG, BMP, GIF or SVG, such as a logo) from the user's computer in the label.",
  "inputSchema": { "type": "object", "properties": {
    "path": { "type": "string", "description": "Absolute path of the image file." },
    "x_mm": { "type": "number" }, "y_mm": { "type": "number" },
    "w_mm": { "type": "number", "description": "Width; give only one of w_mm and h_mm to keep the proportions." },
    "h_mm": { "type": "number" },
    "document": { "type": "integer" }
  }, "required": ["path"] }
},
{
  "name": "save_label",
  "description": "Save the label as a .glabels file: to its current file, or to a new path. Refuses to replace a different existing file unless overwrite is true; ask the user first.",
  "inputSchema": { "type": "object", "properties": {
    "path": { "type": "string", "description": "Absolute path. Omit to save to the label's current file." },
    "overwrite": { "type": "boolean" },
    "document": { "type": "integer" }
  } }
},
{
  "name": "open_label",
  "description": "Open a .glabels file in gLabels Plus.",
  "inputSchema": { "type": "object", "properties": {
    "path": { "type": "string", "description": "Absolute path of the .glabels file." }
  }, "required": ["path"] }
},
{
  "name": "export_pdf",
  "description": "Write the label to a PDF file, laid out on its sheet or roll, for proofing or printing later. Merge labels produce one label per record.",
  "inputSchema": { "type": "object", "properties": {
    "path": { "type": "string", "description": "Absolute path of the PDF to write." },
    "pages": { "type": "integer", "description": "Labels without merge data: number of full sheets. Default 1." },
    "copies": { "type": "integer", "description": "Labels with merge data: copies of each record. Default 1." },
    "overwrite": { "type": "boolean" },
    "document": { "type": "integer" }
  }, "required": ["path"] }
},
{
  "name": "undo",
  "description": "Undo the last change to the label (yours or the user's).",
  "inputSchema": { "type": "object", "properties": {
    "document": { "type": "integer" }
  } }
},
{
  "name": "redo",
  "description": "Redo the change that undo last took back.",
  "inputSchema": { "type": "object", "properties": {
    "document": { "type": "integer" }
  } }
},
{
  "name": "create_template",
  "description": "Create a custom label stock template when no template from list_templates fits: any label size on a thermal roll, a standard sheet (A4, US-Letter...) or a custom page, with rows and columns of labels. It is saved with the user's own templates (as in the Template Designer) and can then be used with new_label or set_template. Sizes in mm.",
  "inputSchema": { "type": "object", "properties": {
    "part": { "type": "string", "description": "Name of the stock, e.g. \"Jar lid 50 mm\" or \"100 x 150 shipping\". The template name is brand + \" \" + part." },
    "brand": { "type": "string", "description": "Default \"Custom\"." },
    "description": { "type": "string", "description": "What the label is for, e.g. \"Shipping label\"." },
    "shape": { "type": "string", "enum": ["rectangle", "round", "ellipse"], "description": "Default rectangle." },
    "width_mm": { "type": "number", "description": "Label width (rectangle, ellipse)." },
    "height_mm": { "type": "number", "description": "Label height (rectangle, ellipse)." },
    "diameter_mm": { "type": "number", "description": "Round labels." },
    "corner_radius_mm": { "type": "number", "description": "Rounded corners of a rectangle. Default 0." },
    "margin_mm": { "type": "number", "description": "Safe-area margin drawn inside the label. Default 1.5." },
    "paper": { "type": "string", "description": "\"roll\" (default; continuous thermal roll, one label after another), a paper id such as A4, US-Letter, A5, or \"other\" for a custom page." },
    "page_width_mm": { "type": "number", "description": "With paper \"other\"." },
    "page_height_mm": { "type": "number", "description": "With paper \"other\"." },
    "roll_width_mm": { "type": "number", "description": "Roll: width of the backing paper. Default: the labels' width." },
    "columns": { "type": "integer", "description": "Labels across. Default 1." },
    "rows": { "type": "integer", "description": "Labels down per sheet (rolls: usually 1). Default 1." },
    "column_gap_mm": { "type": "number", "description": "Space between columns. Default 0." },
    "row_gap_mm": { "type": "number", "description": "Space between rows. Default 0." },
    "left_mm": { "type": "number", "description": "Distance from the page's left edge to the first label. Sheets: default centers the grid; rolls: default 0." },
    "top_mm": { "type": "number", "description": "Distance from the page's top edge to the first label. Sheets: default centers the grid; rolls: default 0." },
    "overwrite": { "type": "boolean", "description": "Replace an existing custom template with the same name. Ask the user first." }
  }, "required": ["part"] }
},
{
  "name": "delete_template",
  "description": "Delete one of the user's own (custom) templates. Built-in templates can't be deleted. Only do this when the user asks.",
  "inputSchema": { "type": "object", "properties": {
    "template": { "type": "string", "description": "Exact template name (list_templates shows custom: true for these)." }
  }, "required": ["template"] },
  "annotations": { "destructiveHint": true }
},
{
  "name": "set_merge_data",
  "description": "Connect a data file (CSV, TSV, text or JSON) to the label for mail merge: each record prints as its own label, filling ${Field} placeholders in text and barcodes. Returns the field names and the first records. Omit path to disconnect the data.",
  "inputSchema": { "type": "object", "properties": {
    "path": { "type": "string", "description": "Absolute path of the data file." },
    "format": { "type": "string", "enum": ["auto", "csv", "tsv", "semicolon", "text", "json"], "description": "Default auto (from the file extension; text auto-detects the delimiter). JSON must be an array of objects." },
    "has_header": { "type": "boolean", "description": "First line holds the column names. Default true." },
    "document": { "type": "integer" }
  } }
},
{
  "name": "get_merge_data",
  "description": "Show the label's merge data: source file, field names, record count and records.",
  "inputSchema": { "type": "object", "properties": {
    "start": { "type": "integer", "description": "First record index. Default 0." },
    "limit": { "type": "integer", "description": "Records to return, 0-200. Default 10." },
    "document": { "type": "integer" }
  } },
  "annotations": { "readOnlyHint": true }
},
{
  "name": "set_variable",
  "description": "Add, change or delete a label variable, used as ${name} in text and barcode data. Use integer variables with an increment for serial numbers and counters (e.g. start 1001, per_item, step 1).",
  "inputSchema": { "type": "object", "properties": {
    "name": { "type": "string" },
    "type": { "type": "string", "enum": ["string", "integer", "float", "color"], "description": "Default string." },
    "initial_value": { "type": "string", "description": "Starting value (a color such as #cc0000 for color variables)." },
    "increment": { "type": "string", "enum": ["never", "per_item", "per_copy", "per_page"], "description": "When a number goes up by step. Default never." },
    "step": { "type": "string", "description": "Amount added at each increment. Default 1 when it increments." },
    "delete": { "type": "boolean", "description": "Delete the variable instead." },
    "document": { "type": "integer" }
  }, "required": ["name"] }
}
])JSON";


        void writeMessage( const QJsonObject& message )
        {
                QByteArray line = QJsonDocument( message ).toJson( QJsonDocument::Compact ) + "\n";
                fwrite( line.constData(), 1, size_t( line.size() ), stdout );
                fflush( stdout );
        }


        void reply( const QJsonValue& id, const QJsonObject& result )
        {
                writeMessage( QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } } );
        }


        void replyError( const QJsonValue& id, int code, const QString& message )
        {
                writeMessage( QJsonObject{ { "jsonrpc", "2.0" }, { "id", id },
                                           { "error", QJsonObject{ { "code", code }, { "message", message } } } } );
        }


        QJsonObject toolError( const QString& text )
        {
                return QJsonObject{ { "content", QJsonArray{ QJsonObject{ { "type", "text" }, { "text", text } } } },
                                    { "isError", true } };
        }


        ///
        /// Connection to the gLabels Plus window
        ///
        class Gui
        {
        public:
                QJsonObject call( const QString& tool, const QJsonObject& args )
                {
                        QString error;
                        for ( int attempt = 0; attempt < 2; attempt++ )
                        {
                                if ( !ensureConnected( &error ) )
                                {
                                        return toolError( error );
                                }

                                int id = ++mNextId;
                                QJsonObject request{ { "id", id }, { "tool", tool }, { "args", args } };
                                mSocket.write( QJsonDocument( request ).toJson( QJsonDocument::Compact ) + "\n" );
                                mSocket.flush();

                                QJsonObject response;
                                if ( readResponse( id, &response ) )
                                {
                                        return toMcp( response.value( "result" ).toObject() );
                                }
                                mSocket.abort();      // lost the window: reconnect once
                        }
                        return toolError( "Lost the connection to gLabels Plus. If it was closed, try again to start it." );
                }

        private:
                bool ensureConnected( QString* error )
                {
                        if ( mSocket.state() == QLocalSocket::ConnectedState )
                        {
                                return true;
                        }
                        if ( tryConnect( 500 ) )
                        {
                                return true;
                        }

                        QSettings settings;
                        if ( !settings.value( "ai/enabled", false ).toBool() )
                        {
                                *error = "AI access is turned off in gLabels Plus. Ask the user to open gLabels Plus, "
                                         "choose Tools > Connect AI > Claude, and turn on \"Let Claude work with my labels\".";
                                return false;
                        }

                        // Not running: start it and wait for its window
                        // Detach its standard streams from ours: stdout is the MCP channel
                        QString program = glabels::mcpCommand();
                        QProcess process;
                        process.setProgram( program );
                        process.setStandardInputFile( QProcess::nullDevice() );
                        process.setStandardOutputFile( QProcess::nullDevice() );
                        process.setStandardErrorFile( QProcess::nullDevice() );
                        if ( !process.startDetached() )
                        {
                                *error = "Could not start gLabels Plus (" + program + "). Ask the user to start it.";
                                return false;
                        }
                        QElapsedTimer timer;
                        timer.start();
                        while ( timer.elapsed() < 45000 )
                        {
                                if ( tryConnect( 500 ) )
                                {
                                        return true;
                                }
                                QThread::msleep( 400 );
                        }
                        *error = "Started gLabels Plus, but it did not answer within 45 seconds. "
                                 "Ask the user to check that it is open, then try again.";
                        return false;
                }


                bool tryConnect( int msecs )
                {
                        mSocket.abort();
                        mSocket.connectToServer( glabels::AiBridge::socketName() );
                        return mSocket.waitForConnected( msecs );
                }


                bool readResponse( int id, QJsonObject* response )
                {
                        // Generous: the user may have a dialog open in the window
                        QDeadlineTimer deadline( 300000 );
                        while ( !deadline.hasExpired() )
                        {
                                while ( mSocket.canReadLine() )
                                {
                                        QJsonObject o = QJsonDocument::fromJson( mSocket.readLine() ).object();
                                        if ( o.value( "id" ).toInt() == id )
                                        {
                                                *response = o;
                                                return true;
                                        }
                                }
                                if ( mSocket.state() != QLocalSocket::ConnectedState )
                                {
                                        return false;
                                }
                                mSocket.waitForReadyRead( 1000 );
                        }
                        return false;
                }


                static QJsonObject toMcp( const QJsonObject& result )
                {
                        QJsonArray content;
                        content.append( QJsonObject{ { "type", "text" }, { "text", result.value( "text" ).toString() } } );
                        if ( result.contains( "image" ) )
                        {
                                content.append( QJsonObject{ { "type", "image" },
                                                             { "data", result.value( "image" ).toString() },
                                                             { "mimeType", "image/png" } } );
                        }
                        return QJsonObject{ { "content", content }, { "isError", result.value( "error" ).toBool() } };
                }


                QLocalSocket mSocket;
                int          mNextId = 0;
        };
}


namespace glabels
{

        QString mcpCommand()
        {
                // Inside an AppImage the program lives in a temporary mount; start the AppImage itself
                QString appImage = qEnvironmentVariable( "APPIMAGE" );
                if ( !appImage.isEmpty() && QFileInfo( appImage ).isFile() )
                {
                        return appImage;
                }
                return QCoreApplication::applicationFilePath();
        }


        int runMcpServer( int argc, char** argv )
        {
#ifdef Q_OS_WIN
                _setmode( _fileno( stdin ), _O_BINARY );
                _setmode( _fileno( stdout ), _O_BINARY );
#endif
                QCoreApplication app( argc, argv );
                QCoreApplication::setOrganizationName( model::Version::ORGANIZATION_NAME );
                QCoreApplication::setOrganizationDomain( model::Version::ORGANIZATION_DOMAIN );
                QCoreApplication::setApplicationName( model::Version::APPLICATION_NAME );

                const QJsonArray tools = QJsonDocument::fromJson( toolsJson ).array();
                Gui gui;

                std::string line;
                while ( std::getline( std::cin, line ) )
                {
                        if ( line.empty() || line == "\r" )
                        {
                                continue;
                        }
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson( QByteArray::fromStdString( line ), &parseError );
                        if ( parseError.error != QJsonParseError::NoError || !doc.isObject() )
                        {
                                replyError( QJsonValue::Null, -32700, "Parse error" );
                                continue;
                        }

                        const QJsonObject message = doc.object();
                        const QString method = message.value( "method" ).toString();
                        const QJsonValue id = message.value( "id" );
                        const QJsonObject params = message.value( "params" ).toObject();
                        bool isRequest = message.contains( "id" ) && !method.isEmpty();

                        if ( method == "initialize" )
                        {
                                QString version = params.value( "protocolVersion" ).toString( "2025-06-18" );
                                reply( id, QJsonObject{
                                        { "protocolVersion", version },
                                        { "capabilities", QJsonObject{ { "tools", QJsonObject{} } } },
                                        { "serverInfo", QJsonObject{ { "name", "glabels-plus" },
                                                                     { "title", "gLabels Plus" },
                                                                     { "version", model::Version::STRING } } },
                                        { "instructions", serverInstructions } } );
                        }
                        else if ( method == "ping" )
                        {
                                reply( id, QJsonObject{} );
                        }
                        else if ( method == "tools/list" )
                        {
                                reply( id, QJsonObject{ { "tools", tools } } );
                        }
                        else if ( method == "tools/call" )
                        {
                                const QString name = params.value( "name" ).toString();
                                bool known = false;
                                for ( const auto& t : tools )
                                {
                                        known = known || t.toObject().value( "name" ).toString() == name;
                                }
                                if ( !known )
                                {
                                        replyError( id, -32602, "Unknown tool: " + name );
                                        continue;
                                }
                                reply( id, gui.call( name, params.value( "arguments" ).toObject() ) );
                        }
                        else if ( method == "resources/list" )
                        {
                                reply( id, QJsonObject{ { "resources", QJsonArray{} } } );
                        }
                        else if ( method == "prompts/list" )
                        {
                                reply( id, QJsonObject{ { "prompts", QJsonArray{} } } );
                        }
                        else if ( isRequest )
                        {
                                replyError( id, -32601, "Method not found: " + method );
                        }
                        // Notifications (no id) need no answer
                }

                return 0;
        }

}
