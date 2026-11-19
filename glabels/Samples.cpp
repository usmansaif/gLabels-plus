//  Samples.cpp
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


#include "Samples.hpp"

#include "MainWindow.hpp"
#include "Projects.hpp"

#include "model/Model.hpp"
#include "model/XmlLabelCreator.hpp"
#include "model/XmlLabelParser.hpp"

#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>

#include <algorithm>


namespace glabels
{

        QDir Samples::userDir()
        {
                QString docs = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                if ( docs.isEmpty() )
                {
                        docs = QDir::homePath();
                }
                return QDir( docs + "/gLabels Plus Templates" );
        }


        QString Samples::userCategory()
        {
                return tr("My templates");
        }


        QList<Samples::Info> Samples::list()
        {
                QList<Info> samples;

                // Built-in samples, in the order of the index
                QFile index( ":/samples/samples.json" );
                if ( index.open( QFile::ReadOnly ) )
                {
                        const QJsonArray array = QJsonDocument::fromJson( index.readAll() ).array();
                        for ( const auto& value : array )
                        {
                                QJsonObject o = value.toObject();
                                Info info;
                                info.path        = ":/samples/" + o.value( "file" ).toString();
                                info.name        = QCoreApplication::translate( "Samples", o.value( "name" ).toString().toUtf8() );
                                info.category    = QCoreApplication::translate( "Samples", o.value( "category" ).toString().toUtf8() );
                                info.description = QCoreApplication::translate( "Samples", o.value( "description" ).toString().toUtf8() );
                                info.builtIn     = true;
                                if ( QFile::exists( info.path ) )
                                {
                                        samples << info;
                                }
                        }
                }

                // The user's own templates
                QDir dir = userDir();
                if ( dir.exists() )
                {
                        QFileInfoList files = dir.entryInfoList( { "*.glabels" }, QDir::Files | QDir::Readable );
                        QCollator collator;
                        collator.setCaseSensitivity( Qt::CaseInsensitive );
                        collator.setNumericMode( true );
                        std::sort( files.begin(), files.end(), [&]( const QFileInfo& a, const QFileInfo& b ) {
                                return collator.compare( a.completeBaseName(), b.completeBaseName() ) < 0;
                        } );
                        for ( const auto& file : files )
                        {
                                Info info;
                                info.path     = file.absoluteFilePath();
                                info.name     = file.completeBaseName();
                                info.category = userCategory();
                                info.builtIn  = false;
                                samples << info;
                        }
                }

                return samples;
        }


        model::Model* Samples::load( const Info& sample )
        {
                model::Model* model = nullptr;
                if ( sample.builtIn )
                {
                        QFile file( sample.path );
                        if ( file.open( QFile::ReadOnly ) )
                        {
                                model = model::XmlLabelParser::readBuffer( file.readAll() );
                        }
                }
                else
                {
                        // Regular reader: handles compressed files and images next to the template
                        model = model::XmlLabelParser::readFile( sample.path );
                }
                if ( !model )
                {
                        return nullptr;
                }

                // An untitled copy: Save asks for a new name, so the sample is never overwritten
                model->setFileName( QString() );
                model->setUntitledName( sample.name );
                model->clearModified();
                return model;
        }


        MainWindow* Samples::open( const Info& sample, MainWindow* window )
        {
                model::Model* model = load( sample );
                if ( !model )
                {
                        QMessageBox::warning( window, tr("Sample Labels"),
                                              tr("Unable to open the sample \"%1\".").arg( sample.name ) );
                        return nullptr;
                }

                if ( window->isEmpty() )
                {
                        window->setModel( model );
                        return window;
                }
                auto* newWindow = new MainWindow();
                newWindow->setModel( model );
                newWindow->show();
                return newWindow;
        }


        bool Samples::saveAsTemplate( MainWindow* window )
        {
                model::Model* model = window->model();
                if ( !model )
                {
                        return false;
                }

                QDir dir = userDir();
                if ( !dir.exists() && !dir.mkpath( "." ) )
                {
                        QMessageBox::warning( window, tr("Save as Template"),
                                              tr("Couldn't create the folder \"%1\".")
                                              .arg( QDir::toNativeSeparators( dir.absolutePath() ) ) );
                        return false;
                }

                QString name = Projects::askName( window,
                                                  tr("Save as Template"),
                                                  tr("Template name (it appears under \"My templates\" in Sample Labels):"),
                                                  model->shortName(),
                                                  dir.absolutePath(),
                                                  ".glabels" );
                if ( name.isEmpty() )
                {
                        return false;
                }

                // Write a copy: the open label keeps its own file name and modified state
                QByteArray buffer;
                model::XmlLabelCreator::writeBuffer( model, buffer );

                QString fileName = dir.absoluteFilePath( name + ".glabels" );
                QFile file( fileName );
                if ( !file.open( QFile::WriteOnly | QFile::Truncate ) || file.write( buffer ) != buffer.size() )
                {
                        QMessageBox::warning( window, tr("Save as Template"),
                                              tr("Couldn't save \"%1\".").arg( QDir::toNativeSeparators( fileName ) ) );
                        return false;
                }
                file.close();

                QMessageBox::information( window, tr("Save as Template"),
                                          tr("Saved \"%1\" to My templates.\n\nFind it under File > New from Sample.")
                                          .arg( name ) );
                return true;
        }

}
