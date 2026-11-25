//  CustomSymbols.cpp
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


#include "CustomSymbols.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QXmlStreamReader>


namespace
{
        const int maxElements = 50000;
        const int maxDepth    = 256;

        const QRegularExpression idPattern( "^custom-[a-z0-9]+(-[a-z0-9]+)*$" );


        QString tr( const char* text )
        {
                return QCoreApplication::translate( "CustomSymbols", text );
        }


        QString indexPath()
        {
                return glabels::CustomSymbols::directory() + "/symbols.json";
        }


        QJsonArray readIndex()
        {
                QFile file( indexPath() );
                if ( !file.open( QFile::ReadOnly ) )
                {
                        return {};
                }
                return QJsonDocument::fromJson( file.readAll() ).array();
        }


        bool writeIndex( const QJsonArray& array, QString* error )
        {
                QSaveFile file( indexPath() );
                if ( !file.open( QFile::WriteOnly ) )
                {
                        *error = tr("Could not save the symbol list: %1").arg( file.errorString() );
                        return false;
                }
                file.write( QJsonDocument( array ).toJson( QJsonDocument::Indented ) );
                if ( !file.commit() )
                {
                        *error = tr("Could not save the symbol list: %1").arg( file.errorString() );
                        return false;
                }
                return true;
        }


        ///
        /// A reference is allowed only if it points inside the file (#id) or is
        /// an embedded raster image; anything else would need a file or the
        /// network and would not print the same everywhere
        ///
        bool isSafeReference( const QString& ref )
        {
                const QString r = ref.trimmed().toLower();
                if ( r.isEmpty() || r.startsWith( '#' ) )
                {
                        return true;
                }
                static const QStringList images = { "data:image/png", "data:image/jpeg", "data:image/jpg",
                                                    "data:image/gif", "data:image/webp", "data:image/bmp" };
                for ( const QString& prefix : images )
                {
                        if ( r.startsWith( prefix ) )
                        {
                                return true;
                        }
                }
                return false;
        }


        /// Check every url(...) in a style or attribute value, and @import
        QString checkCss( const QString& text )
        {
                if ( text.contains( "@import", Qt::CaseInsensitive ) )
                {
                        return tr("The SVG imports an external style sheet. Embed the styles in the file instead.");
                }
                if ( text.contains( "javascript:", Qt::CaseInsensitive ) )
                {
                        return tr("The SVG contains script code, which is not allowed in symbols.");
                }
                static const QRegularExpression urlPattern( "url\\(\\s*['\"]?\\s*([^'\")]*)",
                                                            QRegularExpression::CaseInsensitiveOption );
                auto it = urlPattern.globalMatch( text );
                while ( it.hasNext() )
                {
                        if ( !isSafeReference( it.next().captured( 1 ) ) )
                        {
                                return tr("The SVG links to an external file. Embed images and fonts in the file, "
                                          "or convert text to paths.");
                        }
                }
                return {};
        }


        QString slugify( const QString& name )
        {
                QString slug;
                bool dash = false;
                for ( const QChar c : name.normalized( QString::NormalizationForm_KD ).toLower() )
                {
                        if ( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) )
                        {
                                if ( dash && !slug.isEmpty() )
                                {
                                        slug += '-';
                                }
                                slug += c;
                                dash = false;
                        }
                        else if ( c.isSpace() || c.isPunct() || c.isSymbol() )
                        {
                                dash = true;
                        }
                }
                slug.truncate( 40 );
                while ( slug.endsWith( '-' ) )
                {
                        slug.chop( 1 );
                }
                return slug.isEmpty() ? QString( "symbol" ) : slug;
        }
}


namespace glabels
{
        namespace CustomSymbols
        {

                QString directory()
                {
                        return QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) + "/symbols";
                }


                QList<Entry> load()
                {
                        QList<Entry> list;
                        QSet<QString> seen;
                        const QString dir = directory();

                        for ( const auto& value : readIndex() )
                        {
                                const QJsonObject o = value.toObject();
                                Entry e;
                                e.id       = o.value( "id" ).toString();
                                e.name     = o.value( "name" ).toString().trimmed();
                                e.category = o.value( "category" ).toString().trimmed();
                                e.keywords = o.value( "keywords" ).toString();

                                // The id names a file: never trust one that could leave the folder
                                if ( !idPattern.match( e.id ).hasMatch() || seen.contains( e.id ) || e.name.isEmpty() )
                                {
                                        continue;
                                }
                                if ( e.category.isEmpty() )
                                {
                                        e.category = tr("My Symbols");
                                }

                                e.path = dir + "/" + e.id + ".svg";
                                QFileInfo info( e.path );
                                if ( !info.isFile() || info.size() > maxFileBytes )
                                {
                                        continue;
                                }

                                seen.insert( e.id );
                                list << e;
                        }
                        return list;
                }


                QString validate( const QByteArray& data )
                {
                        if ( data.trimmed().isEmpty() )
                        {
                                return tr("The file is empty.");
                        }
                        if ( data.size() > maxFileBytes )
                        {
                                return tr("The file is larger than %1 MB. Simplify the drawing or save it as a "
                                          "plain (not Inkscape) SVG.").arg( maxFileBytes / ( 1024 * 1024 ) );
                        }
                        if ( data.startsWith( "\x1f\x8b" ) )
                        {
                                return tr("Compressed SVG (.svgz) is not supported. Save the drawing as a plain .svg file.");
                        }

                        // Structure: well formed, an <svg> root, and nothing active or external
                        QXmlStreamReader xml( data );
                        int elements = 0;
                        int depth = 0;
                        bool sawRoot = false;
                        while ( !xml.atEnd() )
                        {
                                switch ( xml.readNext() )
                                {
                                case QXmlStreamReader::DTD:
                                        if ( !xml.entityDeclarations().isEmpty() || !xml.notationDeclarations().isEmpty() )
                                        {
                                                return tr("The SVG declares XML entities, which are not allowed in symbols. "
                                                          "Export it again as a plain SVG.");
                                        }
                                        break;

                                case QXmlStreamReader::EntityReference:
                                        return tr("The SVG uses XML entities, which are not allowed in symbols. "
                                                  "Export it again as a plain SVG.");

                                case QXmlStreamReader::StartElement:
                                {
                                        const QString name = xml.name().toString().toLower();
                                        if ( !sawRoot )
                                        {
                                                if ( name != "svg" )
                                                {
                                                        return tr("This is not an SVG drawing: the file starts with <%1> "
                                                                  "instead of <svg>.").arg( xml.name().toString() );
                                                }
                                                sawRoot = true;
                                        }
                                        if ( ++elements > maxElements )
                                        {
                                                return tr("The drawing is too complex (more than %1 elements).").arg( maxElements );
                                        }
                                        if ( ++depth > maxDepth )
                                        {
                                                return tr("The drawing is nested too deeply.");
                                        }

                                        if ( name == "script" || name == "handler" || name == "listener" )
                                        {
                                                return tr("The SVG contains script code, which is not allowed in symbols.");
                                        }
                                        if ( name == "foreignobject" || name == "iframe" || name == "embed" || name == "object"
                                             || name == "audio" || name == "video" )
                                        {
                                                return tr("The SVG contains <%1>, which cannot be printed. Remove it, "
                                                          "or convert it to plain shapes.").arg( xml.name().toString() );
                                        }

                                        for ( const auto& attr : xml.attributes() )
                                        {
                                                const QString attrName = attr.name().toString().toLower();
                                                const QString value = attr.value().toString();
                                                if ( attrName.startsWith( "on" ) )
                                                {
                                                        return tr("The SVG contains script code (%1), which is not allowed in "
                                                                  "symbols.").arg( attr.qualifiedName().toString() );
                                                }
                                                if ( attrName == "href" || attrName == "src" )
                                                {
                                                        if ( !isSafeReference( value ) )
                                                        {
                                                                return tr("The SVG links to an external file or address (%1). "
                                                                          "Embed images in the file instead.")
                                                                        .arg( value.left( 60 ) );
                                                        }
                                                }
                                                QString cssError = checkCss( value );
                                                if ( !cssError.isEmpty() )
                                                {
                                                        return cssError;
                                                }
                                        }
                                        break;
                                }

                                case QXmlStreamReader::EndElement:
                                        depth--;
                                        break;

                                case QXmlStreamReader::Characters:
                                {
                                        QString cssError = checkCss( xml.text().toString() );
                                        if ( !cssError.isEmpty() )
                                        {
                                                return cssError;
                                        }
                                        break;
                                }

                                default:
                                        break;
                                }
                        }
                        if ( xml.hasError() )
                        {
                                return tr("The file is not a valid SVG (line %1: %2).")
                                        .arg( xml.lineNumber() ).arg( xml.errorString() );
                        }
                        if ( !sawRoot )
                        {
                                return tr("This is not an SVG drawing.");
                        }

                        // Rendering: Qt must understand it, it must have a size, and draw something
                        QSvgRenderer renderer( data );
                        if ( !renderer.isValid() )
                        {
                                return tr("The SVG could not be read. Try saving it as a plain SVG.");
                        }
                        QRectF box = renderer.viewBoxF();
                        if ( box.isEmpty() )
                        {
                                box = QRectF( QPointF( 0, 0 ), renderer.defaultSize() );
                        }
                        if ( box.isEmpty() )
                        {
                                return tr("The SVG has no size. Give it a viewBox, or a width and height.");
                        }
                        double aspect = box.width() / box.height();
                        if ( aspect > 50 || aspect < 1.0 / 50 )
                        {
                                return tr("The drawing is too long and thin to use as a symbol.");
                        }

                        QImage image( 96, 96, QImage::Format_ARGB32_Premultiplied );
                        image.fill( Qt::transparent );
                        {
                                QPainter painter( &image );
                                renderer.render( &painter, QRectF( 0, 0, 96, 96 ) );
                        }
                        bool drawsSomething = false;
                        for ( int y = 0; y < image.height() && !drawsSomething; y++ )
                        {
                                const QRgb* line = reinterpret_cast<const QRgb*>( image.constScanLine( y ) );
                                for ( int x = 0; x < image.width(); x++ )
                                {
                                        if ( qAlpha( line[x] ) > 0 )
                                        {
                                                drawsSomething = true;
                                                break;
                                        }
                                }
                        }
                        if ( !drawsSomething )
                        {
                                return tr("The SVG does not draw anything that can be printed.");
                        }

                        return {};
                }


                bool add( const QByteArray& svg,
                          const QString&    nameIn,
                          const QString&    categoryIn,
                          const QString&    keywordsIn,
                          Entry*            added,
                          QString*          error )
                {
                        const QString name     = nameIn.simplified();
                        QString       category = categoryIn.simplified();
                        const QString keywords = keywordsIn.simplified();
                        if ( category.isEmpty() )
                        {
                                category = tr("My Symbols");
                        }

                        if ( name.isEmpty() )
                        {
                                *error = tr("Enter a name for the symbol.");
                                return false;
                        }
                        if ( name.size() > maxNameLength )
                        {
                                *error = tr("The name is too long (at most %1 characters).").arg( maxNameLength );
                                return false;
                        }
                        if ( category.size() > maxCategoryLen )
                        {
                                *error = tr("The category is too long (at most %1 characters).").arg( maxCategoryLen );
                                return false;
                        }
                        if ( keywords.size() > maxKeywordsLen )
                        {
                                *error = tr("The keywords are too long (at most %1 characters).").arg( maxKeywordsLen );
                                return false;
                        }

                        const QList<Entry> existing = load();
                        for ( const Entry& e : existing )
                        {
                                if ( e.name.compare( name, Qt::CaseInsensitive ) == 0
                                     && e.category.compare( category, Qt::CaseInsensitive ) == 0 )
                                {
                                        *error = tr("You already have a symbol called \"%1\" in %2. Choose another name.")
                                                .arg( name, category );
                                        return false;
                                }
                        }

                        QString problem = validate( svg );
                        if ( !problem.isEmpty() )
                        {
                                *error = problem;
                                return false;
                        }

                        const QString dir = directory();
                        if ( !QDir().mkpath( dir ) )
                        {
                                *error = tr("Could not create the folder %1.").arg( QDir::toNativeSeparators( dir ) );
                                return false;
                        }

                        // Unique id, also against files left behind without an index entry
                        QJsonArray index = readIndex();
                        QSet<QString> ids;
                        for ( const auto& value : index )
                        {
                                ids.insert( value.toObject().value( "id" ).toString() );
                        }
                        const QString base = "custom-" + slugify( name );
                        QString id = base;
                        for ( int n = 2; ids.contains( id ) || QFileInfo::exists( dir + "/" + id + ".svg" ); n++ )
                        {
                                id = QString( "%1-%2" ).arg( base ).arg( n );
                        }

                        const QString path = dir + "/" + id + ".svg";
                        QSaveFile file( path );
                        if ( !file.open( QFile::WriteOnly ) || file.write( svg ) != svg.size() || !file.commit() )
                        {
                                *error = tr("Could not save the symbol: %1").arg( file.errorString() );
                                return false;
                        }

                        QJsonObject o;
                        o.insert( "id", id );
                        o.insert( "name", name );
                        o.insert( "category", category );
                        o.insert( "keywords", keywords );
                        index.append( o );
                        if ( !writeIndex( index, error ) )
                        {
                                QFile::remove( path );
                                return false;
                        }

                        if ( added )
                        {
                                *added = Entry{ id, name, category, keywords, path };
                        }
                        return true;
                }


                bool remove( const QString& id, QString* error )
                {
                        if ( !idPattern.match( id ).hasMatch() )
                        {
                                *error = tr("This is not a custom symbol.");
                                return false;
                        }

                        QJsonArray index = readIndex();
                        for ( int i = index.size() - 1; i >= 0; i-- )
                        {
                                if ( index.at( i ).toObject().value( "id" ).toString() == id )
                                {
                                        index.removeAt( i );
                                }
                        }
                        if ( !writeIndex( index, error ) )
                        {
                                return false;
                        }
                        QFile::remove( directory() + "/" + id + ".svg" );
                        return true;
                }

        }
}
