//  AboutDialog.cpp
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


#include "AboutDialog.hpp"

#include "DialogUtil.hpp"

#include "AppearanceModel.hpp"

#include "model/Version.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibraryInfo>
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>


namespace glabels
{

        namespace
        {
                const QString SITE_URL    = QStringLiteral( "https://usmansaif.github.io/gLabels-plus/" );
                const QString SOURCE_URL  = QStringLiteral( "https://github.com/usmansaif/gLabels-plus" );
                const QString DONATE_URL  = QStringLiteral( "https://usmansaif.github.io/gLabels-plus/donate.html" );
                const QString CUSTOM_URL  = QStringLiteral( "https://usmansaif.com/en/contact" );
                const QString LICENSE_URL = QStringLiteral( "https://www.gnu.org/licenses/gpl-3.0.html" );
                const QString ORIGINAL_URL = QStringLiteral( "https://glabels.org" );

                // Brand accent (same orange as the website); lighter on dark backgrounds
                const QColor ACCENT_LIGHT( "#e8590c" );
                const QColor ACCENT_DARK( "#ff8a4c" );


                ///
                /// A row that opens a link when clicked: title, detail and an "open" mark
                ///
                class LinkRow : public QFrame
                {
                public:
                        LinkRow( const QString& url, QWidget* parent = nullptr )
                                : QFrame( parent ), mUrl( url )
                        {
                                setObjectName( "abLink" );
                                setCursor( Qt::PointingHandCursor );
                                setFocusPolicy( Qt::StrongFocus );
                                setToolTip( url );
                                setAttribute( Qt::WA_Hover, true );
                        }

                protected:
                        void mouseReleaseEvent( QMouseEvent* event ) override
                        {
                                if ( event->button() == Qt::LeftButton && rect().contains( event->position().toPoint() ) )
                                {
                                        QDesktopServices::openUrl( QUrl( mUrl ) );
                                }
                                QFrame::mouseReleaseEvent( event );
                        }

                        void keyPressEvent( QKeyEvent* event ) override
                        {
                                if ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
                                     event->key() == Qt::Key_Space )
                                {
                                        QDesktopServices::openUrl( QUrl( mUrl ) );
                                        return;
                                }
                                QFrame::keyPressEvent( event );
                        }

                private:
                        QString mUrl;
                };


                QLabel* textLabel( const QString& text, const char* name = nullptr, bool wrap = true )
                {
                        auto* label = new QLabel( text );
                        if ( name ) label->setObjectName( name );
                        label->setWordWrap( wrap );
                        return label;
                }
        }


        ///
        /// Constructor
        ///
        AboutDialog::AboutDialog( QWidget *parent )
                : QDialog(parent)
        {
                setObjectName( "aboutDialog" );
                setWindowTitle( tr("About gLabels Plus") );
                setModal( true );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 0, 0, 0, 0 );
                root->setSpacing( 0 );

                root->addWidget( createHeader() );

                // Tab bar
                auto* tabBar = new QWidget();
                tabBar->setObjectName( "abTabBar" );
                auto* tabLayout = new QHBoxLayout( tabBar );
                tabLayout->setContentsMargins( 28, 0, 28, 0 );
                tabLayout->setSpacing( 4 );
                mTabs = new QButtonGroup( this );
                const QStringList names = { tr("About"), tr("Credits"), tr("System"), tr("License") };
                for ( int i = 0; i < names.size(); i++ )
                {
                        auto* tab = new QPushButton( names[i] );
                        tab->setObjectName( "abTab" );
                        tab->setCheckable( true );
                        tab->setAutoDefault( false );
                        tab->setCursor( Qt::PointingHandCursor );
                        mTabs->addButton( tab, i );
                        tabLayout->addWidget( tab );
                }
                tabLayout->addStretch( 1 );
                root->addWidget( tabBar );

                // Pages
                mPages = new QStackedWidget();
                mPages->setObjectName( "abPages" );
                mPages->addWidget( createPage( createAboutPage() ) );
                mPages->addWidget( createPage( createCreditsPage() ) );
                mPages->addWidget( createPage( createSystemPage() ) );
                mPages->addWidget( createPage( createLicensePage() ) );
                root->addWidget( mPages, 1 );

                connect( mTabs, &QButtonGroup::idClicked, mPages, &QStackedWidget::setCurrentIndex );
                mTabs->button( 0 )->setChecked( true );

                // Footer
                auto* footer = new QWidget();
                footer->setObjectName( "abFooter" );
                auto* footerLayout = new QHBoxLayout( footer );
                footerLayout->setContentsMargins( 28, 12, 28, 14 );
                footerLayout->setSpacing( 8 );

                auto* note = textLabel( tr("Free and open source · GPL 3.0 · Works offline"), "abMuted", false );
                footerLayout->addWidget( note );
                footerLayout->addStretch( 1 );

                auto* closeButton = new QPushButton( tr("Close") );
                closeButton->setDefault( true );
                connect( closeButton, &QPushButton::clicked, this, &QDialog::accept );
                footerLayout->addWidget( closeButton );
                root->addWidget( footer );

                applyStyle();
                DialogUtil::makeResizableWindow( this );
                resize( 660, 740 );
                closeButton->setFocus();
        }


        ///
        /// Header: logo, wordmark, version with copy, tagline and facts
        ///
        QWidget* AboutDialog::createHeader()
        {
                auto* header = new QWidget();
                header->setObjectName( "abHeader" );
                auto* layout = new QHBoxLayout( header );
                layout->setContentsMargins( 28, 26, 28, 22 );
                layout->setSpacing( 22 );

                auto* logo = new QLabel();
                qreal dpr = devicePixelRatioF();
                QPixmap pixmap = QPixmap( ":/images/glabels-logo.png" )
                        .scaled( int( 88 * dpr ), int( 88 * dpr ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
                pixmap.setDevicePixelRatio( dpr );
                logo->setPixmap( pixmap );
                layout->addWidget( logo, 0, Qt::AlignTop );

                auto* text = new QVBoxLayout();
                text->setSpacing( 8 );

                mWordmark = new QLabel();
                mWordmark->setAccessibleName( "gLabels Plus" );
                text->addWidget( mWordmark );

                auto* tagline = textLabel( tr("Design and print labels, barcodes and business cards "
                                              "for sheet and thermal roll printers."), "abTagline" );
                text->addWidget( tagline );

                // Version pill with copy
                auto* versionRow = new QHBoxLayout();
                versionRow->setSpacing( 8 );
                auto* version = textLabel( tr("Version %1").arg( model::Version::LONG_STRING ), "abVersion", false );
                version->setTextInteractionFlags( Qt::TextSelectableByMouse );
                versionRow->addWidget( version );
                auto* copy = new QPushButton( tr("Copy") );
                copy->setObjectName( "abSmallButton" );
                copy->setAutoDefault( false );
                copy->setCursor( Qt::PointingHandCursor );
                copy->setToolTip( tr("Copy the version and system information, for example for a bug report") );
                connect( copy, &QPushButton::clicked, this, &AboutDialog::copySystemInfo );
                versionRow->addWidget( copy );
                mCopiedLabel = textLabel( tr("Copied"), "abAccentText", false );
                mCopiedLabel->hide();
                versionRow->addWidget( mCopiedLabel );
                versionRow->addStretch( 1 );
                text->addLayout( versionRow );

                // Facts
                auto* facts = new QHBoxLayout();
                facts->setSpacing( 6 );
                for ( const auto& fact : { tr("1,800+ templates"), tr("70+ brands"), tr("Mail merge"),
                                           tr("Thermal rolls") } )
                {
                        facts->addWidget( textLabel( fact, "abChip", false ) );
                }
                facts->addStretch( 1 );
                text->addSpacing( 2 );
                text->addLayout( facts );

                layout->addLayout( text, 1 );
                return header;
        }


        ///
        /// About: what it is and where to go
        ///
        QWidget* AboutDialog::createAboutPage()
        {
                auto* page = new QWidget();
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 8 );

                layout->addWidget( textLabel(
                        tr("gLabels Plus is a label and business card designer. Create barcode, address, "
                           "shipping and product labels, name badges, CD/DVD labels and business cards, "
                           "fill them from a spreadsheet with mail merge, and print on sheets from more "
                           "than 70 brands or on thermal label rolls."), "abBodyText" ) );

                layout->addSpacing( 8 );
                layout->addWidget( createSection( tr("Links") ) );
                layout->addWidget( createLink( tr("Website"),
                                               tr("Guides, features and downloads"), SITE_URL ) );
                layout->addWidget( createLink( tr("Source code"),
                                               tr("gLabels Plus on GitHub"), SOURCE_URL ) );
                layout->addWidget( createLink( tr("Report a problem"),
                                               tr("Open an issue on GitHub. Click Copy above to include your system details."),
                                               model::Version::BUG_WEBSITE ) );
                layout->addWidget( createLink( tr("Support the project"),
                                               tr("Donate or sponsor a feature"), DONATE_URL ) );
                layout->addWidget( createLink( tr("Customization service"),
                                               tr("Custom label designs, printing from your inventory, POS or ERP data, "
                                                  "and thermal printer setup"), CUSTOM_URL ) );
                layout->addStretch( 1 );
                return page;
        }


        ///
        /// Credits: people and what it is built on
        ///
        QWidget* AboutDialog::createCreditsPage()
        {
                auto* page = new QWidget();
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 8 );

                layout->addWidget( createSection( tr("People") ) );
                layout->addWidget( createPerson( "Usman Saif", tr("Maintainer"),
                                                 tr("gLabels Plus"),
                                                 "usman.saif22@gmail.com", "usmansaif.com" ) );
                layout->addWidget( createPerson( "Jaye Evins", tr("Original author"),
                                                 tr("gLabels, which gLabels Plus is built on"),
                                                 "evins@snaught.com", ORIGINAL_URL ) );

                layout->addSpacing( 8 );
                layout->addWidget( createSection( tr("Built with") ) );
                layout->addWidget( createLink( tr("Qt %1").arg( QLibraryInfo::version().toString() ),
                                               tr("Cross-platform application framework"), "https://www.qt.io" ) );
                layout->addWidget( createLink( tr("glbarcode++ and libqrencode"),
                                               tr("Barcode and QR code generation"),
                                               "https://fukuchi.org/works/qrencode/" ) );

                layout->addSpacing( 8 );
                layout->addWidget( textLabel( tr("Thanks to everyone who has contributed code, templates, "
                                                 "translations and bug reports to gLabels and gLabels Plus."),
                                              "abMuted" ) );
                layout->addStretch( 1 );
                return page;
        }


        ///
        /// System: details for bug reports, with copy
        ///
        QWidget* AboutDialog::createSystemPage()
        {
                auto* page = new QWidget();
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 8 );

                layout->addWidget( createSection( tr("This installation") ) );

                auto* card = new QFrame();
                card->setObjectName( "abCard" );
                auto* form = new QFormLayout( card );
                form->setContentsMargins( 16, 12, 16, 12 );
                form->setHorizontalSpacing( 24 );
                form->setVerticalSpacing( 8 );
                form->setLabelAlignment( Qt::AlignLeft );
                for ( const auto& row : systemInfo() )
                {
                        auto* key = textLabel( row.first, "abMuted", false );
                        auto* value = textLabel( row.second, "abValue" );
                        value->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        form->addRow( key, value );
                }
                layout->addWidget( card );

                auto* row = new QHBoxLayout();
                auto* copy = new QPushButton( tr("Copy System Info") );
                copy->setAutoDefault( false );
                copy->setToolTip( tr("Copy these details to paste into a bug report") );
                connect( copy, &QPushButton::clicked, this, &AboutDialog::copySystemInfo );
                row->addWidget( copy );
                row->addStretch( 1 );
                layout->addLayout( row );

                layout->addStretch( 1 );
                return page;
        }


        ///
        /// License
        ///
        QWidget* AboutDialog::createLicensePage()
        {
                auto* page = new QWidget();
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 8 );

                layout->addWidget( createSection( tr("Copyright") ) );
                auto* copyright = textLabel( "Copyright © 2016-2026 Jaye Evins &lt;evins@snaught.com&gt;<br>"
                                             "Copyright © 2026 gLabels Plus contributors (Usman Saif)",
                                             "abBodyText" );
                copyright->setTextFormat( Qt::RichText );
                copyright->setTextInteractionFlags( Qt::TextSelectableByMouse );
                layout->addWidget( copyright );

                layout->addSpacing( 8 );
                layout->addWidget( createSection( tr("GNU General Public License 3.0") ) );
                auto* license = textLabel(
                        "<p>" + tr( "gLabels Plus is free software: you can redistribute it and/or modify "
                                    "it under the terms of the GNU General Public License as published by "
                                    "the Free Software Foundation, either version 3 of the License, or "
                                    "(at your option) any later version." ) + "</p>"
                        "<p>" + tr( "gLabels Plus is distributed in the hope that it will be useful, "
                                    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                                    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
                                    "GNU General Public License for more details." ) + "</p>",
                        "abBodyText" );
                license->setTextFormat( Qt::RichText );
                license->setTextInteractionFlags( Qt::TextSelectableByMouse );
                layout->addWidget( license );

                layout->addWidget( createLink( tr("Read the full license"),
                                               tr("GNU General Public License, version 3"), LICENSE_URL ) );
                layout->addStretch( 1 );
                return page;
        }


        ///
        /// Building blocks
        ///
        QWidget* AboutDialog::createPage( QWidget* content )
        {
                auto* inner = new QWidget();
                inner->setObjectName( "abPage" );
                auto* layout = new QVBoxLayout( inner );
                layout->setContentsMargins( 28, 20, 28, 20 );
                layout->addWidget( content );

                auto* scroll = new QScrollArea();
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidgetResizable( true );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                scroll->setWidget( inner );
                return scroll;
        }


        QWidget* AboutDialog::createSection( const QString& title )
        {
                return textLabel( title.toUpper(), "abSection", false );
        }


        QWidget* AboutDialog::createLink( const QString& title, const QString& detail, const QString& url )
        {
                auto* row = new LinkRow( url );
                auto* layout = new QHBoxLayout( row );
                layout->setContentsMargins( 14, 8, 14, 8 );
                layout->setSpacing( 12 );

                auto* text = new QVBoxLayout();
                text->setSpacing( 1 );
                text->addWidget( textLabel( title, "abLinkTitle", false ) );
                text->addWidget( textLabel( detail, "abMuted" ) );
                layout->addLayout( text, 1 );

                auto* open = textLabel( tr("Open"), "abAccentText", false );
                layout->addWidget( open, 0, Qt::AlignVCenter );

                row->setAccessibleName( title );
                row->setAccessibleDescription( detail );
                return row;
        }


        QWidget* AboutDialog::createPerson( const QString& name, const QString& role, const QString& note,
                                            const QString& email, const QString& website )
        {
                auto* card = new QFrame();
                card->setObjectName( "abCard" );
                auto* layout = new QHBoxLayout( card );
                layout->setContentsMargins( 14, 12, 14, 12 );
                layout->setSpacing( 14 );

                QString initials;
                for ( const auto& part : name.split( ' ', Qt::SkipEmptyParts ) )
                {
                        initials += part.at( 0 ).toUpper();
                }
                auto* avatar = textLabel( initials.left( 2 ), "abAvatar", false );
                avatar->setAlignment( Qt::AlignCenter );
                avatar->setFixedSize( 44, 44 );
                layout->addWidget( avatar, 0, Qt::AlignTop );

                auto* text = new QVBoxLayout();
                text->setSpacing( 3 );

                auto* nameRow = new QHBoxLayout();
                nameRow->setSpacing( 8 );
                nameRow->addWidget( textLabel( name, "abLinkTitle", false ) );
                nameRow->addWidget( textLabel( role, "abRole", false ) );
                nameRow->addStretch( 1 );
                text->addLayout( nameRow );
                text->addWidget( textLabel( note, "abMuted" ) );

                QStringList links;
                links << QString( "<a href='mailto:%1' style='color:ACCENT'>%1</a>" ).arg( email.toHtmlEscaped() );
                if ( !website.isEmpty() )
                {
                        QString url = website.startsWith( "http" ) ? website : "https://" + website;
                        QString shown = QString( website ).remove( "https://" ).remove( "http://" );
                        links << QString( "<a href='%1' style='color:ACCENT'>%2</a>" ).arg( url.toHtmlEscaped(), shown.toHtmlEscaped() );
                }
                auto* contact = textLabel( QString(), "abContact", false );
                contact->setProperty( "html", links.join( " &nbsp;·&nbsp; " ) );
                contact->setTextFormat( Qt::RichText );
                contact->setOpenExternalLinks( true );
                contact->setTextInteractionFlags( Qt::TextBrowserInteraction );
                mContactLabels << contact;
                text->addWidget( contact );

                layout->addLayout( text, 1 );
                return card;
        }


        ///
        /// System information
        ///
        QList<QPair<QString,QString>> AboutDialog::systemInfo() const
        {
                QList<QPair<QString,QString>> rows;
                rows << qMakePair( tr("Version"),  QString( "gLabels Plus %1" ).arg( model::Version::LONG_STRING ) );
                rows << qMakePair( tr("System"),   QSysInfo::prettyProductName() );
                rows << qMakePair( tr("Kernel"),   QSysInfo::kernelType() + " " + QSysInfo::kernelVersion() );
                rows << qMakePair( tr("Processor"), QSysInfo::currentCpuArchitecture() );
                rows << qMakePair( tr("Qt"),       QLibraryInfo::version().toString() );
                rows << qMakePair( tr("Display"),  QGuiApplication::platformName() );
                rows << qMakePair( tr("Language"), QLocale::system().name() );
                return rows;
        }


        QString AboutDialog::systemInfoText() const
        {
                QStringList lines;
                for ( const auto& row : systemInfo() )
                {
                        lines << row.first + ": " + row.second;
                }
                return lines.join( "\n" );
        }


        void AboutDialog::copySystemInfo()
        {
                QApplication::clipboard()->setText( systemInfoText() );
                mCopiedLabel->show();
                QTimer::singleShot( 2000, mCopiedLabel, &QWidget::hide );
        }


        ///
        /// Palette-based styling (follows light/dark appearance) with the brand accent
        ///
        void AboutDialog::applyStyle()
        {
                if ( mStyling )
                {
                        return;
                }
                mStyling = true;

                bool dark = AppearanceModel::style() == AppearanceModel::STYLE_DARK ||
                            palette().color( QPalette::Window ).lightness() < 128;
                qreal dpr = devicePixelRatioF();
                QPixmap wordmark( dark ? ":images/glabels-label-designer-dark.png"
                                       : ":images/glabels-label-designer.png" );
                if ( wordmark.height() > 0 )
                {
                        wordmark = wordmark.scaledToHeight( int( 34 * dpr ), Qt::SmoothTransformation );
                        wordmark.setDevicePixelRatio( dpr );
                }
                mWordmark->setPixmap( wordmark );

                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted( (fg.red()*3 + bg.red()*2) / 5, (fg.green()*3 + bg.green()*2) / 5,
                              (fg.blue()*3 + bg.blue()*2) / 5 );
                QColor accent = dark ? ACCENT_DARK : ACCENT_LIGHT;
                QColor accentSoft = accent;
                accentSoft.setAlpha( dark ? 46 : 28 );
                QString soft = QString( "rgba(%1,%2,%3,%4)" ).arg( accentSoft.red() ).arg( accentSoft.green() )
                                                              .arg( accentSoft.blue() ).arg( accentSoft.alpha() );

                // Links in the accent color
                for ( auto* label : mContactLabels )
                {
                        label->setText( label->property( "html" ).toString().replace( "ACCENT", accent.name() ) );
                }

                setStyleSheet( QString(
                        "#abHeader { background: palette(base); }"
                        "#abTagline { font-size: 11pt; }"
                        "#abVersion { padding: 2px 10px; border: 1px solid palette(mid); border-radius: 10px;"
                        "  background: palette(window); }"
                        "#abSmallButton { padding: 2px 10px; border: 1px solid palette(mid); border-radius: 10px;"
                        "  background: palette(base); }"
                        "#abSmallButton:hover { border-color: %1; color: %1; }"
                        "#abChip { padding: 3px 10px; border-radius: 10px; background: %3; color: %1;"
                        "  font-weight: 600; }"
                        "#abTabBar { background: palette(base); border-bottom: 1px solid palette(mid); }"
                        "#abTab { border: none; border-bottom: 2px solid transparent; padding: 10px 12px 8px 12px;"
                        "  background: transparent; color: %2; font-weight: 600; border-radius: 0; }"
                        "#abTab:hover { color: palette(window-text); }"
                        "#abTab:checked { color: palette(window-text); border-bottom: 2px solid %1; }"
                        "#abPages, #abPage, QScrollArea { background: palette(window); }"
                        "#abSection { color: %2; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }"
                        "#abBodyText { font-size: 10.5pt; }"
                        "#abMuted { color: %2; }"
                        "#abValue { font-weight: 600; }"
                        "#abAccentText { color: %1; font-weight: 600; }"
                        "#abLinkTitle { font-weight: 700; }"
                        "#abRole { color: %1; font-weight: 600; }"
                        "#abCard, #abLink { background: palette(base); border: 1px solid palette(mid);"
                        "  border-radius: 8px; }"
                        "#abLink:hover, #abLink:focus { border-color: %1; }"
                        "#abAvatar { background: %1; color: white; border-radius: 22px; font-weight: 700;"
                        "  font-size: 12pt; }"
                        "#abFooter { background: palette(window); border-top: 1px solid palette(mid); }"
                ).arg( accent.name(), muted.name(), soft ) );

                mStyling = false;
        }


        void AboutDialog::changeEvent( QEvent* event )
        {
                QDialog::changeEvent( event );
                if ( event->type() == QEvent::PaletteChange )
                {
                        applyStyle();
                }
        }

} // namespace glabels
