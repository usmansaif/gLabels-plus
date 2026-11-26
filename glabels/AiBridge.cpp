//  AiBridge.cpp
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


#include "AiBridge.hpp"

#include "CustomSymbols.hpp"
#include "MainWindow.hpp"
#include "SymbolsDialog.hpp"
#include "UndoRedoModel.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "merge/Merge.hpp"
#include "merge/Record.hpp"
#include "model/Db.hpp"
#include "model/FileUtil.hpp"
#include "model/FrameEllipse.hpp"
#include "model/FrameRect.hpp"
#include "model/FrameRound.hpp"
#include "model/Layout.hpp"
#include "model/Markup.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/PageRenderer.hpp"
#include "model/Settings.hpp"
#include "model/Version.hpp"
#include "model/XmlLabelCreator.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QColor>
#include <QBuffer>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPainter>
#include <QPrinter>
#include <QRegularExpression>
#include <QSettings>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QtDebug>


namespace
{
        const qint64 maxRequestBytes = 32 * 1024 * 1024;

        const QStringList objectTags = { "Object-text", "Object-box", "Object-ellipse", "Object-line",
                                         "Object-image", "Object-barcode" };


        QString tr( const char* text )
        {
                return QCoreApplication::translate( "AiBridge", text );
        }


        QJsonObject ok( const QString& text )
        {
                return QJsonObject{ { "text", text } };
        }


        QJsonObject ok( const QJsonObject& json )
        {
                return QJsonObject{ { "text", QString::fromUtf8( QJsonDocument( json ).toJson( QJsonDocument::Indented ) ) } };
        }


        QJsonObject fail( const QString& text )
        {
                return QJsonObject{ { "text", text }, { "error", true } };
        }


        double round2( double v )
        {
                return std::round( v * 100.0 ) / 100.0;
        }


        int docId( glabels::MainWindow* window )
        {
                static int next = 1;
                QVariant id = window->property( "aiDocId" );
                if ( !id.isValid() )
                {
                        id = next++;
                        window->setProperty( "aiDocId", id );
                }
                return id.toInt();
        }


        QList<glabels::MainWindow*> mainWindows()
        {
                QList<glabels::MainWindow*> list;
                for ( QWidget* widget : QApplication::topLevelWidgets() )
                {
                        if ( auto* window = qobject_cast<glabels::MainWindow*>( widget ) )
                        {
                                if ( window->isVisible() )
                                {
                                        list << window;
                                }
                        }
                }
                std::sort( list.begin(), list.end(), []( auto* a, auto* b ) { return docId( a ) < docId( b ); } );
                return list;
        }


        QString objectType( const glabels::model::ModelObject* object )
        {
                using namespace glabels::model;
                if ( dynamic_cast<const ModelTextObject*>( object ) )    return "text";
                if ( dynamic_cast<const ModelBarcodeObject*>( object ) ) return "barcode";
                if ( dynamic_cast<const ModelImageObject*>( object ) )   return "image";
                if ( dynamic_cast<const ModelLineObject*>( object ) )    return "line";
                if ( dynamic_cast<const ModelEllipseObject*>( object ) ) return "ellipse";
                if ( dynamic_cast<const ModelBoxObject*>( object ) )     return "box";
                return "object";
        }


        QJsonObject templateJson( const glabels::model::Template& t )
        {
                QJsonObject o;
                o.insert( "name", t.name() );
                o.insert( "description", t.description() );
                if ( const auto* frame = t.frame() )
                {
                        o.insert( "label_width_mm", round2( frame->w().mm() ) );
                        o.insert( "label_height_mm", round2( frame->h().mm() ) );
                        o.insert( "labels_per_page", frame->nLabels() );
                        o.insert( "layout", frame->layoutDescription() );
                }
                o.insert( "roll", t.isRoll() );
                if ( t.isUserDefined() )
                {
                        o.insert( "custom", true );
                }
                o.insert( "page", t.isRoll() ? QString( "roll %1 mm wide" ).arg( round2( t.rollWidth().mm() ) )
                                             : t.paperDescription( glabels::model::Units( glabels::model::Units::MM ) ) );
                return o;
        }


        ///
        /// Parse <Object-*> elements, given either as an <Objects> element or as
        /// bare elements, into objects for model.  Images embedded in model stay
        /// available to the new objects by their src name.
        ///
        QList<glabels::model::ModelObject*> parseObjects( const QString& xml,
                                                          const glabels::model::Model* model,
                                                          QString* rotate,
                                                          QString* error )
        {
                using namespace glabels::model;

                QString text = xml.trimmed();
                if ( text.startsWith( "<?xml" ) )
                {
                        text = text.mid( text.indexOf( "?>" ) + 2 ).trimmed();
                }
                if ( text.startsWith( "```" ) )
                {
                        *error = tr("Pass the XML itself, without a Markdown code fence.");
                        return {};
                }

                QDomDocument input;
                QString parseError;
                int line = 0, column = 0;
                bool wrapped = !text.startsWith( "<Objects" );
                QString source = wrapped ? "<Objects>" + text + "</Objects>" : text;
                if ( !input.setContent( source, false, &parseError, &line, &column ) )
                {
                        *error = tr("The XML is not well formed (line %1, column %2: %3).")
                                         .arg( line ).arg( column ).arg( parseError );
                        return {};
                }
                QDomElement objectsElement = input.documentElement();
                if ( objectsElement.tagName() != "Objects" )
                {
                        *error = tr("Expected an <Objects> element or <Object-...> elements.");
                        return {};
                }
                if ( rotate && objectsElement.hasAttribute( "rotate" ) )
                {
                        *rotate = objectsElement.attribute( "rotate" );
                }

                int count = 0;
                for ( QDomElement e = objectsElement.firstChildElement(); !e.isNull(); e = e.nextSiblingElement() )
                {
                        if ( !objectTags.contains( e.tagName() ) )
                        {
                                *error = tr("Unknown element <%1>. Objects are %2.").arg( e.tagName(), objectTags.join( ", " ) );
                                return {};
                        }

                        // Text written straight inside <Object-text> instead of in <p> lines: accept it
                        if ( e.tagName() == "Object-text" && e.firstChildElement( "p" ).isNull() )
                        {
                                const QString text = e.text().trimmed();
                                while ( e.hasChildNodes() )
                                {
                                        e.removeChild( e.firstChild() );
                                }
                                for ( const QString& line : text.split( '\n' ) )
                                {
                                        QDomElement p = input.createElement( "p" );
                                        p.appendChild( input.createTextNode( line.trimmed() ) );
                                        e.appendChild( p );
                                }
                        }
                        count++;
                }

                // Build a Glabels-objects stream with the model's embedded files
                QByteArray current = XmlLabelCreator::serializeObjects( model->objectList(), model );
                QDomDocument doc;
                doc.setContent( current );
                QDomElement root = doc.documentElement();
                QDomElement oldObjects = root.firstChildElement( "Objects" );
                QDomNode newObjects = doc.importNode( objectsElement, true );
                if ( oldObjects.isNull() )
                {
                        root.appendChild( newObjects );
                }
                else
                {
                        root.replaceChild( newObjects, oldObjects );
                }

                QList<ModelObject*> objects = XmlLabelParser::deserializeObjects( doc.toByteArray(), model );
                if ( objects.size() != count )
                {
                        qDeleteAll( objects );
                        *error = tr("Only %1 of %2 objects could be read. Check the element and attribute names "
                                    "against get_format_guide.").arg( objects.size() ).arg( count );
                        return {};
                }
                return objects;
        }


        QString uniqueSymbolName( const QString& id )
        {
                return "symbol-" + id + ".svg";
        }
}


namespace glabels
{

        AiBridge* AiBridge::instance()
        {
                static AiBridge* bridge = new AiBridge();
                return bridge;
        }


        AiBridge::AiBridge()
                : QObject( qApp )
        {
                // Remember the label window the user worked in last
                connect( qApp, &QApplication::focusChanged, this, [this]( QWidget*, QWidget* now ) {
                        if ( now )
                        {
                                if ( auto* window = qobject_cast<MainWindow*>( now->window() ) )
                                {
                                        mLastWindow = window;
                                }
                        }
                } );
        }


        QString AiBridge::socketName()
        {
                // In a Flatpak, Claude starts "glabels-qt --mcp" in its own sandbox; this folder
                // is the one place both sandboxes of the app share
                const QString flatpakId = qEnvironmentVariable( "FLATPAK_ID" );
                const QString runtimeDir = qEnvironmentVariable( "XDG_RUNTIME_DIR" );
                if ( !flatpakId.isEmpty() && !runtimeDir.isEmpty() )
                {
                        return runtimeDir + "/app/" + flatpakId + "/glabels-plus-ai";
                }

                QString user = qEnvironmentVariable( "USER", qEnvironmentVariable( "USERNAME", "user" ) );
                user.remove( QRegularExpression( "[^A-Za-z0-9_.-]" ) );
                return "glabels-plus-ai-" + user;
        }


        int AiBridge::documentId( MainWindow* window )
        {
                return docId( window );
        }


        bool AiBridge::isEnabled()
        {
                return QSettings().value( "ai/enabled", false ).toBool();
        }


        void AiBridge::setEnabled( bool enabled )
        {
                QSettings().setValue( "ai/enabled", enabled );
                instance()->update();
        }


        bool AiBridge::isListening() const
        {
                return mServer && mServer->isListening();
        }


        int AiBridge::clientCount() const
        {
                return mClients.size();
        }


        void AiBridge::update()
        {
                if ( !isEnabled() )
                {
                        if ( mServer )
                        {
                                for ( auto* client : mClients )
                                {
                                        client->disconnectFromServer();
                                }
                                mServer->close();
                                delete mServer;
                                mServer = nullptr;
                        }
                        return;
                }
                if ( isListening() )
                {
                        return;
                }

                // Another running copy of the program already serves assistants
                QLocalSocket probe;
                probe.connectToServer( socketName() );
                if ( probe.waitForConnected( 300 ) )
                {
                        probe.disconnectFromServer();
                        return;
                }

                QLocalServer::removeServer( socketName() );     // left over from a crash
                mServer = new QLocalServer( this );
                mServer->setSocketOptions( QLocalServer::UserAccessOption );
                connect( mServer, &QLocalServer::newConnection, this, &AiBridge::onNewConnection );
                if ( !mServer->listen( socketName() ) )
                {
                        qWarning() << "AI bridge: cannot listen on" << socketName() << mServer->errorString();
                        delete mServer;
                        mServer = nullptr;
                }
        }


        void AiBridge::onNewConnection()
        {
                while ( QLocalSocket* socket = mServer->nextPendingConnection() )
                {
                        mClients << socket;
                        connect( socket, &QLocalSocket::readyRead, this, [this, socket]() { onReadyRead( socket ); } );
                        connect( socket, &QLocalSocket::disconnected, this, [this, socket]() {
                                mClients.removeAll( socket );
                                socket->deleteLater();
                                emit clientCountChanged( mClients.size() );
                        } );
                        emit clientCountChanged( mClients.size() );
                }
        }


        ///
        /// One JSON object per line: {"id": n, "tool": "...", "args": {...}}
        /// answered with {"id": n, "result": {"text": "...", "image": "<base64 png>", "error": bool}}
        ///
        void AiBridge::onReadyRead( QLocalSocket* socket )
        {
                while ( socket->canReadLine() )
                {
                        QByteArray line = socket->readLine();
                        QJsonParseError parseError;
                        QJsonObject request = QJsonDocument::fromJson( line, &parseError ).object();

                        QJsonObject result;
                        if ( parseError.error != QJsonParseError::NoError )
                        {
                                result = fail( "Bad request: " + parseError.errorString() );
                        }
                        else
                        {
                                result = call( request.value( "tool" ).toString(), request.value( "args" ).toObject() );
                        }

                        QJsonObject response{ { "id", request.value( "id" ) }, { "result", result } };
                        socket->write( QJsonDocument( response ).toJson( QJsonDocument::Compact ) + "\n" );
                        socket->flush();
                }
                if ( socket->bytesAvailable() > maxRequestBytes )
                {
                        socket->disconnectFromServer();
                }
        }


        QJsonObject AiBridge::call( const QString& tool, const QJsonObject& args )
        {
                using Handler = QJsonObject (AiBridge::*)( const QJsonObject& );
                static const QHash<QString, Handler> handlers = {
                        { "get_status",         &AiBridge::toolGetStatus },
                        { "get_format_guide",   &AiBridge::toolGetFormatGuide },
                        { "get_label",          &AiBridge::toolGetLabel },
                        { "set_label_objects",  &AiBridge::toolSetLabelObjects },
                        { "add_objects",        &AiBridge::toolAddObjects },
                        { "delete_objects",     &AiBridge::toolDeleteObjects },
                        { "render_preview",     &AiBridge::toolRenderPreview },
                        { "list_templates",     &AiBridge::toolListTemplates },
                        { "new_label",          &AiBridge::toolNewLabel },
                        { "set_template",       &AiBridge::toolSetTemplate },
                        { "list_symbols",       &AiBridge::toolListSymbols },
                        { "insert_symbol",      &AiBridge::toolInsertSymbol },
                        { "add_image",          &AiBridge::toolAddImage },
                        { "save_label",         &AiBridge::toolSaveLabel },
                        { "open_label",         &AiBridge::toolOpenLabel },
                        { "export_pdf",         &AiBridge::toolExportPdf },
                        { "undo",               &AiBridge::toolUndo },
                        { "redo",               &AiBridge::toolRedo },
                        { "create_template",    &AiBridge::toolCreateTemplate },
                        { "delete_template",    &AiBridge::toolDeleteTemplate },
                        { "set_merge_data",     &AiBridge::toolSetMergeData },
                        { "get_merge_data",     &AiBridge::toolGetMergeData },
                        { "set_variable",       &AiBridge::toolSetVariable },
                };

                auto it = handlers.find( tool );
                if ( it == handlers.end() )
                {
                        return fail( "Unknown tool: " + tool );
                }
                return ( this->*( it.value() ) )( args );
        }


        MainWindow* AiBridge::target( const QJsonObject& args, QString* error, bool needModel )
        {
                const QList<MainWindow*> windows = mainWindows();

                MainWindow* window = nullptr;
                if ( args.contains( "document" ) )
                {
                        int id = args.value( "document" ).toInt();
                        for ( auto* w : windows )
                        {
                                if ( docId( w ) == id )
                                {
                                        window = w;
                                }
                        }
                        if ( !window )
                        {
                                *error = QString( "No open document has id %1. Call get_status to see the open documents." ).arg( id );
                                return nullptr;
                        }
                }
                else if ( mLastWindow && windows.contains( mLastWindow.data() ) )
                {
                        window = mLastWindow;
                }
                else
                {
                        for ( auto* w : windows )
                        {
                                if ( !window || ( !w->isEmpty() && window->isEmpty() ) )
                                {
                                        window = w;
                                }
                        }
                }

                if ( !window )
                {
                        *error = "gLabels Plus has no window open.";
                        return nullptr;
                }
                if ( needModel && window->isEmpty() )
                {
                        *error = "No label is open. Create one with new_label (find a template with list_templates) "
                                 "or open a file with open_label.";
                        return nullptr;
                }
                return window;
        }


        void AiBridge::noteActivity( MainWindow* window, const QString& text )
        {
                if ( window )
                {
                        window->statusBar()->showMessage( tr("Claude: %1").arg( text ), 6000 );
                }
        }


        ///////////////////////////////////////////////////////////////////////////
        // Tools
        ///////////////////////////////////////////////////////////////////////////

        QJsonObject AiBridge::toolGetStatus( const QJsonObject& )
        {
                QJsonObject status;
                status.insert( "application", "gLabels Plus " + model::Version::LONG_STRING );
                status.insert( "units", "All tool arguments use millimetres. Label XML may use any unit suffix (mm, in, pt)." );

                QString error;
                MainWindow* active = target( QJsonObject(), &error, false );

                QJsonArray docs;
                for ( auto* window : mainWindows() )
                {
                        QJsonObject d;
                        d.insert( "document", docId( window ) );
                        d.insert( "active", window == active );
                        if ( window->isEmpty() )
                        {
                                d.insert( "empty", true );
                        }
                        else
                        {
                                auto* m = window->model();
                                d.insert( "name", m->shortName() );
                                d.insert( "file", m->fileName() );
                                d.insert( "modified", m->isModified() );
                                d.insert( "template", m->tmplate().name() );
                                d.insert( "label_width_mm", round2( m->w().mm() ) );
                                d.insert( "label_height_mm", round2( m->h().mm() ) );
                                d.insert( "rotated", m->rotate() );
                                d.insert( "objects", m->objectList().size() );
                                d.insert( "has_merge_data", !m->merge()->keys().empty() );
                        }
                        docs.append( d );
                }
                status.insert( "documents", docs );

                QJsonArray recent;
                for ( const QString& f : model::Settings::recentFileList().mid( 0, 10 ) )
                {
                        recent.append( f );
                }
                status.insert( "recent_files", recent );
                status.insert( "tip", "Read get_format_guide once before writing label XML. "
                                      "Use render_preview to check your work." );
                return ok( status );
        }


        QJsonObject AiBridge::toolGetFormatGuide( const QJsonObject& )
        {
                QStringList styles;
                for ( const auto& style : barcode::Backends::styleList() )
                {
                        styles << QString( "%1%2 (%3)" ).arg( style.backendId().isEmpty() ? "" : "backend=\"" + style.backendId() + "\" ",
                                                              "style=\"" + style.id() + "\"", style.name() );
                }

                QString guide = R"(# gLabels label XML

A label holds objects inside <Objects>, drawn in order (first = bottom).
Coordinates are from the label's top-left corner. Lengths take a unit: "12mm", "0.5in", "10pt".
Object x/y/w/h describe the unrotated box; rotation goes in the affine matrix a0..a5
(a0=cos, a1=sin, a2=-sin, a3=cos, a4=a5=0 for rotation about the top-left; identity is a0="1" a1="0" a2="0" a3="1" a4="0" a5="0").
Colors are RGBA numbers: "0x000000ff" black, "0xffffffff" white, "0xff0000ff" red, "0x00000000" transparent.
Attributes you leave out get sensible defaults.

<Object-text x="2mm" y="2mm" w="36mm" h="8mm" color="0x000000ff"
    font_family="Sans" font_size="14" font_weight="bold|normal" font_italic="false" font_underline="false"
    align="left|hcenter|right" valign="top|vcenter|bottom" wrap="word|anywhere|none"
    line_spacing="1" auto_shrink="true|false"
    text_path="straight|arc-top|arc-bottom|circle|wave|rectangle" letter_spacing="0">
  <p>First line</p><p>Second line with a merge field ${Name} or variable ${Price}</p>
</Object-text>
  font_size is in points. auto_shrink shrinks text to fit the box.
  Write merge fields and variables as ${Name} in XML (the editor displays them as {{Name}}).

<Object-box x="0mm" y="0mm" w="40mm" h="10mm" line_width="0.5mm" line_color="0x000000ff" fill_color="0xffffffff"/>
<Object-ellipse x="5mm" y="5mm" w="10mm" h="10mm" line_width="0.3mm" line_color="0x000000ff" fill_color="0x00000000"/>
<Object-line x="2mm" y="12mm" dx="36mm" dy="0mm" line_width="0.4mm" line_color="0x000000ff"/>

<Object-barcode x="5mm" y="20mm" w="30mm" h="8mm" style="code128" text="true" checksum="true" color="0x000000ff" data="ABC-123"/>
  data may contain ${Field}. Available styles:
  )" + styles.join( "\n  " ) + R"(

<Object-image x="2mm" y="2mm" w="10mm" h="10mm" src="logo.png"/>
  src is a file path, or the name of an image already embedded in the label (see get_label).
  To add a new picture use add_image or insert_symbol instead of writing XML.

Common optional attributes on every object: name="Price" (shown in the object list), lock_aspect_ratio="true",
shadow="true" shadow_x="1pt" shadow_y="1pt" shadow_color="0x000000ff" shadow_opacity="0.5".
Use a field like color_field="Colour" instead of color to take a color from merge data.

Tips for good labels:
- Leave a margin of about 1.5-2 mm inside the label edge; thermal printers can't print to the edge.
- Thermal labels print black only: prefer black text and shapes, no greys or thin hairlines below 0.25 mm.
- Barcodes need quiet space at the sides and at least ~8 mm height for reliable scanning.
- Check with render_preview after each change.)";
                return ok( guide );
        }


        QJsonObject AiBridge::toolGetLabel( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                QByteArray buffer;
                model::XmlLabelCreator::writeBuffer( m, buffer );
                QDomDocument doc;
                doc.setContent( buffer );
                QDomElement root = doc.documentElement();

                // Embedded files can be megabytes of base64: list them by name instead
                QStringList embedded;
                QDomElement data = root.firstChildElement( "Data" );
                if ( !data.isNull() )
                {
                        for ( QDomElement f = data.firstChildElement(); !f.isNull(); f = f.nextSiblingElement() )
                        {
                                embedded << f.attribute( "name" );
                        }
                        root.removeChild( data );
                }

                QJsonArray objects;
                int i = 0;
                for ( const auto* object : m->objectList() )
                {
                        QJsonObject o;
                        o.insert( "index", i++ );
                        o.insert( "type", objectType( object ) );
                        if ( !object->name().isEmpty() )
                        {
                                o.insert( "name", object->name() );
                        }
                        o.insert( "x_mm", round2( object->x0().mm() ) );
                        o.insert( "y_mm", round2( object->y0().mm() ) );
                        o.insert( "w_mm", round2( object->w().mm() ) );
                        o.insert( "h_mm", round2( object->h().mm() ) );
                        if ( auto* text = dynamic_cast<const model::ModelTextObject*>( object ) )
                        {
                                o.insert( "text", text->text().left( 120 ) );
                        }
                        else if ( auto* barcode = dynamic_cast<const model::ModelBarcodeObject*>( object ) )
                        {
                                o.insert( "data", barcode->bcData().left( 120 ) );
                        }
                        if ( object->rotation() != 0 )
                        {
                                o.insert( "rotation", object->rotation() );
                        }
                        if ( object->isLocked() )
                        {
                                o.insert( "locked", true );
                        }
                        objects.append( o );
                }

                QJsonObject result;
                result.insert( "document", docId( window ) );
                result.insert( "name", m->shortName() );
                result.insert( "label_width_mm", round2( m->w().mm() ) );
                result.insert( "label_height_mm", round2( m->h().mm() ) );
                result.insert( "objects", objects );
                if ( !embedded.isEmpty() )
                {
                        result.insert( "embedded_files", QJsonArray::fromStringList( embedded ) );
                }

                QString text = QString::fromUtf8( QJsonDocument( result ).toJson( QJsonDocument::Indented ) )
                               + "\nLabel XML (embedded file data omitted; keep src names to reuse them):\n"
                               + doc.toString( 2 );
                return ok( text );
        }


        QJsonObject AiBridge::toolSetLabelObjects( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                QString rotate;
                auto objects = parseObjects( args.value( "objects_xml" ).toString(), m, &rotate, &error );
                if ( !error.isEmpty() )
                {
                        return fail( error );
                }

                window->undoRedoModel()->checkpoint( tr("Claude: Edit Label") );
                m->unselectAll();
                for ( auto* object : QList<model::ModelObject*>( m->objectList() ) )
                {
                        m->deleteObject( object );
                }
                if ( !rotate.isEmpty() )
                {
                        m->setRotate( rotate == "true" );
                }
                for ( auto* object : objects )
                {
                        m->addObject( object );
                }
                window->showEditorPage();
                noteActivity( window, tr("updated the label (%n object(s))", "", objects.size()) );

                return ok( QString( "The label now has %1 objects. Use render_preview to check it." ).arg( objects.size() ) );
        }


        QJsonObject AiBridge::toolAddObjects( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                auto objects = parseObjects( args.value( "objects_xml" ).toString(), m, nullptr, &error );
                if ( !error.isEmpty() )
                {
                        return fail( error );
                }
                if ( objects.isEmpty() )
                {
                        return fail( "No objects given." );
                }

                window->undoRedoModel()->checkpoint( tr("Claude: Add Objects") );
                m->unselectAll();
                int first = m->objectList().size();
                for ( auto* object : objects )
                {
                        m->addObject( object );
                        m->selectObject( object );
                }
                window->showEditorPage();
                noteActivity( window, tr("added %n object(s)", "", objects.size()) );

                return ok( QString( "Added %1 objects (indices %2-%3)." )
                           .arg( objects.size() ).arg( first ).arg( first + objects.size() - 1 ) );
        }


        QJsonObject AiBridge::toolDeleteObjects( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                QList<model::ModelObject*> doomed;
                for ( const auto& value : args.value( "indices" ).toArray() )
                {
                        int index = value.toInt( -1 );
                        if ( index < 0 || index >= m->objectList().size() )
                        {
                                return fail( QString( "No object at index %1; the label has %2 objects." )
                                             .arg( index ).arg( m->objectList().size() ) );
                        }
                        if ( !doomed.contains( m->objectList()[index] ) )
                        {
                                doomed << m->objectList()[index];
                        }
                }
                if ( doomed.isEmpty() )
                {
                        return fail( "Give the indices of the objects to delete (see get_label)." );
                }

                window->undoRedoModel()->checkpoint( tr("Claude: Delete Objects") );
                m->unselectAll();
                for ( auto* object : doomed )
                {
                        m->deleteObject( object );
                }
                noteActivity( window, tr("deleted %n object(s)", "", doomed.size()) );
                return ok( QString( "Deleted %1 objects; %2 remain. Indices of the remaining objects have shifted." )
                           .arg( doomed.size() ).arg( m->objectList().size() ) );
        }


        QJsonObject AiBridge::toolRenderPreview( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                int maxPx = qBound( 200, args.value( "max_pixels" ).toInt( 900 ), 2000 );
                double wPt = m->w().pt();
                double hPt = m->h().pt();
                if ( wPt <= 0 || hPt <= 0 )
                {
                        return fail( "The label has no size." );
                }
                double scale = maxPx / std::max( wPt, hPt );
                const int margin = 12;
                QImage image( int( wPt * scale ) + 2 * margin, int( hPt * scale ) + 2 * margin, QImage::Format_ARGB32 );
                image.fill( QColor( 0xd0, 0xd0, 0xd0 ) );

                QPainter painter( &image );
                painter.setRenderHint( QPainter::Antialiasing );
                painter.setRenderHint( QPainter::SmoothPixmapTransform );
                painter.translate( margin, margin );
                painter.scale( scale, scale );

                // Label shape
                painter.save();
                if ( m->rotate() )
                {
                        painter.rotate( -90 );
                        painter.translate( -m->frame()->w().pt(), 0 );
                }
                painter.setPen( Qt::NoPen );
                painter.setBrush( Qt::white );
                painter.drawPath( m->frame()->path() );
                painter.restore();

                // Objects, with fields shown as ${name} placeholders, or filled from a merge record
                int recordIndex = args.value( "record" ).toInt( -1 );
                const QList<merge::Record> records = m->merge() ? m->merge()->recordList() : QList<merge::Record>();
                if ( args.contains( "record" ) && ( recordIndex < 0 || recordIndex >= records.size() ) )
                {
                        painter.end();
                        return fail( QString( "No record %1; the merge data has %2 records (see get_merge_data)." )
                                     .arg( recordIndex ).arg( records.size() ) );
                }
                if ( recordIndex >= 0 )
                {
                        model::Variables variables( m->variables() );
                        variables.resetVariables();
                        m->draw( &painter, false, records[recordIndex], variables );
                }
                else
                {
                        m->draw( &painter, true, merge::NullRecord(), model::Variables() );
                }

                // Outline on top, so overflow past the edge is visible
                painter.save();
                if ( m->rotate() )
                {
                        painter.rotate( -90 );
                        painter.translate( -m->frame()->w().pt(), 0 );
                }
                painter.setPen( QPen( QColor( 0x80, 0x80, 0x80 ), 1.0 / scale ) );
                painter.setBrush( Qt::NoBrush );
                painter.drawPath( m->frame()->path() );
                painter.restore();
                painter.end();

                QByteArray png;
                QBuffer buffer( &png );
                buffer.open( QIODevice::WriteOnly );
                image.save( &buffer, "PNG" );

                QJsonObject result = ok( QString( "Preview of %1 (%2 × %3 mm, %4 px per mm). Grey is outside the label." )
                                         .arg( m->shortName() )
                                         .arg( round2( m->w().mm() ) ).arg( round2( m->h().mm() ) )
                                         .arg( round2( scale * 72.0 / 25.4 ) ) );
                result.insert( "image", QString::fromLatin1( png.toBase64() ) );
                return result;
        }


        QJsonObject AiBridge::toolListTemplates( const QJsonObject& args )
        {
                const QStringList words = args.value( "search" ).toString().simplified().split( ' ', Qt::SkipEmptyParts );
                int limit = qBound( 1, args.value( "limit" ).toInt( 25 ), 100 );
                double wantW = args.value( "width_mm" ).toDouble( 0 );
                double wantH = args.value( "height_mm" ).toDouble( 0 );

                QJsonArray list;
                int matches = 0;

                // Recently used templates first, then the rest
                QList<model::Template> templates;
                for ( const QString& name : model::Settings::recentTemplateList() )
                {
                        auto t = model::Db::lookupTemplateFromName( name );
                        if ( !t.isNull() )
                        {
                                templates << t;
                        }
                }
                for ( const auto& t : model::Db::templates() )
                {
                        if ( !templates.contains( t ) )
                        {
                                templates << t;
                        }
                }

                for ( const auto& t : templates )
                {
                        const auto* frame = t.frame();
                        if ( !frame )
                        {
                                continue;
                        }
                        if ( wantW > 0 && wantH > 0 )
                        {
                                double w = frame->w().mm(), h = frame->h().mm();
                                bool fits = ( std::abs( w - wantW ) < 1.0 && std::abs( h - wantH ) < 1.0 )
                                            || ( std::abs( w - wantH ) < 1.0 && std::abs( h - wantW ) < 1.0 );
                                if ( !fits )
                                {
                                        continue;
                                }
                        }
                        QString haystack = t.name() + " " + t.description() + " "
                                           + t.paperDescription( model::Units( model::Units::MM ) )
                                           + ( t.isRoll() ? " roll continuous thermal" : " sheet" );
                        bool match = true;
                        for ( const QString& word : words )
                        {
                                match = match && haystack.contains( word, Qt::CaseInsensitive );
                        }
                        if ( !match )
                        {
                                continue;
                        }
                        if ( ++matches <= limit )
                        {
                                list.append( templateJson( t ) );
                        }
                }

                QJsonObject result;
                result.insert( "matches", matches );
                result.insert( "templates", list );
                if ( matches > limit )
                {
                        result.insert( "note", QString( "Showing %1 of %2. Narrow the search, or give width_mm and height_mm." )
                                               .arg( limit ).arg( matches ) );
                }
                return ok( result );
        }


        QJsonObject AiBridge::toolNewLabel( const QJsonObject& args )
        {
                const QString name = args.value( "template" ).toString();
                model::Template tmplate = model::Db::lookupTemplateFromName( name );
                if ( tmplate.isNull() )
                {
                        return fail( QString( "No template named \"%1\". Use list_templates and pass its exact name." ).arg( name ) );
                }

                auto* m = new model::Model();
                m->setTmplate( tmplate );
                auto* frame = tmplate.frame();
                m->setRotate( args.contains( "rotate" ) ? args.value( "rotate" ).toBool() : frame->h() > frame->w() );
                m->clearModified();
                model::Settings::addToRecentTemplateList( tmplate.name() );

                QString error;
                MainWindow* window = target( QJsonObject(), &error, false );
                if ( !window || !window->isEmpty() )
                {
                        window = new MainWindow();
                        window->show();
                }
                window->setModel( m );
                window->showEditorPage();
                window->raise();
                mLastWindow = window;
                noteActivity( window, tr("created a new label") );

                QJsonObject result = templateJson( tmplate );
                result.insert( "document", docId( window ) );
                result.insert( "rotated", m->rotate() );
                result.insert( "design_width_mm", round2( m->w().mm() ) );
                result.insert( "design_height_mm", round2( m->h().mm() ) );
                return ok( result );
        }


        QJsonObject AiBridge::toolSetTemplate( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                const QString name = args.value( "template" ).toString();
                model::Template tmplate = model::Db::lookupTemplateFromName( name );
                if ( tmplate.isNull() )
                {
                        return fail( QString( "No template named \"%1\". Use list_templates and pass its exact name." ).arg( name ) );
                }

                model::Model* m = window->model();
                window->undoRedoModel()->checkpoint( tr("Claude: Change Template") );
                m->setTmplate( tmplate );
                if ( args.contains( "rotate" ) )
                {
                        m->setRotate( args.value( "rotate" ).toBool() );
                }
                model::Settings::addToRecentTemplateList( tmplate.name() );
                noteActivity( window, tr("changed the template to %1").arg( tmplate.name() ) );

                QJsonObject result = templateJson( tmplate );
                result.insert( "design_width_mm", round2( m->w().mm() ) );
                result.insert( "design_height_mm", round2( m->h().mm() ) );
                result.insert( "note", "Objects keep their positions; move any that are now outside the label." );
                return ok( result );
        }


        QJsonObject AiBridge::toolListSymbols( const QJsonObject& args )
        {
                const QStringList words = args.value( "search" ).toString().simplified().split( ' ', Qt::SkipEmptyParts );
                QJsonArray list;
                for ( const auto& s : SymbolsDialog::allSymbols() )
                {
                        QString haystack = s.id + " " + s.name + " " + s.category + " " + s.keywords;
                        bool match = true;
                        for ( const QString& word : words )
                        {
                                match = match && haystack.contains( word, Qt::CaseInsensitive );
                        }
                        if ( match )
                        {
                                QJsonObject o{ { "id", s.id }, { "name", s.name }, { "category", s.category } };
                                if ( s.custom )
                                {
                                        o.insert( "custom", true );
                                }
                                list.append( o );
                        }
                }
                return ok( QJsonObject{ { "symbols", list } } );
        }


        QJsonObject AiBridge::toolInsertSymbol( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                const QString id = args.value( "id" ).toString();
                for ( const auto& s : SymbolsDialog::allSymbols() )
                {
                        if ( s.id != id )
                        {
                                continue;
                        }
                        QFile file( s.path );
                        if ( !file.open( QFile::ReadOnly ) )
                        {
                                return fail( "Could not read the symbol." );
                        }
                        QByteArray svg = file.readAll();
                        if ( s.custom && !CustomSymbols::validate( svg ).isEmpty() )
                        {
                                return fail( "This custom symbol's file is no longer valid." );
                        }

                        double size = args.value( "size_mm" ).toDouble( 10 );
                        if ( size <= 0 || size > 1000 )
                        {
                                return fail( "size_mm must be between 0 and 1000." );
                        }
                        double x = args.value( "x_mm" ).toDouble( ( m->w().mm() - size ) / 2 );
                        double y = args.value( "y_mm" ).toDouble( ( m->h().mm() - size ) / 2 );

                        window->undoRedoModel()->checkpoint( tr("Claude: Insert Symbol") );
                        auto* object = new model::ModelImageObject( model::Distance::mm( x ), model::Distance::mm( y ),
                                                                    model::Distance::mm( size ), model::Distance::mm( size ),
                                                                    true, uniqueSymbolName( s.id ), svg );
                        object->setName( s.name );
                        m->addObject( object );
                        m->unselectAll();
                        m->selectObject( object );
                        window->showEditorPage();
                        noteActivity( window, tr("inserted the %1 symbol").arg( s.name ) );
                        return ok( QString( "Inserted \"%1\" as object %2." ).arg( s.name ).arg( m->objectList().size() - 1 ) );
                }
                return fail( QString( "No symbol with id \"%1\". Use list_symbols." ).arg( id ) );
        }


        QJsonObject AiBridge::toolAddImage( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                const QString path = QDir::fromNativeSeparators( args.value( "path" ).toString() );
                QFileInfo info( path );
                if ( !info.isAbsolute() || !info.isFile() )
                {
                        return fail( "Give the absolute path of an existing image file." );
                }
                if ( info.size() > 20 * 1024 * 1024 )
                {
                        return fail( "The image is larger than 20 MB." );
                }
                QFile file( path );
                if ( !file.open( QFile::ReadOnly ) )
                {
                        return fail( "Could not read the file: " + file.errorString() );
                }
                QByteArray data = file.readAll();

                QSizeF natural;
                QImage image;
                bool isSvg = info.suffix().compare( "svg", Qt::CaseInsensitive ) == 0;
                if ( isSvg )
                {
                        QString problem = CustomSymbols::validate( data );
                        if ( !problem.isEmpty() )
                        {
                                return fail( problem );
                        }
                        QSvgRenderer renderer( data );
                        natural = renderer.viewBoxF().isEmpty() ? QSizeF( renderer.defaultSize() ) : renderer.viewBoxF().size();
                }
                else
                {
                        image.loadFromData( data );
                        if ( image.isNull() )
                        {
                                return fail( "Not a readable image. Use PNG, JPEG, BMP, GIF or SVG." );
                        }
                        natural = image.size();
                }

                double w = args.value( "w_mm" ).toDouble( 0 );
                double h = args.value( "h_mm" ).toDouble( 0 );
                double aspect = natural.height() / natural.width();
                if ( w <= 0 && h <= 0 )
                {
                        w = std::min( 20.0, m->w().mm() * 0.5 );
                }
                if ( w <= 0 )
                {
                        w = h / aspect;
                }
                if ( h <= 0 )
                {
                        h = w * aspect;
                }
                double x = args.value( "x_mm" ).toDouble( ( m->w().mm() - w ) / 2 );
                double y = args.value( "y_mm" ).toDouble( ( m->h().mm() - h ) / 2 );

                window->undoRedoModel()->checkpoint( tr("Claude: Add Image") );
                model::ModelImageObject* object;
                if ( isSvg )
                {
                        object = new model::ModelImageObject( model::Distance::mm( x ), model::Distance::mm( y ),
                                                              model::Distance::mm( w ), model::Distance::mm( h ),
                                                              true, info.fileName(), data );
                }
                else
                {
                        object = new model::ModelImageObject( model::Distance::mm( x ), model::Distance::mm( y ),
                                                              model::Distance::mm( w ), model::Distance::mm( h ),
                                                              true, info.fileName(), image );
                }
                m->addObject( object );
                m->unselectAll();
                m->selectObject( object );
                window->showEditorPage();
                noteActivity( window, tr("added the image %1").arg( info.fileName() ) );
                return ok( QString( "Added %1 as object %2 (%3 × %4 mm), embedded in the label." )
                           .arg( info.fileName() ).arg( m->objectList().size() - 1 ).arg( round2( w ) ).arg( round2( h ) ) );
        }


        QJsonObject AiBridge::toolSaveLabel( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                QString path = args.value( "path" ).toString();
                if ( path.isEmpty() )
                {
                        path = m->fileName();
                        if ( path.isEmpty() )
                        {
                                return fail( "This label has never been saved. Give a path ending in .glabels." );
                        }
                }
                else
                {
                        path = model::FileUtil::addExtension( QDir::fromNativeSeparators( path ), ".glabels" );
                        if ( !QFileInfo( path ).isAbsolute() )
                        {
                                return fail( "Give an absolute path." );
                        }
                        bool sameFile = QFileInfo( path ).absoluteFilePath() == QFileInfo( m->fileName() ).absoluteFilePath();
                        if ( QFileInfo::exists( path ) && !sameFile && !args.value( "overwrite" ).toBool() )
                        {
                                return fail( path + " already exists. Ask the user, then pass overwrite: true to replace it." );
                        }
                        if ( !QDir().mkpath( QFileInfo( path ).absolutePath() ) )
                        {
                                return fail( "Could not create the folder for " + path );
                        }
                }

                model::XmlLabelCreator::writeFile( m, path );
                if ( m->isModified() || m->fileName() != QFileInfo( path ).canonicalFilePath() )
                {
                        return fail( "Could not save " + path + ". Check that the folder is writable." );
                }
                model::Settings::addToRecentFileList( path );
                noteActivity( window, tr("saved %1").arg( QFileInfo( path ).fileName() ) );
                return ok( "Saved " + QDir::toNativeSeparators( path ) );
        }


        QJsonObject AiBridge::toolOpenLabel( const QJsonObject& args )
        {
                const QString path = QDir::fromNativeSeparators( args.value( "path" ).toString() );
                if ( !QFileInfo( path ).isAbsolute() || !QFileInfo( path ).isFile() )
                {
                        return fail( "Give the absolute path of an existing .glabels file." );
                }

                // Already open: switch to it
                for ( auto* w : mainWindows() )
                {
                        if ( !w->isEmpty() && QFileInfo( w->model()->fileName() ) == QFileInfo( path ) )
                        {
                                w->raise();
                                mLastWindow = w;
                                return ok( QJsonObject{ { "document", docId( w ) }, { "note", "This file was already open." } } );
                        }
                }

                model::Model* m = model::XmlLabelParser::readFile( path );
                if ( !m )
                {
                        return fail( "Could not read " + path + " as a gLabels file." );
                }

                QString error;
                MainWindow* window = target( QJsonObject(), &error, false );
                if ( !window || !window->isEmpty() )
                {
                        window = new MainWindow();
                        window->show();
                }
                window->setModel( m );
                window->showEditorPage();
                window->raise();
                mLastWindow = window;
                model::Settings::addToRecentFileList( m->fileName() );
                noteActivity( window, tr("opened %1").arg( QFileInfo( path ).fileName() ) );

                return ok( QJsonObject{ { "document", docId( window ) }, { "objects", m->objectList().size() },
                                        { "template", m->tmplate().name() } } );
        }


        QJsonObject AiBridge::toolExportPdf( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                QString path = model::FileUtil::addExtension( QDir::fromNativeSeparators( args.value( "path" ).toString() ), ".pdf" );
                if ( !QFileInfo( path ).isAbsolute() )
                {
                        return fail( "Give an absolute path for the PDF." );
                }
                if ( QFileInfo::exists( path ) && !args.value( "overwrite" ).toBool() )
                {
                        return fail( path + " already exists. Ask the user, then pass overwrite: true to replace it." );
                }

                model::PageRenderer renderer( m );
                bool merged = !m->merge()->keys().empty();
                if ( merged )
                {
                        renderer.setNCopies( qBound( 1, args.value( "copies" ).toInt( 1 ), 1000 ) );
                }
                else
                {
                        int pages = qBound( 1, args.value( "pages" ).toInt( 1 ), 1000 );
                        renderer.setNCopies( pages * m->frame()->nLabels() );
                }
                renderer.setStartItem( 0 );

                QPrinter printer( QPrinter::HighResolution );
                printer.setOutputFormat( QPrinter::PdfFormat );
                printer.setOutputFileName( path );
                renderer.print( &printer );

                if ( !QFileInfo::exists( path ) )
                {
                        return fail( "Could not write " + path );
                }
                noteActivity( window, tr("exported %1").arg( QFileInfo( path ).fileName() ) );
                return ok( QString( "Wrote %1 (%2 pages)." ).arg( QDir::toNativeSeparators( path ) ).arg( renderer.nPages() ) );
        }


        QJsonObject AiBridge::toolUndo( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                auto* undo = window->undoRedoModel();
                if ( !undo->canUndo() )
                {
                        return fail( "Nothing to undo." );
                }
                QString what = undo->undoDescription();
                undo->undo();
                noteActivity( window, tr("undid %1").arg( what ) );
                return ok( "Undid: " + what );
        }


        QJsonObject AiBridge::toolRedo( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                auto* undo = window->undoRedoModel();
                if ( !undo->canRedo() )
                {
                        return fail( "Nothing to redo." );
                }
                QString what = undo->redoDescription();
                undo->redo();
                noteActivity( window, tr("redid %1").arg( what ) );
                return ok( "Redid: " + what );
        }


        QJsonObject AiBridge::toolCreateTemplate( const QJsonObject& args )
        {
                using model::Distance;

                const QString brand = args.value( "brand" ).toString( "Custom" ).simplified();
                const QString part = args.value( "part" ).toString().simplified();
                const QString description = args.value( "description" ).toString().simplified();
                static const QRegularExpression badChars( R"([/\\:*?"<>|\x00-\x1f])" );
                if ( part.isEmpty() )
                {
                        return fail( "Give the template a part name, e.g. \"Jar lid 50 mm\"." );
                }
                if ( brand.isEmpty() || brand.contains( badChars ) || part.contains( badChars )
                     || brand.size() > 60 || part.size() > 80 )
                {
                        return fail( "Brand and part must be short and must not contain / \\ : * ? \" < > |." );
                }

                // Label shape
                const QString shape = args.value( "shape" ).toString( "rectangle" ).toLower();
                double w = args.value( "width_mm" ).toDouble( 0 );
                double h = args.value( "height_mm" ).toDouble( 0 );
                if ( shape == "round" || shape == "circle" )
                {
                        double d = args.value( "diameter_mm" ).toDouble( w > 0 ? w : h );
                        w = h = d;
                }
                else if ( shape != "rectangle" && shape != "ellipse" )
                {
                        return fail( "shape must be rectangle, round or ellipse." );
                }
                if ( w < 3 || h < 3 || w > 2000 || h > 5000 )
                {
                        return fail( shape == "round" || shape == "circle"
                                     ? "Give diameter_mm between 3 and 2000."
                                     : "Give width_mm and height_mm of the label (3-2000 mm wide, 3-5000 mm high)." );
                }
                double radius = std::max( 0.0, args.value( "corner_radius_mm" ).toDouble( 0 ) );
                if ( radius > std::min( w, h ) / 2 )
                {
                        return fail( "corner_radius_mm can be at most half the shorter side." );
                }
                double margin = args.value( "margin_mm" ).toDouble( 1.5 );
                if ( margin < 0 || margin > std::min( w, h ) / 2 )
                {
                        return fail( "margin_mm must be between 0 and half the shorter side." );
                }

                // Grid of labels
                int nx = args.value( "columns" ).toInt( 1 );
                int ny = args.value( "rows" ).toInt( 1 );
                if ( nx < 1 || ny < 1 || nx * ny > 1000 )
                {
                        return fail( "columns and rows must be at least 1 (at most 1000 labels)." );
                }
                double gapX = args.value( "column_gap_mm" ).toDouble( 0 );
                double gapY = args.value( "row_gap_mm" ).toDouble( 0 );
                if ( gapX < 0 || gapY < 0 )
                {
                        return fail( "Gaps can't be negative." );
                }
                double dx = w + gapX, dy = h + gapY;
                double gridW = ( nx - 1 ) * dx + w;
                double gridH = ( ny - 1 ) * dy + h;

                // Page: a roll, a known paper size or a custom page
                const QString paperArg = args.value( "paper" ).toString( "roll" ).trimmed();
                QString paperId;
                double pageW = 0, pageH = 0, rollW = 0;
                double x0 = args.value( "left_mm" ).toDouble( -1 );
                double y0 = args.value( "top_mm" ).toDouble( -1 );
                if ( paperArg.compare( "roll", Qt::CaseInsensitive ) == 0 )
                {
                        paperId = "roll";
                        x0 = std::max( 0.0, x0 );
                        y0 = std::max( 0.0, y0 );
                        pageW = gridW + 2 * x0;
                        pageH = gridH + 2 * y0;
                        rollW = args.value( "roll_width_mm" ).toDouble( pageW );
                        if ( rollW < pageW - 0.01 )
                        {
                                return fail( QString( "roll_width_mm must be at least the width of the labels across (%1 mm)." )
                                             .arg( round2( pageW ) ) );
                        }
                }
                else if ( paperArg.compare( "other", Qt::CaseInsensitive ) == 0
                          || paperArg.compare( "custom", Qt::CaseInsensitive ) == 0 )
                {
                        paperId = "other";
                        pageW = args.value( "page_width_mm" ).toDouble( 0 );
                        pageH = args.value( "page_height_mm" ).toDouble( 0 );
                        if ( pageW <= 0 || pageH <= 0 )
                        {
                                return fail( "With paper \"other\", give page_width_mm and page_height_mm." );
                        }
                }
                else
                {
                        model::Paper paper;
                        for ( const auto& p : model::Db::papers() )
                        {
                                if ( p.id().compare( paperArg, Qt::CaseInsensitive ) == 0
                                     || p.name().compare( paperArg, Qt::CaseInsensitive ) == 0 )
                                {
                                        paper = p;
                                }
                        }
                        if ( paper.id().isEmpty() )
                        {
                                QStringList names;
                                for ( const auto& p : model::Db::papers() )
                                {
                                        names << p.id();
                                }
                                return fail( QString( "Unknown paper \"%1\". Use \"roll\", \"other\" (with page_width_mm and "
                                                      "page_height_mm) or one of: %2." ).arg( paperArg, names.join( ", " ) ) );
                        }
                        paperId = paper.id();
                        pageW = paper.width().mm();
                        pageH = paper.height().mm();
                }

                if ( paperId != "roll" )
                {
                        // Center the grid on the page unless told otherwise
                        if ( x0 < 0 )
                        {
                                x0 = ( pageW - gridW ) / 2;
                        }
                        if ( y0 < 0 )
                        {
                                y0 = ( pageH - gridH ) / 2;
                        }
                        if ( x0 < -0.01 || y0 < -0.01 || x0 + gridW > pageW + 0.01 || y0 + gridH > pageH + 0.01 )
                        {
                                return fail( QString( "The labels (%1 × %2 mm with margins %3, %4 mm) don't fit on the %5 × %6 mm page. "
                                                      "Use fewer columns or rows, smaller gaps or other offsets." )
                                             .arg( round2( gridW ) ).arg( round2( gridH ) )
                                             .arg( round2( std::max( 0.0, x0 ) ) ).arg( round2( std::max( 0.0, y0 ) ) )
                                             .arg( round2( pageW ) ).arg( round2( pageH ) ) );
                        }
                        x0 = std::max( 0.0, x0 );
                        y0 = std::max( 0.0, y0 );
                }

                const bool overwrite = args.value( "overwrite" ).toBool();
                if ( model::Db::isSystemTemplateKnown( brand, part ) )
                {
                        return fail( QString( "\"%1\" is a built-in template name. Choose another part name." )
                                     .arg( model::Template::brandPartToName( brand, part ) ) );
                }
                if ( model::Db::isUserTemplateKnown( brand, part ) && !overwrite )
                {
                        return fail( QString( "A custom template named \"%1\" already exists. Ask the user, then pass "
                                              "overwrite: true to replace it." ).arg( model::Template::brandPartToName( brand, part ) ) );
                }

                model::Template t( brand, part, description.isEmpty() ? QString( "Custom label" ) : description, paperId,
                                   Distance::mm( pageW ), Distance::mm( pageH ), Distance::mm( rollW ), "", true );
                model::Layout layout( nx, ny, Distance::mm( x0 ), Distance::mm( y0 ),
                                      Distance::mm( nx > 1 ? dx : 0 ), Distance::mm( ny > 1 ? dy : 0 ) );
                if ( shape == "ellipse" )
                {
                        model::FrameEllipse frame( Distance::mm( w ), Distance::mm( h ), Distance::mm( 0 ) );
                        frame.addMarkup( model::MarkupMargin( Distance::mm( margin ) ) );
                        frame.addLayout( layout );
                        t.addFrame( frame );
                }
                else if ( shape == "rectangle" )
                {
                        model::FrameRect frame( Distance::mm( w ), Distance::mm( h ), Distance::mm( radius ),
                                                Distance::mm( 0 ), Distance::mm( 0 ) );
                        frame.addMarkup( model::MarkupMargin( Distance::mm( margin ), Distance::mm( margin ) ) );
                        frame.addLayout( layout );
                        t.addFrame( frame );
                }
                else
                {
                        model::FrameRound frame( Distance::mm( w / 2 ), Distance::mm( 0 ) );
                        frame.addMarkup( model::MarkupMargin( Distance::mm( margin ) ) );
                        frame.addLayout( layout );
                        t.addFrame( frame );
                }

                if ( model::Db::isUserTemplateKnown( brand, part ) )
                {
                        model::Db::deleteUserTemplateByBrandPart( brand, part );
                }
                model::Db::registerUserTemplate( t );
                model::Template saved = model::Db::lookupTemplateFromName( t.name() );
                if ( saved.isNull() || !saved.isUserDefined() )
                {
                        return fail( "Could not save the template in " + model::FileUtil::userTemplatesDir().path() );
                }

                QJsonObject result = templateJson( saved );
                result.insert( "file", QDir::toNativeSeparators( saved.fileName() ) );
                result.insert( "note", "Saved with the user's own templates (also listed in File > New under the brand). "
                                       "Use new_label or set_template with this name." );
                if ( auto* window = mLastWindow.data() )
                {
                        noteActivity( window, tr("created the template %1").arg( saved.name() ) );
                }
                return ok( result );
        }


        QJsonObject AiBridge::toolDeleteTemplate( const QJsonObject& args )
        {
                const QString name = args.value( "template" ).toString();
                model::Template t = model::Db::lookupTemplateFromName( name );
                if ( t.isNull() )
                {
                        return fail( QString( "No template named \"%1\"." ).arg( name ) );
                }
                if ( !t.isUserDefined() )
                {
                        return fail( "Only the user's own (custom) templates can be deleted; this one is built in." );
                }
                model::Db::deleteUserTemplateByBrandPart( t.brand(), t.part() );
                if ( model::Db::isUserTemplateKnown( t.brand(), t.part() ) )
                {
                        return fail( "Could not delete the template." );
                }
                return ok( QString( "Deleted the custom template \"%1\". Labels already made with it keep their layout." ).arg( name ) );
        }


        QJsonObject AiBridge::toolSetMergeData( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                const QString path = QDir::fromNativeSeparators( args.value( "path" ).toString() );
                if ( path.isEmpty() )
                {
                        window->undoRedoModel()->checkpoint( tr("Claude: Disconnect Data") );
                        m->setMerge( merge::Factory::createMerge( "None" ) );
                        noteActivity( window, tr("disconnected the merge data") );
                        return ok( "Disconnected the merge data. The label now prints without records." );
                }
                QFileInfo info( path );
                if ( !info.isAbsolute() || !info.isFile() )
                {
                        return fail( "Give the absolute path of an existing CSV, TSV, TXT or JSON file (or no path to disconnect)." );
                }

                QString format = args.value( "format" ).toString( "auto" ).toLower();
                bool header = args.value( "has_header" ).toBool( true );
                if ( format == "auto" )
                {
                        const QString ext = info.suffix().toLower();
                        format = ext == "json" ? "json" : ext == "tsv" || ext == "tab" ? "tsv" : ext == "csv" ? "csv" : "text";
                }
                QString id;
                if ( format == "json" )       id = "Json";
                else if ( format == "csv" )   id = header ? "Text/Comma/Line1Keys" : "Text/Comma";
                else if ( format == "tsv" )   id = header ? "Text/Tab/Line1Keys" : "Text/Tab";
                else if ( format == "semicolon" ) id = header ? "Text/Semicolon/Keys" : "Text/Semicolon";
                else if ( format == "text" )  id = header ? "Text/Auto/Line1Keys" : "Text/Auto";
                else
                {
                        return fail( "format must be auto, csv, tsv, semicolon, text (auto-detect delimiter) or json." );
                }

                merge::Merge* merge = merge::Factory::createMerge( id );
                if ( !merge )
                {
                        return fail( "This merge format is not available." );
                }
                merge->setSource( info.absoluteFilePath() );
                if ( merge->recordList().isEmpty() && merge->keys().isEmpty() )
                {
                        delete merge;
                        return fail( "No records could be read from " + path + ". Check the file and the format." );
                }

                window->undoRedoModel()->checkpoint( tr("Claude: Connect Data") );
                m->setMerge( merge );
                noteActivity( window, tr("connected %1").arg( info.fileName() ) );

                QJsonObject result = toolGetMergeData( QJsonObject{ { "document", docId( window ) }, { "limit", 3 } } );
                result.insert( "text", "Connected. Put fields on the label as ${Field} in text and barcode data.\n"
                                       + result.value( "text" ).toString() );
                return result;
        }


        QJsonObject AiBridge::toolGetMergeData( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                const merge::Merge* merge = window->model()->merge();
                if ( !merge || merge->id() == "None" || merge->source().isEmpty() )
                {
                        return ok( "This label has no merge data. Connect a file with set_merge_data." );
                }

                int limit = qBound( 0, args.value( "limit" ).toInt( 10 ), 200 );
                int start = std::max( 0, args.value( "start" ).toInt( 0 ) );
                const auto& records = merge->recordList();

                QJsonArray list;
                for ( int i = start; i < records.size() && i < start + limit; i++ )
                {
                        QJsonObject r{ { "index", i } };
                        if ( !records[i].isSelected() )
                        {
                                r.insert( "selected", false );
                        }
                        for ( const QString& key : merge->keys() )
                        {
                                r.insert( key, records[i].value( key ).left( 200 ) );
                        }
                        list.append( r );
                }

                QJsonObject result;
                result.insert( "source", QDir::toNativeSeparators( merge->source() ) );
                result.insert( "format", merge::Factory::idToName( merge->id() ) );
                result.insert( "fields", QJsonArray::fromStringList( merge->keys() ) );
                result.insert( "records", int( records.size() ) );
                result.insert( "selected_records", merge->nSelectedRecords() );
                result.insert( "rows", list );
                return ok( result );
        }


        QJsonObject AiBridge::toolSetVariable( const QJsonObject& args )
        {
                QString error;
                MainWindow* window = target( args, &error );
                if ( !window )
                {
                        return fail( error );
                }
                model::Model* m = window->model();

                const QString name = args.value( "name" ).toString().trimmed();
                static const QRegularExpression validName( "^[A-Za-z_][A-Za-z0-9_ .-]{0,59}$" );
                if ( !validName.match( name ).hasMatch() )
                {
                        return fail( "Give a variable name of letters, digits and _ (starting with a letter)." );
                }

                if ( args.value( "delete" ).toBool() )
                {
                        if ( !m->variables().hasVariable( name ) )
                        {
                                return fail( QString( "The label has no variable \"%1\"." ).arg( name ) );
                        }
                        window->undoRedoModel()->checkpoint( tr("Claude: Delete Variable") );
                        m->variables().deleteVariable( name );
                        noteActivity( window, tr("deleted the variable %1").arg( name ) );
                        return ok( QString( "Deleted the variable \"%1\". Remove any ${%1} left on the label." ).arg( name ) );
                }

                const QString type = args.value( "type" ).toString( "string" ).toLower();
                const QString increment = args.value( "increment" ).toString( "never" ).toLower();
                if ( !QStringList{ "string", "integer", "float", "color" }.contains( type ) )
                {
                        return fail( "type must be string, integer, float or color." );
                }
                if ( !QStringList{ "never", "per_item", "per_copy", "per_page" }.contains( increment ) )
                {
                        return fail( "increment must be never, per_item, per_copy or per_page." );
                }
                QString value = args.value( "initial_value" ).toVariant().toString();
                QString step = args.value( "step" ).toVariant().toString();
                if ( step.isEmpty() )
                {
                        step = increment == "never" ? "0" : "1";
                }
                if ( type == "integer" || type == "float" )
                {
                        if ( value.isEmpty() )
                        {
                                value = "0";
                        }
                        bool okValue = false, okStep = false;
                        value.toDouble( &okValue );
                        step.toDouble( &okStep );
                        if ( !okValue || !okStep || ( type == "integer" && ( value.contains( '.' ) || step.contains( '.' ) ) ) )
                        {
                                return fail( "initial_value and step must be " + QString( type == "integer" ? "whole numbers." : "numbers." ) );
                        }
                }
                if ( type == "color" && !QColor::isValidColorName( value ) )
                {
                        return fail( "A color variable needs a color such as #ff0000 or black as initial_value." );
                }

                model::Variable variable( model::Variable::idStringToType( type ), name, value,
                                          model::Variable::idStringToIncrement( increment ), step );
                bool existed = m->variables().hasVariable( name );
                window->undoRedoModel()->checkpoint( tr("Claude: Set Variable") );
                m->variables().replaceVariable( name, variable );
                noteActivity( window, tr("set the variable %1").arg( name ) );
                return ok( QString( "%1 the %2 variable \"%3\" (start %4, increment %5). Use it as ${%3} in text or barcode data." )
                           .arg( existed ? "Updated" : "Added", type, name, value, increment ) );
        }

}
