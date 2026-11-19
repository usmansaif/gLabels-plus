//  main.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
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


#include "MainWindow.hpp"
#include "SplashScreen.hpp"

#include "AppearanceModel.hpp"

#include "model/BuiltInFonts.hpp"
#include "model/Db.hpp"
#include "model/FileUtil.hpp"
#include "model/Model.hpp"
#include "model/Settings.hpp"
#include "model/Version.hpp"
#include "model/XmlLabelParser.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QIcon>
#include <QLibraryInfo>
#include <QList>
#include <QLocale>
#include <QTextStream>
#include <QTranslator>


int main( int argc, char **argv )
{
        QApplication app( argc, argv );

        QCoreApplication::setOrganizationName( glabels::model::Version::ORGANIZATION_NAME );
        QCoreApplication::setOrganizationDomain( glabels::model::Version::ORGANIZATION_DOMAIN );
        QCoreApplication::setApplicationName( glabels::model::Version::APPLICATION_NAME );
        QCoreApplication::setApplicationVersion( glabels::model::Version::LONG_STRING );
        QApplication::setDesktopFileName( glabels::model::Version::DESKTOP_FILE_NAME );

        // gLabels Plus app icon for every window and dialog (main windows, dialogs,
        // message boxes, wizards, splash, ...): Qt uses the application icon for
        // any top-level window that doesn't set its own.
        {
                QIcon appIcon;
                for ( int size : { 16, 22, 24, 32, 48, 64, 128 } )
                {
                        appIcon.addFile( QString( ":icons/app/glabels-plus-%1.png" ).arg( size ) );
                }
                appIcon.addFile( ":images/glabels-logo.png" );
                QApplication::setWindowIcon( appIcon );
        }


        //
        // Setup translators
        //
        QLocale locale = QLocale::system();
        QString qtTranslationsDir = QLibraryInfo::path( QLibraryInfo::TranslationsPath );
        QString myTranslationsDir = glabels::model::FileUtil::translationsDir().canonicalPath();

        QTranslator qtTranslator;
        if ( qtTranslator.load( locale, "qt", "_", qtTranslationsDir ) )
        {
                app.installTranslator(&qtTranslator);
        }

        QTranslator glabelsTranslator;
        if ( glabelsTranslator.load( locale, "glabels", "_", myTranslationsDir ) )
        {
                app.installTranslator(&glabelsTranslator);
        }

        QTranslator templatesTranslator;
        if ( templatesTranslator.load( locale, "templates", "_", myTranslationsDir ) )
        {
                app.installTranslator(&templatesTranslator);
        }


        //
        // Parse command line
        //
        QCommandLineParser parser;
        parser.setApplicationDescription( QCoreApplication::translate( "main", "gLabels Plus Label Designer" ) );
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addOption( { { "V", "Version" }, QCoreApplication::translate( "main", "More detailed version information." ) } );
        parser.addPositionalArgument( "files",
                                      QCoreApplication::translate( "main", "gLabels Plus project files to open, optionally." ),
                                      "[files...]" );
        parser.process( app );

        // Handle verbose version option
        if ( parser.isSet( "Version" ) )
        {
                QTextStream(stdout) << glabels::model::Version::details() << Qt::endl;
                return 0;
        }


        //
        // Initialize subsystems
        //
        glabels::SplashScreen splash;
        splash.show();
        splash.setProgress( 5, QCoreApplication::translate( "main", "Loading settings..." ) );
        glabels::model::Settings::init();
        glabels::model::BuiltInFonts::init();
        splash.setProgress( 20, QCoreApplication::translate( "main", "Loading label templates..." ) );
        glabels::model::Db::init();
        splash.setProgress( 65, QCoreApplication::translate( "main", "Initializing merge sources..." ) );
        glabels::merge::Factory::init();
        splash.setProgress( 75, QCoreApplication::translate( "main", "Initializing barcode backends..." ) );
        glabels::barcode::Backends::init();
        splash.setProgress( 85, QCoreApplication::translate( "main", "Applying appearance..." ) );
        glabels::AppearanceModel::init();


        //
        // Set initial working directory
        //
        auto path = glabels::model::Settings::startupPath();
        if ( !path.isEmpty() )
        {
                QDir::setCurrent( path );
        }


        //
        // Open each file in its own main window
        //
        splash.setProgress( 92, QCoreApplication::translate( "main", "Preparing workspace..." ) );
        QList<glabels::MainWindow*> windows;
        for ( QString filename : parser.positionalArguments() )
        {
                glabels::model::Model *model = glabels::model::XmlLabelParser::readFile( filename );
                if ( model )
                {
                        auto *newWindow = new glabels::MainWindow();
                        newWindow->setModel( model );
                        windows << newWindow;

                        glabels::model::Settings::addToRecentFileList( model->fileName() );
                }
        }


        //
        // Launch main window
        //
        if ( windows.isEmpty() )
        {
                windows << new glabels::MainWindow();
        }

        splash.finishLoading();
        for ( auto* window : windows )
        {
                window->showMaximized();
        }
        splash.finish( windows.first() );


        return app.exec();
}
