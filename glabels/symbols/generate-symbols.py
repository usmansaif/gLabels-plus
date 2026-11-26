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
# Shared pieces for the hazard, prohibition and care symbols
# ---------------------------------------------------------------------------
def S( w ):
    """Black stroke of width w (never below 3, so it holds on 203 dpi printers)."""
    return STROKE.replace( "3.5", str( max( 3, w ) ) )


def WS( w ):
    return WSTROKE.replace( "3.5", str( w ) )


TRIANGLE = '<path d="M24 4.5 L44.5 41.5 H3.5 Z" %s/>' % S( 3.5 )


def hazard( inner ):
    """Warning triangle around a pictogram drawn in its lower middle."""
    return TRIANGLE + inner


def prohibited( inner ):
    """Pictogram struck through by the red-sign style circle and slash."""
    return inner + '<path d="M10 10 L38 38" fill="none" stroke="#fff" stroke-width="8"/>' + SLASH


def flake_at( cx, cy, r, w=3.5 ):
    out = ""
    for d in range( 0, 180, 60 ):
        a = math.radians( d + 90 )
        out += '<path d="M%.2f %.2f L%.2f %.2f" %s/>' % ( cx - r * math.cos( a ), cy - r * math.sin( a ),
                                                         cx + r * math.cos( a ), cy + r * math.sin( a ), S( w ) )
    return out


def dots( n, cx, cy, gap=7.5, r=2.6 ):
    x0 = cx - gap * ( n - 1 ) / 2
    return "".join( '<circle cx="%.2f" cy="%.2f" r="%g"/>' % ( x0 + i * gap, cy, r ) for i in range( n ) )


def ring_sector( cx, cy, r0, r1, a0, a1 ):
    p = lambda r, a: ( cx + r * math.cos( math.radians( a ) ), cy + r * math.sin( math.radians( a ) ) )
    ( x0, y0 ), ( x1, y1 ), ( x2, y2 ), ( x3, y3 ) = p( r1, a0 ), p( r1, a1 ), p( r0, a1 ), p( r0, a0 )
    return ( '<path d="M%.2f %.2f A%g %g 0 0 1 %.2f %.2f L%.2f %.2f A%g %g 0 0 0 %.2f %.2f Z"/>'
             % ( x0, y0, r1, r1, x1, y1, x2, y2, r0, r0, x3, y3 ) )


CROSS_OUT = '<path d="M7 7 L41 41 M41 7 L7 41" %s/>' % S( 3.5 )

# ---------------------------------------------------------------------------
# Handling (more)
# ---------------------------------------------------------------------------
symbol( "center-of-gravity", "Centre of Gravity", "Handling", "center gravity balance weight lift",
        '<circle cx="24" cy="24" r="17" %s/>' % S( 3.5 )
        + '<path d="M24 24 V7 A17 17 0 0 1 41 24 Z"/><path d="M24 24 V41 A17 17 0 0 1 7 24 Z"/>' )
symbol( "no-hooks", "Do Not Use Hooks", "Handling", "hook hand hooks prohibited",
        prohibited( '<path d="M26 7 V29 A7.5 7.5 0 0 1 11 29 V26" %s/>' % S( 4 ) ) )
symbol( "keep-away-from-water", "Keep Away from Water", "Handling", "water wet drop liquid moisture",
        prohibited( '<path d="M24 8 C24 8 34 20 34 27 A10 10 0 0 1 14 27 C14 20 24 8 24 8 Z"/>' ) )
symbol( "no-knife", "Do Not Use Knife", "Handling", "knife cutter blade open carefully",
        prohibited( '<path d="M13 35 L21 27" %s/>' % S( 6 )
                    + '<path d="M21 27 L35 13 L36.5 18 L24.5 30 Z" stroke="#000" stroke-width="1.5" '
                      'stroke-linejoin="round"/>' ) )

# ---------------------------------------------------------------------------
# Warning (hazard triangles and prohibition signs)
# ---------------------------------------------------------------------------
symbol( "hazard-electric", "Electrical Hazard", "Warning", "electric voltage shock lightning danger",
        hazard( '<path d="M26.5 15 L17 30 H23 L21 39 L31 24 H25 L27.5 15 Z" stroke="#000" stroke-width="1" '
                'stroke-linejoin="round"/>' ) )
symbol( "hazard-flammable", "Flammable", "Warning", "fire flame burn flammable danger",
        hazard( '<path d="M24 14 C28 20 33 24 33 31 C33 36 29 39 24 39 C19 39 15 36 15 31 C15 27 17 25 19 22 '
                'C19 26 21 28 22.5 28 C21 23 22 18 24 14 Z"/>' ) )
symbol( "hazard-hot-surface", "Hot Surface", "Warning", "hot heat surface burn temperature",
        hazard( '<rect x="14" y="33" width="20" height="4.5" rx="1"/>'
                + "".join( '<path d="M%g 30 C%g 27.5 %g 25 %g 22" %s/>' % ( x, x - 3, x + 3, x, S( 3 ) )
                           for x in ( 19, 24, 29 ) ) ) )
symbol( "hazard-laser", "Laser", "Warning", "laser beam radiation light",
        hazard( '<circle cx="22" cy="30" r="3.5"/>'
                + "".join( '<path d="M%.2f %.2f L%.2f %.2f" %s/>'
                           % ( 22 + 6 * math.cos( a ), 30 + 6 * math.sin( a ), 22 + 9.5 * math.cos( a ),
                               30 + 9.5 * math.sin( a ), S( 3 ) )
                           for a in [ math.radians( d ) for d in range( 45, 360, 45 ) ] )
                + '<path d="M22 30 H36" %s/>' % S( 3 ) ) )
symbol( "hazard-magnetic", "Magnetic Field", "Warning", "magnet magnetic field",
        hazard( '<path d="M19 21 V29 A5 5 0 0 0 29 29 V21" fill="none" stroke="#000" stroke-width="5"/>' ) )
symbol( "hazard-radiation", "Radiation", "Warning", "radiation radioactive nuclear trefoil",
        hazard( '<circle cx="24" cy="29" r="2.6"/>'
                + "".join( ring_sector( 24, 29, 4.5, 11, a - 30, a + 30 ) for a in ( -90, 30, 150 ) ) ) )
symbol( "hazard-toxic", "Toxic", "Warning", "poison toxic skull danger",
        hazard( '<path d="M15.5 35.5 L32.5 40 M32.5 35.5 L15.5 40" %s/>' % S( 3 )
                + '<circle cx="24" cy="25.5" r="7.5"/><rect x="20" y="29" width="8" height="6.5" rx="1.5"/>'
                  '<circle cx="21" cy="25" r="2.1" fill="#fff"/><circle cx="27" cy="25" r="2.1" fill="#fff"/>' ) )
symbol( "hazard-explosive", "Explosive", "Warning", "explosion explosive bang burst",
        hazard( '<polygon points="%s"/>' % star_points( 24, 29.5, 11, 5, n=8 ) ) )
symbol( "hazard-cold", "Low Temperature", "Warning", "cold freeze frost low temperature",
        hazard( flake_at( 24, 29.5, 9, 3 ) ) )
symbol( "hazard-battery", "Battery Hazard", "Warning", "battery lithium cell charge",
        hazard( '<rect x="15.5" y="25.5" width="15" height="9" rx="1.5" %s/>' % S( 3 )
                + '<rect x="31" y="28" width="2.8" height="4"/><rect x="18" y="28" width="7" height="4"/>' ) )
symbol( "no-smoking", "No Smoking", "Warning", "smoking cigarette smoke prohibited",
        prohibited( '<rect x="8" y="25" width="25" height="6" rx="1"/><rect x="34.5" y="25" width="4.5" height="6" rx="1"/>'
                    '<path d="M37 21 C35 18 39 16 37 12" %s/>' % S( 3 ) ) )
symbol( "no-open-flame", "No Open Flame", "Warning", "fire flame open light prohibited",
        prohibited( '<path d="M24 9 C30 17 35 22 35 30 C35 36 30 40 24 40 C18 40 13 36 13 30 C13 25 16 22 18 18 '
                    'C18 23 20 26 22 26 C21 20 22 14 24 9 Z"/>' ) )
symbol( "no-food-drink", "No Food or Drink", "Warning", "food drink eating cup prohibited",
        prohibited( '<path d="M15 15 H33 L31 39 H17 Z"/><path d="M26 15 L30 6" %s/>' % S( 3 ) ) )
symbol( "no-phone", "No Mobile Phones", "Warning", "phone mobile cell prohibited",
        prohibited( '<rect x="17" y="8.5" width="14" height="31" rx="3" %s/>' % S( 3.5 )
                    + '<circle cx="24" cy="34" r="1.8"/>' ) )

# ---------------------------------------------------------------------------
# Care (textile care, ISO 3758 style)
# ---------------------------------------------------------------------------
TUB = ( '<path d="M4 12 L9 40 H39 L44 12" %s/>' % S( 3.5 )
        + '<path d="M7 19.5 Q11.25 15.5 15.5 19.5 T24 19.5 T32.5 19.5 T41 19.5" %s/>' % S( 3 ) )
for n, sid, name, temp in [ ( 1, "wash-30", "Wash at 30 °C", "cold 30" ),
                            ( 2, "wash-40", "Wash at 40 °C", "warm 40" ),
                            ( 3, "wash-50", "Wash at 50 °C", "hot 50" ) ]:
    symbol( sid, name, "Care", "wash washing machine laundry " + temp, TUB + dots( n, 24, 30.5 ) )
symbol( "hand-wash", "Hand Wash", "Care", "hand wash gentle laundry",
        TUB + '<rect x="17" y="3" width="15" height="12" rx="3"/>'
        + "".join( '<rect x="%.2f" y="12" width="2.6" height="17" rx="1.3"/>' % ( 17.6 + i * 3.6 ) for i in range( 4 ) ) )
symbol( "do-not-wash", "Do Not Wash", "Care", "no wash washing laundry", TUB + CROSS_OUT )
BLEACH = '<path d="M24 6 L44 40 H4 Z" %s/>' % S( 3.5 )
symbol( "bleach", "Bleach Allowed", "Care", "bleach chlorine laundry", BLEACH )
symbol( "do-not-bleach", "Do Not Bleach", "Care", "no bleach chlorine laundry", BLEACH + CROSS_OUT )
DRYER = '<rect x="6" y="6" width="36" height="36" rx="2" %s/>' % S( 3.5 )
DRUM = '<circle cx="24" cy="24" r="12.5" %s/>' % S( 3.5 )
symbol( "tumble-dry", "Tumble Dry", "Care", "tumble dryer dry laundry", DRYER + DRUM )
symbol( "tumble-dry-low", "Tumble Dry Low", "Care", "tumble dryer low heat dry laundry",
        DRYER + DRUM + '<circle cx="24" cy="24" r="3"/>' )
symbol( "do-not-tumble-dry", "Do Not Tumble Dry", "Care", "no tumble dryer laundry", DRYER + DRUM + CROSS_OUT )
symbol( "line-dry", "Line Dry", "Care", "line hang dry laundry",
        DRYER + '<path d="M8 9 C15 20 33 20 40 9" %s/>' % S( 3.5 ) )
symbol( "dry-flat", "Dry Flat", "Care", "flat dry laundry", DRYER + '<path d="M13 24 H35" %s/>' % S( 3.5 ) )
IRON = ( '<path d="M5 38 Q8 24 18 22 H41 L43 38 Z" %s/>' % S( 3.5 )
         + '<path d="M22 22 V14 H39 V22" %s/>' % S( 3.5 ) )
for n, sid, name, temp in [ ( 1, "iron-low", "Iron Low (110 °C)", "low cool 110" ),
                            ( 2, "iron-medium", "Iron Medium (150 °C)", "medium warm 150" ),
                            ( 3, "iron-high", "Iron High (200 °C)", "high hot 200" ) ]:
    symbol( sid, name, "Care", "iron ironing laundry " + temp, IRON + dots( n, 27, 31, gap=7 ) )
symbol( "do-not-iron", "Do Not Iron", "Care", "no iron ironing laundry", IRON + CROSS_OUT )
CLEAN = '<circle cx="24" cy="24" r="16" %s/>' % S( 3.5 )
symbol( "dry-clean", "Dry Clean", "Care", "dry clean professional laundry", CLEAN )
symbol( "do-not-dry-clean", "Do Not Dry Clean", "Care", "no dry clean laundry", CLEAN + CROSS_OUT )

# ---------------------------------------------------------------------------
# Food (more)
# ---------------------------------------------------------------------------
LEAF = ( '<path d="M9 39 C9 19 21 9 40 8 C40 27 29 39 9 39 Z"/>'
         '<path d="M11.5 36.5 L31 17" %s/>' % WS( 3 ) )
symbol( "organic", "Organic", "Food", "organic natural leaf bio eco", LEAF )
symbol( "vegan", "Vegan", "Food", "vegan plant based leaf",
        '<circle cx="24" cy="24" r="20" %s/>' % S( 3.5 )
        + '<path d="M14 34 C14 21 22 14 34 13.5 C34 26 27 34 14 34 Z"/><path d="M16.5 31.5 L28.5 19.5" %s/>' % WS( 3 ) )
symbol( "contains-nuts", "Contains Nuts", "Food", "nuts peanut allergen allergy",
        '<g transform="rotate(-35 24 24)"><circle cx="24" cy="14.5" r="9"/><circle cx="24" cy="33.5" r="9"/>'
        '<rect x="18.5" y="14.5" width="11" height="19"/>'
        '<path d="M20 13 H28 M20 35 H28" %s/></g>' % WS( 3 ) )
symbol( "dairy", "Contains Milk", "Food", "milk dairy lactose carton allergen",
        '<path d="M15 17 L19 8 H29 L33 17 V42 H15 Z" %s/>' % S( 3.5 )
        + '<path d="M15 17 H33"  %s/><rect x="19" y="24" width="10" height="11"/>' % S( 3.5 ) )
symbol( "egg", "Contains Egg", "Food", "egg allergen",
        '<path d="M24 5 C33 5 38 19 38 28 C38 37 32 43 24 43 C16 43 10 37 10 28 C10 19 15 5 24 5 Z"/>' )
symbol( "fish", "Contains Fish", "Food", "fish seafood allergen",
        '<path d="M4 24 C11 13 25 12 33 24 C25 36 11 35 4 24 Z"/><path d="M32 24 L44 14 V34 Z"/>'
        '<circle cx="12" cy="22" r="2.4" fill="#fff"/>' )
symbol( "keep-frozen", "Keep Frozen", "Food", "frozen freezer freeze cold -18",
        '<rect x="6" y="6" width="36" height="36" rx="2" %s/>' % S( 3.5 ) + flake_at( 24, 24, 12, 3.5 ) )
symbol( "food-contact", "Food Safe", "Food", "food contact safe glass fork packaging",
        '<path d="M8 8 H24 L23 16 C22 21 20 23 16 23 C12 23 10 21 9 16 Z"/>'
        '<path d="M16 23 V37 M10.5 39.5 H21.5" %s/>' % S( 3.5 )
        + '<path d="M28 8 V16 M32.5 8 V16 M37 8 V16" %s/>' % S( 3 )
        + '<path d="M28 16 Q28 21.5 32.5 21.5 Q37 21.5 37 16 M32.5 21.5 V40" %s/>' % S( 3.5 ) )
symbol( "alcohol", "Contains Alcohol", "Food", "alcohol wine glass drink",
        '<path d="M13 6 H35 L34 16 C33 24 29 28 24 28 C19 28 15 24 14 16 Z"/>'
        '<path d="M24 28 V40 M16 42 H32" %s/>' % S( 3.5 ) )
symbol( "dishwasher-safe", "Dishwasher Safe", "Food", "dishwasher dish wash plates",
        '<rect x="6" y="6" width="36" height="36" rx="2" %s/>' % S( 3.5 )
        + '<ellipse cx="18.5" cy="21" rx="4" ry="9"/><ellipse cx="29.5" cy="21" rx="4" ry="9"/>'
        + dots( 3, 24, 35.5, gap=7, r=2.2 ) )
symbol( "oven-safe", "Oven Safe", "Food", "oven bake heat safe",
        '<rect x="6" y="7" width="36" height="34" rx="2" %s/>' % S( 3.5 )
        + '<rect x="12" y="19" width="24" height="16" rx="1.5"/>' + dots( 3, 19, 13, gap=6, r=2.2 ) )

# ---------------------------------------------------------------------------
# Printing & labeling
# ---------------------------------------------------------------------------
bars, x = "", 6.0
for w in ( 3, 3, 6, 3, 4.5, 3, 6, 3, 3 ):
    bars += '<rect x="%.2f" y="7" width="%g" height="28"/>' % ( x, w )
    x += w + 3.25
symbol( "barcode", "Barcode", "Printing & labeling", "barcode bars scan product ean upc",
        bars + '<path d="M6 40.5 H42" %s/>' % S( 3 ) )
QR = ""
for fx, fy in ( ( 4, 4 ), ( 30, 4 ), ( 4, 30 ) ):
    QR += ( '<rect x="%g" y="%g" width="14" height="14" rx="1" %s/>' % ( fx + 1.75, fy + 1.75, S( 3.5 ) )
            + '<rect x="%g" y="%g" width="5" height="5"/>' % ( fx + 6.25, fy + 6.25 ) )
for bx, by in ( ( 24, 6 ), ( 24, 14 ), ( 30, 24 ), ( 38, 24 ), ( 24, 30 ), ( 32, 32 ), ( 38, 38 ), ( 24, 38 ),
                ( 6, 24 ), ( 14, 24 ) ):
    QR += '<rect x="%g" y="%g" width="4.5" height="4.5"/>' % ( bx, by )
symbol( "qr-code", "QR Code", "Printing & labeling", "qr code scan matrix link", QR )
symbol( "printer", "Printer", "Printing & labeling", "print printer printing",
        '<rect x="14" y="5.5" width="20" height="12" %s/>' % S( 3 )
        + '<rect x="5" y="17" width="38" height="17" rx="2.5"/>'
        + '<rect x="14" y="28" width="20" height="14" fill="#fff" %s/>' % S( 3 ).replace( 'fill="none" ', "" )
        + '<circle cx="37" cy="22.5" r="2" fill="#fff"/>' )
symbol( "label-roll", "Label Roll", "Printing & labeling", "roll labels thermal sticker reel",
        '<circle cx="18" cy="20" r="13"/><circle cx="18" cy="20" r="4.5" fill="#fff"/>'
        '<rect x="18" y="27" width="26" height="8" />'
        '<path d="M28 27 V35 M37 27 V35" %s/>' % WS( 2.5 ) )
symbol( "scissors", "Scissors", "Printing & labeling", "scissors cut trim",
        '<circle cx="11" cy="14" r="5.5" %s/><circle cx="11" cy="34" r="5.5" %s/>' % ( S( 3.5 ), S( 3.5 ) )
        + '<path d="M15.5 17.5 L43 32 M15.5 30.5 L43 16" %s/>' % S( 4 ) )
symbol( "fold-here", "Fold Here", "Printing & labeling", "fold crease bend line",
        '<path d="M3 37 H45" fill="none" stroke="#000" stroke-width="3" stroke-dasharray="5 3.5"/>'
        + bent_arrow( 180, 150, r=11, cx=24, cy=27, head=8 ) )
symbol( "peel-here", "Peel Here", "Printing & labeling", "peel off sticker backing liner",
        '<path d="M6 6 H42 V27 L27 42 H6 Z" %s/>' % S( 3.5 ) + '<path d="M42 27 H27 V42 Z"/>' )
symbol( "crop-marks", "Crop Marks", "Printing & labeling", "crop trim marks cut bleed print",
        '<path d="M3 13 H10 M13 3 V10 M38 13 H45 M35 3 V10 M3 35 H10 M13 38 V45 M38 35 H45 M35 38 V45" %s/>'
        % S( 3 ) + '<rect x="16" y="16" width="16" height="16"/>' )
symbol( "registration-mark", "Registration Mark", "Printing & labeling", "registration target print align",
        '<circle cx="24" cy="24" r="13" %s/>' % S( 3.5 ) + '<circle cx="24" cy="24" r="5"/>'
        + '<path d="M24 3 V45 M3 24 H45" %s/>' % S( 3 ) )
symbol( "number-sign", "Number Sign", "Printing & labeling", "number hash serial batch lot",
        '<path d="M19 7 L15 41 M33 7 L29 41 M8 17.5 H41 M7 30.5 H40" %s/>' % S( 4 ) )
SHEET = '<rect x="9" y="4" width="30" height="40" rx="1.5" %s/>' % S( 3 )
for lx in ( 13, 25 ):
    for ly in ( 9, 17, 25, 33 ):
        SHEET += '<rect x="%g" y="%g" width="10" height="5.5" rx="1"/>' % ( lx, ly )
symbol( "label-sheet", "Label Sheet", "Printing & labeling", "sheet a4 letter labels page", SHEET )
symbol( "ink", "Ink", "Printing & labeling", "ink drop toner color",
        '<path d="M24 4 C24 4 38 21 38 30 A14 14 0 0 1 10 30 C10 21 24 4 24 4 Z"/>'
        '<path d="M17 30 A7 7 0 0 0 24 37" %s/>' % WS( 3 ) )

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
symbol( "open-box", "Open Box", "Packaging", "box carton open unpack",
        '<path d="M8 21 H40 V42 H8 Z" %s/>' % S( 3.5 )
        + '<path d="M8 21 L3 12 M40 21 L45 12 M8 21 L15 13 M40 21 L33 13" %s/>' % S( 3.5 ) )
symbol( "pallet", "Pallet", "Packaging", "pallet shipping freight skid",
        '<rect x="11.75" y="7.75" width="24.5" height="12.5" %s/>' % S( 3.5 )
        + '<rect x="4" y="24" width="40" height="5"/><rect x="6" y="29" width="6" height="6"/>'
          '<rect x="21" y="29" width="6" height="6"/><rect x="36" y="29" width="6" height="6"/>'
          '<rect x="4" y="35" width="40" height="5"/>' )
symbol( "packing-tape", "Packing Tape", "Packaging", "tape roll seal adhesive",
        '<circle cx="19" cy="21" r="12" fill="none" stroke="#000" stroke-width="7"/>'
        '<path d="M19 30.5 H44 V37 H19 Z"/>' )
symbol( "weight", "Weight", "Packaging", "weight heavy kg lb mass",
        '<path d="M10 42 L15 18 H33 L38 42 Z"/><path d="M18 18 C18 8 30 8 30 18" %s/>' % S( 4 ) )
symbol( "dimensions", "Dimensions", "Packaging", "size dimensions measure width height",
        '<rect x="14" y="16" width="27" height="25" %s/>' % S( 3.5 )
        + arrow( 27.5, 7, 15, 7, head=6.5, width=3 ) + arrow( 27.5, 7, 40, 7, head=6.5, width=3 )
        + arrow( 5.5, 28.5, 5.5, 17, head=6.5, width=3 ) + arrow( 5.5, 28.5, 5.5, 40, head=6.5, width=3 ) )
symbol( "compostable", "Compostable", "Packaging", "compost compostable biodegradable eco",
        bent_arrow( 200, 150, r=19, cx=24, cy=24, head=8 ) + bent_arrow( 20, 150, r=19, cx=24, cy=24, head=8 )
        + '<path d="M15 33 C15 22 22 16 33 15 C33 26 27 33 15 33 Z"/><path d="M17.5 30.5 L27.5 20.5" %s/>' % WS( 3 ) )
symbol( "dispose-in-bin", "Dispose in Bin", "Packaging", "bin trash waste litter dispose",
        '<rect x="9" y="10" width="30" height="4.5" rx="1"/><path d="M20 10 V6 H28 V10" %s/>' % S( 3 )
        + '<path d="M12 17 H36 L34 43 H14 Z"/><path d="M20 22 V38 M28 22 V38" %s/>' % WS( 3 ) )
symbol( "reusable", "Reusable", "Packaging", "reuse reusable refill return",
        bent_arrow( -60, 140, r=16, cx=24, cy=24 ) + bent_arrow( 120, 140, r=16, cx=24, cy=24 ) )


# ---------------------------------------------------------------------------
# Social media
#
# Brand marks from Simple Icons (https://simpleicons.org, CC0 1.0) drawn on a
# 24 x 24 grid; scaled to fill the 48 x 48 symbol with a 3 unit margin.
# LinkedIn comes from Simple Icons 9.21.0, newer releases dropped it.
# The marks are trademarks of their owners.
# ---------------------------------------------------------------------------
BRANDS = [
    ( "facebook", "Facebook", "fb meta social follow like page",
      "M9.101 23.691v-7.98H6.627v-3.667h2.474v-1.58c0-4.085 1.848-5.978 5.858-5.978.401 0 .955.042 1.468.103a8.68 8.68 0 0 1 1.141.195v3.325a8.623 8.623 0 0 0-.653-.036 26.805 26.805 0 0 0-.733-.009c-.707 0-1.259.096-1.675.309a1.686 1.686 0 0 0-.679.622c-.258.42-.374.995-.374 1.752v1.297h3.919l-.386 2.103-.287 1.564h-3.246v8.245C19.396 23.238 24 18.179 24 12.044c0-6.627-5.373-12-12-12s-12 5.373-12 12c0 5.628 3.874 10.35 9.101 11.647Z" ),
    ( "instagram", "Instagram", "insta ig meta social follow photo",
      "M7.0301.084c-1.2768.0602-2.1487.264-2.911.5634-.7888.3075-1.4575.72-2.1228 1.3877-.6652.6677-1.075 1.3368-1.3802 2.127-.2954.7638-.4956 1.6365-.552 2.914-.0564 1.2775-.0689 1.6882-.0626 4.947.0062 3.2586.0206 3.6671.0825 4.9473.061 1.2765.264 2.1482.5635 2.9107.308.7889.72 1.4573 1.388 2.1228.6679.6655 1.3365 1.0743 2.1285 1.38.7632.295 1.6361.4961 2.9134.552 1.2773.056 1.6884.069 4.9462.0627 3.2578-.0062 3.668-.0207 4.9478-.0814 1.28-.0607 2.147-.2652 2.9098-.5633.7889-.3086 1.4578-.72 2.1228-1.3881.665-.6682 1.0745-1.3378 1.3795-2.1284.2957-.7632.4966-1.636.552-2.9124.056-1.2809.0692-1.6898.063-4.948-.0063-3.2583-.021-3.6668-.0817-4.9465-.0607-1.2797-.264-2.1487-.5633-2.9117-.3084-.7889-.72-1.4568-1.3876-2.1228C21.2982 1.33 20.628.9208 19.8378.6165 19.074.321 18.2017.1197 16.9244.0645 15.6471.0093 15.236-.005 11.977.0014 8.718.0076 8.31.0215 7.0301.0839m.1402 21.6932c-1.17-.0509-1.8053-.2453-2.2287-.408-.5606-.216-.96-.4771-1.3819-.895-.422-.4178-.6811-.8186-.9-1.378-.1644-.4234-.3624-1.058-.4171-2.228-.0595-1.2645-.072-1.6442-.079-4.848-.007-3.2037.0053-3.583.0607-4.848.05-1.169.2456-1.805.408-2.2282.216-.5613.4762-.96.895-1.3816.4188-.4217.8184-.6814 1.3783-.9003.423-.1651 1.0575-.3614 2.227-.4171 1.2655-.06 1.6447-.072 4.848-.079 3.2033-.007 3.5835.005 4.8495.0608 1.169.0508 1.8053.2445 2.228.408.5608.216.96.4754 1.3816.895.4217.4194.6816.8176.9005 1.3787.1653.4217.3617 1.056.4169 2.2263.0602 1.2655.0739 1.645.0796 4.848.0058 3.203-.0055 3.5834-.061 4.848-.051 1.17-.245 1.8055-.408 2.2294-.216.5604-.4763.96-.8954 1.3814-.419.4215-.8181.6811-1.3783.9-.4224.1649-1.0577.3617-2.2262.4174-1.2656.0595-1.6448.072-4.8493.079-3.2045.007-3.5825-.006-4.848-.0608M16.953 5.5864A1.44 1.44 0 1 0 18.39 4.144a1.44 1.44 0 0 0-1.437 1.4424M5.8385 12.012c.0067 3.4032 2.7706 6.1557 6.173 6.1493 3.4026-.0065 6.157-2.7701 6.1506-6.1733-.0065-3.4032-2.771-6.1565-6.174-6.1498-3.403.0067-6.156 2.771-6.1496 6.1738M8 12.0077a4 4 0 1 1 4.008 3.9921A3.9996 3.9996 0 0 1 8 12.0077" ),
    ( "x-twitter", "X (Twitter)", "twitter tweet x social follow",
      "M14.234 10.162 22.977 0h-2.072l-7.591 8.824L7.251 0H.258l9.168 13.343L.258 24H2.33l8.016-9.318L16.749 24h6.993zm-2.837 3.299-.929-1.329L3.076 1.56h3.182l5.965 8.532.929 1.329 7.754 11.09h-3.182z" ),
    ( "tiktok", "TikTok", "tik tok video social follow",
      "M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-.93-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 1.75-.21.51-.15 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79.06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z" ),
    ( "youtube", "YouTube", "yt video channel subscribe social",
      "M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 .502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12l-6.273 3.568z" ),
    ( "whatsapp", "WhatsApp", "whatsapp chat message phone order social",
      "M17.472 14.382c-.297-.149-1.758-.867-2.03-.967-.273-.099-.471-.148-.67.15-.197.297-.767.966-.94 1.164-.173.199-.347.223-.644.075-.297-.15-1.255-.463-2.39-1.475-.883-.788-1.48-1.761-1.653-2.059-.173-.297-.018-.458.13-.606.134-.133.298-.347.446-.52.149-.174.198-.298.298-.497.099-.198.05-.371-.025-.52-.075-.149-.669-1.612-.916-2.207-.242-.579-.487-.5-.669-.51-.173-.008-.371-.01-.57-.01-.198 0-.52.074-.792.372-.272.297-1.04 1.016-1.04 2.479 0 1.462 1.065 2.875 1.213 3.074.149.198 2.096 3.2 5.077 4.487.709.306 1.262.489 1.694.625.712.227 1.36.195 1.871.118.571-.085 1.758-.719 2.006-1.413.248-.694.248-1.289.173-1.413-.074-.124-.272-.198-.57-.347m-5.421 7.403h-.004a9.87 9.87 0 01-5.031-1.378l-.361-.214-3.741.982.998-3.648-.235-.374a9.86 9.86 0 01-1.51-5.26c.001-5.45 4.436-9.884 9.888-9.884 2.64 0 5.122 1.03 6.988 2.898a9.825 9.825 0 012.893 6.994c-.003 5.45-4.437 9.884-9.885 9.884m8.413-18.297A11.815 11.815 0 0012.05 0C5.495 0 .16 5.335.157 11.892c0 2.096.547 4.142 1.588 5.945L.057 24l6.305-1.654a11.882 11.882 0 005.683 1.448h.005c6.554 0 11.89-5.335 11.893-11.893a11.821 11.821 0 00-3.48-8.413Z" ),
    ( "linkedin", "LinkedIn", "linked in business professional social",
      "M20.447 20.452h-3.554v-5.569c0-1.328-.027-3.037-1.852-3.037-1.853 0-2.136 1.445-2.136 2.939v5.667H9.351V9h3.414v1.561h.046c.477-.9 1.637-1.85 3.37-1.85 3.601 0 4.267 2.37 4.267 5.455v6.286zM5.337 7.433c-1.144 0-2.063-.926-2.063-2.065 0-1.138.92-2.063 2.063-2.063 1.14 0 2.064.925 2.064 2.063 0 1.139-.925 2.065-2.064 2.065zm1.782 13.019H3.555V9h3.564v11.452zM22.225 0H1.771C.792 0 0 .774 0 1.729v20.542C0 23.227.792 24 1.771 24h20.451C23.2 24 24 23.227 24 22.271V1.729C24 .774 23.2 0 22.222 0h.003z" ),
    ( "pinterest", "Pinterest", "pin pins social follow",
      "M12.017 0C5.396 0 .029 5.367.029 11.987c0 5.079 3.158 9.417 7.618 11.162-.105-.949-.199-2.403.041-3.439.219-.937 1.406-5.957 1.406-5.957s-.359-.72-.359-1.781c0-1.663.967-2.911 2.168-2.911 1.024 0 1.518.769 1.518 1.688 0 1.029-.653 2.567-.992 3.992-.285 1.193.6 2.165 1.775 2.165 2.128 0 3.768-2.245 3.768-5.487 0-2.861-2.063-4.869-5.008-4.869-3.41 0-5.409 2.562-5.409 5.199 0 1.033.394 2.143.889 2.741.099.12.112.225.085.345-.09.375-.293 1.199-.334 1.363-.053.225-.172.271-.401.165-1.495-.69-2.433-2.878-2.433-4.646 0-3.776 2.748-7.252 7.92-7.252 4.158 0 7.392 2.967 7.392 6.923 0 4.135-2.607 7.462-6.233 7.462-1.214 0-2.354-.629-2.758-1.379l-.749 2.848c-.269 1.045-1.004 2.352-1.498 3.146 1.123.345 2.306.535 3.55.535 6.607 0 11.985-5.365 11.985-11.987C23.97 5.39 18.592.026 11.985.026L12.017 0z" ),
    ( "snapchat", "Snapchat", "snap ghost social",
      "M12.206.793c.99 0 4.347.276 5.93 3.821.529 1.193.403 3.219.299 4.847l-.003.06c-.012.18-.022.345-.03.51.075.045.203.09.401.09.3-.016.659-.12 1.033-.301.165-.088.344-.104.464-.104.182 0 .359.029.509.09.45.149.734.479.734.838.015.449-.39.839-1.213 1.168-.089.029-.209.075-.344.119-.45.135-1.139.36-1.333.81-.09.224-.061.524.12.868l.015.015c.06.136 1.526 3.475 4.791 4.014.255.044.435.27.42.509 0 .075-.015.149-.045.225-.24.569-1.273.988-3.146 1.271-.059.091-.12.375-.164.57-.029.179-.074.36-.134.553-.076.271-.27.405-.555.405h-.03c-.135 0-.313-.031-.538-.074-.36-.075-.765-.135-1.273-.135-.3 0-.599.015-.913.074-.6.104-1.123.464-1.723.884-.853.599-1.826 1.288-3.294 1.288-.06 0-.119-.015-.18-.015h-.149c-1.468 0-2.427-.675-3.279-1.288-.599-.42-1.107-.779-1.707-.884-.314-.045-.629-.074-.928-.074-.54 0-.958.089-1.272.149-.211.043-.391.074-.54.074-.374 0-.523-.224-.583-.42-.061-.192-.09-.389-.135-.567-.046-.181-.105-.494-.166-.57-1.918-.222-2.95-.642-3.189-1.226-.031-.063-.052-.15-.055-.225-.015-.243.165-.465.42-.509 3.264-.54 4.73-3.879 4.791-4.02l.016-.029c.18-.345.224-.645.119-.869-.195-.434-.884-.658-1.332-.809-.121-.029-.24-.074-.346-.119-1.107-.435-1.257-.93-1.197-1.273.09-.479.674-.793 1.168-.793.146 0 .27.029.383.074.42.194.789.3 1.104.3.234 0 .384-.06.465-.105l-.046-.569c-.098-1.626-.225-3.651.307-4.837C7.392 1.077 10.739.807 11.727.807l.419-.015h.06z" ),
    ( "telegram", "Telegram", "chat message channel social",
      "M11.944 0A12 12 0 0 0 0 12a12 12 0 0 0 12 12 12 12 0 0 0 12-12A12 12 0 0 0 12 0a12 12 0 0 0-.056 0zm4.962 7.224c.1-.002.321.023.465.14a.506.506 0 0 1 .171.325c.016.093.036.306.02.472-.18 1.898-.962 6.502-1.36 8.627-.168.9-.499 1.201-.82 1.23-.696.065-1.225-.46-1.9-.902-1.056-.693-1.653-1.124-2.678-1.8-1.185-.78-.417-1.21.258-1.91.177-.184 3.247-2.977 3.307-3.23.007-.032.014-.15-.056-.212s-.174-.041-.249-.024c-.106.024-1.793 1.14-5.061 3.345-.48.33-.913.49-1.302.48-.428-.008-1.252-.241-1.865-.44-.752-.245-1.349-.374-1.297-.789.027-.216.325-.437.893-.663 3.498-1.524 5.83-2.529 6.998-3.014 3.332-1.386 4.025-1.627 4.476-1.635z" ),
    ( "threads", "Threads", "meta threads social follow",
      "M18.263 11.097c-.03-3.486-1.92-5.586-5.111-5.586-2.13 0-3.922.963-4.863 2.499l2.062 1.438c.535-.843 1.272-1.543 2.628-1.543 1.528 0 2.318.85 2.544 2.431a15 15 0 0 0-2.236-.173c-4.125 0-6.068 1.867-6.068 4.336s1.943 3.99 4.804 3.99c3.139 0 5.013-2.115 5.781-4.735.798.361 1.348 1.204 1.348 2.47 0 3.387-3.907 5.232-7.22 5.232-4.885 0-8.077-3.207-8.077-8.424 0-6.392 4.223-10.487 9.9-10.487 3.808 0 5.69 1.671 6.97 3.914l2.108-1.475C21.44 2.078 18.331 0 13.663 0 6.227 0 1.168 5.277 1.168 12.934c0 7 4.953 11.066 10.856 11.066 4.878 0 9.809-2.846 9.809-7.716 0-2.545-1.46-4.231-3.569-5.187m-6.33 4.855c-1.077 0-2.026-.512-2.026-1.453 0-1.483 1.822-1.934 3.606-1.934.678 0 1.34.045 1.927.173-.422 1.927-1.671 3.215-3.508 3.214Z" ),
    ( "messenger", "Messenger", "facebook messenger chat message social",
      "M12 0C5.24 0 0 4.952 0 11.64c0 3.499 1.434 6.521 3.769 8.61a.96.96 0 0 1 .323.683l.065 2.135a.96.96 0 0 0 1.347.85l2.381-1.053a.96.96 0 0 1 .641-.046A13 13 0 0 0 12 23.28c6.76 0 12-4.952 12-11.64S18.76 0 12 0m6.806 7.44c.522-.03.971.567.63 1.094l-4.178 6.457a.707.707 0 0 1-.977.208l-3.87-2.504a.44.44 0 0 0-.49.007l-4.363 3.01c-.637.438-1.415-.317-.995-.966l4.179-6.457a.706.706 0 0 1 .977-.21l3.87 2.505c.15.097.344.094.491-.007l4.362-3.008a.7.7 0 0 1 .364-.13" ),
    ( "bluesky", "Bluesky", "bsky butterfly social follow",
      "M5.202 2.857C7.954 4.922 10.913 9.11 12 11.358c1.087-2.247 4.046-6.436 6.798-8.501C20.783 1.366 24 .213 24 3.883c0 .732-.42 6.156-.667 7.037-.856 3.061-3.978 3.842-6.755 3.37 4.854.826 6.089 3.562 3.422 6.299-5.065 5.196-7.28-1.304-7.847-2.97-.104-.305-.152-.448-.153-.327 0-.121-.05.022-.153.327-.568 1.666-2.782 8.166-7.847 2.97-2.667-2.737-1.432-5.473 3.422-6.3-2.777.473-5.899-.308-6.755-3.369C.42 10.04 0 4.615 0 3.883c0-3.67 3.217-2.517 5.202-1.026" ),
    ( "mastodon", "Mastodon", "fediverse toot social follow",
      "M23.268 5.313c-.35-2.578-2.617-4.61-5.304-5.004C17.51.242 15.792 0 11.813 0h-.03c-3.98 0-4.835.242-5.288.309C3.882.692 1.496 2.518.917 5.127.64 6.412.61 7.837.661 9.143c.074 1.874.088 3.745.26 5.611.118 1.24.325 2.47.62 3.68.55 2.237 2.777 4.098 4.96 4.857 2.336.792 4.849.923 7.256.38.265-.061.527-.132.786-.213.585-.184 1.27-.39 1.774-.753a.057.057 0 0 0 .023-.043v-1.809a.052.052 0 0 0-.02-.041.053.053 0 0 0-.046-.01 20.282 20.282 0 0 1-4.709.545c-2.73 0-3.463-1.284-3.674-1.818a5.593 5.593 0 0 1-.319-1.433.053.053 0 0 1 .066-.054c1.517.363 3.072.546 4.632.546.376 0 .75 0 1.125-.01 1.57-.044 3.224-.124 4.768-.422.038-.008.077-.015.11-.024 2.435-.464 4.753-1.92 4.989-5.604.008-.145.03-1.52.03-1.67.002-.512.167-3.63-.024-5.545zm-3.748 9.195h-2.561V8.29c0-1.309-.55-1.976-1.67-1.976-1.23 0-1.846.79-1.846 2.35v3.403h-2.546V8.663c0-1.56-.617-2.35-1.848-2.35-1.112 0-1.668.668-1.67 1.977v6.218H4.822V8.102c0-1.31.337-2.35 1.011-3.12.696-.77 1.608-1.164 2.74-1.164 1.311 0 2.302.5 2.962 1.498l.638 1.06.638-1.06c.66-.999 1.65-1.498 2.96-1.498 1.13 0 2.043.395 2.74 1.164.675.77 1.012 1.81 1.012 3.12z" ),
    ( "discord", "Discord", "chat community server social",
      "M20.317 4.3698a19.7913 19.7913 0 00-4.8851-1.5152.0741.0741 0 00-.0785.0371c-.211.3753-.4447.8648-.6083 1.2495-1.8447-.2762-3.68-.2762-5.4868 0-.1636-.3933-.4058-.8742-.6177-1.2495a.077.077 0 00-.0785-.037 19.7363 19.7363 0 00-4.8852 1.515.0699.0699 0 00-.0321.0277C.5334 9.0458-.319 13.5799.0992 18.0578a.0824.0824 0 00.0312.0561c2.0528 1.5076 4.0413 2.4228 5.9929 3.0294a.0777.0777 0 00.0842-.0276c.4616-.6304.8731-1.2952 1.226-1.9942a.076.076 0 00-.0416-.1057c-.6528-.2476-1.2743-.5495-1.8722-.8923a.077.077 0 01-.0076-.1277c.1258-.0943.2517-.1923.3718-.2914a.0743.0743 0 01.0776-.0105c3.9278 1.7933 8.18 1.7933 12.0614 0a.0739.0739 0 01.0785.0095c.1202.099.246.1981.3728.2924a.077.077 0 01-.0066.1276 12.2986 12.2986 0 01-1.873.8914.0766.0766 0 00-.0407.1067c.3604.698.7719 1.3628 1.225 1.9932a.076.076 0 00.0842.0286c1.961-.6067 3.9495-1.5219 6.0023-3.0294a.077.077 0 00.0313-.0552c.5004-5.177-.8382-9.6739-3.5485-13.6604a.061.061 0 00-.0312-.0286zM8.02 15.3312c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9555-2.4189 2.157-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.9555 2.4189-2.1569 2.4189zm7.9748 0c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9554-2.4189 2.1569-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.946 2.4189-2.1568 2.4189Z" ),
    ( "reddit", "Reddit", "subreddit community social",
      "M12 0C5.373 0 0 5.373 0 12c0 3.314 1.343 6.314 3.515 8.485l-2.286 2.286C.775 23.225 1.097 24 1.738 24H12c6.627 0 12-5.373 12-12S18.627 0 12 0Zm4.388 3.199c1.104 0 1.999.895 1.999 1.999 0 1.105-.895 2-1.999 2-.946 0-1.739-.657-1.947-1.539v.002c-1.147.162-2.032 1.15-2.032 2.341v.007c1.776.067 3.4.567 4.686 1.363.473-.363 1.064-.58 1.707-.58 1.547 0 2.802 1.254 2.802 2.802 0 1.117-.655 2.081-1.601 2.531-.088 3.256-3.637 5.876-7.997 5.876-4.361 0-7.905-2.617-7.998-5.87-.954-.447-1.614-1.415-1.614-2.538 0-1.548 1.255-2.802 2.803-2.802.645 0 1.239.218 1.712.585 1.275-.79 2.881-1.291 4.64-1.365v-.01c0-1.663 1.263-3.034 2.88-3.207.188-.911.993-1.595 1.959-1.595Zm-8.085 8.376c-.784 0-1.459.78-1.506 1.797-.047 1.016.64 1.429 1.426 1.429.786 0 1.371-.369 1.418-1.385.047-1.017-.553-1.841-1.338-1.841Zm7.406 0c-.786 0-1.385.824-1.338 1.841.047 1.017.634 1.385 1.418 1.385.785 0 1.473-.413 1.426-1.429-.046-1.017-.721-1.797-1.506-1.797Zm-3.703 4.013c-.974 0-1.907.048-2.77.135-.147.015-.241.168-.183.305.483 1.154 1.622 1.964 2.953 1.964 1.33 0 2.47-.81 2.953-1.964.057-.137-.037-.29-.184-.305-.863-.087-1.795-.135-2.769-.135Z" ),
    ( "tumblr", "Tumblr", "blog social",
      "M14.563 24c-5.093 0-7.031-3.756-7.031-6.411V9.747H5.116V6.648c3.63-1.313 4.512-4.596 4.71-6.469C9.84.051 9.941 0 9.999 0h3.517v6.114h4.801v3.633h-4.82v7.47c.016 1.001.375 2.371 2.207 2.371h.09c.631-.02 1.486-.205 1.936-.419l1.156 3.425c-.436.636-2.4 1.374-4.156 1.404h-.178l.011.002z" ),
    ( "twitch", "Twitch", "stream live video social",
      "M11.571 4.714h1.715v5.143H11.57zm4.715 0H18v5.143h-1.714zM6 0L1.714 4.286v15.428h5.143V24l4.286-4.286h3.428L22.286 12V0zm14.571 11.143l-3.428 3.428h-3.429l-3 3v-3H6.857V1.714h13.714Z" ),
    ( "spotify", "Spotify", "music podcast playlist",
      "M12 0C5.4 0 0 5.4 0 12s5.4 12 12 12 12-5.4 12-12S18.66 0 12 0zm5.521 17.34c-.24.359-.66.48-1.021.24-2.82-1.74-6.36-2.101-10.561-1.141-.418.122-.779-.179-.899-.539-.12-.421.18-.78.54-.9 4.56-1.021 8.52-.6 11.64 1.32.42.18.479.659.301 1.02zm1.44-3.3c-.301.42-.841.6-1.262.3-3.239-1.98-8.159-2.58-11.939-1.38-.479.12-1.02-.12-1.14-.6-.12-.48.12-1.021.6-1.141C9.6 9.9 15 10.561 18.72 12.84c.361.181.54.78.241 1.2zm.12-3.36C15.24 8.4 8.82 8.16 5.16 9.301c-.6.179-1.2-.181-1.38-.721-.18-.601.18-1.2.72-1.381 4.26-1.26 11.28-1.02 15.721 1.621.539.3.719 1.02.419 1.56-.299.421-1.02.599-1.559.3z" ),
    ( "wechat", "WeChat", "weixin chat message social",
      "M8.691 2.188C3.891 2.188 0 5.476 0 9.53c0 2.212 1.17 4.203 3.002 5.55a.59.59 0 0 1 .213.665l-.39 1.48c-.019.07-.048.141-.048.213 0 .163.13.295.29.295a.326.326 0 0 0 .167-.054l1.903-1.114a.864.864 0 0 1 .717-.098 10.16 10.16 0 0 0 2.837.403c.276 0 .543-.027.811-.05-.857-2.578.157-4.972 1.932-6.446 1.703-1.415 3.882-1.98 5.853-1.838-.576-3.583-4.196-6.348-8.596-6.348zM5.785 5.991c.642 0 1.162.529 1.162 1.18a1.17 1.17 0 0 1-1.162 1.178A1.17 1.17 0 0 1 4.623 7.17c0-.651.52-1.18 1.162-1.18zm5.813 0c.642 0 1.162.529 1.162 1.18a1.17 1.17 0 0 1-1.162 1.178 1.17 1.17 0 0 1-1.162-1.178c0-.651.52-1.18 1.162-1.18zm5.34 2.867c-1.797-.052-3.746.512-5.28 1.786-1.72 1.428-2.687 3.72-1.78 6.22.942 2.453 3.666 4.229 6.884 4.229.826 0 1.622-.12 2.361-.336a.722.722 0 0 1 .598.082l1.584.926a.272.272 0 0 0 .14.047c.134 0 .24-.111.24-.247 0-.06-.023-.12-.038-.177l-.327-1.233a.582.582 0 0 1-.023-.156.49.49 0 0 1 .201-.398C23.024 18.48 24 16.82 24 14.98c0-3.21-2.931-5.837-6.656-6.088V8.89c-.135-.01-.27-.027-.407-.03zm-2.53 3.274c.535 0 .969.44.969.982a.976.976 0 0 1-.969.983.976.976 0 0 1-.969-.983c0-.542.434-.982.97-.982zm4.844 0c.535 0 .969.44.969.982a.976.976 0 0 1-.969.983.976.976 0 0 1-.969-.983c0-.542.434-.982.969-.982z" ),
    ( "signal", "Signal", "chat message secure social",
      "M12 0q-.934 0-1.83.139l.17 1.111a11 11 0 0 1 3.32 0l.172-1.111A12 12 0 0 0 12 0M9.152.34A12 12 0 0 0 5.77 1.742l.584.961a10.8 10.8 0 0 1 3.066-1.27zm5.696 0-.268 1.094a10.8 10.8 0 0 1 3.066 1.27l.584-.962A12 12 0 0 0 14.848.34M12 2.25a9.75 9.75 0 0 0-8.539 14.459c.074.134.1.292.064.441l-1.013 4.338 4.338-1.013a.62.62 0 0 1 .441.064A9.7 9.7 0 0 0 12 21.75c5.385 0 9.75-4.365 9.75-9.75S17.385 2.25 12 2.25m-7.092.068a12 12 0 0 0-2.59 2.59l.909.664a11 11 0 0 1 2.345-2.345zm14.184 0-.664.909a11 11 0 0 1 2.345 2.345l.909-.664a12 12 0 0 0-2.59-2.59M1.742 5.77A12 12 0 0 0 .34 9.152l1.094.268a10.8 10.8 0 0 1 1.269-3.066zm20.516 0-.961.584a10.8 10.8 0 0 1 1.27 3.066l1.093-.268a12 12 0 0 0-1.402-3.383M.138 10.168A12 12 0 0 0 0 12q0 .934.139 1.83l1.111-.17A11 11 0 0 1 1.125 12q0-.848.125-1.66zm23.723.002-1.111.17q.125.812.125 1.66c0 .848-.042 1.12-.125 1.66l1.111.172a12.1 12.1 0 0 0 0-3.662M1.434 14.58l-1.094.268a12 12 0 0 0 .96 2.591l-.265 1.14 1.096.255.36-1.539-.188-.365a10.8 10.8 0 0 1-.87-2.35m21.133 0a10.8 10.8 0 0 1-1.27 3.067l.962.584a12 12 0 0 0 1.402-3.383zm-1.793 3.848a11 11 0 0 1-2.345 2.345l.664.909a12 12 0 0 0 2.59-2.59zm-19.959 1.1L.357 21.48a1.8 1.8 0 0 0 2.162 2.161l1.954-.455-.256-1.095-1.953.455a.675.675 0 0 1-.81-.81l.454-1.954zm16.832 1.769a10.8 10.8 0 0 1-3.066 1.27l.268 1.093a12 12 0 0 0 3.382-1.402zm-10.94.213-1.54.36.256 1.095 1.139-.266c.814.415 1.683.74 2.591.961l.268-1.094a10.8 10.8 0 0 1-2.35-.869zm3.634 1.24-.172 1.111a12.1 12.1 0 0 0 3.662 0l-.17-1.111q-.812.125-1.66.125a11 11 0 0 1-1.66-.125" ),
    ( "github", "GitHub", "git code developer repository",
      "M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12" ),
    ( "medium", "Medium", "blog article social",
      "M4.21 0A4.201 4.201 0 0 0 0 4.21v15.58A4.201 4.201 0 0 0 4.21 24h15.58A4.201 4.201 0 0 0 24 19.79v-1.093c-.137.013-.278.02-.422.02-2.577 0-4.027-2.146-4.09-4.832a7.592 7.592 0 0 1 .022-.708c.093-1.186.475-2.241 1.105-3.022a3.885 3.885 0 0 1 1.395-1.1c.468-.237 1.127-.367 1.664-.367h.023c.101 0 .202.004.303.01V4.211A4.201 4.201 0 0 0 19.79 0Zm.198 5.583h4.165l3.588 8.435 3.59-8.435h3.864v.146l-.019.004c-.705.16-1.063.397-1.063 1.254h-.003l.003 10.274c.06.676.424.885 1.063 1.03l.02.004v.145h-4.923v-.145l.019-.005c.639-.144.994-.353 1.054-1.03V7.267l-4.745 11.15h-.261L6.15 7.569v9.445c0 .857.358 1.094 1.063 1.253l.02.004v.147H4.405v-.147l.019-.004c.705-.16 1.065-.397 1.065-1.253V6.987c0-.857-.358-1.094-1.064-1.254l-.018-.004zm19.25 3.668c-1.086.023-1.733 1.323-1.813 3.124H24V9.298a1.378 1.378 0 0 0-.342-.047Zm-1.862 3.632c-.1 1.756.86 3.239 2.204 3.634v-3.634z" ),
    ( "behance", "Behance", "portfolio design social",
      "M16.969 16.927a2.561 2.561 0 0 0 1.901.677 2.501 2.501 0 0 0 1.531-.475c.362-.235.636-.584.779-.99h2.585a5.091 5.091 0 0 1-1.9 2.896 5.292 5.292 0 0 1-3.091.88 5.839 5.839 0 0 1-2.284-.433 4.871 4.871 0 0 1-1.723-1.211 5.657 5.657 0 0 1-1.08-1.874 7.057 7.057 0 0 1-.383-2.393c-.005-.8.129-1.595.396-2.349a5.313 5.313 0 0 1 5.088-3.604 4.87 4.87 0 0 1 2.376.563c.661.362 1.231.87 1.668 1.485a6.2 6.2 0 0 1 .943 2.133c.194.821.263 1.666.205 2.508h-7.699c-.063.79.184 1.574.688 2.187ZM6.947 4.084a8.065 8.065 0 0 1 1.928.198 4.29 4.29 0 0 1 1.49.638c.418.303.748.711.958 1.182.241.579.357 1.203.341 1.83a3.506 3.506 0 0 1-.506 1.961 3.726 3.726 0 0 1-1.503 1.287 3.588 3.588 0 0 1 2.027 1.437c.464.747.697 1.615.67 2.494a4.593 4.593 0 0 1-.423 2.032 3.945 3.945 0 0 1-1.163 1.413 5.114 5.114 0 0 1-1.683.807 7.135 7.135 0 0 1-1.928.259H0V4.084h6.947Zm-.235 12.9c.308.004.616-.029.916-.099a2.18 2.18 0 0 0 .766-.332c.228-.158.411-.371.534-.619.142-.317.208-.663.191-1.009a2.08 2.08 0 0 0-.642-1.715 2.618 2.618 0 0 0-1.696-.505h-3.54v4.279h3.471Zm13.635-5.967a2.13 2.13 0 0 0-1.654-.619 2.336 2.336 0 0 0-1.163.259 2.474 2.474 0 0 0-.738.62 2.359 2.359 0 0 0-.396.792c-.074.239-.12.485-.137.734h4.769a3.239 3.239 0 0 0-.679-1.785l-.002-.001Zm-13.813-.648a2.254 2.254 0 0 0 1.423-.433c.399-.355.607-.88.56-1.413a1.916 1.916 0 0 0-.178-.891 1.298 1.298 0 0 0-.495-.533 1.851 1.851 0 0 0-.711-.274 3.966 3.966 0 0 0-.835-.073H3.241v3.631h3.293v-.014ZM21.62 5.122h-5.976v1.527h5.976V5.122Z" ),
    ( "dribbble", "Dribbble", "portfolio design social",
      "M12 24C5.385 24 0 18.615 0 12S5.385 0 12 0s12 5.385 12 12-5.385 12-12 12zm10.12-10.358c-.35-.11-3.17-.953-6.384-.438 1.34 3.684 1.887 6.684 1.992 7.308 2.3-1.555 3.936-4.02 4.395-6.87zm-6.115 7.808c-.153-.9-.75-4.032-2.19-7.77l-.066.02c-5.79 2.015-7.86 6.025-8.04 6.4 1.73 1.358 3.92 2.166 6.29 2.166 1.42 0 2.77-.29 4-.814zm-11.62-2.58c.232-.4 3.045-5.055 8.332-6.765.135-.045.27-.084.405-.12-.26-.585-.54-1.167-.832-1.74C7.17 11.775 2.206 11.71 1.756 11.7l-.004.312c0 2.633.998 5.037 2.634 6.855zm-2.42-8.955c.46.008 4.683.026 9.477-1.248-1.698-3.018-3.53-5.558-3.8-5.928-2.868 1.35-5.01 3.99-5.676 7.17zM9.6 2.052c.282.38 2.145 2.914 3.822 6 3.645-1.365 5.19-3.44 5.373-3.702-1.81-1.61-4.19-2.586-6.795-2.586-.825 0-1.63.1-2.4.285zm10.335 3.483c-.218.29-1.935 2.493-5.724 4.04.24.49.47.985.68 1.486.08.18.15.36.22.53 3.41-.43 6.8.26 7.14.33-.02-2.42-.88-4.64-2.31-6.38z" ),
    ( "vimeo", "Vimeo", "video social",
      "M23.9765 6.4168c-.105 2.338-1.739 5.5429-4.894 9.6088-3.2679 4.247-6.0258 6.3699-8.2898 6.3699-1.409 0-2.578-1.294-3.553-3.881l-1.9179-7.1138c-.719-2.584-1.488-3.878-2.312-3.878-.179 0-.806.378-1.8809 1.132l-1.129-1.457a315.06 315.06 0 003.501-3.1279c1.579-1.368 2.765-2.085 3.5539-2.159 1.867-.18 3.016 1.1 3.447 3.838.465 2.953.789 4.789.971 5.5069.5389 2.45 1.1309 3.674 1.7759 3.674.502 0 1.256-.796 2.265-2.385 1.004-1.589 1.54-2.797 1.612-3.628.144-1.371-.395-2.061-1.614-2.061-.574 0-1.167.121-1.777.391 1.186-3.8679 3.434-5.7568 6.7619-5.6368 2.4729.06 3.6279 1.664 3.4929 4.7969z" ),
    ( "google", "Google", "google review search business",
      "M12.48 10.92v3.28h7.84c-.24 1.84-.853 3.187-1.787 4.133-1.147 1.147-2.933 2.4-6.053 2.4-4.827 0-8.6-3.893-8.6-8.72s3.773-8.72 8.6-8.72c2.6 0 4.507 1.027 5.907 2.347l2.307-2.307C18.747 1.44 16.133 0 12.48 0 5.867 0 .307 5.387.307 12s5.56 12 12.173 12c3.573 0 6.267-1.173 8.373-3.36 2.16-2.16 2.84-5.213 2.84-7.667 0-.76-.053-1.467-.173-2.053H12.48z" ),
    ( "google-maps", "Google Maps", "maps location directions review pin",
      "M19.527 4.799c1.212 2.608.937 5.678-.405 8.173-1.101 2.047-2.744 3.74-4.098 5.614-.619.858-1.244 1.75-1.669 2.727-.141.325-.263.658-.383.992-.121.333-.224.673-.34 1.008-.109.314-.236.684-.627.687h-.007c-.466-.001-.579-.53-.695-.887-.284-.874-.581-1.713-1.019-2.525-.51-.944-1.145-1.817-1.79-2.671L19.527 4.799zM8.545 7.705l-3.959 4.707c.724 1.54 1.821 2.863 2.871 4.18.247.31.494.622.737.936l4.984-5.925-.029.01c-1.741.601-3.691-.291-4.392-1.987a3.377 3.377 0 0 1-.209-.716c-.063-.437-.077-.761-.004-1.198l.001-.007zM5.492 3.149l-.003.004c-1.947 2.466-2.281 5.88-1.117 8.77l4.785-5.689-.058-.05-3.607-3.035zM14.661.436l-3.838 4.563a.295.295 0 0 1 .027-.01c1.6-.551 3.403.15 4.22 1.626.176.319.323.683.377 1.045.068.446.085.773.012 1.22l-.003.016 3.836-4.561A8.382 8.382 0 0 0 14.67.439l-.009-.003zM9.466 5.868L14.162.285l-.047-.012A8.31 8.31 0 0 0 11.986 0a8.439 8.439 0 0 0-6.169 2.766l-.016.018 3.665 3.084z" ),
    ( "tripadvisor", "Tripadvisor", "review travel restaurant hotel",
      "M12.006 4.295c-2.67 0-5.338.784-7.645 2.353H0l1.963 2.135a5.997 5.997 0 0 0 4.04 10.43 5.976 5.976 0 0 0 4.075-1.6L12 19.705l1.922-2.09a5.972 5.972 0 0 0 4.072 1.598 6 6 0 0 0 6-5.998 5.982 5.982 0 0 0-1.957-4.432L24 6.648h-4.35a13.573 13.573 0 0 0-7.644-2.353zM12 6.255c1.531 0 3.063.303 4.504.903C13.943 8.138 12 10.43 12 13.1c0-2.671-1.942-4.962-4.504-5.942A11.72 11.72 0 0 1 12 6.256zM6.002 9.157a4.059 4.059 0 1 1 0 8.118 4.059 4.059 0 0 1 0-8.118zm11.992.002a4.057 4.057 0 1 1 .003 8.115 4.057 4.057 0 0 1-.003-8.115zm-11.992 1.93a2.128 2.128 0 0 0 0 4.256 2.128 2.128 0 0 0 0-4.256zm11.992 0a2.128 2.128 0 0 0 0 4.256 2.128 2.128 0 0 0 0-4.256z" ),
    ( "yelp", "Yelp", "review restaurant business",
      "m7.6885 15.1415-3.6715.8483c-.3769.0871-.755.183-1.1452.155-.2611-.0188-.5122-.0414-.7606-.213a1.179 1.179 0 0 1-.331-.3594c-.3486-.5519-.3656-1.3661-.3697-2.0004a6.2874 6.2874 0 0 1 .3314-2.0642 1.857 1.857 0 0 1 .1073-.2474 2.3426 2.3426 0 0 1 .1255-.2165 2.4572 2.4572 0 0 1 .1563-.1975 1.1736 1.1736 0 0 1 .399-.2831 1.082 1.082 0 0 1 .4592-.0837c.2355.0016.5139.052.91.1734.0555.0191.1237.0382.1856.0572.3277.1013.7048.2404 1.1499.3987.6863.2404 1.3663.487 2.0463.7397l1.2117.4423c.2217.0807.4363.18.6412.297.174.0984.3273.2298.4512.387a1.217 1.217 0 0 1 .192.4309 1.2205 1.2205 0 0 1-.872 1.4522c-.0468.0151-.0852.0239-.1085.0293l-1.105.2553-.0031-.001zM18.8208 7.565a1.8506 1.8506 0 0 0-.2042-.1754 2.4082 2.4082 0 0 0-.2077-.1394 2.3607 2.3607 0 0 0-.2269-.109 1.1705 1.1705 0 0 0-.482-.0796 1.0862 1.0862 0 0 0-.4498.1263c-.2107.1048-.4388.2732-.742.5551-.042.0417-.0947.0886-.142.133-.2502.2351-.5286.5252-.8599.863a114.6363 114.6363 0 0 0-1.5166 1.5629l-.8962.9293a4.1897 4.1897 0 0 0-.4466.5483 1.541 1.541 0 0 0-.2364.5459 1.2199 1.2199 0 0 0 .0107.4518l.0046.02a1.218 1.218 0 0 0 1.4184.923 1.162 1.162 0 0 0 .1105-.0213l4.7781-1.104c.3766-.087.7587-.1667 1.097-.3631.2269-.1316.4428-.262.5909-.5252a1.1793 1.1793 0 0 0 .1405-.4683c.0733-.6512-.2668-1.3908-.5403-1.963a6.2792 6.2792 0 0 0-1.2001-1.7103zM8.9703.0754a8.6724 8.6724 0 0 0-.83.1564c-.2754.066-.548.1383-.8146.2236-.868.2844-2.0884.8063-2.295 1.8065-.1165.5655.1595 1.1439.3737 1.66.2595.6254.614 1.1889.9373 1.7777.8543 1.5545 1.7245 3.0993 2.5922 4.6457.259.4617.5416 1.0464 1.043 1.2856a1.058 1.058 0 0 0 .1013.0383c.2248.0851.4699.1016.7041.0471a4.3015 4.3015 0 0 0 .0418-.0097 1.2136 1.2136 0 0 0 .5658-.3397 1.1033 1.1033 0 0 0 .079-.0822c.3463-.435.3454-1.0833.3764-1.6134.1042-1.771.2139-3.5423.3009-5.3142.0332-.6712.1055-1.3333.0655-2.0096-.0328-.5579-.0368-1.1984-.3891-1.6563-.6218-.8073-1.9476-.741-2.8523-.6158zm2.084 15.9505a1.1053 1.1053 0 0 0-1.2306-.4145 1.1398 1.1398 0 0 0-.1526.0633 1.4806 1.4806 0 0 0-.2171.1354c-.1992.1475-.3668.3392-.5196.5315-.0386.049-.074.1143-.12.1562l-.7686 1.0573a113.9168 113.9168 0 0 0-1.2913 1.789c-.278.3895-.5184.7184-.7083 1.0094-.036.0547-.0734.116-.1075.1647-.2277.3522-.3566.6092-.4228.8381a1.0945 1.0945 0 0 0-.046.4721c.0211.1655.0768.3246.1635.467.046.0715.0957.1406.1487.207a2.334 2.334 0 0 0 .1754.1825 1.843 1.843 0 0 0 .2108.1732c.5304.369 1.1112.6342 1.722.8391a6.0958 6.0958 0 0 0 1.5716.3004c.091.0046.1821.0025.2728-.006a2.3878 2.3878 0 0 0 .2506-.0351 2.3862 2.3862 0 0 0 .2447-.071 1.1927 1.1927 0 0 0 .4175-.2658c.1127-.113.1994-.249.2541-.3989.0889-.2214.1473-.5026.1857-.92.0034-.0593.0118-.1305.0177-.1958.0304-.3463.0443-.7531.0666-1.2315.0375-.7357.067-1.4681.0903-2.2026 0 0 .0495-1.3053.0494-1.306.0113-.3008.002-.6342-.0814-.9336a1.396 1.396 0 0 0-.1756-.4054zm8.6754 2.0439c-.1605-.176-.3878-.3514-.7462-.5682-.0518-.0288-.1124-.0674-.1684-.1009-.2985-.1795-.658-.3684-1.078-.5965a120.7615 120.7615 0 0 0-1.9427-1.042l-1.1515-.6107c-.0597-.0175-.1203-.0607-.1766-.0878-.2212-.1058-.4558-.2045-.6992-.2498a1.4915 1.4915 0 0 0-.2545-.0265 1.1527 1.1527 0 0 0-.1648.01 1.1077 1.1077 0 0 0-.9227.9133 1.4186 1.4186 0 0 0 .0159.439c.0563.3065.1932.6096.3346.875l.615 1.1526c.3422.65.6884 1.2963 1.0435 1.9406.229.4202.4196.7799.5982 1.078.0338.056.0721.1163.1011.1682.2173.3584.392.584.569.7458.1146.1107.252.195.4026.247.1583.0525.326.071.4919.0546a2.368 2.368 0 0 0 .251-.0435c.0817-.022.1622-.048.241-.0784a1.863 1.863 0 0 0 .2475-.1143 6.1018 6.1018 0 0 0 1.2818-.9597c.4596-.4522.8659-.9454 1.182-1.51.044-.08.0819-.163.1138-.2483a2.49 2.49 0 0 0 .0773-.2411c.0186-.083.033-.1669.0429-.2513a1.188 1.188 0 0 0-.0565-.491 1.0933 1.0933 0 0 0-.248-.4041zm2.86 3.742a.8523.8523 0 0 1-.111.4236c-.074.132-.178.2377-.3115.3172a.8428.8428 0 0 1-.4385.119.847.847 0 0 1-.4373-.1179.8526.8526 0 0 1-.3125-.3171.8548.8548 0 0 1-.111-.4248c0-.1526.038-.2958.1143-.4294a.8405.8405 0 0 1 .315-.3159.849.849 0 0 1 .4315-.1156.8514.8514 0 0 1 .4294.1144.84.84 0 0 1 .316.3148.8494.8494 0 0 1 .1156.4317zm-.1202 0c0-.1328-.0332-.256-.0996-.3698s-.1564-.2038-.2702-.2702a.7125.7125 0 0 0-.371-.1007.7204.7204 0 0 0-.3698.0996.7487.7487 0 0 0-.2713.2702.7181.7181 0 0 0-.0996.3709c0 .132.0332.2557.0996.371a.7355.7355 0 0 0 .2713.2713.7354.7354 0 0 0 .3698.0985.7205.7205 0 0 0 .3698-.0996.7423.7423 0 0 0 .2702-.2691.7186.7186 0 0 0 .1008-.3721zm-.577.0584.2724.4522h-.1922l-.237-.4052h-.1546v.4052h-.1695v-1.02h.2988c.1268 0 .2195.0247.2783.0744.0595.0496.0892.1252.0892.2267a.2785.2785 0 0 1-.0492.1625c-.032.0466-.0775.0813-.1362.1042zm-.0412-.1408a.1532.1532 0 0 0 .056-.1214c0-.0573-.0164-.0981-.0491-.1225-.0329-.0251-.0847-.0377-.1557-.0377h-.1214v.3285h.1237c.061 0 .1098-.0157.1465-.047z" ),
    ( "etsy", "Etsy", "shop handmade store marketplace",
      "M8.559 2.445c0-.325.033-.52.59-.52h7.465c1.3 0 2.02 1.11 2.54 3.193l.42 1.666h1.27c.23-4.728.43-6.784.43-6.784s-3.196.36-5.09.36H6.635L1.521.196v1.37l1.725.326c1.21.24 1.5.496 1.6 1.606 0 0 .11 3.27.11 8.64 0 5.385-.09 8.61-.09 8.61 0 .973-.39 1.333-1.59 1.573l-1.722.33V24l5.13-.165h8.55c1.935 0 6.39.165 6.39.165.105-1.17.75-6.48.855-7.064h-1.2l-1.284 2.91c-1.005 2.28-2.476 2.445-4.11 2.445h-4.906c-1.63 0-2.415-.64-2.415-2.05V12.8s3.62 0 4.79.096c.912.064 1.463.325 1.76 1.598l.39 1.695h1.41l-.09-4.278.192-4.305h-1.391l-.45 1.89c-.283 1.244-.48 1.47-1.754 1.6-1.666.17-4.815.14-4.815.14V2.45h-.05z" ),
]
for sid, name, kw, d in BRANDS:
    symbol( sid, name, "Social media", kw,
            '<path transform="translate(3 3) scale(1.75)" d="%s"/>' % d )


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
