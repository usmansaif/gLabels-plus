//  WhatsNewDialog.cpp
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

#include "WhatsNewDialog.hpp"

#include "DialogUtil.hpp"

#include "model/Version.hpp"

#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>


namespace glabels
{

        ///
        /// Constructor
        ///
        WhatsNewDialog::WhatsNewDialog( QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( tr("What's New in gLabels Plus") );

                auto* title = new QLabel( tr("What's New") );
                QFont titleFont = title->font();
                titleFont.setPointSizeF( titleFont.pointSizeF() * 1.6 );
                titleFont.setBold( true );
                title->setFont( titleFont );

                auto* subtitle = new QLabel( tr("Version %1 — highlights of the latest features")
                                             .arg( model::Version::STRING ) );
                subtitle->setStyleSheet( "color: palette(placeholder-text);" );

                mBrowser = new QTextBrowser();
                mBrowser->setOpenLinks( false );
                mBrowser->setFrameShape( QFrame::StyledPanel );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
                buttons->button( QDialogButtonBox::Close )->setDefault( true );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( title );
                layout->addWidget( subtitle );
                layout->addSpacing( 6 );
                layout->addWidget( mBrowser, 1 );
                layout->addWidget( buttons );

                updateContent();

                DialogUtil::makeResizableWindow( this );
                resize( 620, 600 );
        }


        ///
        /// Rebuild text when language or theme changes
        ///
        void WhatsNewDialog::changeEvent( QEvent* event )
        {
                QDialog::changeEvent( event );
                if ( event->type() == QEvent::LanguageChange ||
                     event->type() == QEvent::PaletteChange )
                {
                        updateContent();
                }
        }


        ///
        /// Feature list
        ///
        void WhatsNewDialog::updateContent()
        {
                struct Section
                {
                        QString     title;
                        QStringList items;
                };

                const QList<Section> sections = {
                        { tr("Latest additions"), {
                                tr("<b>Page selector</b> in the toolbar, next to Units: click it to choose or change "
                                   "the page (product) at any time (also <b>Edit &gt; Page Setup</b>, Ctrl+Shift+P). "
                                   "Objects keep their positions and sizes, and the change can be undone in one step."),
                                tr("<b>24 new barcode label sizes</b> for thermal roll printers (Zebra, TSC, Xprinter, ...), "
                                   "from 30 &times; 15 mm jewelry tags to 4 &times; 6&Prime; shipping labels. "
                                   "Find them under the brand <i>Barcode Labels</i>."),
                                tr("New <b>Barcode labels</b> category in the product chooser to show only those sizes."),
                                tr("<b>Startup splash screen</b> with loading progress."),
                                tr("The main window now <b>opens maximized</b> at startup."),
                                tr("The <b>gLabels Plus icon</b> now appears on every window and dialog."),
                        } },
                        { tr("Designing labels"), {
                                tr("<b>Rulers</b> along the top and left of the label. Click the units corner to switch units."),
                                tr("<b>Objects panel</b> lists every object from top-most to bottom-most. "
                                   "Click the lock column to <b>lock</b> an object so it can't be selected or moved."),
                                tr("<b>Edit text in place</b>: double-click a text or barcode object (or press Enter/F2) "
                                   "to type directly on the label."),
                                tr("<b>Live position and size</b> tooltip follows the cursor while you move, resize or draw objects."),
                                tr("<b>Bring Forward / Send Backward</b> moves objects one layer at a time."),
                                tr("<b>Rotation box</b> in the toolbar sets an exact angle for the selected objects."),
                                tr("<b>Align to</b> the whole selection, the last selected object, or the label itself."),
                                tr("Hold <b>Shift</b> with the arrow keys to nudge objects in larger steps."),
                        } },
                        { tr("Mail merge"), {
                                tr("<b>Merge preview</b> shows real records on the label instead of field names, "
                                   "with First / Previous / Next / Last buttons to step through them."),
                        } },
                        { tr("Products and templates"), {
                                tr("<b>Redesigned product chooser</b>: search by brand, part number, description "
                                   "or size (e.g. <i>avery 5160</i>, <i>2x4</i>), with brand, category and size filters."),
                                tr("<b>Grid and list views</b>, plus <b>favorites</b> and <b>recently used</b> products."),
                                tr("<b>Also sold as</b>: see equivalent part numbers from other brands."),
                                tr("The product template designer uses your preferred units and supports roll labels."),
                        } },
                        { tr("Printing"), {
                                tr("<b>Click a label</b> in the print preview to choose where printing starts; "
                                   "Shift+click to choose the last label."),
                                tr("Default <b>label outlines</b> and <b>crop marks</b> can be set in Preferences."),
                        } },
                        { tr("Workspace"), {
                                tr("<b>New welcome screen</b> with quick actions and recent projects "
                                   "(right-click a project to remove it from the list)."),
                                tr("<b>Redesigned Preferences</b> with a search box and sections for general, "
                                   "appearance, editor and printing options."),
                                tr("<b>Light and dark color schemes</b> and a choice of icon styles."),
                                tr("<b>Custom keyboard shortcuts</b> for every menu command (Edit &gt; Keyboard Shortcuts)."),
                                tr("Show or hide <b>toolbars and panels</b>, and choose icon size and text style "
                                   "for the page buttons and toolbars."),
                                tr("<b>Units selector</b> in the toolbar, and a zoom box that accepts typed values."),
                                tr("New <b>About</b> dialog with one-click copy of version information."),
                                tr("Refreshed application icon and a new set of toolbar icons."),
                        } },
                };

                QColor accent = palette().color( QPalette::Highlight );
                QString html = QString( "<style>"
                                        "h3 { color: %1; margin-top: 14px; margin-bottom: 4px; }"
                                        "li { margin-bottom: 5px; }"
                                        "</style>" ).arg( accent.name() );
                for ( const auto& section : sections )
                {
                        html += "<h3>" + section.title.toHtmlEscaped() + "</h3><ul>";
                        for ( const auto& item : section.items )
                        {
                                html += "<li>" + item + "</li>";
                        }
                        html += "</ul>";
                }

                mBrowser->setHtml( html );
        }

}
