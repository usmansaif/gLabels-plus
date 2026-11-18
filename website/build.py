#!/usr/bin/env python3
"""
Build the gLabels Plus website into ../docs (served by GitHub Pages).

    python3 website/build.py

Each page body lives in website/pages/<name>.html. This script wraps it with the
shared header and footer and adds per-page SEO metadata: title, description,
canonical URL, Open Graph / Twitter cards and JSON-LD structured data. It also
writes sitemap.xml, robots.txt and llms.txt. Only the Python standard library
is used.

Shared data (FAQ, barcode presets, release notes) is defined once here, so the
visible text and the structured data always match.
"""

import html
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT.parent / "docs"

SITE_URL = "https://usmansaif.github.io/gLabels-plus/"
REPO_URL = "https://github.com/usmansaif/gLabels-plus"
RELEASES_URL = REPO_URL + "/releases"
NAME = "gLabels Plus"
VERSION = "5.0"
RELEASE_DATE = "2026-11-19"          # ISO date of the 5.0 release
UPDATED = "2026-11-19"               # last content update, shown on every page
UPDATED_HUMAN = "19 November 2026"
CONTACT_URL = "https://usmansaif.com/en/contact"   # custom barcode label printing solutions
MAINTAINER = {"@type": "Person", "@id": SITE_URL + "#maintainer", "name": "Usman Saif",
              "url": "https://usmansaif.com", "jobTitle": "Maintainer of gLabels Plus"}
OG_IMAGE = SITE_URL + "images/screenshots/editor.png"


# --------------------------------------------------------------------------
# Shared data
# --------------------------------------------------------------------------

# (size mm, size in, typical use, width mm, height mm)
PRESETS = [
    ("30 × 15", "1.18 × 0.59", "Mini / Jewelry Tag", 30, 15),
    ("38 × 25", "1.50 × 0.98", "Compact Retail & Stock", 38, 25),
    ("38.1 × 25.4", "1.50 × 1.00", "Compact Retail & Logistics", 38.1, 25.4),
    ("40 × 30", "1.57 × 1.18", "Healthcare & Asset Tag", 40, 30),
    ("50 × 25", "1.97 × 0.98", "Standard Retail / MRP", 50, 25),
    ("50.8 × 25.4", "2.00 × 1.00", "Compact Barcode Label", 50.8, 25.4),
    ("50 × 30", "1.97 × 1.18", "Product / Shelf Label", 50, 30),
    ("50 × 50", "1.97 × 1.97", "Square Retail / Product", 50, 50),
    ("52 × 38", "2.05 × 1.50", "Compact Shelf / Bin ID", 52, 38),
    ("57 × 32", "2.24 × 1.26", "Small Retail & Logistics", 57, 32),
    ("60 × 40", "2.36 × 1.57", "Mid-Size Warehouse / Retail", 60, 40),
    ("76 × 38", "2.99 × 1.50", "Courier & Warehouse", 76, 38),
    ("76 × 50", "2.99 × 1.97", "Stock Control & Packing", 76, 50),
    ("76 × 51", "3.00 × 2.00", "Inventory / Warehouse", 76, 51),
    ("80 × 50", "3.15 × 1.97", "Wide Product & Box ID", 80, 50),
    ("89 × 28", "3.50 × 1.10", "Address / Dymo 30252", 89, 28),
    ("100 × 50", "3.94 × 1.97", "Box / Tray Marking", 100, 50),
    ("100 × 100", "3.94 × 3.94", "Pallet / Warehouse Tracking", 100, 100),
    ("100 × 150", "3.94 × 5.91", "Shipping & Courier (4×6)", 100, 150),
    ("101.6 × 76.2", "4.00 × 3.00", "Outer Box Logistics", 101.6, 76.2),
    ("101.6 × 101.6", "4.00 × 4.00", "Pallet / Location Square", 101.6, 101.6),
    ("101.6 × 152.4", "4.00 × 6.00", "Exact 4×6″ Zebra Shipping", 101.6, 152.4),
    ("102 × 38", "4.02 × 1.50", "Long Barcode / ID Strip", 102, 38),
    ("102 × 50", "4.02 × 1.97", "Carton & Inventory", 102, 50),
]

# Keyboard tool shortcuts (key, tool)
TOOL_KEYS = [("V", "Select"), ("T", "Text"), ("R", "Box"), ("L", "Line"), ("O", "Ellipse"),
             ("I", "Image"), ("B", "Barcode"), ("M", "Measure"), ("E", "Erase (delete) the selection")]

# Latest additions, shown as highlight cards on whats-new.html.
# icon: key into ICONS; keys: show TOOL_KEYS chips; how: where to find it in the app.
LATEST = [
    dict(icon="guides", title="Smart guides and snapping",
         summary="While you move or resize, red lines show the distance to the nearest object or label edge in your chosen units (mm, inches, points...). Objects snap to other objects' edges and centers and to the label's edges and center.",
         how="Just drag an object · hold <kbd>Ctrl</kbd> while dragging to turn snapping off"),
    dict(icon="ruler", title="Measure tool",
         summary="Measure anywhere on the label: between objects, to the label edge, or any two points. Shows the length, plus width × height for diagonal lines.",
         how="Press <kbd>M</kbd> or the ruler button · <kbd>Shift</kbd> keeps the line straight · <kbd>Esc</kbd> clears it"),
    dict(icon="layers", title="Layer groups and names",
         summary="Name your objects and organize them into collapsible groups in the Objects panel. Select a group to move, align or lock all of it at once.",
         how="<kbd>Ctrl</kbd>+<kbd>G</kbd> to group · double-click or <kbd>F2</kbd> to rename · toolbar at the top of the Objects panel"),
    dict(icon="keyboard", title="Single-key tool shortcuts", wide=True, keys=True,
         summary="Pick any drawing tool with one key, no menus needed.",
         how="Hover a toolbar button to see its key · change them in <b>Edit &gt; Keyboard Shortcuts</b>"),
    dict(icon="search", title="Quick font search",
         summary="Type part of a font name (<i>mono</i>, <i>sans</i>) in the Family box and pick from matches shown in their own typeface.",
         how="Press <kbd>Enter</kbd> or click a match to apply"),
    dict(icon="language", title="Urdu labels in Nastaliq",
         summary="Noto Nastaliq Urdu (regular and bold) is built in, so Urdu text is shaped and printed in true Nastaliq on every computer, even without the font installed.",
         how="Type <i>urdu</i> in the Family box · Left, Center and Right align right-to-left text exactly as shown"),
    dict(icon="folder", title="Projects",
         summary="Keep each business or client's labels together in one folder that's easy to back up or share.",
         how="<b>File &gt; New Project</b> · right-click to rename, duplicate or remove"),
    dict(icon="page", title="Page selector",
         summary="Change the page (product) at any time. Objects keep their positions and sizes, and one Undo reverts it.",
         how="Toolbar, next to Units · <b>Edit &gt; Page Setup</b> (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd>)"),
    dict(icon="barcode", title="24 barcode label sizes",
         summary="Thermal roll presets for Zebra, TSC, Xprinter and Godex, from 30 × 15 mm jewelry tags to 4 × 6″ shipping labels.",
         how="Product chooser &gt; <b>Barcode labels</b> category"),
]
LATEST_MINOR = [
    "Objects panel toolbar and right-click menu: rename, lock, duplicate, group, reorder and delete",
    "Duplicate objects in one click",
    "Readable selected rows on the welcome screen",
    "Startup splash screen with loading progress",
    "Windows always open maximized",
    "gLabels Plus icon on every window and dialog",
]

ICONS = {
    "keyboard": '<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>',
    "search": '<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>',
    "folder": '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
    "page": '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5M9 13h6M9 17h4"/>',
    "language": '<path d="M4 5h9M8.5 3v2M6 5c0 4 2.5 7 6 8M11 5c0 3.5-2.5 7-7 9"/><path d="m13 21 4-9 4 9M14.5 18h5"/>',
    "barcode": '<path d="M4 5v14M7 5v14M11 5v14M14 5v14M17 5v14M20 5v14"/>',
    "guides": '<rect x="4" y="4" width="7" height="7" rx="1"/><rect x="14" y="13" width="6" height="7" rx="1"/><path d="M11 7.5h9M17 3v7M4 16.5h6M7 14v5"/>',
    "ruler": '<rect x="2" y="8" width="20" height="8" rx="1"/><path d="M6 8v3M10 8v4M14 8v3M18 8v4"/>',
    "layers": '<path d="m12 3 9 5-9 5-9-5z"/><path d="m3 13 9 5 9-5"/>',
}


def highlight_text(h):
    """Flat one-line version of a highlight card (home teaser, llms.txt)."""
    return f"<b>{h['title']}</b>: {h['summary']}"


# Release notes, mirrored from Help > What's New in the app (glabels/WhatsNewDialog.cpp)
RELEASE_NOTES = [
    ("Latest additions", [highlight_text(h) for h in LATEST] + LATEST_MINOR),
    ("Designing labels", [
        "<b>Rulers</b> along the top and left of the label. Click the units corner to switch units.",
        "<b>Objects panel</b> lists every object from top-most to bottom-most. Click the lock column to <b>lock</b> an object so it can't be selected or moved.",
        "<b>Name objects</b> and organize them into <b>layer groups</b> (<b>Ctrl+G</b>). A toolbar at the top of the Objects panel renames, locks, duplicates, groups, reorders and deletes the selection.",
        "<b>Smart guides</b> show red distance lines to nearby objects and the label edge while you move or resize, and objects snap to edges and centers (hold Ctrl to turn snapping off).",
        "<b>Measure tool</b> (<b>M</b>): drag between any two points to measure, in your chosen units.",
        "<b>Edit text in place</b>: double-click a text or barcode object (or press Enter/F2) to type directly on the label.",
        "<b>Live position and size</b> tooltip follows the cursor while you move, resize or draw objects.",
        "<b>Bring Forward / Send Backward</b> moves objects one layer at a time.",
        "<b>Rotation box</b> in the toolbar sets an exact angle for the selected objects.",
        "<b>Align to</b> the whole selection, the last selected object, or the label itself.",
        "Hold <b>Shift</b> with the arrow keys to nudge objects in larger steps.",
    ]),
    ("Mail merge", [
        "<b>Merge preview</b> shows real records on the label instead of field names, with First / Previous / Next / Last buttons to step through them.",
    ]),
    ("Products and templates", [
        "<b>Redesigned product chooser</b>: search by brand, part number, description or size (for example <i>avery 5160</i> or <i>2x4</i>), with brand, category and size filters.",
        "<b>Grid and list views</b>, plus <b>favorites</b> and <b>recently used</b> products.",
        "<b>Also sold as</b>: see equivalent part numbers from other brands.",
        "The product template designer uses your preferred units and supports roll labels.",
    ]),
    ("Printing", [
        "<b>Click a label</b> in the print preview to choose where printing starts; Shift+click to choose the last label.",
        "Default <b>label outlines</b> and <b>crop marks</b> can be set in Preferences.",
    ]),
    ("Workspace", [
        "<b>New welcome screen</b> with quick actions and recent labels (right-click a label to remove it from the list).",
        "<b>Redesigned Preferences</b> with a search box and sections for general, appearance, editor and printing options.",
        "<b>Light and dark color schemes</b> and a choice of icon styles.",
        "<b>Custom keyboard shortcuts</b> for every menu command (<b>Edit &gt; Keyboard Shortcuts</b>).",
        "Show or hide <b>toolbars and panels</b>, and choose icon size and text style for the page buttons and toolbars.",
        "<b>Units selector</b> in the toolbar, and a zoom box that accepts typed values.",
        "New <b>About</b> dialog with one-click copy of version information.",
        "Refreshed application icon and a new set of toolbar icons.",
    ]),
]

FAQ = [
    ("What is gLabels Plus?",
     "gLabels Plus is a free, open-source label and business card designer for Linux, Windows and macOS. It designs barcode, address, shipping and product labels, name badges, CD/DVD labels and business cards, with mail merge and over 1,800 ready-made templates. It is maintained by Usman Saif and built on gLabels by Jaye Evins."),
    ("Is gLabels Plus really free?",
     "Yes. gLabels Plus is free and open source under the GNU General Public License v3 or later. There is no trial period, no watermark and no account, and you may use it for business labels."),
    ("Which operating systems does gLabels Plus support?",
     "gLabels Plus runs on Linux (AppImage, .deb, .rpm, .tar.gz), Windows 10 and 11 (installer and portable .zip) and macOS (.dmg). It works fully offline."),
    ("Does gLabels Plus work with thermal label printers like Zebra, TSC or Xprinter?",
     "Yes. If your operating system can print to the printer, gLabels Plus can. It includes 24 barcode label presets for thermal roll printers, from 30 × 15 mm to 4 × 6 inch shipping labels, and you can define your own roll sizes."),
    ("Is gLabels Plus a free alternative to BarTender, ZebraDesigner or Avery Design & Print?",
     "Yes, for designing and printing barcode, product, address and shipping labels. gLabels Plus is free and open source, works offline on Windows, Linux and macOS, needs no account, and prints to any printer your computer can print to, including Zebra, TSC, Xprinter and Godex thermal printers. Avery sheet templates are built in, so you can design Avery labels without an internet connection. It prints through the normal printer driver and does not output printer languages such as ZPL."),
    ("Can I generate bulk barcodes from a CSV file?",
     "Yes. Add a barcode object, set its data to a column from your CSV file, and gLabels Plus prints one label per row, each with its own barcode and text. Check the records in the live merge preview before printing, or print from scripts with glabels-batch-qt."),
    ("Can I print Urdu labels with gLabels Plus?",
     "Yes. gLabels Plus has the Noto Nastaliq Urdu font (regular and bold) built in, so Urdu text is shaped correctly in Nastaliq style on screen, in PDF and on paper, on Linux, Windows and macOS, even if the font is not installed. Urdu, English and numbers can be mixed on the same label, merged from a CSV file and printed next to barcodes."),
    ("Can you build a customized barcode label printing solution for my business?",
     "Yes. Usman Saif, the maintainer, builds customized barcode label printing solutions: label designs and templates, printing labels from your inventory, POS or ERP data, thermal printer setup for Zebra, TSC, Xprinter and Godex, and bilingual English and Urdu labels. Get in touch at usmansaif.com/en/contact."),
    ("Can gLabels Plus print Avery labels?",
     "Yes. gLabels Plus includes over 1,800 product templates from more than 70 brands, including Avery, Herma, Zweckform, Brother, Dymo, Uline and Worldlabel. Search by part number, for example avery 5160, in the product chooser."),
    ("Can I make barcodes and QR codes?",
     "Yes. gLabels Plus creates barcodes and QR codes, including EAN, UPC, Code 128, Code 39, DataMatrix and QR, using its built-in glbarcode++ library and optional GNU Barcode, Zint and libqrencode back ends."),
    ("How do I print labels from a spreadsheet (mail merge)?",
     "Save the spreadsheet as CSV, open the Merge page in gLabels Plus and choose the file. Insert merge fields into text or barcode objects, check the live merge preview, and print. Each record becomes one label."),
    ("How is gLabels Plus different from gLabels?",
     "gLabels Plus is built on gLabels-qt (gLabels 4). It adds Projects, a page selector, 24 thermal roll presets, single-key tool shortcuts, quick font search, a redesigned product chooser and Preferences, rulers, smart guides with snapping, a measure tool, an objects panel with layer names, groups and locking, and a refreshed interface. It opens existing .glabels files."),
    ("My label sheet isn't listed. What can I do?",
     "Search by part number in the product chooser, because many sheets are listed under an equivalent brand. If it is still missing, use the built-in product template designer to measure and save it."),
    ("Where are my labels saved?",
     "On your own computer. Project labels are stored in Documents/gLabels Plus Projects, one folder per project, so you can back them up or share them like any other files. Nothing is uploaded."),
    ("What keyboard shortcuts does gLabels Plus have?",
     "Single keys pick tools: V Select, T Text, R Box, L Line, O Ellipse, I Image, B Barcode and E to erase the selection. Every menu command can also be given a custom shortcut in Edit > Keyboard Shortcuts."),
    ("How can I support gLabels Plus?",
     "Email the maintainer, Usman Saif, to donate or sponsor a feature, star the project on GitHub, report bugs, translate the app or contribute product templates."),
    ("How do I report a bug or request a feature?",
     "Open an issue on GitHub at github.com/usmansaif/gLabels-plus/issues. Pull requests, translations and new product templates are welcome."),
]

FAQ_HOME = 6   # questions shown (and marked up as FAQPage) on the home page

NAV = [("features.html", "Features"), ("download.html", "Download"), ("whats-new.html", "What's New"),
       ("faq.html", "FAQ"), ("donate.html", "Donate")]


# --------------------------------------------------------------------------
# Pages
# --------------------------------------------------------------------------

PAGES = {
    "index": dict(
        file="index.html", label="Home",
        title="gLabels Plus: Free Barcode & Thermal Label Designer",
        description="Free, open-source, offline label designer for Windows, Linux and macOS. 1,800+ templates, CSV mail merge and barcode labels for Zebra, TSC and Xprinter.",
    ),
    "features": dict(
        file="features.html", label="Features",
        title="gLabels Plus Features: Barcodes, CSV Merge, 1,800+ Templates",
        description="All gLabels Plus features: barcodes and QR codes, CSV mail merge, 1,800+ templates, thermal roll presets, Projects, one-key tools and print to PDF.",
    ),
    "download": dict(
        file="download.html", label="Download",
        title="Download gLabels Plus 5.0 Free for Linux, Windows and macOS",
        description="Download gLabels Plus 5.0 free: AppImage, .deb and .rpm for Linux, installer and portable .zip for Windows, .dmg for macOS, or build from source.",
    ),
    "whats-new": dict(
        file="whats-new.html", label="What's New",
        title="What's New in gLabels Plus 5.0: Release Notes",
        description="gLabels Plus 5.0 release notes: single-key tool shortcuts, quick font search, Projects, page selector, 24 thermal barcode label sizes, splash screen and more.",
    ),
    "barcode-labels": dict(
        file="barcode-labels.html", label="Barcode Labels",
        title="Barcode Label Sizes for Thermal Printers | gLabels Plus",
        description="24 free barcode label templates for Zebra, TSC, Xprinter and Godex thermal printers, from 30 × 15 mm and 50 × 25 mm to 4 × 6 inch shipping labels.",
    ),
    "faq": dict(
        file="faq.html", label="FAQ",
        title="gLabels Plus FAQ: Free Label Designer Questions Answered",
        description="Is gLabels Plus free? Does it print Avery and Zebra labels, barcodes, QR codes and mail merge from Excel? Answers to the most common questions.",
    ),
    "donate": dict(
        file="donate.html", label="Donate",
        title="Support gLabels Plus: Donate or Sponsor a Feature",
        description="gLabels Plus is free and open source. Support it by donating, sponsoring a feature, starring it on GitHub or contributing templates and translations.",
    ),
}


# --------------------------------------------------------------------------
# Rendering helpers
# --------------------------------------------------------------------------

def esc(text):
    return html.escape(text, quote=True)


def strip_tags(text):
    return html.unescape(re.sub(r"<[^>]+>", "", text))


def preset_cards(presets=PRESETS):
    scale = 0.62  # px per mm
    out = []
    for mm, inch, use, w, h in presets:
        group = "s" if w <= 52 else ("m" if w <= 80 else "l")
        out.append(
            f'          <li class="preset" data-size="{group}">\n'
            f'            <div class="pv" aria-hidden="true"><div class="lbl" style="width:{w*scale:.0f}px;height:{h*scale:.0f}px"><i></i></div></div>\n'
            f'            <div class="pinfo"><b>{mm} <small>mm</small></b><span>{inch} in</span><p>{esc(use)}</p></div>\n'
            f'          </li>')
    return "\n".join(out)


def preset_table():
    rows = "\n".join(f"            <tr><td>{mm} mm</td><td>{inch} in</td><td>{esc(use)}</td></tr>"
                     for mm, inch, use, *_ in PRESETS)
    return rows


def faq_html(items):
    out = []
    for q, a in items:
        out.append(f'        <details>\n          <summary><h3>{esc(q)}</h3></summary>\n          <p>{esc(a)}</p>\n        </details>')
    return "\n".join(out)


def icon_svg(name):
    return (f'<svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="2" '
            f'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{ICONS[name]}</svg>')


def latest_html():
    jump = "".join(f'<li><a href="#new-{i + 1}">{esc(h["title"])}</a></li>' for i, h in enumerate(LATEST))
    cards = []
    for i, h in enumerate(LATEST):
        keys = ""
        if h.get("keys"):
            keys = ('\n            <ul class="hl-keys">' +
                    "".join(f'<li><kbd>{k}</kbd>{esc(t.split(" (")[0])}</li>'
                            for k, t in TOOL_KEYS) + "</ul>")
        cards.append(
            f'          <article class="hl{" hl-wide" if h.get("wide") else ""}" id="new-{i + 1}">\n'
            f'            <div class="hl-head"><span class="hl-icon">{icon_svg(h["icon"])}</span>'
            f'<h3>{esc(h["title"])}</h3><span class="hl-badge">New</span></div>\n'
            f'            <p>{h["summary"]}</p>{keys}\n'
            f'            <p class="hl-how"><span>How</span>{h["how"]}</p>\n'
            f'          </article>')
    minor = "".join(f"<li>{esc(m)}</li>" for m in LATEST_MINOR)
    return (f'      <section class="notes latest" id="latest-additions" aria-labelledby="latest-additions-h">\n'
            f'        <h2 id="latest-additions-h">Latest additions</h2>\n'
            f'        <p class="latest-sub">{len(LATEST)} headline features and {len(LATEST_MINOR)} smaller touches in this release.</p>\n'
            f'        <nav aria-label="Latest additions"><ul class="hl-jump">{jump}</ul></nav>\n'
            f'        <div class="hl-grid">\n' + "\n".join(cards) + '\n        </div>\n'
            f'        <div class="hl-minor"><h3>Also in {VERSION}</h3><ul>{minor}</ul></div>\n'
            f'      </section>')


def release_notes_html():
    out = [latest_html()]
    for title, items in RELEASE_NOTES[1:]:
        slug = re.sub(r"[^a-z]+", "-", title.lower()).strip("-")
        lis = "\n".join(f"          <li>{i}</li>" for i in items)
        out.append(f'      <section class="notes" id="{slug}" aria-labelledby="{slug}-h">\n'
                   f'        <h2 id="{slug}-h">{esc(title)}</h2>\n        <ul class="ticks">\n{lis}\n        </ul>\n      </section>')
    return "\n".join(out)


def whats_new_teaser():
    return "\n".join(f"        <li>{i}</li>" for i in RELEASE_NOTES[0][1][:5])


def tool_keys_html():
    return "\n".join(f'          <li><kbd>{k}</kbd><span>{esc(t)}</span></li>' for k, t in TOOL_KEYS)


PLACEHOLDERS = {
    "{{presets}}": lambda: preset_cards(),
    "{{presets_teaser}}": lambda: preset_cards([PRESETS[i] for i in (4, 7, 12, 18)]),
    "{{preset_table}}": preset_table,
    "{{faq_all}}": lambda: faq_html(FAQ),
    "{{faq_home}}": lambda: faq_html(FAQ[:FAQ_HOME]),
    "{{release_notes}}": release_notes_html,
    "{{whats_new_teaser}}": whats_new_teaser,
    "{{tool_keys}}": tool_keys_html,
    "{{version}}": lambda: VERSION,
    "{{release_date}}": lambda: RELEASE_DATE,
    "{{updated_human}}": lambda: UPDATED_HUMAN,
    "{{releases_url}}": lambda: RELEASES_URL,
    "{{repo_url}}": lambda: REPO_URL,
}


# --------------------------------------------------------------------------
# Structured data (JSON-LD)
# --------------------------------------------------------------------------

def software_app(extra=None):
    data = {
        "@type": "SoftwareApplication",
        "@id": SITE_URL + "#software",
        "name": NAME,
        "alternateName": ["gLabels+", "glabels-plus"],
        "description": "Free, open-source label and business card designer for Linux, Windows and macOS with barcodes, QR codes, mail merge, 1,800+ templates and thermal printer presets.",
        "applicationCategory": "BusinessApplication",
        "applicationSubCategory": "Label design software",
        "operatingSystem": "Linux, Windows 10, Windows 11, macOS",
        "softwareVersion": VERSION,
        "datePublished": RELEASE_DATE,
        "license": "https://www.gnu.org/licenses/gpl-3.0.html",
        "isAccessibleForFree": True,
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "USD"},
        "downloadUrl": SITE_URL + "download.html",
        "installUrl": RELEASES_URL,
        "releaseNotes": SITE_URL + "whats-new.html",
        "screenshot": [SITE_URL + "images/screenshots/" + n for n in
                       ("editor.png", "welcome.png", "product-chooser.png", "print.png")],
        "image": OG_IMAGE,
        "url": SITE_URL,
        "sameAs": [REPO_URL],
        "author": {"@id": MAINTAINER["@id"]},
        "maintainer": {"@id": MAINTAINER["@id"]},
        "contributor": {"@type": "Person", "name": "Jaye Evins"},
        "isBasedOn": {"@type": "SoftwareApplication", "name": "gLabels",
                      "url": "https://github.com/j-evins/glabels-qt"},
        "codeRepository": REPO_URL,
        "programmingLanguage": "C++",
        "keywords": "free barcode label designer, open source thermal printer software, offline label maker, free BarTender alternative, ZebraDesigner alternative, Avery Design and Print offline alternative, bulk barcode generator from CSV, 4x6 shipping label software, 50x25 mm MRP barcode labels, Xprinter TSC Godex label software, Urdu Nastaliq labels, QR code labels, mail merge labels",
        "featureList": [
            "Barcodes and QR codes", "Bulk barcodes from CSV with live merge preview",
            "1,800+ label templates from 70+ brands", "24 thermal roll barcode label presets",
            "Projects to group labels by client", "Single-key tool shortcuts",
            "Quick font search", "Rulers, alignment and object locking",
            "Smart distance guides and snapping", "Measure tool", "Layer groups and names",
            "Print to PDF, crop marks and mirror printing", "Built-in Urdu Nastaliq font",
            "Works offline, no account", "Light and dark themes",
        ],
    }
    if extra:
        data.update(extra)
    return data


def breadcrumb(page):
    items = [{"@type": "ListItem", "position": 1, "name": "Home", "item": SITE_URL}]
    if page["file"] != "index.html":
        items.append({"@type": "ListItem", "position": 2, "name": page["label"], "item": SITE_URL + page["file"]})
    return {"@type": "BreadcrumbList", "itemListElement": items}


def webpage(page, kind="WebPage"):
    url = SITE_URL + ("" if page["file"] == "index.html" else page["file"])
    return {
        "@type": kind, "@id": url + "#webpage", "url": url, "name": page["title"],
        "description": page["description"], "inLanguage": "en",
        "isPartOf": {"@id": SITE_URL + "#website"}, "about": {"@id": SITE_URL + "#software"},
        "primaryImageOfPage": {"@type": "ImageObject", "url": OG_IMAGE},
        "dateModified": UPDATED, "author": {"@id": MAINTAINER["@id"]},
        "speakable": {"@type": "SpeakableSpecification", "cssSelector": [".answer", "h1"]},
    }


def faq_schema(items):
    return {"@type": "FAQPage", "mainEntity": [
        {"@type": "Question", "name": q, "acceptedAnswer": {"@type": "Answer", "text": a}} for q, a in items]}


def schema_for(key, page):
    graph = [
        {"@type": "WebSite", "@id": SITE_URL + "#website", "url": SITE_URL, "name": NAME,
         "description": PAGES["index"]["description"], "inLanguage": "en", "publisher": {"@id": MAINTAINER["@id"]}},
        MAINTAINER,
        breadcrumb(page),
    ]
    if key == "index":
        graph += [webpage(page), software_app(), faq_schema(FAQ[:FAQ_HOME])]
    elif key == "features":
        graph += [webpage(page), software_app()]
    elif key == "download":
        graph += [webpage(page), software_app(), {
            "@type": "HowTo", "name": "Build gLabels Plus from source",
            "description": "Compile gLabels Plus on Linux, macOS or Windows (Git Bash) with the included build script.",
            "tool": [{"@type": "HowToTool", "name": n} for n in ("Git", "CMake 3.22+", "C++20 compiler", "Qt 6.2+")],
            "step": [
                {"@type": "HowToStep", "name": "Clone the repository", "text": "git clone " + REPO_URL + ".git && cd gLabels-plus"},
                {"@type": "HowToStep", "name": "Install dependencies", "text": "./build.sh --deps (first time only)"},
                {"@type": "HowToStep", "name": "Build", "text": "./build.sh"},
                {"@type": "HowToStep", "name": "Run", "text": "./build/glabels/glabels-qt"},
            ]}]
    elif key == "whats-new":
        graph += [webpage(page), {
            "@type": "TechArticle", "@id": SITE_URL + "whats-new.html#article",
            "headline": f"What's new in {NAME} {VERSION}", "description": page["description"],
            "datePublished": RELEASE_DATE, "dateModified": UPDATED, "inLanguage": "en",
            "author": {"@id": MAINTAINER["@id"]}, "publisher": {"@id": MAINTAINER["@id"]},
            "image": OG_IMAGE, "about": {"@id": SITE_URL + "#software"},
            "mainEntityOfPage": SITE_URL + "whats-new.html"}]
    elif key == "barcode-labels":
        graph += [webpage(page), {
            "@type": "ItemList", "name": "gLabels Plus barcode label presets for thermal roll printers",
            "numberOfItems": len(PRESETS),
            "itemListElement": [{"@type": "ListItem", "position": i + 1,
                                 "name": f"{mm} mm ({inch} in) {use}"} for i, (mm, inch, use, *_) in enumerate(PRESETS)]},
            {"@type": "HowTo", "name": "Use a barcode label preset in gLabels Plus",
             "step": [
                 {"@type": "HowToStep", "text": "Open gLabels Plus and click New label (or File > New)."},
                 {"@type": "HowToStep", "text": "In the product chooser, select the Barcode labels category."},
                 {"@type": "HowToStep", "text": "Pick your size, for example 50 × 25 mm or 100 × 150 mm."},
                 {"@type": "HowToStep", "text": "Press B to add a barcode, T to add text, then print to your thermal printer."},
             ]}]
    elif key == "faq":
        graph += [webpage(page, "WebPage"), faq_schema(FAQ)]
    elif key == "donate":
        graph += [webpage(page)]
    return json.dumps({"@context": "https://schema.org", "@graph": graph}, ensure_ascii=False, indent=1)


# --------------------------------------------------------------------------
# Layout
# --------------------------------------------------------------------------

GITHUB_SVG = ('<svg viewBox="0 0 16 16" width="16" height="16" aria-hidden="true"><path fill="currentColor" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/></svg>')


def header(current):
    links = []
    for href, label in NAV:
        cur = ' aria-current="page"' if href == current else ""
        links.append(f'      <a href="{href}"{cur}>{label}</a>')
    links = "\n".join(links)
    return f'''<a class="skip" href="#main">Skip to content</a>

<header class="site-header">
  <div class="wrap header-inner">
    <a class="brand" href="./" aria-label="gLabels Plus home">
      <img src="assets/icon.svg" alt="" width="32" height="32">
      <span>gLabels <b>Plus</b></span>
    </a>
    <button class="nav-toggle" aria-expanded="false" aria-controls="nav" aria-label="Menu">
      <span></span><span></span><span></span>
    </button>
    <nav id="nav" class="nav" aria-label="Main">
{links}
      <a class="btn btn-sm" href="{REPO_URL}" target="_blank" rel="noopener">
        {GITHUB_SVG}
        GitHub
      </a>
    </nav>
  </div>
</header>
'''


def custom_solution():
    """Contact band for custom barcode label printing work, shown above the footer on every page."""
    return f'''<section class="custom-solution" aria-labelledby="custom-h">
  <div class="wrap custom-inner">
    <div>
      <p class="eyebrow">Custom solutions</p>
      <h2 id="custom-h">Need a customized barcode label printing solution?</h2>
      <p>I build label printing that fits the way your business works: custom label designs and templates, printing straight from your inventory, POS or ERP data, thermal printer setup (Zebra, TSC, Xprinter, Godex) and bilingual English and Urdu labels.</p>
    </div>
    <a class="btn btn-primary" href="{CONTACT_URL}" target="_blank" rel="noopener">Contact me</a>
  </div>
</section>

'''


def footer():
    return custom_solution() + f'''<footer class="site-footer">
  <div class="wrap footer-grid">
    <div class="footer-about">
      <a class="brand" href="./"><img src="assets/icon.svg" alt="" width="28" height="28"><span>gLabels <b>Plus</b></span></a>
      <p>A free, open-source label and business card designer for Linux, Windows and macOS. Maintained by <a href="https://usmansaif.com" target="_blank" rel="noopener">Usman Saif</a>, built on gLabels by Jaye Evins.</p>
      <p class="ver"><span class="dot"></span> Latest version {VERSION} · Updated <time datetime="{UPDATED}">{UPDATED_HUMAN}</time></p>
    </div>
    <nav aria-label="Product">
      <h2>Product</h2>
      <a href="features.html">Features</a>
      <a href="download.html">Download</a>
      <a href="whats-new.html">What's New</a>
      <a href="barcode-labels.html">Barcode label sizes</a>
      <a href="./#screenshots">Screenshots</a>
    </nav>
    <nav aria-label="Developers">
      <h2>Developers</h2>
      <a href="{REPO_URL}" target="_blank" rel="noopener">Source code</a>
      <a href="{REPO_URL}/blob/master/dev-docs/CONTRIBUTING.md" target="_blank" rel="noopener">Contributing</a>
      <a href="{REPO_URL}/blob/master/dev-docs/PRODUCT-TEMPLATES.md" target="_blank" rel="noopener">Product templates</a>
      <a href="{REPO_URL}/blob/master/dev-docs/TRANSLATIONS.md" target="_blank" rel="noopener">Translations</a>
    </nav>
    <nav aria-label="Support">
      <h2>Support</h2>
      <a href="faq.html">FAQ</a>
      <a href="donate.html">Donate</a>
      <a href="{REPO_URL}/issues" target="_blank" rel="noopener">Report a bug</a>
      <a href="{CONTACT_URL}" target="_blank" rel="noopener">Custom solutions</a>
      <a href="https://usmansaif.com" target="_blank" rel="noopener">Maintainer</a>
    </nav>
  </div>
  <div class="wrap footer-bottom">
    <span>© <span id="year">2026</span> Usman Saif and gLabels contributors. Licensed under the GNU GPL v3.</span>
    <a href="#top">Back to top</a>
  </div>
</footer>

<div class="lightbox" id="lightbox" hidden>
  <button class="lb-close" aria-label="Close">×</button>
  <img alt="">
</div>

<script src="assets/site.js" defer></script>
'''


def head(key, page):
    url = SITE_URL + ("" if page["file"] == "index.html" else page["file"])
    robots = "noindex, follow" if key == "404" else "index, follow, max-image-preview:large, max-snippet:-1, max-video-preview:-1"
    ld = "" if key == "404" else f'  <script type="application/ld+json">\n{schema_for(key, page)}\n  </script>\n'
    base = f'  <base href="{SITE_URL}">\n' if key == "404" else ""
    canonical = "" if key == "404" else f'  <link rel="canonical" href="{url}">\n'
    return f'''<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="google-site-verification" content="Gp_U57yyeAGGdQGBVQgmgPYSD87tRu7QaAAQKwBh7Wc" />
  <meta name="viewport" content="width=device-width, initial-scale=1">
{base}  <title>{esc(page["title"])}</title>
  <meta name="description" content="{esc(page["description"])}">
  <meta name="robots" content="{robots}">
  <meta name="author" content="Usman Saif">
  <meta name="theme-color" content="#fbfaf8">
{canonical}  <link rel="icon" type="image/svg+xml" href="assets/icon.svg">
  <link rel="sitemap" type="application/xml" href="sitemap.xml">
  <link rel="alternate" type="text/plain" title="LLM summary" href="llms.txt">
  <meta property="og:type" content="{"article" if key == "whats-new" else "website"}">
  <meta property="og:site_name" content="{NAME}">
  <meta property="og:locale" content="en_US">
  <meta property="og:title" content="{esc(page["title"])}">
  <meta property="og:description" content="{esc(page["description"])}">
  <meta property="og:url" content="{url}">
  <meta property="og:image" content="{OG_IMAGE}">
  <meta property="og:image:width" content="1440">
  <meta property="og:image:height" content="880">
  <meta property="og:image:alt" content="gLabels Plus label editor designing a product label with a barcode">
  <meta name="twitter:card" content="summary_large_image">
  <meta name="twitter:title" content="{esc(page["title"])}">
  <meta name="twitter:description" content="{esc(page["description"])}">
  <meta name="twitter:image" content="{OG_IMAGE}">
  <meta name="twitter:image:alt" content="gLabels Plus label editor designing a product label with a barcode">
  <link rel="stylesheet" href="assets/style.css">
{ld}</head>'''


def crumbs(page):
    if page["file"] == "index.html":
        return ""
    return f'''    <nav class="crumbs wrap" aria-label="Breadcrumb">
      <ol><li><a href="./">Home</a></li><li aria-current="page">{esc(page["label"])}</li></ol>
    </nav>
'''


def render(key, page, body):
    for token, fn in PLACEHOLDERS.items():
        if token in body:
            body = body.replace(token, fn())
    leftover = re.findall(r"\{\{[a-z_]+\}\}", body)
    if leftover:
        raise SystemExit(f"{key}: unknown placeholders {leftover}")
    return (head(key, page) + '\n<body id="top">\n\n' + header(page["file"]) +
            '\n<main id="main">\n' + crumbs(page) + body.rstrip() + "\n\n</main>\n\n" + footer() +
            "</body>\n</html>\n")


# --------------------------------------------------------------------------
# Extra files
# --------------------------------------------------------------------------

def sitemap():
    prio = {"index": "1.0", "download": "0.9", "features": "0.9", "whats-new": "0.8",
            "barcode-labels": "0.8", "faq": "0.7", "donate": "0.5"}
    urls = []
    for key, page in PAGES.items():
        loc = SITE_URL + ("" if page["file"] == "index.html" else page["file"])
        image = ""
        if key == "index":
            image = "".join(f"\n    <image:image><image:loc>{SITE_URL}images/screenshots/{n}</image:loc></image:image>"
                            for n in ("editor.png", "welcome.png", "product-chooser.png", "print.png", "properties.png"))
        urls.append(f"  <url>\n    <loc>{loc}</loc>\n    <lastmod>{UPDATED}</lastmod>\n    <priority>{prio[key]}</priority>{image}\n  </url>")
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" '
            'xmlns:image="http://www.google.com/schemas/sitemap-image/1.1">\n' + "\n".join(urls) + "\n</urlset>\n")


def robots():
    bots = ["Googlebot", "Bingbot", "GPTBot", "OAI-SearchBot", "ChatGPT-User", "ClaudeBot", "Claude-User",
            "Claude-SearchBot", "PerplexityBot", "Perplexity-User", "Google-Extended", "Applebot-Extended",
            "CCBot", "DuckAssistBot"]
    rules = "\n\n".join(f"User-agent: {b}\nAllow: /" for b in bots)
    return f"# gLabels Plus website: all search engines and AI assistants are welcome.\n\nUser-agent: *\nAllow: /\n\n{rules}\n\nSitemap: {SITE_URL}sitemap.xml\n"


def llms_txt():
    pages = "\n".join(f"- [{p['label']}]({SITE_URL}{'' if p['file'] == 'index.html' else p['file']}): {p['description']}"
                      for p in PAGES.values())
    faq = "\n\n".join(f"### {q}\n{a}" for q, a in FAQ)
    presets = "\n".join(f"- {mm} mm ({inch} in): {use}" for mm, inch, use, *_ in PRESETS)
    notes = "\n".join(f"- {strip_tags(i)}" for i in RELEASE_NOTES[0][1])
    keys = ", ".join(f"{k} = {t}" for k, t in TOOL_KEYS)
    return f"""# {NAME}

> {NAME} is a free, open-source (GPL v3) label and business card designer for Linux, Windows and macOS. It makes barcode, QR code, address, shipping and product labels, name badges, CD/DVD labels and business cards, with mail merge from CSV, over 1,800 templates from 70+ brands and 24 thermal roll printer presets. Latest version: {VERSION} ({RELEASE_DATE}). Maintainer: Usman Saif (https://usmansaif.com). Based on gLabels by Jaye Evins.

## Key facts
- Price: free, no account, no trial, no watermark
- License: GNU GPL v3 or later
- Platforms: Linux (AppImage, .deb, .rpm, .tar.gz), Windows 10/11 (.exe installer, portable .zip), macOS (.dmg)
- Works offline; files are stored locally (Documents/gLabels Plus Projects)
- Built with Qt 6 and C++20
- Source code: {REPO_URL}
- Downloads: {RELEASES_URL}
- Single-key tool shortcuts: {keys}
- Urdu: Noto Nastaliq Urdu (regular and bold) is built in; Urdu labels display and print in true Nastaliq on every platform
- Custom barcode label printing solutions for businesses: contact the maintainer at {CONTACT_URL}

## Pages
{pages}

## What's new in {VERSION}
{notes}

## Barcode label presets (thermal roll printers)
{presets}

## FAQ
{faq}
"""


# --------------------------------------------------------------------------

def main():
    OUT.mkdir(exist_ok=True)
    for key, page in PAGES.items():
        body = (ROOT / "pages" / (key + ".html")).read_text(encoding="utf-8")
        (OUT / page["file"]).write_text(render(key, page, body), encoding="utf-8")
        print("wrote", page["file"])
    notfound = dict(file="404.html", label="Page not found", title="Page not found (404) | gLabels Plus",
                    description="This page doesn't exist. Go to the gLabels Plus home page, features, download or FAQ to find the free label designer.")
    body = (ROOT / "pages" / "404.html").read_text(encoding="utf-8")
    (OUT / "404.html").write_text(render("404", notfound, body), encoding="utf-8")
    (OUT / "sitemap.xml").write_text(sitemap(), encoding="utf-8")
    (OUT / "robots.txt").write_text(robots(), encoding="utf-8")
    (OUT / "llms.txt").write_text(llms_txt(), encoding="utf-8")
    (OUT / ".nojekyll").write_text("", encoding="utf-8")
    print("wrote 404.html sitemap.xml robots.txt llms.txt")


if __name__ == "__main__":
    main()
