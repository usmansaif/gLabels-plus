#!/usr/bin/env python3
#
#  generate-symbols.py
#
#  Writes the built-in symbol set (Tools > Insert Symbol): one SVG per symbol,
#  their index (symbols.json) and ../symbols.qrc.
#
#  Every symbol is pure black on transparent, drawn with solid shapes and
#  strokes of at least 3 units on a 48 x 48 grid, so it stays sharp on 203 dpi
#  direct thermal printers even at 5 mm.  No grey, gradients or thin hairlines.
#
#  Run it again after editing a symbol:  python3 generate-symbols.py
#

import json
import math
import os

HERE = os.path.dirname( os.path.abspath( __file__ ) )

K = "#000"
W = "#fff"
STROKE = 'fill="none" stroke="#000" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"'
WSTROKE = 'fill="none" stroke="#fff" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"'

SYMBOLS = []


def symbol( sid, name, category, keywords, body ):
    SYMBOLS.append( dict( id=sid, name=name, category=category, keywords=keywords, body=body ) )


def arrow( x1, y1, x2, y2, head=9, width=3.5 ):
    """Line from (x1,y1) to (x2,y2) with a filled head at the end."""
    a = math.atan2( y2 - y1, x2 - x1 )
    bx, by = x2 - head * math.cos( a ), y2 - head * math.sin( a )
    px, py = -math.sin( a ) * head * 0.6, math.cos( a ) * head * 0.6
    return ( '<path d="M%.2f %.2f L%.2f %.2f" %s/>' % ( x1, y1, bx, by, STROKE.replace( "3.5", str( width ) ) )
             + '<path d="M%.2f %.2f L%.2f %.2f L%.2f %.2f Z" fill="#000" stroke="#000" stroke-width="1.5" '
               'stroke-linejoin="round"/>' % ( x2, y2, bx + px, by + py, bx - px, by - py ) )


def star_points( cx, cy, r_out, r_in, n=5, rot=-90 ):
    pts = []
    for i in range( 2 * n ):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians( rot + i * 180 / n )
        pts.append( "%.2f,%.2f" % ( cx + r * math.cos( a ), cy + r * math.sin( a ) ) )
    return " ".join( pts )


SLASH = ( '<circle cx="24" cy="24" r="20" %s/>' % STROKE
          + '<path d="M10 10 L38 38" %s/>' % STROKE )

# ---------------------------------------------------------------------------
# Handling (cartons and parcels)
# ---------------------------------------------------------------------------
symbol( "fragile", "Fragile", "Handling", "glass breakable care",
        '<path d="M13 5 H35 L34 17 C33 25 29 29 24 29 C19 29 15 25 14 17 Z"/>'
        '<path d="M23 5 L26.5 12 L21.5 16.5 L25 22" fill="none" stroke="#fff" stroke-width="2.6" '
        'stroke-linecap="round" stroke-linejoin="round"/>'
        '<path d="M24 29 V41 M15 43 H33" %s/>' % STROKE )
symbol( "this-way-up", "This Way Up", "Handling", "arrows upright orientation top",
        arrow( 15, 41, 15, 5, head=11 ) + arrow( 33, 41, 33, 5, head=11 )
        + '<path d="M5 44 H43" %s/>' % STROKE )
symbol( "keep-dry", "Keep Dry", "Handling", "umbrella rain water moisture",
        '<path d="M5 25 C5 15 13 9 24 9 C35 9 43 15 43 25 C40 22 37 22 34 25 C31 22 27 22 24 25 '
        'C21 22 17 22 14 25 C11 22 8 22 5 25 Z"/>'
        '<path d="M24 25 V39 C24 44 17 44 17 39" %s/>' % STROKE
        + '<path d="M12 2 L10.5 5.5 M24 1.5 V4.5 M36 2 L37.5 5.5" %s/>' % STROKE.replace( "3.5", "3" ) )
symbol( "keep-away-from-heat", "Keep Away from Heat", "Handling", "sun sunlight hot temperature",
        '<circle cx="24" cy="24" r="6.5"/>'
        + "".join( '<path d="M%.2f %.2f L%.2f %.2f" %s/>'
                   % ( 24 + 10 * math.cos( a ), 24 + 10 * math.sin( a ), 24 + 14 * math.cos( a ),
                       24 + 14 * math.sin( a ), STROKE.replace( "3.5", "3" ) )
                   for a in [ math.radians( d ) for d in range( 0, 360, 45 ) ] )
        + SLASH )
symbol( "do-not-stack", "Do Not Stack", "Handling", "stacking weight top load",
        '<rect x="8" y="27" width="32" height="16" rx="1.5"/>'
        '<rect x="14" y="5" width="20" height="15" rx="1.5" %s/>' % STROKE
        + '<path d="M9 4 L39 21 M39 4 L9 21" %s/>' % STROKE )
symbol( "temperature", "Temperature", "Handling", "thermometer limit storage cold hot",
        '<rect x="19" y="4" width="10" height="30" rx="5" %s/>' % STROKE
        + '<circle cx="24" cy="38" r="7"/><path d="M24 36 V16" %s/>' % STROKE
        + '<path d="M34 10 H39 M34 17 H39 M34 24 H39" %s/>' % STROKE.replace( "3.5", "3" ) )
def bent_arrow( start_deg, sweep_deg, r=16, cx=24, cy=25, head=8.5 ):
    """Arc round (cx, cy), clockwise from start_deg, with a filled head at its end."""
    a0 = math.radians( start_deg )
    a1 = math.radians( start_deg + sweep_deg )
    x0, y0 = cx + r * math.cos( a0 ), cy + r * math.sin( a0 )
    x1, y1 = cx + r * math.cos( a1 ), cy + r * math.sin( a1 )
    tx, ty = -math.sin( a1 ), math.cos( a1 )               # clockwise tangent at the end
    nx, ny = math.cos( a1 ), math.sin( a1 )
    hx, hy = x1 + tx * head * 0.75, y1 + ty * head * 0.75
    return ( '<path d="M%.2f %.2f A%g %g 0 0 1 %.2f %.2f" %s/>' % ( x0, y0, r, r, x1, y1, STROKE.replace( "3.5", "4.5" ) )
             + '<path d="M%.2f %.2f L%.2f %.2f L%.2f %.2f Z" stroke="#000" stroke-width="1.5" stroke-linejoin="round"/>'
             % ( hx, hy, x1 + nx * head * 0.6, y1 + ny * head * 0.6, x1 - nx * head * 0.6, y1 - ny * head * 0.6 ) )


symbol( "recycle", "Recyclable", "Handling", "recycle environment green arrows",
        bent_arrow( -80, 75 ) + bent_arrow( 40, 75 ) + bent_arrow( 160, 75 ) )
symbol( "handle-with-care", "Handle with Care", "Handling", "careful gentle hands",
        '<path d="M4 30 C10 30 14 33 18 33 H27 C30 33 30 29 27 29 H20" %s/>' % STROKE
        + '<path d="M4 41 H16 L32 41 C35 41 38 39 41 36 L44 33 C45 31 43 29 41 30 L33 34" %s/>' % STROKE
        + '<path d="M24 23 L18.5 17.5 C15.5 14.5 17.5 9 21.5 9 C23 9 24 10 24 11 C24 10 25 9 26.5 9 '
          'C30.5 9 32.5 14.5 29.5 17.5 Z"/>' )

# ---------------------------------------------------------------------------
# Food
# ---------------------------------------------------------------------------
flake = ""
for d in range( 0, 360, 60 ):
    a = math.radians( d - 90 )
    tip = ( 24 + 20 * math.cos( a ), 24 + 20 * math.sin( a ) )
    flake += '<path d="M24 24 L%.2f %.2f" %s/>' % ( tip[ 0 ], tip[ 1 ], STROKE )
    tx, ty = 24 + 12 * math.cos( a ), 24 + 12 * math.sin( a )
    for side in ( -1, 1 ):
        b = a + side * math.radians( 45 )
        flake += '<path d="M%.2f %.2f L%.2f %.2f" %s/>' % ( tx, ty, tx + 7 * math.cos( b ), ty + 7 * math.sin( b ),
                                                         STROKE.replace( "3.5", "3" ) )
symbol( "keep-refrigerated", "Keep Refrigerated", "Food", "snowflake cold frozen fridge freezer chilled", flake )
symbol( "vegetarian", "Vegetarian Mark", "Food", "veg vegetarian dot square",
        '<rect x="6" y="6" width="36" height="36" rx="2" %s/>' % STROKE + '<circle cx="24" cy="24" r="10"/>' )
symbol( "non-vegetarian", "Non-Vegetarian Mark", "Food", "non veg meat triangle square",
        '<rect x="6" y="6" width="36" height="36" rx="2" %s/>' % STROKE + '<path d="M24 13 L35 33 H13 Z"/>' )
wheat = '<path d="M24 42 V12" %s/>' % STROKE.replace( "3.5", "3" )
for i in range( 4 ):
    y = 15 + i * 7
    for s in ( -1, 1 ):
        wheat += ( '<ellipse cx="%g" cy="%g" rx="3" ry="5.5" transform="rotate(%d %g %g)"/>'
                   % ( 24 + s * 4.5, y, s * 35, 24 + s * 4.5, y ) )
wheat += '<ellipse cx="24" cy="9" rx="2.8" ry="5"/>'
symbol( "gluten-free", "Gluten Free", "Food", "wheat gluten free allergen", wheat
        .replace( "<path", "<path", 1 ) + SLASH.replace( "M10 10 L38 38", "M10 38 L38 10" ) )
symbol( "spicy", "Spicy", "Food", "chili chilli pepper hot",
        '<path d="M13 15 C22 13 31 19 35 28 C38 35 40 40 43 44 C30 43 17 35 13 24 C12 21 12 18 13 15 Z"/>'
        '<path d="M8 17 C10 11 16 10 20 14 C16 15 13 17 12 21 Z"/>'
        '<path d="M12 13 C12 8 15 5 20 4" %s/>' % STROKE )
symbol( "microwave-safe", "Microwave Safe", "Food", "microwave heat reheat oven",
        '<rect x="4" y="10" width="40" height="28" rx="3" %s/>' % STROKE
        + '<path d="M10 20 C13 17 15 23 18 20 C21 17 23 23 26 20 M10 28 C13 25 15 31 18 28 C21 25 23 31 26 28" '
          'fill="none" stroke="#000" stroke-width="2.6" stroke-linecap="round"/>'
        + '<path d="M33 16 V32" %s/><circle cx="38.5" cy="18" r="2"/><circle cx="38.5" cy="25" r="2"/>' % STROKE )
symbol( "natural", "Natural", "Food", "leaf organic plant vegan eco green",
        '<path d="M8 40 C8 18 21 7 42 7 C42 28 30 40 8 40 Z"/>'
        '<path d="M10 38 L33 15" %s/>' % WSTROKE.replace( "3.5", "2.6" ) )
symbol( "hot-drink", "Hot Drink", "Food", "coffee tea cup mug cafe",
        '<path d="M7 19 H33 V33 C33 39 29 43 23 43 H17 C11 43 7 39 7 33 Z"/>'
        '<path d="M33 23 H36 C40 23 40 33 36 33 H33" %s/>' % STROKE
        + '<path d="M14 13 C12 10 16 8 14 4 M21 13 C19 10 23 8 21 4 M28 13 C26 10 30 8 28 4" %s/>'
        % STROKE.replace( "3.5", "2.8" ) )
symbol( "cupcake", "Cupcake", "Food", "bakery cake cupcake dessert sweet",
        '<path d="M13 27 H35 L32 43 H16 Z"/>'
        '<path d="M20 29 L21 41 M28 29 L27 41" %s/>' % WSTROKE.replace( "3.5", "2" )
        + '<path d="M11 27 C11 19 17 15 24 15 C31 15 37 19 37 27 Z" %s/>' % STROKE
        + '<circle cx="24" cy="9.5" r="3.5"/>' )

# ---------------------------------------------------------------------------
# Contact
# ---------------------------------------------------------------------------
symbol( "phone", "Phone", "Contact", "telephone call landline number",
        '<path d="M14 5 L20 5 L23 15 L18.5 18.5 C21 24.5 23.5 27 29.5 29.5 L33 25 L43 28 L43 34 '
        'C43 38.5 39.5 43 35 43 C18 42 6 30 5 13 C5 8.5 9.5 5 14 5 Z"/>' )
symbol( "mobile", "Mobile", "Contact", "cell phone smartphone sms",
        '<rect x="13" y="3" width="22" height="42" rx="4" %s/>' % STROKE
        + '<path d="M21 9 H27" %s/><circle cx="24" cy="38.5" r="2.4"/>' % STROKE )
symbol( "email", "Email", "Contact", "mail envelope letter",
        '<rect x="5" y="11" width="38" height="26" rx="2.5" %s/>' % STROKE
        + '<path d="M6 13 L24 27 L42 13" %s/>' % STROKE )
symbol( "website", "Website", "Contact", "web globe internet url world",
        '<circle cx="24" cy="24" r="19" %s/><ellipse cx="24" cy="24" rx="8" ry="19" %s/>' % ( STROKE, STROKE )
        + '<path d="M5 24 H43 M8.5 14 H39.5 M8.5 34 H39.5" %s/>' % STROKE.replace( "3.5", "3" ) )
symbol( "location", "Location", "Contact", "address map pin place shop",
        '<path d="M24 45 C24 45 9 29 9 18.5 A15 15 0 0 1 39 18.5 C39 29 24 45 24 45 Z"/>'
        '<circle cx="24" cy="18.5" r="5.5" fill="#fff"/>' )
symbol( "clock", "Opening Hours", "Contact", "clock time hours open",
        '<circle cx="24" cy="24" r="19" %s/>' % STROKE + '<path d="M24 12 V24 L32 29" %s/>' % STROKE )
cal = '<rect x="5" y="9" width="38" height="34" rx="3" %s/><rect x="5" y="9" width="38" height="9" rx="3"/>' % STROKE
cal += '<path d="M15 4 V12 M33 4 V12" %s/>' % STROKE
for i in range( 3 ):
    for j in range( 2 ):
        cal += '<rect x="%g" y="%g" width="5" height="5" rx="1"/>' % ( 12 + i * 10, 24 + j * 9 )
symbol( "calendar", "Date", "Contact", "calendar date day best before made on", cal )
symbol( "person", "Person", "Contact", "user name contact customer",
        '<circle cx="24" cy="14" r="8"/><path d="M8 43 C8 32 15 26 24 26 C33 26 40 32 40 43 Z"/>' )
symbol( "message", "Message", "Contact", "chat bubble message text sms",
        '<path d="M10 7 H38 Q44 7 44 13 V28 Q44 34 38 34 H22 L12 42 L14 34 H10 Q4 34 4 28 V13 Q4 7 10 7 Z"/>'
        '<circle cx="15" cy="20.5" r="2.6" fill="#fff"/><circle cx="24" cy="20.5" r="2.6" fill="#fff"/>'
        '<circle cx="33" cy="20.5" r="2.6" fill="#fff"/>' )
symbol( "home", "Home", "Contact", "house home address shop store",
        '<path d="M4 24 L24 6 L44 24 H38 V43 H10 V24 Z"/><rect x="20" y="30" width="8" height="13" fill="#fff"/>' )

# ---------------------------------------------------------------------------
# Shop and delivery
# ---------------------------------------------------------------------------
symbol( "delivery", "Delivery", "Shop & delivery", "truck van shipping courier",
        '<rect x="3" y="11" width="26" height="22" rx="1.5"/><path d="M29 18 H37 L44 26 V33 H29 Z"/>'
        '<path d="M32 21 H36 L40 26 H32 Z" fill="#fff"/>'
        '<circle cx="12" cy="36" r="5.5" stroke="#fff" stroke-width="2"/><circle cx="36" cy="36" r="5.5" '
        'stroke="#fff" stroke-width="2"/>' )
symbol( "package", "Package", "Shop & delivery", "box parcel carton shipment",
        '<path d="M24 5 L42 13 V35 L24 43 L6 35 V13 Z M6 13 L24 21 L42 13 M24 21 V43" %s/>' % STROKE
        + '<path d="M15 9 L33 17 V24" fill="none" stroke="#000" stroke-width="3" stroke-linejoin="round"/>' )
symbol( "shopping-bag", "Shopping Bag", "Shop & delivery", "bag shop store buy",
        '<path d="M9 16 H39 L37 43 H11 Z"/><path d="M17 20 V12 A7 7 0 0 1 31 12 V20" %s/>'
        % WSTROKE.replace( "#fff", "#000" ).replace( "3.5", "3.2" ) )
symbol( "price-tag", "Price Tag", "Shop & delivery", "price tag sale label",
        '<path d="M6 6 H23 L43 26 L26 43 L6 23 Z"/><circle cx="15" cy="15" r="3.5" fill="#fff"/>' )
symbol( "discount", "Discount", "Shop & delivery", "percent sale offer off promo",
        '<circle cx="24" cy="24" r="21"/><circle cx="17" cy="17" r="4" fill="#fff"/>'
        '<circle cx="31" cy="31" r="4" fill="#fff"/><path d="M32 15 L16 33" %s/>' % WSTROKE )
symbol( "gift", "Gift", "Shop & delivery", "present gift wrap",
        '<rect x="8" y="21" width="32" height="22" rx="1.5"/><rect x="5" y="13" width="38" height="8" rx="1.5"/>'
        '<path d="M24 13 V43" stroke="#fff" stroke-width="4"/>'
        '<path d="M24 13 C18 3 9 7 14 13 M24 13 C30 3 39 7 34 13" %s/>' % STROKE )
symbol( "cut-here", "Cut Here", "Shop & delivery", "scissors cut tear coupon",
        '<circle cx="10" cy="13" r="6" %s/><circle cx="10" cy="35" r="6" %s/>' % ( STROKE, STROKE )
        + '<path d="M15 16 L43 31 M15 32 L43 17" %s/>' % STROKE )
symbol( "quality", "Quality Badge", "Shop & delivery", "award rosette quality best premium",
        '<path d="M16 26 L11 43 L17 40 L21 45 L24 32 Z M32 26 L37 43 L31 40 L27 45 L24 32 Z"/>'
        '<circle cx="24" cy="18" r="14"/>'
        '<polygon points="%s" fill="#fff"/>' % star_points( 24, 18, 8, 3.5 ) )

# ---------------------------------------------------------------------------
# Marks and shapes
# ---------------------------------------------------------------------------
symbol( "star", "Star", "Marks & shapes", "star rating favourite",
        '<polygon points="%s" stroke="#000" stroke-width="2" stroke-linejoin="round"/>' % star_points( 24, 25.5, 21, 9 ) )
symbol( "sparkle", "Sparkle", "Marks & shapes", "sparkle shine new twinkle",
        '<path d="M24 2 C25.5 15 33 22.5 46 24 C33 25.5 25.5 33 24 46 C22.5 33 15 25.5 2 24 '
        'C15 22.5 22.5 15 24 2 Z"/>' )
symbol( "heart", "Heart", "Marks & shapes", "love heart like thank you",
        '<path d="M24 42 L21 39.3 C10.5 29.8 4 23.9 4 16.6 C4 10.7 8.6 6 14.5 6 C17.8 6 21 7.6 24 10.1 '
        'C27 7.6 30.2 6 33.5 6 C39.4 6 44 10.7 44 16.6 C44 23.9 37.5 29.8 27 39.3 Z"/>' )
symbol( "check", "Check Mark", "Marks & shapes", "tick yes ok done approved",
        '<path d="M7 25 L18 36 L41 12" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round" '
        'stroke-linejoin="round"/>' )
symbol( "check-circle", "Check Circle", "Marks & shapes", "tick approved passed verified",
        '<circle cx="24" cy="24" r="21"/><path d="M13 24.5 L21 32 L35 17" fill="none" stroke="#fff" '
        'stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>' )
symbol( "cross", "Cross", "Marks & shapes", "x no wrong cancel",
        '<path d="M10 10 L38 38 M38 10 L10 38" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>' )
symbol( "checkbox", "Check Box", "Marks & shapes", "box tick option empty square",
        '<rect x="7" y="7" width="34" height="34" rx="3" %s/>' % STROKE )
symbol( "checkbox-ticked", "Ticked Box", "Marks & shapes", "box tick selected checked",
        '<rect x="7" y="7" width="34" height="34" rx="3" %s/>' % STROKE
        + '<path d="M14 24.5 L21.5 32 L35 16" fill="none" stroke="#000" stroke-width="4.5" stroke-linecap="round" '
          'stroke-linejoin="round"/>' )
symbol( "warning", "Warning", "Marks & shapes", "caution attention alert exclamation",
        '<path d="M24 4 L45 41 Q46 44 43 44 H5 Q2 44 3 41 Z"/>'
        '<path d="M24 17 V30" %s/><circle cx="24" cy="36.5" r="2.6" fill="#fff"/>' % WSTROKE.replace( "3.5", "4.5" ) )
symbol( "info", "Information", "Marks & shapes", "info i note help",
        '<circle cx="24" cy="24" r="21"/><circle cx="24" cy="14" r="3" fill="#fff"/>'
        '<path d="M24 21 V35" %s/>' % WSTROKE.replace( "3.5", "5" ) )
symbol( "circle", "Circle", "Marks & shapes", "dot bullet round",
        '<circle cx="24" cy="24" r="18"/>' )
symbol( "square", "Square", "Marks & shapes", "block bullet",
        '<rect x="7" y="7" width="34" height="34" rx="2"/>' )

# ---------------------------------------------------------------------------
# Arrows
# ---------------------------------------------------------------------------
ARROW = '<path d="M4 19 H26 V9 L44 24 L26 39 V29 H4 Z" stroke="#000" stroke-width="2" stroke-linejoin="round"/>'
for sid, name, rot, kw in [ ( "arrow-right", "Arrow Right", 0, "right next forward" ),
                            ( "arrow-down", "Arrow Down", 90, "down below" ),
                            ( "arrow-left", "Arrow Left", 180, "left back" ),
                            ( "arrow-up", "Arrow Up", 270, "up top" ) ]:
    symbol( sid, name, "Arrows", "arrow direction " + kw,
            ARROW if rot == 0 else '<g transform="rotate(%d 24 24)">%s</g>' % ( rot, ARROW ) )
symbol( "arrow-both", "Arrows Both Ways", "Arrows", "arrow double left right width",
        '<path d="M2 24 L14 12 V19 H34 V12 L46 24 L34 36 V29 H14 V36 Z" stroke="#000" stroke-width="2" '
        'stroke-linejoin="round"/>' )
symbol( "open-here", "Open Here", "Arrows", "open tear pull here",
        arrow( 6, 40, 30, 16, head=12, width=4.5 ) + '<path d="M28 6 H42 V20" %s/>' % STROKE )


# ---------------------------------------------------------------------------
os.makedirs( HERE, exist_ok=True )
index = []
for s in SYMBOLS:
    svg = ( '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="48" height="48">'
            '<g fill="#000">%s</g></svg>\n' % s[ "body" ] )
    with open( os.path.join( HERE, s[ "id" ] + ".svg" ), "w", encoding="utf-8" ) as f:
        f.write( svg )
    index.append( { k: s[ k ] for k in ( "id", "name", "category", "keywords" ) } )

with open( os.path.join( HERE, "symbols.json" ), "w", encoding="utf-8" ) as f:
    json.dump( index, f, indent=2, ensure_ascii=False )
    f.write( "\n" )

with open( os.path.join( HERE, "..", "symbols.qrc" ), "w", encoding="utf-8" ) as f:
    f.write( '<!DOCTYPE RCC>\n\n<RCC version="1.0">\n<qresource>\n\n' )
    f.write( "\t<file>symbols/symbols.json</file>\n" )
    for s in SYMBOLS:
        f.write( "\t<file>symbols/%s.svg</file>\n" % s[ "id" ] )
    f.write( "\n</qresource>\n</RCC>\n" )

print( "Wrote %d symbols" % len( SYMBOLS ) )
