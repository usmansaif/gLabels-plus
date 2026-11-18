//  Projects.cpp
//
//  Copyright (C) 2026  Usman Saif <usmansaif.com>
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


#include "Projects.hpp"

#include "MainWindow.hpp"

#include "model/Model.hpp"

#include <QApplication>
#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include <algorithm>


namespace
{
        const QString LABEL_SUFFIX = ".glabels";

        bool sameName( const QString& a, const QString& b )
        {
                return a.compare( b, Qt::CaseInsensitive ) == 0;
        }

        /// Does dir already contain an entry named name (case-insensitive, so names
        /// stay unique on Windows and macOS too)?
        bool entryExists( const QDir& dir, const QString& name )
        {
                const auto entries = dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden );
                return std::any_of( entries.begin(), entries.end(),
                                    [&]( const QString& e ) { return sameName( e, name ); } );
        }

        /// Read "Brand Part" from a label file's <Template> element without parsing
        /// the whole document
        QString readProductName( const QString& filePath )
        {
                QFile file( filePath );
                if ( !file.open( QIODevice::ReadOnly ) )
                {
                        return {};
                }
                QXmlStreamReader xml( &file );
                while ( !xml.atEnd() )
                {
                        if ( xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String( "Template" ) )
                        {
                                auto attrs = xml.attributes();
                                return QString( "%1 %2" ).arg( attrs.value( "brand" ).toString(),
                                                              attrs.value( "part" ).toString() ).trimmed();
                        }
                }
                return {};
        }
}


namespace glabels
{

        QDir Projects::rootDir()
        {
                QString docs = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                if ( docs.isEmpty() )
                {
                        docs = QDir::homePath();
                }
                QDir dir( docs + "/gLabels Plus Projects" );
                if ( !dir.exists() )
                {
                        dir.mkpath( "." );
                }
                return dir;
        }


        QStringList Projects::list()
        {
                QStringList names = rootDir().entryList( QDir::Dirs | QDir::NoDotAndDotDot );

                QCollator collator;
                collator.setCaseSensitivity( Qt::CaseInsensitive );
                collator.setNumericMode( true );
                std::sort( names.begin(), names.end(), collator );
                return names;
        }


        QString Projects::path( const QString& project )
        {
                return rootDir().absoluteFilePath( project );
        }


        int Projects::labelCount( const QString& project )
        {
                return QDir( path( project ) ).entryList( { "*" + LABEL_SUFFIX }, QDir::Files ).size();
        }


        QList<Projects::LabelInfo> Projects::labels( const QString& project )
        {
                QList<LabelInfo> list;
                QDir dir( path( project ) );
                for ( const auto& info : dir.entryInfoList( { "*" + LABEL_SUFFIX }, QDir::Files, QDir::Time ) )
                {
                        list << LabelInfo{ info.absoluteFilePath(),
                                           info.completeBaseName(),
                                           readProductName( info.absoluteFilePath() ),
                                           info.lastModified() };
                }
                return list;
        }


        QString Projects::projectOf( const QString& filePath )
        {
                if ( filePath.isEmpty() )
                {
                        return {};
                }
                QFileInfo info( filePath );
                QString parent = QFileInfo( info.absolutePath() ).canonicalFilePath();
                QString root   = rootDir().canonicalPath();
                if ( !parent.isEmpty() && QFileInfo( parent ).absolutePath() == root )
                {
                        return QFileInfo( parent ).fileName();
                }
                return {};
        }


        QString Projects::validateName( const QString& rawName, QString* error )
        {
                QString name = rawName.simplified();

                static const QString forbidden = "/\\:*?\"<>|";
                if ( name.isEmpty() )
                {
                        *error = tr("Please enter a name.");
                        return {};
                }
                if ( name.length() > 100 )
                {
                        *error = tr("The name is too long (100 characters at most).");
                        return {};
                }
                if ( name.startsWith( '.' ) )
                {
                        *error = tr("The name can't start with a dot.");
                        return {};
                }
                for ( QChar c : name )
                {
                        if ( forbidden.contains( c ) || c.unicode() < 32 )
                        {
                                *error = tr("The name can't contain any of these characters: %1")
                                         .arg( "/ \\ : * ? \" < > |" );
                                return {};
                        }
                }
                return name;
        }


        bool Projects::create( const QString& project, QString* error )
        {
                QString name = validateName( project, error );
                if ( name.isEmpty() )
                {
                        return false;
                }
                QDir root = rootDir();
                if ( entryExists( root, name ) )
                {
                        *error = tr("A project called \"%1\" already exists.").arg( name );
                        return false;
                }
                if ( !root.mkdir( name ) )
                {
                        *error = tr("Couldn't create the folder \"%1\".").arg( QDir::toNativeSeparators( path( name ) ) );
                        return false;
                }
                return true;
        }


        bool Projects::rename( const QString& project, const QString& newName, QString* error )
        {
                QString name = validateName( newName, error );
                if ( name.isEmpty() )
                {
                        return false;
                }
                if ( name == project )
                {
                        return true;
                }
                if ( isOpen( path( project ) ) )
                {
                        *error = tr("Close the labels from \"%1\" that are open first.").arg( project );
                        return false;
                }
                QDir root = rootDir();
                // Allow changing only the letter case of the same project
                if ( !sameName( name, project ) && entryExists( root, name ) )
                {
                        *error = tr("A project called \"%1\" already exists.").arg( name );
                        return false;
                }
                if ( !root.rename( project, name ) )
                {
                        *error = tr("Couldn't rename the project folder.");
                        return false;
                }
                return true;
        }


        bool Projects::moveToTrash( const QString& project, QString* error )
        {
                if ( isOpen( path( project ) ) )
                {
                        *error = tr("Close the labels from \"%1\" that are open first.").arg( project );
                        return false;
                }
                if ( !QFile::moveToTrash( path( project ) ) )
                {
                        *error = tr("Couldn't move the project folder to the Trash. "
                                    "You can delete it yourself from:\n%1")
                                 .arg( QDir::toNativeSeparators( path( project ) ) );
                        return false;
                }
                return true;
        }


        bool Projects::deletePermanently( const QString& project, QString* error )
        {
                if ( isOpen( path( project ) ) )
                {
                        *error = tr("Close the labels from \"%1\" that are open first.").arg( project );
                        return false;
                }
                if ( !QDir( path( project ) ).removeRecursively() )
                {
                        *error = tr("Couldn't delete the project folder. "
                                    "You can delete it yourself from:\n%1")
                                 .arg( QDir::toNativeSeparators( path( project ) ) );
                        return false;
                }
                return true;
        }


        QString Projects::labelPath( const QString& project, const QString& labelName )
        {
                return QDir( path( project ) ).absoluteFilePath( labelName + LABEL_SUFFIX );
        }


        QString Projects::nextLabelName( const QString& project )
        {
                QDir dir( path( project ) );
                for ( int i = 1; ; i++ )
                {
                        QString name = tr("Label %1").arg( i );
                        if ( !entryExists( dir, name + LABEL_SUFFIX ) )
                        {
                                return name;
                        }
                }
        }


        bool Projects::isOpen( const QString& path )
        {
                QString target = QFileInfo( path ).canonicalFilePath();
                if ( target.isEmpty() )
                {
                        return false;
                }
                bool isDir = QFileInfo( target ).isDir();

                for ( QWidget* widget : QApplication::topLevelWidgets() )
                {
                        auto* window = qobject_cast<MainWindow*>( widget );
                        if ( !window || !window->model() )
                        {
                                continue;
                        }
                        QString open = QFileInfo( window->model()->fileName() ).canonicalFilePath();
                        if ( open.isEmpty() )
                        {
                                continue;
                        }
                        if ( open == target || ( isDir && open.startsWith( target + "/" ) ) )
                        {
                                return true;
                        }
                }
                return false;
        }


        QString Projects::askName( QWidget*       parent,
                                   const QString& title,
                                   const QString& prompt,
                                   const QString& initial,
                                   const QString& existsDir,
                                   const QString& existsSuffix )
        {
                QString text = initial;
                while ( true )
                {
                        bool ok = false;
                        text = QInputDialog::getText( parent, title, prompt, QLineEdit::Normal, text, &ok );
                        if ( !ok )
                        {
                                return {};
                        }

                        QString error;
                        QString name = validateName( text, &error );
                        if ( !name.isEmpty() && sameName( name, initial ) && !initial.isEmpty() )
                        {
                                return name;   // unchanged (or only the letter case changed)
                        }
                        if ( !name.isEmpty() && !existsDir.isEmpty() && entryExists( QDir( existsDir ), name + existsSuffix ) )
                        {
                                error = tr("\"%1\" already exists. Please choose another name.").arg( name );
                                name.clear();
                        }
                        if ( !name.isEmpty() )
                        {
                                return name;
                        }
                        QMessageBox::warning( parent, title, error );
                }
        }


        QString Projects::askNewProject( QWidget* parent )
        {
                QString name = askName( parent,
                                        tr("New Project"),
                                        tr("Project name (for example a business or client name):"),
                                        QString(),
                                        rootDir().absolutePath(),
                                        QString() );
                if ( name.isEmpty() )
                {
                        return {};
                }

                QString error;
                if ( !create( name, &error ) )
                {
                        QMessageBox::warning( parent, tr("New Project"), error );
                        return {};
                }
                return name;
        }

}
