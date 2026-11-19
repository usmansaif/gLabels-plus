//  Settings.cpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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


#include "Settings.hpp"

#include "Version.hpp"

#include <QDebug>
#include <QLocale>
#include <QPrinterInfo>
#include <QString>


namespace glabels::model
{

        //
        // Static data
        //
        std::unique_ptr<Settings> Settings::mInstance;



        void Settings::init()
        {
                // Note: init() hould be called after
                //   - QCoreApplication::setOrganizationName(), and
                //   - QCoreApplication::setApplicationName()

                if ( !mInstance )
                {
                        mInstance.reset( new Settings() );
                }
        }


        Settings* Settings::instance()
        {
                init();
                return mInstance.get();
        }


        Units Settings::units()
        {
                // Guess at a suitable default
                QString defaultIdString;
                if ( QLocale::system().measurementSystem() == QLocale::ImperialSystem )
                {
                        defaultIdString = Units(Units::IN).toIdString();
                }
                else
                {
                        defaultIdString = Units(Units::MM).toIdString();
                }

                mInstance->beginGroup( "Locale" );
                QString idString = mInstance->value( "units", defaultIdString ).toString();
                mInstance->endGroup();

                return Units( idString );
        }


        void Settings::setUnits( Units units )
        {
                QString idString = units.toIdString();

                mInstance->beginGroup( "Locale" );
                mInstance->setValue( "units", idString );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        Settings::PageSizeFamily Settings::preferedPageSizeFamily()
        {
                // Guess at a suitable default
                QString defaultFamily;
                switch (QLocale::system().territory())
                {
                case QLocale::UnitedStates:
                case QLocale::Canada:
                        defaultFamily = "us";
                        break;

                default:
                        defaultFamily = "iso";
                        break;
                }

                mInstance->beginGroup( "Locale" );
                QString value = mInstance->value( "preferedPageSizeFamily", defaultFamily ).toString();
                mInstance->endGroup();

                return (value == "iso") ? ISO : US;
        }


        void Settings::setPreferedPageSizeFamily( PageSizeFamily preferedPageSizeFamily )
        {
                mInstance->beginGroup( "Locale" );
                mInstance->setValue( "preferedPageSizeFamily", preferedPageSizeFamily == ISO ? "iso" : "us" );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        bool Settings::searchIsoPaperSizes()
        {
                // Guess at a suitable default
                bool defaultValue;
                switch (QLocale::system().territory())
                {
                case QLocale::UnitedStates:
                case QLocale::Canada:
                        defaultValue = false;
                        break;

                default:
                        defaultValue = true;
                        break;
                }

                mInstance->beginGroup( "Search" );
                bool returnValue = mInstance->value( "isoPaperSizes", defaultValue ).toBool();
                mInstance->endGroup();

                return returnValue;
        }


        void Settings::setSearchIsoPaperSizes( bool searchIsoPaperSizes )
        {
                mInstance->beginGroup( "Search" );
                mInstance->setValue( "isoPaperSizes", searchIsoPaperSizes );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        bool Settings::searchUsPaperSizes()
        {
                // Guess at a suitable default
                bool defaultValue;
                switch (QLocale::system().territory())
                {
                case QLocale::UnitedStates:
                case QLocale::Canada:
                        defaultValue = true;
                        break;

                default:
                        defaultValue = false;
                        break;
                }

                mInstance->beginGroup( "Search" );
                bool returnValue = mInstance->value( "usPaperSizes", defaultValue ).toBool();
                mInstance->endGroup();

                return returnValue;
        }


        void Settings::setSearchUsPaperSizes( bool searchUsPaperSizes )
        {
                mInstance->beginGroup( "Search" );
                mInstance->setValue( "usPaperSizes", searchUsPaperSizes );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        bool Settings::searchOtherPaperSizes()
        {
                // Guess at a suitable default
                bool defaultValue = true;

                mInstance->beginGroup( "Search" );
                bool returnValue = mInstance->value( "otherPaperSizes", defaultValue ).toBool();
                mInstance->endGroup();

                return returnValue;
        }


        void Settings::setSearchOtherPaperSizes( bool searchOtherPaperSizes )
        {
                mInstance->beginGroup( "Search" );
                mInstance->setValue( "otherPaperSizes", searchOtherPaperSizes );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        bool Settings::searchAllCategories()
        {
                // Guess at a suitable default
                bool defaultValue = true;

                mInstance->beginGroup( "Search" );
                bool returnValue = mInstance->value( "allCategories", defaultValue ).toBool();
                mInstance->endGroup();

                return returnValue;
        }


        void Settings::setSearchAllCategories( bool searchAllCategories )
        {
                mInstance->beginGroup( "Search" );
                mInstance->setValue( "allCategories", searchAllCategories );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        QStringList Settings::searchCategoryList()
        {
                QStringList defaultList;

                mInstance->beginGroup( "Search" );
                QStringList returnList = mInstance->value( "categoryList", defaultList ).toStringList();
                mInstance->endGroup();

                return returnList;
        }


        void Settings::setSearchCategoryList( const QStringList& searchCategoryList )
        {
                mInstance->beginGroup( "Search" );
                mInstance->setValue( "categoryList", searchCategoryList );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        QListView::ViewMode Settings::templatePickerMode()
        {
                QString defaultMode = "icon";

                mInstance->beginGroup( "TemplatePicker" );
                QString returnMode = mInstance->value( "viewMode", defaultMode ).toString();
                mInstance->endGroup();

                return returnMode == "icon" ? QListView::IconMode : QListView::ListMode;
        }


        void Settings::setTemplatePickerMode( QListView::ViewMode viewMode )
        {
                mInstance->beginGroup( "TemplatePicker" );
                mInstance->setValue( "viewMode", viewMode == QListView::IconMode ? "icon" : "list" );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        QStringList Settings::recentTemplateList()
        {
                QStringList defaultList;

                mInstance->beginGroup( "Recent" );
                QStringList returnList = mInstance->value( "templates", defaultList ).toStringList();
                mInstance->endGroup();

                return returnList;
        }


        void Settings::addToRecentTemplateList( const QString& name )
        {
                mInstance->beginGroup( "Recent" );

                QStringList list = mInstance->value( "templates" ).toStringList();
                if ( !list.isEmpty() && list.first() == name )
                {
                        mInstance->endGroup();
                        return;   // already the most recent: nothing changes
                }

                list.removeAll( name );
                list.prepend( name );
                while ( list.count() > 10 )
                {
                        list.removeLast();
                }

                mInstance->setValue( "templates", list );

                mInstance->endGroup();

                emit mInstance->changed();
        }


        int Settings::maxRecentFiles()
        {
                return mMaxRecentFiles;
        }


        QStringList Settings::recentFileList()
        {
                QStringList defaultList;

                mInstance->beginGroup( "Recent" );
                QStringList returnList = mInstance->value( "files", defaultList ).toStringList();
                mInstance->endGroup();

                return returnList;
        }


        void Settings::addToRecentFileList( const QString& filePath )
        {
                mInstance->beginGroup( "Recent" );

                QStringList list = mInstance->value( "files" ).toStringList();

                list.removeAll( filePath );
                list.prepend( filePath );
                while ( list.count() > mMaxRecentFiles )
                {
                        list.removeLast();
                }

                mInstance->setValue( "files", list );

                mInstance->endGroup();

                emit mInstance->changed();
        }


        void Settings::removeFromRecentFileList( const QString& filePath )
        {
                mInstance->beginGroup( "Recent" );

                QStringList list = mInstance->value( "files" ).toStringList();
                list.removeAll( filePath );
                mInstance->setValue( "files", list );

                mInstance->endGroup();

                emit mInstance->changed();
        }


        QString Settings::recentPrinter()
        {
                mInstance->beginGroup( "Recent" );
                QString printer = mInstance->value( "printer", QPrinterInfo::defaultPrinterName() ).toString();
                mInstance->endGroup();

                return printer;
        }


        void Settings::setRecentPrinter( const QString& printer )
        {
                mInstance->beginGroup( "Recent" );
                mInstance->setValue( "printer", printer );
                mInstance->endGroup();
        }


        Settings::GridOrigin Settings::gridOrigin()
        {
                mInstance->beginGroup( "Grid" );
                QString value = mInstance->value( "origin", "top_left" ).toString();
                mInstance->endGroup();

                return (value == "top_left") ? ORIGIN_TL : ORIGIN_CENTER;
        }


        void Settings::setGridOrigin( GridOrigin origin )
        {
                mInstance->beginGroup( "Grid" );
                mInstance->setValue( "origin", origin == ORIGIN_TL ? "top_left" : "center" );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        Distance Settings::gridSpacing()
        {
                // Guess at a suitable default
                QString defaultSpacingString;
                if ( QLocale::system().measurementSystem() == QLocale::ImperialSystem )
                {
                        defaultSpacingString = Distance::in(0.125).toString( Units::IN );
                }
                else
                {
                        defaultSpacingString = Distance::mm(5).toString( Units::MM );
                }

                mInstance->beginGroup( "Grid" );
                QString spacingString = mInstance->value( "spacing", defaultSpacingString ).toString();
                mInstance->endGroup();

                return Distance::fromString( spacingString );
        }


        void Settings::setGridSpacing( Distance spacing )
        {
                QString spacingString = spacing.toString( Settings::units() );

                mInstance->beginGroup( "Grid" );
                mInstance->setValue( "spacing", spacingString );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        void Settings::resetGridSpacing()
        {
                mInstance->beginGroup( "Grid" );
                mInstance->remove( "spacing" );
                mInstance->endGroup();

                emit mInstance->changed();
        }


        Settings::ColorScheme Settings::colorScheme()
        {
                mInstance->beginGroup( "Appearance" );
                QString value = mInstance->value( "colorScheme", "light" ).toString();
                mInstance->endGroup();

                if ( value == "light" ) return LIGHT_COLOR_SCHEME;
                if ( value == "dark" ) return DARK_COLOR_SCHEME;
                if ( value == "system" ) return SYSTEM_COLOR_SCHEME;
                return LIGHT_COLOR_SCHEME;
        }


        void Settings::setColorScheme( ColorScheme mode )
        {

                mInstance->beginGroup( "Appearance" );
                switch ( mode )
                {
                case LIGHT_COLOR_SCHEME:
                        mInstance->setValue( "colorScheme", "light" );
                        break;
                case DARK_COLOR_SCHEME:
                        mInstance->setValue( "colorScheme", "dark" );
                        break;
                case SYSTEM_COLOR_SCHEME:
                        mInstance->setValue( "colorScheme", "system" );
                        break;
                default:
                        mInstance->setValue( "colorScheme", "light" );
                        break;
                }
                mInstance->endGroup();

                emit mInstance->changed();
        }


        Settings::IconStyle Settings::iconStyle()
        {
                mInstance->beginGroup( "Appearance" );
                QString value = mInstance->value( "iconStyle", "flat" ).toString();
                mInstance->endGroup();

                if ( value == "flat" ) return FLAT_ICON_STYLE;
                if ( value == "vector" ) return VECTOR_ICON_STYLE;
                return FLAT_ICON_STYLE;
        }


        void Settings::setIconStyle( IconStyle style )
        {

                mInstance->beginGroup( "Appearance" );
                switch ( style )
                {
                case FLAT_ICON_STYLE:
                        mInstance->setValue( "iconStyle", "flat" );
                        break;
                case VECTOR_ICON_STYLE:
                        mInstance->setValue( "iconStyle", "vector" );
                        break;
                default:
                        mInstance->setValue( "iconStyle", "flat" );
                        break;
                }
                mInstance->endGroup();

                emit mInstance->changed();
        }


        QString Settings::startupPath()
        {
                mInstance->beginGroup( "Startup" );
                QString path = mInstance->value( "path", QString("") ).toString();
                mInstance->endGroup();

                return path;
        }


        void Settings::setStartupPath( const QString& path )
        {
                mInstance->beginGroup( "Startup" );
                mInstance->setValue( "path", path );
                mInstance->endGroup();
        }



        //
        // Generic helpers
        //
        QVariant Settings::getValue( const QString& group, const QString& key, const QVariant& defaultValue )
        {
                mInstance->beginGroup( group );
                QVariant v = mInstance->value( key, defaultValue );
                mInstance->endGroup();
                return v;
        }


        void Settings::putValue( const QString& group, const QString& key, const QVariant& value )
        {
                mInstance->beginGroup( group );
                bool changedFlag = mInstance->value( key ) != value;
                mInstance->QSettings::setValue( key, value );
                mInstance->endGroup();

                if ( changedFlag )
                {
                        emit mInstance->changed();
                }
        }


        bool Settings::showGrid() { return getValue( "Editor", "showGrid", true ).toBool(); }
        void Settings::setShowGrid( bool value ) { putValue( "Editor", "showGrid", value ); }

        bool Settings::showMarkup() { return getValue( "Editor", "showMarkup", true ).toBool(); }
        void Settings::setShowMarkup( bool value ) { putValue( "Editor", "showMarkup", value ); }

        bool Settings::showRulers() { return getValue( "Editor", "showRulers", true ).toBool(); }
        void Settings::setShowRulers( bool value ) { putValue( "Editor", "showRulers", value ); }

        bool Settings::propertiesShowUnits() { return getValue( "Editor", "propertiesShowUnits", true ).toBool(); }
        void Settings::setPropertiesShowUnits( bool value ) { putValue( "Editor", "propertiesShowUnits", value ); }

        int Settings::alignReference() { return getValue( "Editor", "alignReference", 0 ).toInt(); }
        void Settings::setAlignReference( int value ) { putValue( "Editor", "alignReference", value ); }


        Settings::SidebarStyle Settings::sidebarStyle()
        {
                int v = getValue( "Window", "sidebarStyle", int(SIDEBAR_ICONS_AND_TEXT) ).toInt();
                if ( v < SIDEBAR_ICONS_AND_TEXT || v > SIDEBAR_TEXT_ONLY )
                {
                        v = SIDEBAR_ICONS_AND_TEXT;
                }
                return SidebarStyle( v );
        }
        void Settings::setSidebarStyle( SidebarStyle style ) { putValue( "Window", "sidebarStyle", int(style) ); }

        int Settings::sidebarIconSize() { return qBound( 16, getValue( "Window", "sidebarIconSize", 32 ).toInt(), 64 ); }
        void Settings::setSidebarIconSize( int size ) { putValue( "Window", "sidebarIconSize", qBound( 16, size, 64 ) ); }

        Qt::ToolButtonStyle Settings::toolbarButtonStyle()
        {
                int v = getValue( "Window", "toolbarButtonStyle", int(Qt::ToolButtonIconOnly) ).toInt();
                if ( v < Qt::ToolButtonIconOnly || v > Qt::ToolButtonTextUnderIcon )
                {
                        v = Qt::ToolButtonIconOnly;
                }
                return Qt::ToolButtonStyle( v );
        }
        void Settings::setToolbarButtonStyle( Qt::ToolButtonStyle style ) { putValue( "Window", "toolbarButtonStyle", int(style) ); }


        QStringList Settings::favoriteTemplateList()
        {
                return getValue( "Templates", "favorites", QStringList() ).toStringList();
        }

        bool Settings::isFavoriteTemplate( const QString& name )
        {
                return favoriteTemplateList().contains( name );
        }

        void Settings::setFavoriteTemplate( const QString& name, bool favorite )
        {
                QStringList list = favoriteTemplateList();
                list.removeAll( name );
                if ( favorite )
                {
                        list.append( name );
                }
                putValue( "Templates", "favorites", list );
        }


        bool Settings::printOutlines() { return getValue( "Print", "outlines", false ).toBool(); }
        void Settings::setPrintOutlines( bool value ) { putValue( "Print", "outlines", value ); }

        bool Settings::printCropMarks() { return getValue( "Print", "cropMarks", false ).toBool(); }
        void Settings::setPrintCropMarks( bool value ) { putValue( "Print", "cropMarks", value ); }


        void Settings::resetToDefaults()
        {
                // Preferences groups only -- keep recent files, favorites, window state etc.
                for ( const char* group : { "Locale", "Editor", "Window", "Print", "Appearance", "Startup", "Grid" } )
                {
                        mInstance->beginGroup( group );
                        mInstance->remove( "" );
                        mInstance->endGroup();
                }

                emit mInstance->changed();
        }

}
