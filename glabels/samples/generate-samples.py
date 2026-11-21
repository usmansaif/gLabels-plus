#!/usr/bin/env python3
#
#  generate-samples.py
#
#  Writes the built-in sample labels (*.glabels) and their index (samples.json)
#  next to this script.  The files are compiled into gLabels Plus as Qt
#  resources; opening one always creates an untitled copy, so the originals
#  are never changed.
#
#  Run it again after editing a design here:  python3 generate-samples.py
#

import json
import os
from xml.sax.saxutils import escape, quoteattr

HERE = os.path.dirname( os.path.abspath( __file__ ) )

BLACK = 0x000000FF
WHITE = 0xFFFFFFFF
GRAY  = 0x666666FF
LIGHT = 0xEEEEEEFF
NONE  = 0x00000000


def mm( v ):
    return "%gmm" % round( v, 4 )


def affine():
    return 'a0="1" a1="0" a2="0" a3="1" a4="0" a5="0"'


def shadow():
    return 'shadow="false" shadow_x="1.3pt" shadow_y="1.3pt" shadow_color="0xff" shadow_opacity="0.5"'


class Label:
    def __init__( self, tmpl ):
        self.tmpl = tmpl
        self.objects = []
        self.files = {}
        CURRENT[0] = tmpl.split( 'part="' )[1].split( '"' )[0] if 'part="' in tmpl else "?"

    def text( self, x, y, w, h, text, size=9, bold=False, align="left", valign="top",
              color=BLACK, italic=False, shrink=False, wrap="word", spacing=1, name=None,
              family="Sans", group=None, path=None, letter_spacing=0 ):
        lines = text.split( "\n" )
        need = len( lines ) * size * 0.3528 * 1.42 * spacing + 2 * 1.06   # text keeps a 3 pt margin inside its box
        if h < need:
            # Grow short boxes so text is neither clipped nor auto-shrunk
            if valign == "vcenter":
                y -= ( need - h ) / 2
            elif valign == "bottom":
                y -= need - h
            if need - h > 3:
                CHECKS.append( "  %s: %r grew from %.1f to %.1f mm" % ( CURRENT[0], lines[0], h, need ) )
            h = need
        body = "".join( "\n      <p>%s</p>" % escape( line ) for line in lines )
        attrs = ( 'x="%s" y="%s" w="%s" h="%s" lock_aspect_ratio="false" color="%d" font_family=%s '
                  'font_size="%g" font_weight="%s" font_italic="%s" font_underline="false" align="%s" '
                  'valign="%s" wrap="%s" line_spacing="%g" auto_shrink="%s" %s %s'
                  % ( mm( x ), mm( y ), mm( w ), mm( h ), color, quoteattr( family ), size, "bold" if bold else "normal",
                      str( italic ).lower(), align, valign, wrap, spacing, str( shrink ).lower(),
                      affine(), shadow() ) )
        if name:
            attrs = 'name=%s ' % quoteattr( name ) + attrs
        if group:
            attrs = 'group=%s ' % quoteattr( group ) + attrs
        if path:
            attrs += ' text_path="%s"' % path
        if letter_spacing:
            attrs += ' letter_spacing="%g"' % round( letter_spacing, 3 )
        self.objects.append( '    <Object-text %s>%s\n    </Object-text>' % ( attrs, body ) )

    def box( self, x, y, w, h, fill=NONE, line=BLACK, lw=0.3 ):
        self.objects.append(
            '    <Object-box x="%s" y="%s" w="%s" h="%s" lock_aspect_ratio="false" line_width="%s" '
            'line_color="%d" fill_color="%d" %s %s/>'
            % ( mm( x ), mm( y ), mm( w ), mm( h ), mm( lw ), line, fill, affine(), shadow() ) )

    def ellipse( self, x, y, w, h, fill=NONE, line=BLACK, lw=0.3 ):
        self.objects.append(
            '    <Object-ellipse x="%s" y="%s" w="%s" h="%s" lock_aspect_ratio="false" line_width="%s" '
            'line_color="%d" fill_color="%d" %s %s/>'
            % ( mm( x ), mm( y ), mm( w ), mm( h ), mm( lw ), line, fill, affine(), shadow() ) )

    def circle( self, cx, cy, r, fill=NONE, line=BLACK, lw=0.3 ):
        self.ellipse( cx - r, cy - r, 2 * r, 2 * r, fill=fill, line=line, lw=lw )

    def image( self, x, y, w, h, svg_name, svg, name=None ):
        """An SVG drawing, embedded in the file's <Data> section."""
        self.files[ svg_name ] = svg
        attrs = ( 'x="%s" y="%s" w="%s" h="%s" lock_aspect_ratio="true" src=%s %s %s'
                  % ( mm( x ), mm( y ), mm( w ), mm( h ), quoteattr( svg_name ), affine(), shadow() ) )
        if name:
            attrs = 'name=%s ' % quoteattr( name ) + attrs
        self.objects.append( '    <Object-image %s/>' % attrs )

    def line( self, x, y, dx, dy=0, lw=0.3, color=BLACK ):
        self.objects.append(
            '    <Object-line x="%s" y="%s" dx="%s" dy="%s" line_width="%s" line_color="%d" %s %s/>'
            % ( mm( x ), mm( y ), mm( dx ), mm( dy ), mm( lw ), color, affine(), shadow() ) )

    def barcode( self, x, y, w, h, data, style="code128", backend="", text=True, checksum=True, name=None ):
        attrs = ( 'x="%s" y="%s" w="%s" h="%s" lock_aspect_ratio="false" backend="%s" style="%s" '
                  'text="%s" checksum="%s" color="%d" data=%s %s'
                  % ( mm( x ), mm( y ), mm( w ), mm( h ), backend, style, str( text ).lower(),
                      str( checksum ).lower(), BLACK, quoteattr( data ), affine() ) )
        if name:
            attrs = 'name=%s ' % quoteattr( name ) + attrs
        self.objects.append( '    <Object-barcode %s/>' % attrs )

    def qr( self, x, y, size, data, name=None ):
        self.barcode( x, y, size, size, data, style="qrcode", backend="qrencode",
                      text=False, checksum=False, name=name )

    def xml( self ):
        if self.files:
            data = "  <Data>\n%s\n  </Data>" % "\n".join(
                '    <File name=%s mimetype="image/svg+xml" encoding="cdata"><![CDATA[%s]]></File>'
                % ( quoteattr( n ), svg ) for n, svg in self.files.items() )
        else:
            data = "  <Data/>"
        return ( '<?xml version="1.0"?>\n<Glabels-document version="4.0">\n%s\n'
                 '  <Objects id="0" rotate="false">\n%s\n  </Objects>\n%s\n</Glabels-document>\n'
                 % ( self.tmpl, "\n".join( self.objects ), data ) )


def roll_template( part, description, w, h, round_=2, margin=1.5 ):
    """A one-label-per-page thermal roll product from the Barcode Labels catalog."""
    return ( '  <Template brand=%s part=%s size="roll" width="%s" height="%s" roll_width="%s" description=%s>\n'
             '    <Label-rectangle id="0" width="%s" height="%s" round="%s" x_waste="0mm" y_waste="0mm">\n'
             '      <Markup-margin x_size="%s" y_size="%s"/>\n'
             '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="0mm" dy="0mm"/>\n'
             '    </Label-rectangle>\n  </Template>'
             % ( quoteattr( "Barcode Labels" ), quoteattr( part ), mm( w ), mm( h ), mm( w + 4 ),
                 quoteattr( description ), mm( w ), mm( h ), mm( round_ ), mm( margin ), mm( margin ) ) )


SAMPLES = []
CHECKS = []
CURRENT = [ "" ]


def sample( file, name, category, description, label ):
    with open( os.path.join( HERE, file ), "w", encoding="utf-8" ) as f:
        f.write( label.xml() )
    SAMPLES.append( { "file": file, "name": name, "category": category, "description": description } )


# ---------------------------------------------------------------------------
# Shipping label, 100 x 150 mm (4 x 6 in)
# ---------------------------------------------------------------------------
L = Label( roll_template( "100 × 150 mm", "Shipping & Courier (4×6)", 100, 150 ) )
L.text( 4, 4, 60, 4, "FROM", size=7, bold=True, color=GRAY )
L.text( 4, 8.5, 60, 18, "Your Company Name\n12 Market Street\nLahore 54000, Pakistan\n+92 300 1234567", size=8.5 )
L.box( 68, 4, 28, 13, fill=BLACK, line=BLACK )
L.text( 68, 4, 28, 13, "EXPRESS", size=13, bold=True, align="hcenter", valign="vcenter", color=WHITE )
L.text( 68, 18.5, 28, 5, "Parcel 1 of 1", size=8, align="hcenter" )
L.line( 0, 28, 100, lw=0.6 )
L.text( 4, 31, 60, 4, "SHIP TO", size=7, bold=True, color=GRAY )
L.text( 4, 36, 92, 8, "Ayesha Khan", size=16, bold=True, shrink=True, name="Recipient" )
L.text( 4, 45, 92, 26, "House 45, Street 7, Block C\nGulberg III\nLahore 54660\nPAKISTAN\nPhone: +92 321 7654321",
        size=10.5, name="Address" )
L.line( 0, 74, 100, lw=0.6 )
for i, ( head, value ) in enumerate( [ ( "ORDER", "#10045" ), ( "WEIGHT", "1.2 kg" ), ( "DATE", "20 Nov 2026" ) ] ):
    x = 4 + i * 32
    L.text( x, 76.5, 30, 4, head, size=7, bold=True, color=GRAY )
    L.text( x, 81, 30, 6, value, size=11, bold=True, shrink=True )
    if i:
        L.line( x - 2, 74, 0, 15, lw=0.3 )
L.line( 0, 89, 100, lw=0.6 )
L.text( 4, 91.5, 92, 4, "TRACKING NUMBER", size=7, bold=True, color=GRAY )
L.barcode( 8, 96.5, 84, 30, "LP1004512345PK", name="Tracking barcode" )
L.line( 0, 129, 100, lw=0.6 )
L.qr( 4, 131.5, 15, "https://example.com/track/LP1004512345PK", name="Tracking QR" )
L.text( 22, 132, 74, 6, "Scan to track your parcel", size=9, bold=True )
L.text( 22, 138, 74, 9, "Handle with care · Do not bend\nReturn to sender if undelivered", size=7.5, color=GRAY )
sample( "shipping-label-4x6.glabels", "Shipping Label 4×6", "Shipping",
        "Sender, recipient, order details, tracking barcode and QR on a 100 × 150 mm thermal label.", L )

# ---------------------------------------------------------------------------
# Product label, 50 x 30 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "50 × 30 mm", "Product / Shelf Label", 50, 30 ) )
L.text( 2, 1.6, 46, 5, "Organic Honey", size=10, bold=True, shrink=True, name="Product name" )
L.text( 2, 7.2, 46, 3.7, "500 g  ·  Pure wildflower", size=6.5, color=GRAY )
L.barcode( 2, 12.4, 29, 15.6, "590123412345", style="ean-13", name="EAN-13" )
L.text( 32.5, 12, 15.5, 3, "PRICE", size=5.5, bold=True, color=GRAY, align="right" )
L.text( 31.5, 15, 16.5, 7, "Rs 950", size=12, bold=True, align="right", shrink=True, name="Price" )
L.text( 31.5, 22.5, 16.5, 5, "incl. all taxes", size=5.5, color=GRAY, align="right" )
sample( "product-label-50x30.glabels", "Product Label", "Retail",
        "Product name, variant, EAN-13 barcode and price on a 50 × 30 mm label.", L )

# ---------------------------------------------------------------------------
# SKU label, 50 x 25 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "50 × 25 mm", "Standard Retail / MRP", 50, 25 ) )
L.text( 2, 1.2, 10, 3.4, "SKU", size=6, bold=True, color=GRAY )
L.text( 2, 3.2, 46, 7.8, "TSH-BLK-M-001", size=11, bold=True, shrink=True, name="SKU" )
L.text( 2, 10.6, 46, 3.7, "Cotton T-Shirt · Black · Size M", size=6.5, color=GRAY )
L.barcode( 2, 15, 46, 8.5, "TSH-BLK-M-001", text=False, name="SKU barcode" )
sample( "sku-label-50x25.glabels", "SKU Label", "Inventory",
        "Large SKU code, short description and a matching Code 128 barcode (50 × 25 mm).", L )

# ---------------------------------------------------------------------------
# Asset tag, 60 x 40 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "60 × 40 mm", "Mid-Size Warehouse / Retail", 60, 40 ) )
L.box( 0, 0, 60, 8, fill=BLACK, line=NONE, lw=0 )
L.text( 2, 0, 56, 8, "PROPERTY OF YOUR COMPANY", size=8, bold=True, color=WHITE,
        align="hcenter", valign="vcenter", shrink=True )
L.qr( 2.5, 10.5, 19, "AST-000123", name="Asset QR" )
L.text( 24, 10, 34, 3, "ASSET ID", size=5.5, bold=True, color=GRAY )
L.text( 24, 13, 34, 6, "AST-000123", size=12, bold=True, shrink=True, name="Asset ID" )
L.text( 24, 19.5, 34, 10, "Laptop · Dell Latitude 5440\nIT Department", size=6.5, shrink=True )
L.barcode( 2.5, 31, 55, 6.5, "AST-000123", text=False, name="Asset barcode" )
sample( "asset-tag-60x40.glabels", "Asset Tag", "Inventory",
        "Company asset tag with QR code, asset ID, description and Code 128 barcode (60 × 40 mm).", L )

# ---------------------------------------------------------------------------
# Simple barcode label, 38 x 25 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "38 × 25 mm", "Compact Retail & Stock", 38, 25 ) )
L.text( 2, 1.5, 34, 4, "Product Name", size=8, bold=True, align="hcenter", shrink=True, name="Name" )
L.barcode( 2, 7.2, 34, 12, "1234567890", name="Barcode" )
L.text( 2, 19.8, 34, 3.6, "Rs 499", size=8, bold=True, align="hcenter", name="Price" )
sample( "barcode-label-38x25.glabels", "Barcode Label", "Retail",
        "A compact Code 128 barcode with product name and price (38 × 25 mm).", L )

# ---------------------------------------------------------------------------
# Price tag, 40 x 30 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "40 × 30 mm", "Healthcare & Asset Tag", 40, 30 ) )
L.box( 2, 2, 11, 5, fill=BLACK, line=NONE, lw=0 )
L.text( 2, 2, 11, 5, "SALE", size=7, bold=True, color=WHITE, align="hcenter", valign="vcenter" )
L.text( 14, 2, 24, 5, "Was Rs 1,599", size=6.5, color=GRAY, align="right", valign="vcenter" )
L.text( 2, 8, 36, 9, "Rs 1,299", size=18, bold=True, align="hcenter", valign="vcenter", shrink=True, name="Price" )
L.text( 2, 17.8, 36, 3.7, "Denim Jacket · Size L", size=6.5, align="hcenter" )
L.barcode( 6, 22.9, 28, 5.6, "PRC-1299-L", text=False )
sample( "price-tag-40x30.glabels", "Sale Price Tag", "Retail",
        "Eye-catching sale price, old price, item name and barcode (40 × 30 mm).", L )

# ---------------------------------------------------------------------------
# Receipt, 80 mm roll
# ---------------------------------------------------------------------------
RECEIPT_H = 150
receipt_tmpl = ( '  <Template brand="Receipt Paper" part="80 × %d mm" size="roll" width="80mm" height="%dmm" '
                 'roll_width="80mm" description="80 mm receipt roll">\n'
                 '    <Label-rectangle id="0" width="80mm" height="%dmm" round="0mm" x_waste="0mm" y_waste="0mm">\n'
                 '      <Markup-margin x_size="3mm" y_size="3mm"/>\n'
                 '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="0mm" dy="0mm"/>\n'
                 '    </Label-rectangle>\n  </Template>' % ( RECEIPT_H, RECEIPT_H, RECEIPT_H ) )
L = Label( receipt_tmpl )
L.text( 4, 5, 72, 8, "YOUR STORE", size=16, bold=True, align="hcenter", shrink=True, name="Store name" )
L.text( 4, 13.5, 72, 12, "12 Market Street, Lahore\nTel +92 300 1234567\nNTN 1234567-8",
        size=7.5, align="hcenter", color=GRAY )
L.line( 4, 27.5, 72, lw=0.3 )
L.text( 4, 29, 36, 8, "Receipt #000123\nCashier: Ali", size=7.5 )
L.text( 40, 29, 36, 8, "20 Nov 2026\n14:32", size=7.5, align="right" )
L.line( 4, 38, 72, lw=0.3 )
L.text( 4, 39.5, 40, 4, "ITEM", size=7, bold=True )
L.text( 44, 39.5, 10, 4, "QTY", size=7, bold=True, align="right" )
L.text( 56, 39.5, 20, 4, "AMOUNT", size=7, bold=True, align="right" )
items = [ ( "Organic Honey 500 g", "2", "1,900.00" ),
          ( "Green Tea (25 bags)", "1", "450.00" ),
          ( "Whole Wheat Bread", "3", "540.00" ),
          ( "Fresh Milk 1 L", "2", "560.00" ),
          ( "Basmati Rice 5 kg", "1", "2,150.00" ) ]
y = 45
for item, qty, amount in items:
    L.text( 4, y, 40, 4.5, item, size=8, shrink=True )
    L.text( 44, y, 10, 4.5, qty, size=8, align="right" )
    L.text( 56, y, 20, 4.5, amount, size=8, align="right" )
    y += 5.5
y += 1
L.line( 4, y, 72, lw=0.3 )
y += 2
for head, value in [ ( "Subtotal", "5,600.00" ), ( "Discount", "-100.00" ), ( "Sales tax (5%)", "275.00" ) ]:
    L.text( 4, y, 50, 4.5, head, size=8 )
    L.text( 46, y, 30, 4.5, value, size=8, align="right" )
    y += 5
L.line( 4, y + 0.5, 72, lw=0.6 )
y += 2.5
L.text( 4, y, 40, 7, "TOTAL", size=12, bold=True, valign="vcenter" )
L.text( 36, y, 40, 7, "Rs 5,775.00", size=12, bold=True, align="right", valign="vcenter", shrink=True, name="Total" )
y += 8
L.line( 4, y, 72, lw=0.6 )
y += 2
L.text( 4, y, 50, 9, "Paid by card\nChange", size=8 )
L.text( 46, y, 30, 9, "5,775.00\n0.00", size=8, align="right" )
y += 12
L.text( 4, y, 72, 10, "Thank you for shopping with us!\nExchanges within 7 days with this receipt.",
        size=8, align="hcenter", italic=True )
y += 12
L.barcode( 12, y, 56, 16, "R000123", name="Receipt barcode" )
sample( "receipt-80mm.glabels", "Sales Receipt", "Business",
        "Store header, items, totals and a receipt barcode on 80 mm thermal receipt paper.", L )

# ---------------------------------------------------------------------------
# Invoice, A4
# ---------------------------------------------------------------------------
invoice_tmpl = ( '  <Template brand="Paper" part="A4 Invoice" size="A4" description="Full-page invoice">\n'
                 '    <Label-rectangle id="0" width="210mm" height="297mm" round="0mm" x_waste="0mm" y_waste="0mm">\n'
                 '      <Markup-margin size="10mm"/>\n'
                 '      <Layout nx="1" ny="1" x0="0mm" y0="0mm" dx="210mm" dy="297mm"/>\n'
                 '    </Label-rectangle>\n  </Template>' )
L = Label( invoice_tmpl )
X0, W = 18, 174
L.text( X0, 18, 100, 10, "Your Company Name", size=18, bold=True, shrink=True, name="Company" )
L.text( X0, 29, 100, 20, "12 Market Street\nLahore 54000, Pakistan\n+92 300 1234567 · hello@example.com",
        size=9.5, color=GRAY )
L.text( 112, 16, 80, 14, "INVOICE", size=30, bold=True, align="right" )
L.text( 112, 32, 46, 18, "Invoice no.\nInvoice date\nDue date", size=9.5, color=GRAY, align="right", spacing=1.3 )
L.text( 160, 32, 32, 18, "INV-2026-0042\n20 Nov 2026\n04 Dec 2026", size=9.5, bold=True, align="right",
        spacing=1.3, name="Invoice details" )
L.line( X0, 54, W, lw=0.5 )
L.text( X0, 60, 80, 5, "BILL TO", size=8, bold=True, color=GRAY )
L.text( X0, 66, 80, 26, "Client Company Ltd.\nAttn: Accounts Payable\n45 Business Avenue\nKarachi 75500, Pakistan",
        size=10, spacing=1.2, name="Bill to" )
L.text( 112, 60, 80, 5, "SHIP TO", size=8, bold=True, color=GRAY, align="right" )
L.text( 112, 66, 80, 26, "Client Company Ltd.\nWarehouse 3, Port Qasim\nKarachi 75020, Pakistan",
        size=10, spacing=1.2, align="right" )
# Table
TY = 98
cols = [ ( X0 + 3, 88, "DESCRIPTION", "left" ), ( 109, 18, "QTY", "right" ),
         ( 129, 30, "UNIT PRICE", "right" ), ( 161, 28, "AMOUNT", "right" ) ]
L.box( X0, TY, W, 9, fill=0x222222FF, line=NONE, lw=0 )
for x, w, head, align in cols:
    L.text( x, TY, w, 9, head, size=8.5, bold=True, color=WHITE, align=align, valign="vcenter" )
rows = [ ( "Website design and development", "1", "45,000.00", "45,000.00" ),
         ( "Web hosting (12 months)", "1", "12,000.00", "12,000.00" ),
         ( "Product labels, printed (50 × 30 mm)", "500", "8.00", "4,000.00" ),
         ( "Barcode setup and SKU numbering", "1", "2,500.00", "2,500.00" ),
         ( "Delivery", "1", "500.00", "500.00" ) ]
y = TY + 9
for i, row in enumerate( rows ):
    if i % 2:
        L.box( X0, y, W, 10, fill=0xF4F4F4FF, line=NONE, lw=0 )
    for ( x, w, _, align ), value in zip( cols, row ):
        L.text( x, y, w, 10, value, size=9.5, align=align, valign="vcenter", shrink=True )
    y += 10
L.line( X0, y, W, lw=0.5 )
# Totals
y += 5
for head, value in [ ( "Subtotal", "64,000.00" ), ( "Sales tax (17%)", "10,880.00" ), ( "Discount", "-880.00" ) ]:
    L.text( 112, y, 45, 6, head, size=9.5, color=GRAY, align="right", valign="vcenter" )
    L.text( 159, y, 30, 6, value, size=9.5, align="right", valign="vcenter" )
    y += 7
L.box( 110, y + 1, 82, 12, fill=0x222222FF, line=NONE, lw=0 )
L.text( 112, y + 1, 45, 12, "TOTAL DUE (PKR)", size=10, bold=True, color=WHITE, align="right", valign="vcenter" )
L.text( 159, y + 1, 30, 12, "74,000.00", size=13, bold=True, color=WHITE, align="right", valign="vcenter",
        shrink=True, name="Total" )
# Payment and notes
NY = 222
L.text( X0, NY, 100, 5, "PAYMENT DETAILS", size=8, bold=True, color=GRAY )
L.text( X0, NY + 6, 110, 24, "Bank: Example Bank Ltd.\nAccount title: Your Company Name\n"
        "IBAN: PK00 EXMP 0000 0000 0000 0000\nPlease quote the invoice number with your payment.",
        size=9, spacing=1.2 )
L.qr( 164, NY, 28, "INV-2026-0042 PKR 74,000.00", name="Payment QR" )
L.text( 154, NY + 29, 38, 5, "Scan for payment reference", size=7, color=GRAY, align="right" )
L.text( X0, NY + 34, 120, 5, "NOTES", size=8, bold=True, color=GRAY )
L.text( X0, NY + 40, 130, 12, "Payment is due within 14 days. Thank you for your business!", size=9 )
L.line( X0, 272, W, lw=0.3 )
L.text( X0, 274, W, 6, "Your Company Name · www.example.com · Registered in Pakistan",
        size=8, color=GRAY, align="hcenter" )
sample( "invoice-a4.glabels", "Invoice (A4)", "Business",
        "A full-page A4 invoice with company details, line items, totals, bank details and a QR code.", L )

# ---------------------------------------------------------------------------
# Address label, 89 x 28 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "89 × 28 mm", "Address / Dymo 30252", 89, 28 ) )
L.text( 4, 2.5, 81, 5.5, "Ayesha Khan", size=11, bold=True, shrink=True, name="Name" )
L.text( 4, 8.5, 81, 17, "House 45, Street 7, Block C, Gulberg III\nLahore 54660\nPakistan",
        size=9, shrink=True, name="Address" )
sample( "address-label-89x28.glabels", "Address Label", "Shipping",
        "Name and mailing address for envelopes and parcels (89 × 28 mm).", L )

# ---------------------------------------------------------------------------
# Warehouse bin / location label, 100 x 50 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "100 × 50 mm", "Box / Tray Marking", 100, 50 ) )
L.text( 4, 2.5, 40, 4, "LOCATION", size=7, bold=True, color=GRAY )
L.text( 4, 6, 62, 15.5, "A-12-03", size=30, bold=True, shrink=True, valign="vcenter", name="Location" )
L.text( 4, 21.5, 62, 5, "Aisle A  ·  Rack 12  ·  Shelf 3", size=9 )
L.qr( 74, 3, 22, "LOC:A-12-03", name="Location QR" )
L.barcode( 4, 28.5, 92, 18, "A-12-03", text=False, name="Location barcode" )
sample( "bin-location-100x50.glabels", "Bin / Shelf Location", "Inventory",
        "Big warehouse location code with QR and Code 128 for scanning (100 × 50 mm).", L )

# ---------------------------------------------------------------------------
# Pallet / carton label, 100 x 100 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "100 × 100 mm", "Pallet / Warehouse Tracking", 100, 100, round_=2 ) )
L.box( 0, 0, 100, 11, fill=BLACK, line=NONE, lw=0 )
L.text( 4, 0, 92, 11, "CARTON / PALLET", size=14, bold=True, color=WHITE, valign="vcenter" )
L.text( 4, 13, 44, 3.5, "FROM", size=6.5, bold=True, color=GRAY )
L.text( 4, 16.5, 44, 12, "Your Company Name\nLahore, Pakistan", size=8 )
L.text( 52, 13, 44, 3.5, "TO", size=6.5, bold=True, color=GRAY )
L.text( 52, 16.5, 44, 12, "Client Company Ltd.\nKarachi, Pakistan", size=8, bold=True )
L.line( 0, 30, 100, lw=0.5 )
L.line( 50, 30, 0, 30, lw=0.5 )
L.line( 0, 45, 100, lw=0.5 )
L.line( 0, 60, 100, lw=0.5 )
cells = [ ( 4, 31, "PO NUMBER", "PO-78421" ), ( 54, 31, "CARTON", "3 of 8" ),
          ( 4, 46, "QUANTITY", "48 units" ), ( 54, 46, "GROSS WEIGHT", "18.5 kg" ) ]
for x, y, head, value in cells:
    L.text( x, y + 1, 42, 3.5, head, size=6.5, bold=True, color=GRAY )
    L.text( x, y + 5, 42, 8, value, size=14, bold=True, shrink=True )
L.text( 4, 62, 92, 3.5, "SSCC", size=6.5, bold=True, color=GRAY )
L.barcode( 4, 66, 92, 24, "00340123450000000017", text=False, name="SSCC barcode" )
L.text( 4, 90.5, 92, 5, "(00) 3 4012345 000000001 7", size=10, bold=True, align="hcenter", name="SSCC text" )
sample( "pallet-label-100x100.glabels", "Carton / Pallet Label", "Shipping",
        "From, to, PO, carton count, quantity, weight and an SSCC barcode (100 × 100 mm).", L )

# ---------------------------------------------------------------------------
# Food date label, 57 x 32 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "57 × 32 mm", "Small Retail & Logistics", 57, 32 ) )
L.text( 2.5, 1.8, 52, 5.5, "Chicken Biryani", size=11, bold=True, shrink=True, name="Product" )
L.line( 2.5, 9.4, 52, lw=0.3 )
rows = [ ( "Prepared", "20 Nov 2026" ), ( "Use by", "22 Nov 2026" ), ( "Batch", "B-1120-07" ) ]
for i, ( head, value ) in enumerate( rows ):
    L.text( 2.5, 10.2 + i * 4.6, 18, 4.5, head, size=7.5, color=GRAY )
    L.text( 20, 10.2 + i * 4.6, 35, 4.5, value, size=8, bold=True )
L.text( 2.5, 24.4, 52, 3.7, "Keep refrigerated at 0–4 °C", size=6.5, italic=True )
sample( "food-date-label-57x32.glabels", "Food Date Label", "Food",
        "Prepared and use-by dates with batch number for kitchens and food packing (57 × 32 mm).", L )

# ---------------------------------------------------------------------------
# QR code label, 50 x 50 mm
# ---------------------------------------------------------------------------
L = Label( roll_template( "50 × 50 mm", "Square Retail / Product", 50, 50 ) )
L.text( 3, 2.5, 44, 5, "SCAN ME", size=11, bold=True, align="hcenter" )
L.qr( 9, 8.5, 32, "https://example.com", name="QR code" )
L.text( 3, 42, 44, 5, "example.com", size=8, align="hcenter", shrink=True, name="Link text" )
sample( "qr-code-label-50x50.glabels", "QR Code Label", "Marketing",
        "A large QR code with a call to action, for links, menus and Wi-Fi (50 × 50 mm).", L )


# ---------------------------------------------------------------------------
# Black and white stickers for thermal printers (up to 4 in / 101.6 mm wide)
# ---------------------------------------------------------------------------
exec( open( os.path.join( HERE, "stickers.py" ), encoding="utf-8" ).read() )


with open( os.path.join( HERE, "samples.json" ), "w", encoding="utf-8" ) as f:
    json.dump( SAMPLES, f, indent=2, ensure_ascii=False )
    f.write( "\n" )

with open( os.path.join( HERE, "..", "samples.qrc" ), "w", encoding="utf-8" ) as f:
    f.write( '<!DOCTYPE RCC>\n\n<RCC version="1.0">\n<qresource>\n\n' )
    f.write( "\t<file>samples/samples.json</file>\n" )
    for s in SAMPLES:
        f.write( "\t<file>samples/%s</file>\n" % s[ "file" ] )
    f.write( "\n</qresource>\n</RCC>\n" )

print( "Wrote %d samples" % len( SAMPLES ) )
if CHECKS:
    print( "Text boxes grown a lot (check the layout):" )
    print( "\n".join( CHECKS ) )
