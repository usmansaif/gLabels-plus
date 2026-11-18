//  Settings.hpp
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

#ifndef model_Settings_hpp
#define model_Settings_hpp


#include "Distance.hpp"
#include "Units.hpp"

#include <QListView>
#include <QSettings>
#include <QStringList>

#include <memory>


namespace glabels::model
{

        ///
        /// Settings Singleton Class
        ///
        class Settings : public QSettings
        {
                Q_OBJECT

        public:
                enum PageSizeFamily { ISO, US, };
                enum GridOrigin { ORIGIN_TL, ORIGIN_CENTER };
                enum ColorScheme { LIGHT_COLOR_SCHEME, DARK_COLOR_SCHEME, SYSTEM_COLOR_SCHEME };
                enum IconStyle { FLAT_ICON_STYLE, VECTOR_ICON_STYLE };
                enum SidebarStyle { SIDEBAR_ICONS_AND_TEXT, SIDEBAR_ICONS_ONLY, SIDEBAR_TEXT_ONLY };


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        private:
                Settings() = default;

        public:
                static void init();
                static Settings* instance();


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                void changed();


                /////////////////////////////////
                // Accessors
                /////////////////////////////////
        public:
                static Units units();
                static void setUnits( Units units );

                static PageSizeFamily preferedPageSizeFamily();
                static void setPreferedPageSizeFamily( PageSizeFamily preferedPageSizeFamily );

                static bool searchIsoPaperSizes();
                static void setSearchIsoPaperSizes( bool searchIsoPaperSizes );

                static bool searchUsPaperSizes();
                static void setSearchUsPaperSizes( bool searchUsPaperSizes );

                static bool searchOtherPaperSizes();
                static void setSearchOtherPaperSizes( bool searchOtherPaperSizes );

                static bool searchAllCategories();
                static void setSearchAllCategories( bool searchAllCategories );

                static QStringList searchCategoryList();
                static void setSearchCategoryList( const QStringList& searchCategoryList );

                static QListView::ViewMode templatePickerMode();
                static void setTemplatePickerMode( QListView::ViewMode viewMode );

                static QStringList recentTemplateList();
                static void addToRecentTemplateList( const QString& name );

                static int maxRecentFiles();
                static QStringList recentFileList();
                static void addToRecentFileList( const QString& filePath );
                static void removeFromRecentFileList( const QString& filePath );

                static QString recentPrinter();
                static void setRecentPrinter( const QString& printer );

                static GridOrigin gridOrigin();
                static void setGridOrigin( GridOrigin origin );

                static Distance gridSpacing();
                static void setGridSpacing( Distance spacing );
                static void resetGridSpacing();

                static ColorScheme colorScheme();
                static void setColorScheme( ColorScheme scheme );

                static IconStyle iconStyle();
                static void setIconStyle( IconStyle style );

                static QString startupPath();
                static void setStartupPath( const QString& path );

                // Editor view
                static bool showGrid();
                static void setShowGrid( bool value );

                static bool showMarkup();
                static void setShowMarkup( bool value );

                static bool showRulers();
                static void setShowRulers( bool value );

                static bool propertiesShowUnits();
                static void setPropertiesShowUnits( bool value );

                static int alignReference();
                static void setAlignReference( int value );

                // Window layout
                static SidebarStyle sidebarStyle();
                static void setSidebarStyle( SidebarStyle style );

                static int sidebarIconSize();
                static void setSidebarIconSize( int size );

                static Qt::ToolButtonStyle toolbarButtonStyle();
                static void setToolbarButtonStyle( Qt::ToolButtonStyle style );

                // Templates
                static QStringList favoriteTemplateList();
                static bool isFavoriteTemplate( const QString& name );
                static void setFavoriteTemplate( const QString& name, bool favorite );

                // Printing defaults
                static bool printOutlines();
                static void setPrintOutlines( bool value );

                static bool printCropMarks();
                static void setPrintCropMarks( bool value );

                // Reset all preferences to defaults
                static void resetToDefaults();

        private:
                static QVariant getValue( const QString& group, const QString& key, const QVariant& defaultValue );
                static void putValue( const QString& group, const QString& key, const QVariant& value );
        public:


        private:
                static std::unique_ptr<Settings> mInstance;
                static const int mMaxRecentFiles{5};

        };

}


#endif // model_Settings_hpp
