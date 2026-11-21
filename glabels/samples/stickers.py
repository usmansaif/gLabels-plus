#
#  stickers.py
#
#  Black and white sticker designs for direct thermal printers: round, oval,
#  square and rectangular labels no wider than 4 in (101.6 mm).  Run through
#  generate-samples.py, which provides Label, sample(), mm() and the colours.
#
#  Thermal printers print one colour, so these use pure black and white only
#  (no grey, which would be dithered) and lines of at least 0.25 mm.
#
#  Curved text uses the text object's Path setting (Arc top / Arc bottom),
#  so it stays one editable object.
#

import math
import subprocess

STICKER_BRAND = "Sticker Labels"
SERIF = "Georgia"      # Windows and macOS have it; fontconfig maps it to a serif elsewhere


def sticker_round( diameter, description ):
    r = diameter / 2
    return ( '  <Template brand=%s part=%s size="roll" width="%s" height="%s" roll_width="%s" description=%s>\n'
             '    <Label-round id="0" radius="%s" waste="0mm">\n'
             '      <Markup-margin size="1.5mm"/>\n'
             '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="0mm" dy="0mm"/>\n'
             '    </Label-round>\n  </Template>'
             % ( quoteattr( STICKER_BRAND ), quoteattr( "Round %g mm" % diameter ), mm( diameter ), mm( diameter ),
                 mm( diameter + 4 ), quoteattr( description ), mm( r ) ) )


def sticker_oval( w, h, description ):
    return ( '  <Template brand=%s part=%s size="roll" width="%s" height="%s" roll_width="%s" description=%s>\n'
             '    <Label-ellipse id="0" width="%s" height="%s" waste="0mm">\n'
             '      <Markup-margin size="2mm"/>\n'
             '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="0mm" dy="0mm"/>\n'
             '    </Label-ellipse>\n  </Template>'
             % ( quoteattr( STICKER_BRAND ), quoteattr( "Oval %g × %g mm" % ( w, h ) ), mm( w ), mm( h ),
                 mm( w + 4 ), quoteattr( description ), mm( w ), mm( h ) ) )


def sticker_rect( w, h, description, round_=2 ):
    return ( '  <Template brand=%s part=%s size="roll" width="%s" height="%s" roll_width="%s" description=%s>\n'
             '    <Label-rectangle id="0" width="%s" height="%s" round="%s" x_waste="0mm" y_waste="0mm">\n'
             '      <Markup-margin x_size="1.5mm" y_size="1.5mm"/>\n'
             '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="0mm" dy="0mm"/>\n'
             '    </Label-rectangle>\n  </Template>'
             % ( quoteattr( STICKER_BRAND ), quoteattr( "%g × %g mm" % ( w, h ) ), mm( w ), mm( h ),
                 mm( w + 4 ), quoteattr( description ), mm( w ), mm( h ), mm( round_ ) ) )


# ---------------------------------------------------------------------------
# Font metrics for curved text
# ---------------------------------------------------------------------------
_FONTS = {}


def _font( family, bold, italic, size ):
    key = ( family, bold, italic, size )
    if key not in _FONTS:
        _FONTS[ key ] = None
        try:
            from PIL import ImageFont
            pattern = ( "serif" if family == SERIF else "sans" ) + ( ":bold" if bold else "" ) \
                + ( ":italic" if italic else "" )
            path = subprocess.run( [ "fc-match", "-f", "%{file}", pattern ],
                                   capture_output=True, text=True ).stdout.strip()
            if path:
                _FONTS[ key ] = ImageFont.truetype( path, size * 10 )
        except Exception:
            pass
    return _FONTS[ key ]


def line_height( size, family="Sans", bold=False, italic=False ):
    """Ascent + descent of the font in mm."""
    font = _font( family, bold, italic, size )
    if font is not None:
        ascent, descent = font.getmetrics()
        return ( ascent + descent ) / 10 * 0.3528
    return size * 0.3528 * 1.36


def arc_text( L, cx, cy, r, text, size, bottom=False, bold=False, italic=False, family="Sans",
              tracking=0.0, color=BLACK, name=None ):
    """Curved text: letters centred on a circle of radius r, over the top or under the bottom.

    Uses the text object's own text path ("arc-top" / "arc-bottom"), which fits the
    curve to the object's box, so the words stay editable as one object.
    """
    margin = 1.06                                       # text boxes keep 3 pt inside
    half = r + margin + line_height( size, family, bold, italic ) / 2
    L.text( cx - half, cy - half, 2 * half, 2 * half, text, size=size, bold=bold, italic=italic, family=family,
            align="hcenter", wrap="none", color=color, name=name or "Curved text",
            path="arc-bottom" if bottom else "arc-top", letter_spacing=tracking / 0.3528 )


# ---------------------------------------------------------------------------
# Line drawings (SVG, black on transparent)
# ---------------------------------------------------------------------------
def svg( view_w, view_h, body ):
    return ( '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %g %g" width="%g" height="%g">%s</svg>'
             % ( view_w, view_h, view_w, view_h, body ) )


def _cookie( cx, cy, r, seed ):
    pts = []
    for i in range( 36 ):
        a = 2 * math.pi * i / 36
        rr = r * ( 1 + 0.035 * math.sin( 5 * a + seed ) + 0.02 * math.sin( 11 * a + 2 * seed ) )
        pts.append( "%.2f,%.2f" % ( cx + rr * math.cos( a ), cy + rr * math.sin( a ) ) )
    out = '<polygon points="%s" fill="#fff" stroke="#000" stroke-width="2.2" stroke-linejoin="round"/>' % " ".join( pts )
    chips = [ ( -0.45, -0.35 ), ( 0.2, -0.5 ), ( 0.5, -0.05 ), ( -0.1, 0.05 ), ( -0.5, 0.35 ),
              ( 0.25, 0.45 ), ( -0.15, -0.7 ), ( 0.65, 0.35 ), ( -0.7, -0.05 ) ]
    for i, ( dx, dy ) in enumerate( chips ):
        k = ( i + seed ) % 3
        out += ( '<ellipse cx="%.2f" cy="%.2f" rx="%.2f" ry="%.2f" transform="rotate(%d %.2f %.2f)"/>'
                 % ( cx + dx * r, cy + dy * r, r * ( 0.09 + 0.02 * k ), r * 0.075, 30 * k + 10,
                     cx + dx * r, cy + dy * r ) )
    return out


COOKIES_SVG = svg( 120, 90, _cookie( 38, 30, 26, 1 ) + _cookie( 84, 34, 25, 4 ) + _cookie( 58, 60, 27, 2 ) )
COOKIE_SVG = svg( 60, 60, _cookie( 30, 30, 27, 3 ) )

HEART_SVG = svg( 24, 24, '<path d="M12 21.4l-1.4-1.3C5.4 15.4 2 12.3 2 8.5 2 5.4 4.4 3 7.5 3c1.7 0 3.4.8 4.5 2.1'
                         'C13.1 3.8 14.8 3 16.5 3 19.6 3 22 5.4 22 8.5c0 3.8-3.4 6.9-8.6 11.6z"/>' )

SPARKLE_SVG = svg( 24, 24, '<path d="M12 0C12.9 7.6 16.4 11.1 24 12 16.4 12.9 12.9 16.4 12 24 11.1 16.4 7.6 12.9 0 12'
                           ' 7.6 11.1 11.1 7.6 12 0z"/>' )


def _wheat():
    out = '<path d="M30 118 C30 90 30 60 30 14" stroke="#000" stroke-width="2.4" fill="none" stroke-linecap="round"/>'
    for i in range( 5 ):
        y = 26 + i * 15
        for side in ( -1, 1 ):
            out += ( '<ellipse cx="%g" cy="%g" rx="5" ry="10" transform="rotate(%d %g %g)"/>'
                     % ( 30 + side * 7, y, side * 32, 30 + side * 7, y ) )
    out += '<ellipse cx="30" cy="12" rx="4.5" ry="9"/>'
    return svg( 60, 120, out )


WHEAT_SVG = _wheat()

BEAN_SVG = svg( 60, 80, '<ellipse cx="30" cy="40" rx="24" ry="34" transform="rotate(20 30 40)"/>'
                        '<path d="M40 9 C20 26 42 52 20 71" stroke="#fff" stroke-width="4" fill="none"'
                        ' stroke-linecap="round"/>' )

BEE_SVG = svg( 100, 80,
               '<ellipse cx="38" cy="22" rx="20" ry="13" transform="rotate(-25 38 22)" fill="#fff" stroke="#000"'
               ' stroke-width="3"/>'
               '<ellipse cx="62" cy="22" rx="20" ry="13" transform="rotate(25 62 22)" fill="#fff" stroke="#000"'
               ' stroke-width="3"/>'
               '<defs><clipPath id="b"><ellipse cx="50" cy="50" rx="30" ry="20"/></clipPath></defs>'
               '<ellipse cx="50" cy="50" rx="30" ry="20" fill="#fff" stroke="#000" stroke-width="3"/>'
               '<g clip-path="url(#b)"><rect x="36" y="28" width="8" height="44"/><rect x="52" y="28" width="8"'
               ' height="44"/><rect x="68" y="28" width="8" height="44"/></g>'
               '<circle cx="24" cy="50" r="10"/><circle cx="21" cy="47" r="2.4" fill="#fff"/>'
               '<path d="M17 42 C12 32 8 30 4 31 M23 41 C22 31 19 27 14 25" stroke="#000" stroke-width="2.4"'
               ' fill="none" stroke-linecap="round"/>'
               '<path d="M80 50 L94 50 L80 56 Z"/>' )


def _sprig():
    out = '<path d="M8 72 C30 58 52 40 92 10" stroke="#000" stroke-width="2.6" fill="none" stroke-linecap="round"/>'
    leaves = [ ( 26, 59, -1 ), ( 40, 48, 1 ), ( 52, 39, -1 ), ( 64, 30, 1 ), ( 76, 21, -1 ) ]
    for x, y, side in leaves:
        ang = -38 + side * 50
        out += ( '<path d="M0 0 C6 -7 18 -7 24 0 C18 7 6 7 0 0Z" transform="translate(%g %g) rotate(%d)"'
                 ' fill="#fff" stroke="#000" stroke-width="2.2"/>' % ( x, y, ang - 90 + 90 ) )
        out += ( '<path d="M0 0 L20 0" transform="translate(%g %g) rotate(%d)" stroke="#000" stroke-width="1.2"/>'
                 % ( x, y, ang ) )
    out += ( '<path d="M0 0 C6 -7 18 -7 24 0 C18 7 6 7 0 0Z" transform="translate(88 13) rotate(-38)"'
             ' fill="#000"/>' )
    return svg( 100, 80, out )


SPRIG_SVG = _sprig()

FLAME_SVG = svg( 40, 60,
                 '<path d="M20 2 C24 14 34 20 34 34 A14 14 0 0 1 6 34 C6 25 11 20 13 12 C15 19 18 21 19 22'
                 ' C21 16 21 9 20 2Z"/>'
                 '<path d="M20 30 C22 36 27 38 27 43 A7 7 0 0 1 13 43 C13 38 18 36 20 30Z" fill="#fff"/>'
                 '<rect x="18.5" y="50" width="3" height="9"/>' )

WHISK_SVG = svg( 60, 100,
                 '<rect x="26" y="62" width="8" height="34" rx="4"/>'
                 '<path d="M30 62 C10 46 12 8 30 4 C48 8 50 46 30 62Z M30 62 C20 46 22 10 30 4 C38 10 40 46 30 62Z'
                 ' M30 62 L30 4" fill="none" stroke="#000" stroke-width="2.4"/>' )


# ===========================================================================
# 1. Homemade cookies, round 63.5 mm (2.5 in)
# ===========================================================================
D = 63.5
C = D / 2
L = Label( sticker_round( D, "Round sticker 2.5 in" ) )
arc_text( L, C, C, 25.5, "HOMEMADE", 9, family=SERIF, tracking=1.6 )
L.text( 4, 16, D - 8, 8, "chocolate chip", size=15, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", name="Flavour" )
L.text( 4, 23.5, D - 8, 10, "cookies", size=22, family=SERIF, italic=True, align="hcenter", valign="vcenter",
        name="Product" )
L.line( 8, 36, 19.5, lw=0.3 )
L.image( 29.75, 34, 4, 4, "heart.svg", HEART_SVG )
L.line( 36, 36, 19.5, lw=0.3 )
L.text( 7, 38, D - 14, 12, "flour, butter, sugar, brown sugar, eggs,\nchocolate chips, walnuts, vanilla,\n"
        "baking soda, salt", size=5.6, family=SERIF, align="hcenter", name="Ingredients" )
arc_text( L, C, C, 25.5, "ingredients", 8.5, bottom=True, family=SERIF, italic=True, tracking=1.0 )
sample( "sticker-homemade-cookies-round.glabels", "Homemade Cookies Sticker", "Stickers",
        "Curved serif heading, script-style flavour name and ingredients on a 2.5 in round sticker.", L )

# ===========================================================================
# 2. Home bakery cookies, round 76.2 mm (3 in)
# ===========================================================================
D = 76.2
C = D / 2
L = Label( sticker_round( D, "Round sticker 3 in" ) )
arc_text( L, C, C, 31.5, "YOUR HOME BAKERY", 9.5, tracking=0.9 )
L.image( 27.6, 11, 21, 15.75, "cookies.svg", COOKIES_SVG, name="Cookie drawing" )
L.line( 2, 36.6, 15, lw=0.3 )
L.text( 13, 26.5, 50.2, 15, "Cookies", size=34, family=SERIF, italic=True, align="hcenter", valign="vcenter",
        name="Product" )
L.line( 59.2, 36.6, 15, lw=0.3 )
L.text( 8, 41.5, D - 16, 6, "CHOC CHIP", size=11, align="hcenter", valign="vcenter", name="Flavour" )
L.text( 10, 47.5, D - 20, 4, "INGREDIENTS:", size=6.5, bold=True, align="hcenter" )
L.text( 10, 51.2, D - 20, 10, "Plain flour, white sugar, chocolate chips,\nbutter, eggs, vanilla extract,\n"
        "baking powder, salt", size=6, align="hcenter", name="Ingredients" )
L.text( 14, 60.5, D - 28, 4.5, "Net Wt. 2 oz | 56 g", size=7, bold=True, align="hcenter", name="Net weight" )
arc_text( L, C, C, 33, "WWW.YOURWEBSITE.COM", 7, bottom=True, tracking=0.5 )
sample( "sticker-bakery-cookies-round.glabels", "Home Bakery Cookie Sticker", "Stickers",
        "Bakery name and website on curves, cookie drawing, flavour, ingredients and net weight (3 in round).", L )

# ===========================================================================
# 3. Cottage food label with the home kitchen notice, round 76.2 mm (3 in)
# ===========================================================================
D = 76.2
C = D / 2
L = Label( sticker_round( D, "Round sticker 3 in" ) )
L.circle( C, C, 30.4, lw=0.3 )
arc_text( L, C, C, 33, "SUNNY SIDE COTTAGE BAKERY", 8.5, bold=True, tracking=0.35 )
arc_text( L, C, C, 33.2, "MADE IN A HOME KITCHEN NOT SUBJECT TO GOVERNMENT FOOD SAFETY INSPECTION", 5.2,
          bottom=True, tracking=0.05 )
L.image( C - 4, 8.5, 8, 8, "cookie.svg", COOKIE_SVG )
L.line( 16, 18, D - 32, lw=0.3 )
L.text( 12, 17.5, D - 24, 11, "Cookies", size=24, family=SERIF, italic=True, align="hcenter", valign="vcenter",
        name="Product" )
L.text( 12, 29, D - 24, 4.5, "OATMEAL RAISIN", size=7.5, bold=True, align="hcenter", name="Flavour" )
L.text( 10, 34.5, D - 20, 3.6, "INGREDIENTS:", size=6, bold=True, align="hcenter" )
L.text( 9, 37.6, D - 18, 9, "Oats, plain flour, butter, brown sugar, raisins,\neggs, cinnamon, baking soda, salt",
        size=5.6, align="hcenter", name="Ingredients" )
L.text( 12, 45.5, D - 24, 3.6, "CONTAINS:", size=6, bold=True, align="hcenter" )
L.text( 12, 48.6, D - 24, 4, "Wheat, gluten, milk, eggs", size=5.6, align="hcenter", name="Allergens" )
L.text( 18, 53, D - 36, 4.5, "Net Wt. 8 oz", size=7, bold=True, align="hcenter", name="Net weight" )
L.text( 16, 58, D - 32, 7.5, "12 Baker Street, Your City\n+92 300 1234567", size=5.3, align="hcenter",
        name="Address" )
sample( "sticker-cottage-food-round.glabels", "Cottage Food Sticker", "Stickers",
        "Home kitchen notice around the edge, contact details, flavour, ingredients and allergens (3 in round).", L )

# ===========================================================================
# 4. Cookie box label, 100 x 70 mm
# ===========================================================================
W, H = 100, 70
L = Label( sticker_rect( W, H, "Rectangle sticker 100 × 70 mm", round_=4 ) )
L.box( 2.5, 2.5, W - 5, H - 5, lw=0.5 )
L.text( 6, 6, W - 12, 9, "MINI CHOCOCHIP", size=17, bold=True, align="hcenter", valign="vcenter",
        shrink=True, name="Product" )
L.text( 6, 14.5, W - 12, 9, "COOKIES", size=17, bold=True, align="hcenter", valign="vcenter", name="Product type" )
L.line( 6, 26, W - 12, lw=0.3 )
L.text( 6, 27.5, W - 12, 8, "Fresh from the kitchen", size=11, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", name="Tagline" )
L.line( 6, 37, W - 12, lw=0.3 )
L.text( 5, 40.5, 26, 6, "PREMIUM", size=8, bold=True, align="hcenter", valign="vcenter" )
L.text( 5, 46.5, 26, 7, "COOKIES", size=11, bold=True, align="hcenter", valign="vcenter" )
L.text( 5, 54, 26, 6, "100 g · 12 pcs", size=6.5, align="hcenter", valign="vcenter", name="Weight" )
L.line( 33, 40, 0, 22, lw=0.3 )
L.text( 34, 40.5, 30, 4, "BEST BEFORE", size=6, bold=True, align="hcenter" )
L.text( 34, 46, 30, 8, "30 Nov 2026", size=10, bold=True, align="hcenter", valign="vcenter", name="Best before" )
L.text( 34, 54, 30, 6, "Store in a cool, dry place", size=5.5, align="hcenter", valign="vcenter" )
L.line( 65, 40, 0, 22, lw=0.3 )
L.text( 66.5, 40.5, 29, 4, "ORDER NOW", size=6, bold=True )
L.text( 66.5, 45, 29, 12, "+92 300 1234567\n@crumbandco\ncrumbandco.com", size=6.5, name="Contact" )
sample( "sticker-cookie-box-100x70.glabels", "Cookie Box Label", "Stickers",
        "Bold product name, tagline, best-before date and contact details for cookie boxes (100 × 70 mm).", L )

# ===========================================================================
# 5. Bakery product label with curved notice, 100 x 100 mm
# ===========================================================================
W = H = 100
L = Label( sticker_rect( W, H, "Square sticker 100 × 100 mm", round_=3 ) )
L.box( 2, 2, 96, 96, lw=0.8 )
arc_text( L, 50, 56, 46, "MADE IN A HOME KITCHEN AND NOT SUBJECT TO ROUTINE FOOD SAFETY INSPECTIONS",
          4.8, tracking=0.1 )
L.text( 22, 18, 56, 10, "GOLDEN CRUMB", size=16, family=SERIF, bold=True, align="hcenter", valign="vcenter",
        shrink=True, name="Business name" )
L.text( 22, 27.5, 56, 5, "HOMEMADE BAKERY", size=7.5, bold=True, align="hcenter", valign="vcenter" )
L.line( 20, 34, 60, lw=0.5 )
L.text( 6, 35.5, 88, 10, "HANDMADE SUGAR", size=18, family=SERIF, align="hcenter", valign="vcenter",
        shrink=True, name="Product" )
L.text( 50, 44, 42, 9, "Cookies", size=20, family=SERIF, italic=True, align="hcenter", valign="vcenter",
        name="Product type" )
L.text( 8, 47, 44, 5, "INGREDIENTS", size=8, bold=True, align="right", valign="vcenter" )
L.text( 6, 54, 88, 9, "ALL PURPOSE FLOUR, CANE SUGAR, BROWN SUGAR, EGGS,\nUNSALTED BUTTER, BAKING POWDER, VANILLA EXTRACT",
        size=6.2, align="hcenter", name="Ingredients" )
L.text( 6, 62.5, 88, 6, "NET WT. 2 OZ / 55 G", size=9.5, align="hcenter", valign="vcenter", name="Net weight" )
L.line( 10, 70, 80, lw=0.5 )
L.text( 8, 72, 40, 4.5, "Golden Crumb", size=9, family=SERIF, italic=True, align="hcenter" )
L.text( 8, 77, 40, 9, "206 Orchard Lane, Your City\nPostcode 54000", size=5.6, align="hcenter", name="Address" )
L.line( 50, 72, 0, 13, lw=0.5 )
L.text( 53, 72, 38, 4, "CONTAINS", size=7.5, bold=True )
L.text( 53, 77, 38, 4, "MILK, EGGS, WHEAT", size=6, name="Allergens" )
L.image( 21, 89, 4, 4, "sparkle.svg", SPARKLE_SVG )
L.text( 26, 88.5, 48, 5, "ALWAYS HOMEMADE WITH LOVE", size=6.5, align="hcenter", valign="vcenter" )
L.image( 75, 89, 4, 4, "sparkle.svg", SPARKLE_SVG )
sample( "sticker-bakery-product-100x100.glabels", "Bakery Product Label", "Stickers",
        "Curved home kitchen notice, bakery name, product, ingredients, weight, address and allergens "
        "(100 × 100 mm).", L )

# ===========================================================================
# 6. Thank you sticker, round 50.8 mm (2 in)
# ===========================================================================
D = 50.8
C = D / 2
L = Label( sticker_round( D, "Round sticker 2 in" ) )
L.circle( C, C, 23.6, lw=0.8 )
L.circle( C, C, 22.2, lw=0.3 )
arc_text( L, C, C, 18.4, "THANK YOU", 9, bold=True, tracking=1.2 )
L.image( C - 3.5, 13.5, 7, 7, "heart.svg", HEART_SVG )
L.text( 5, 21, D - 10, 12, "for supporting\nsmall business", size=8.5, family=SERIF, italic=True,
        align="hcenter", valign="vcenter", name="Message" )
arc_text( L, C, C, 18.4, "@yourshop", 7, bottom=True, tracking=0.4 )
sample( "sticker-thank-you-round.glabels", "Thank You Sticker", "Stickers",
        "Thank-you message with a heart and your shop handle for packaging (2 in round).", L )

# ===========================================================================
# 7. Handmade seal, round 38.1 mm (1.5 in)
# ===========================================================================
D = 38.1
C = D / 2
L = Label( sticker_round( D, "Round sticker 1.5 in" ) )
L.circle( C, C, 18.3, fill=BLACK, line=NONE, lw=0 )
L.circle( C, C, 12.2, line=WHITE, lw=0.35 )
arc_text( L, C, C, 15.2, "HANDMADE WITH LOVE", 6, bold=True, tracking=0.35, color=WHITE )
arc_text( L, C, C, 15.2, "SMALL BATCH", 6, bottom=True, bold=True, tracking=0.6, color=WHITE )
L.text( 8, 12.5, D - 16, 13, "100%\nHANDMADE", size=7, bold=True, color=WHITE, align="hcenter", valign="vcenter",
        name="Centre text" )
sample( "sticker-handmade-seal-round.glabels", "Handmade Seal", "Stickers",
        "Solid black seal with white curved text for boxes and bags (1.5 in round).", L )

# ===========================================================================
# 8. Jam jar label, 76 x 51 mm (3 x 2 in)
# ===========================================================================
W, H = 76.2, 50.8
L = Label( sticker_rect( W, H, "Rectangle sticker 3 × 2 in", round_=3 ) )
L.box( 2, 2, W - 4, H - 4, lw=0.6 )
L.box( 3.4, 3.4, W - 6.8, H - 6.8, lw=0.25 )
L.text( 6, 5.5, W - 12, 4.5, "H O M E M A D E", size=7, bold=True, align="hcenter" )
L.text( 6, 10.5, W - 12, 13, "Strawberry", size=24, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", shrink=True, name="Flavour" )
L.text( 6, 22.3, W - 12, 6, "J A M", size=11, bold=True, align="hcenter", valign="vcenter", name="Product" )
L.line( 13, 32.3, 18, lw=0.3 )
L.image( 33.6, 28.8, 9, 7.2, "sprig.svg", SPRIG_SVG )
L.line( 45.2, 32.3, 18, lw=0.3 )
L.text( 6, 35.6, W - 12, 4, "Strawberries, cane sugar, lemon juice", size=6, align="hcenter", name="Ingredients" )
L.text( 6, 40, 34, 4.5, "Made: ____________", size=6.5 )
L.text( 38, 40, W - 44, 4.5, "Net wt 250 g", size=6.5, bold=True, align="right", name="Net weight" )
sample( "sticker-jam-jar-76x51.glabels", "Jam Jar Label", "Stickers",
        "Double-border jar label with flavour, ingredients, a date to fill in and net weight (3 × 2 in).", L )

# ===========================================================================
# 9. Honey jar label, oval 90 x 60 mm
# ===========================================================================
W, H = 90, 60
L = Label( sticker_oval( W, H, "Oval sticker 90 × 60 mm" ) )
L.ellipse( 2, 2, W - 4, H - 4, lw=0.6 )
L.ellipse( 3.6, 3.6, W - 7.2, H - 7.2, lw=0.25 )
L.image( 37.5, 6.5, 15, 12, "bee.svg", BEE_SVG, name="Bee drawing" )
L.text( 10, 18.5, W - 20, 10, "RAW HONEY", size=17, bold=True, align="hcenter", valign="vcenter",
        name="Product" )
L.text( 10, 27.5, W - 20, 9, "Wildflower", size=15, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", name="Variety" )
L.line( 22, 38, W - 44, lw=0.3 )
L.text( 12, 39, W - 24, 4.5, "PURE · UNFILTERED · LOCAL", size=6.5, bold=True, align="hcenter" )
L.text( 16, 44, W - 32, 4.5, "Honey Lane Apiary", size=7, family=SERIF, italic=True, align="hcenter",
        name="Business name" )
L.text( 22, 49, W - 44, 4.5, "Net wt 500 g", size=6.5, align="hcenter", name="Net weight" )
sample( "sticker-honey-oval-90x60.glabels", "Honey Jar Label", "Stickers",
        "Oval honey label with a bee drawing, variety, apiary name and net weight (90 × 60 mm).", L )

# ===========================================================================
# 10. Candle label, 101.6 x 50.8 mm (4 x 2 in)
# ===========================================================================
W, H = 101.6, 50.8
L = Label( sticker_rect( W, H, "Rectangle sticker 4 × 2 in", round_=3 ) )
L.box( 2, 2, W - 4, H - 4, lw=0.4 )
L.image( 7, 9, 12, 18, "flame.svg", FLAME_SVG, name="Flame drawing" )
L.text( 4, 28, 18, 4, "NO. 07", size=7, bold=True, align="hcenter", name="Number" )
L.line( 24, 6, 0, 28, lw=0.3 )
L.text( 27, 5.5, 70, 10, "VANILLA & AMBER", size=17, family=SERIF, bold=True, valign="vcenter",
        shrink=True, name="Scent" )
L.text( 27, 15.5, 70, 6, "Hand-poured soy wax candle", size=9, family=SERIF, italic=True, valign="vcenter" )
L.text( 27, 23, 70, 4.5, "WARM VANILLA · AMBER · SANDALWOOD", size=6.5, bold=True, name="Notes" )
L.text( 27, 28.5, 70, 4.5, "8 oz / 226 g  ·  approx. 45 hour burn", size=6.5, name="Size" )
L.line( 5, 36, W - 10, lw=0.3 )
L.text( 5, 37, W - 10, 9, "Never leave a burning candle unattended. Burn within sight, away from anything that can "
        "catch fire and out of reach of children and pets. Trim the wick to 5 mm before each use.",
        size=5.6, name="Warning" )
sample( "sticker-candle-4x2.glabels", "Candle Label", "Stickers",
        "Scent name, notes, size, burn time and the safety warning, with a flame drawing (4 × 2 in).", L )

# ===========================================================================
# 11. Handmade soap label, 76.2 x 76.2 mm (3 x 3 in)
# ===========================================================================
W = H = 76.2
C = W / 2
L = Label( sticker_rect( W, H, "Square sticker 3 × 3 in", round_=3 ) )
L.circle( C, C, 35, lw=0.6 )
L.circle( C, C, 26.5, lw=0.25 )
arc_text( L, C, C, 30.8, "NATURAL · HANDMADE · COLD PROCESS", 6.3, bold=True, tracking=0.3 )
arc_text( L, C, C, 30.8, "MAPLE & OAK SOAP CO.", 6.3, bottom=True, bold=True, tracking=0.5 )
L.image( C - 6, 13.5, 12, 9.6, "sprig.svg", SPRIG_SVG )
L.text( 13, 23, W - 26, 8, "LAVENDER", size=14, bold=True, align="hcenter", valign="vcenter", name="Product" )
L.text( 13, 30.5, W - 26, 7, "Goat Milk Soap", size=11, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", name="Type" )
L.text( 16, 39, W - 32, 12, "Olive oil, coconut oil, goat milk,\nshea butter, lavender essential oil",
        size=5.6, align="hcenter", name="Ingredients" )
L.text( 20, 51, W - 40, 4.5, "Net wt 4 oz / 113 g", size=6.5, bold=True, align="hcenter", name="Net weight" )
sample( "sticker-soap-76x76.glabels", "Handmade Soap Label", "Stickers",
        "Circular badge with curved text, soap name, ingredients and weight on a 3 × 3 in square.", L )

# ===========================================================================
# 12. Coffee bag label, round 63.5 mm (2.5 in)
# ===========================================================================
D = 63.5
C = D / 2
L = Label( sticker_round( D, "Round sticker 2.5 in" ) )
L.circle( C, C, 30, lw=0.5 )
arc_text( L, C, C, 26.4, "NORTHSIDE ROASTERS", 7.5, bold=True, tracking=0.6 )
arc_text( L, C, C, 26.4, "WHOLE BEAN · 250 G", 7, bottom=True, bold=True, tracking=0.6 )
L.image( C - 3, 9.5, 6, 8, "bean.svg", BEAN_SVG )
L.text( 8, 17, D - 16, 8, "ETHIOPIA", size=15, bold=True, align="hcenter", valign="vcenter", name="Origin" )
L.text( 8, 24.5, D - 16, 7, "Yirgacheffe", size=11, family=SERIF, italic=True, align="hcenter",
        valign="vcenter", name="Region" )
L.text( 10, 32, D - 20, 4, "Jasmine · lemon · honey", size=6.5, align="hcenter", name="Tasting notes" )
L.text( 13, 37, 13, 4, "ROAST", size=6, bold=True, valign="vcenter" )
for i in range( 3 ):
    L.box( 27 + i * 8, 37.6, 6.5, 2.8, fill=BLACK if i == 0 else NONE, lw=0.3 )
L.text( 25.5, 40.8, 11, 3.5, "LIGHT", size=5, align="hcenter" )
L.text( 41.5, 40.8, 11, 3.5, "DARK", size=5, align="hcenter" )
L.text( 9, 45, D - 18, 4.5, "Roasted: ____________", size=6.5, align="hcenter", name="Roast date" )
sample( "sticker-coffee-round.glabels", "Coffee Bag Label", "Stickers",
        "Roaster name on curves, origin, tasting notes, roast level and a roast date to fill in (2.5 in round).", L )

# ===========================================================================
# 13. Cake box label, 100 x 75 mm
# ===========================================================================
W, H = 100, 75
L = Label( sticker_rect( W, H, "Rectangle sticker 100 × 75 mm", round_=3 ) )
L.box( 1.5, 1.5, W - 3, 13, fill=BLACK, line=NONE, lw=0 )
L.image( 5, 3.5, 6, 9, "whisk.svg", WHISK_SVG.replace( '#000', '#fff' ).replace( '<rect ', '<rect fill="#fff" ' ) )
L.text( 13, 1.5, 58, 13, "Celebration Cake", size=15, family=SERIF, italic=True, color=WHITE, valign="vcenter",
        name="Product" )
L.text( 66, 1.5, 30, 13, "LITTLE WHISK\nBAKE HOUSE", size=6.5, bold=True, color=WHITE, align="right",
        valign="vcenter", name="Business name" )
rows = [ ( "FLAVOUR", "Chocolate fudge" ), ( "SIZE", "8 in · serves 12" ), ( "MADE ON", "" ),
         ( "BEST BEFORE", "" ) ]
y = 18
for head, value in rows:
    L.text( 4, y, 26, 6, head, size=7, bold=True, valign="vcenter" )
    L.line( 31, y + 5.3, 65, lw=0.25 )
    if value:
        L.text( 31, y, 65, 6, value, size=8.5, valign="vcenter", name=head.title() )
    y += 8.5
L.text( 4, y + 0.5, 30, 4.5, "CONTAINS", size=7, bold=True )
for i, allergen in enumerate( [ "Wheat", "Milk", "Eggs", "Nuts", "Soy" ] ):
    x = 4 + i * 19
    L.box( x, y + 6.3, 3.2, 3.2, lw=0.3 )
    L.text( x + 3.4, y + 5.2, 15, 5.4, allergen, size=7, valign="vcenter" )
L.text( 4, y + 13, W - 8, 4.5, "Keep refrigerated. Enjoy within 3 days.", size=6.5, italic=True )
sample( "sticker-cake-box-100x75.glabels", "Cake Box Label", "Stickers",
        "Cake name, flavour, size, dates to fill in and allergen tick boxes for cake boxes (100 × 75 mm).", L )

# ===========================================================================
# 14. Box seal strip, 100 x 30 mm
# ===========================================================================
W, H = 100, 30
L = Label( sticker_rect( W, H, "Seal strip 100 × 30 mm", round_=2 ) )
L.box( 1.5, 1.5, 25, H - 3, fill=BLACK, line=NONE, lw=0 )
L.image( 9, 5, 9, 9, "heart-white.svg", HEART_SVG.replace( '<path ', '<path fill="#fff" ' ) )
L.text( 2, 15.5, 24, 10, "SEALED\nFRESH", size=7.5, bold=True, color=WHITE, align="hcenter", valign="vcenter" )
L.text( 30, 2.5, 66, 9, "Sealed with love", size=16, family=SERIF, italic=True, valign="vcenter",
        name="Heading" )
L.text( 30, 11.5, 66, 9, "If this seal is broken, please do not accept\nthe package and contact us.",
        size=6.8, valign="vcenter", name="Message" )
L.line( 30, 21.3, 66, lw=0.25 )
L.text( 30, 22, 66, 5.5, "@crumbandco  ·  +92 300 1234567", size=6.5, bold=True, valign="vcenter",
        name="Contact" )
sample( "sticker-box-seal-100x30.glabels", "Box Seal Sticker", "Stickers",
        "Tamper-evident seal strip with a message and contact details, for boxes and bags (100 × 30 mm).", L )

# ===========================================================================
# 15. Spice jar label, round 38.1 mm (1.5 in)
# ===========================================================================
D = 38.1
C = D / 2
L = Label( sticker_round( D, "Round sticker 1.5 in" ) )
L.circle( C, C, 18.3, lw=0.5 )
arc_text( L, C, C, 15.6, "SPICE PANTRY", 5.8, bold=True, tracking=0.7 )
arc_text( L, C, C, 15.6, "NET 50 G", 5.8, bottom=True, bold=True, tracking=0.7 )
L.text( 5, 10.5, D - 10, 11, "Smoked\nPaprika", size=9.5, bold=True, align="hcenter", valign="vcenter",
        spacing=0.9, name="Spice" )
L.line( 11, 22.5, D - 22, lw=0.25 )
L.text( 6, 23, D - 12, 4.5, "Best before 11/2027", size=5, align="hcenter", name="Best before" )
sample( "sticker-spice-jar-round.glabels", "Spice Jar Label", "Stickers",
        "Small round spice label with curved brand and weight, spice name and best-before date (1.5 in round).", L )
