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
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>


namespace glabels
{

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

                //
                // Header: logo, product name, version, tagline
                //
                auto* header = new QWidget();
                header->setObjectName( "abHeader" );
                auto* headerLayout = new QHBoxLayout( header );
                headerLayout->setContentsMargins( 24, 20, 24, 20 );
                headerLayout->setSpacing( 18 );

                auto* icon = new QLabel();
                icon->setPixmap( QPixmap( ":/images/glabels-logo.png" )
                                 .scaled( 96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
                icon->setAlignment( Qt::AlignTop );
                headerLayout->addWidget( icon, 0, Qt::AlignVCenter );

                auto* headerText = new QVBoxLayout();
                headerText->setSpacing( 6 );
                mTitleLabel = new QLabel();
                headerText->addWidget( mTitleLabel );

                QString description = tr("A professional label and business card designer. "
                                         "Create barcode, address, shipping and product labels, "
                                         "name badges, CD/DVD labels and business cards, with "
                                         "mail merge and over 1,800 ready-made templates from "
                                         "70+ brands, for sheet and thermal roll printers.");
                auto* tagline = new QLabel( description );
                tagline->setObjectName( "abTagline" );
                tagline->setWordWrap( true );
                headerText->addWidget( tagline );

                QString version = tr("Version") + " " + model::Version::LONG_STRING;
                auto* versionRow = new QHBoxLayout();
                versionRow->setSpacing( 8 );
                auto* versionPill = new QLabel( version );
                versionPill->setObjectName( "abVersion" );
                versionPill->setTextInteractionFlags( Qt::TextSelectableByMouse );
                versionRow->addWidget( versionPill );
                auto* copyVersion = new QLabel( QString( "<a href='copy'>%1</a>" ).arg( tr("Copy") ) );
                copyVersion->setToolTip( tr("Copy version information to the clipboard") );
                connect( copyVersion, &QLabel::linkActivated, this, [version]{
                        QApplication::clipboard()->setText( "gLabels Plus " + version );
                } );
                versionRow->addWidget( copyVersion );
                versionRow->addStretch( 1 );
                headerText->addLayout( versionRow );

                headerLayout->addLayout( headerText, 1 );
                root->addWidget( header );

                //
                // Body: cards in a scroll area
                //
                auto* scroll = new QScrollArea();
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setWidgetResizable( true );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

                auto* body = new QWidget();
                body->setObjectName( "abBody" );
                auto* bodyLayout = new QVBoxLayout( body );
                bodyLayout->setContentsMargins( 24, 18, 24, 18 );
                bodyLayout->setSpacing( 14 );

                // Credits
                auto* credits = new QWidget();
                auto* creditsLayout = new QVBoxLayout( credits );
                creditsLayout->setContentsMargins( 0, 0, 0, 0 );
                creditsLayout->setSpacing( 12 );
                creditsLayout->addWidget( createPerson( tr("Author"), "Jaye Evins", "evins@snaught.com" ) );
                creditsLayout->addWidget( createPerson( tr("Maintainer"), "Usman Saif",
                                                        "usman.saif22@gmail.com", "usmansaif.com" ) );
                bodyLayout->addWidget( createCard( tr("Credits"), credits ) );

                // License (unchanged text)
                QString copyright = "Copyright &copy; 2016-2026 Jaye Evins &lt;evins@snaught.com&gt;";

                QString licenseParagraph1 =
                        tr( "gLabels Plus is free software: you can redistribute it and/or modify "
                            "it under the terms of the GNU General Public License as published by "
                            "the Free Software Foundation, either version 3 of the License, or "
                            "(at your option) any later version." );

                QString licenseParagraph2 =
                        tr( "gLabels Plus is distributed in the hope that it will be useful, "
                            "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
                            "GNU General Public License for more details." );

                auto* license = new QLabel(
                        "<p><b>" + copyright + "</b></p>" +
                        "<p>" + licenseParagraph1 + "</p>" +
                        "<p>" + licenseParagraph2 + "</p>" );
                license->setObjectName( "abLicense" );
                license->setTextFormat( Qt::RichText );
                license->setWordWrap( true );
                license->setTextInteractionFlags( Qt::TextSelectableByMouse );
                bodyLayout->addWidget( createCard( tr("License"), license ) );

                bodyLayout->addStretch( 1 );
                scroll->setWidget( body );
                root->addWidget( scroll, 1 );

                //
                // Footer buttons
                //
                auto* footer = new QWidget();
                footer->setObjectName( "abFooter" );
                auto* footerLayout = new QHBoxLayout( footer );
                footerLayout->setContentsMargins( 24, 12, 24, 14 );
                footerLayout->setSpacing( 8 );

                auto* licenseButton = new QPushButton( tr("&License") );
                licenseButton->setToolTip( tr("Read the full GNU General Public License") );
                licenseButton->setAutoDefault( false );
                connect( licenseButton, &QPushButton::clicked, this, &AboutDialog::onLicenseButtonClicked );
                footerLayout->addWidget( licenseButton );

                auto* websiteButton = new QPushButton( tr("&Website") );
                websiteButton->setToolTip( model::Version::WEBSITE );
                websiteButton->setAutoDefault( false );
                connect( websiteButton, &QPushButton::clicked, this, &AboutDialog::onWebsiteButtonClicked );
                footerLayout->addWidget( websiteButton );

                footerLayout->addStretch( 1 );

                auto* closeButton = new QPushButton( tr("&Close") );
                closeButton->setDefault( true );
                connect( closeButton, &QPushButton::clicked, this, &QDialog::accept );
                footerLayout->addWidget( closeButton );

                root->addWidget( footer );

                applyStyle();
                DialogUtil::makeResizableWindow( this );
                resize( 560, 620 );
                closeButton->setFocus();
        }


        ///
        /// A titled card
        ///
        QWidget* AboutDialog::createCard( const QString& title, QWidget* body )
        {
                auto* card = new QWidget();
                card->setObjectName( "abCard" );
                card->setAttribute( Qt::WA_StyledBackground, true );
                auto* layout = new QVBoxLayout( card );
                layout->setContentsMargins( 16, 12, 16, 14 );
                layout->setSpacing( 10 );

                auto* heading = new QLabel( title.toUpper() );
                heading->setObjectName( "abSection" );
                layout->addWidget( heading );
                layout->addWidget( body );
                return card;
        }


        ///
        /// A person entry: initials avatar, name, role and contact links
        ///
        QWidget* AboutDialog::createPerson( const QString& role, const QString& name,
                                            const QString& email, const QString& website )
        {
                auto* row = new QWidget();
                auto* layout = new QHBoxLayout( row );
                layout->setContentsMargins( 0, 0, 0, 0 );
                layout->setSpacing( 12 );

                QString initials;
                for ( const auto& part : name.split( ' ', Qt::SkipEmptyParts ) )
                {
                        initials += part.at( 0 ).toUpper();
                }
                auto* avatar = new QLabel( initials.left( 2 ) );
                avatar->setObjectName( "abAvatar" );
                avatar->setAlignment( Qt::AlignCenter );
                avatar->setFixedSize( 40, 40 );
                layout->addWidget( avatar, 0, Qt::AlignTop );

                auto* text = new QVBoxLayout();
                text->setSpacing( 2 );

                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted( (fg.red()*2 + bg.red()*3) / 5, (fg.green()*2 + bg.green()*3) / 5, (fg.blue()*2 + bg.blue()*3) / 5 );
                auto* nameLabel = new QLabel( QString( "<b>%1</b> &nbsp;<span style='color:%3'>%2</span>" )
                                              .arg( name.toHtmlEscaped(), role.toHtmlEscaped(), muted.name() ) );
                nameLabel->setObjectName( "abName" );
                text->addWidget( nameLabel );

                QStringList links;
                links << QString( "<a href='mailto:%1'>%1</a>" ).arg( email.toHtmlEscaped() );
                if ( !website.isEmpty() )
                {
                        QString url = website.startsWith( "http" ) ? website : "https://" + website;
                        links << QString( "<a href='%1'>%2</a>" ).arg( url.toHtmlEscaped(), website.toHtmlEscaped() );
                }
                auto* contact = new QLabel( links.join( " &nbsp;·&nbsp; " ) );
                contact->setTextFormat( Qt::RichText );
                contact->setOpenExternalLinks( true );
                contact->setTextInteractionFlags( Qt::TextBrowserInteraction );
                text->addWidget( contact );

                layout->addLayout( text, 1 );
                return row;
        }


        ///
        /// Palette-based styling (follows light/dark appearance)
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
                mTitleLabel->setPixmap( QPixmap( dark ? ":images/glabels-label-designer-dark.png"
                                                      : ":images/glabels-label-designer.png" ) );

                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted( (fg.red()*2 + bg.red()*3) / 5, (fg.green()*2 + bg.green()*3) / 5, (fg.blue()*2 + bg.blue()*3) / 5 );
                QColor accent = palette().color( QPalette::Highlight );

                setStyleSheet( QString(
                        "#abHeader { background: palette(base); border-bottom: 1px solid palette(mid); }"
                        "#abTagline { font-size: 11pt; }"
                        "#abVersion { padding: 2px 8px; border: 1px solid palette(mid); border-radius: 9px;"
                        "  background: palette(window); }"
                        "#abBody { background: palette(window); }"
                        "#abCard { background: palette(base); border: 1px solid palette(mid); border-radius: 8px; }"
                        "#abSection { color: %1; font-size: 8pt; font-weight: 600; letter-spacing: 1px; }"
                        "#abAvatar { background: %2; color: palette(highlighted-text); border-radius: 20px;"
                        "  font-weight: 600; }"
                        "#abFooter { background: palette(window); border-top: 1px solid palette(mid); }"
                ).arg( muted.name(), accent.name() ) );

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


        ///
        /// "License" Button Clicked Slot
        ///
        void AboutDialog::onLicenseButtonClicked()
        {
                QDesktopServices::openUrl( QUrl("http://www.gnu.org/licenses/gpl-3.0.txt") );
        }


        ///
        /// "Website" Button Clicked Slot
        ///
        void AboutDialog::onWebsiteButtonClicked()
        {
                QDesktopServices::openUrl( QUrl(model::Version::WEBSITE) );
        }

} // namespace glabels
