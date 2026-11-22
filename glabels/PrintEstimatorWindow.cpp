//  PrintEstimatorWindow.cpp
//
//  Copyright (C) 2026  gLabels Plus contributors (Usman Saif)
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

#include "PrintEstimatorWindow.hpp"

#include "Help.hpp"
#include "SelectProductDialog.hpp"

#include "model/Distance.hpp"
#include "model/Frame.hpp"
#include "model/Settings.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace glabels
{

        namespace
        {
                const QString SETTINGS_GROUP = QStringLiteral( "PrintEstimator" );

                // Largest sheet or product accepted (2 m)
                const double MAX_LENGTH_PTS = model::Distance::mm( 2000 ).pt();

                // Smallest printing sheet accepted: A4, either way round
                const double MIN_SHEET_SHORT_PTS = model::Distance::mm( 210 ).pt();
                const double MIN_SHEET_LONG_PTS  = model::Distance::mm( 297 ).pt();

                // Common non-printable gripper/edge margin of digital and offset presses
                const double DEFAULT_MARGIN_MM = 5.0;
                const double DEFAULT_MARGIN_IN = 0.25;

                // Colors used in saved images and PDFs (ink and a single accent)
                const QColor EXPORT_INK( "#1f2328" );
                const QColor EXPORT_MUTED( "#6e7781" );
                const QColor EXPORT_ACCENT( "#e8590c" );


                ///
                /// Standard commercial printing sheets, A4 and larger, smallest first
                ///
                struct SheetSize
                {
                        const char* name;
                        double      w;        // in mm, or inches when isInches
                        double      h;
                        bool        isInches;

                        double wPts() const { return isInches ? model::Distance::in( w ).pt() : model::Distance::mm( w ).pt(); }
                        double hPts() const { return isInches ? model::Distance::in( h ).pt() : model::Distance::mm( h ).pt(); }
                };

                const SheetSize SHEETS[] = {
                        { "A4",                 210,  297,  false },
                        { "SRA4",               225,  320,  false },
                        { "US Legal",           8.5,  14,   true  },
                        { "A3",                 297,  420,  false },
                        { "Tabloid (11 × 17 in)", 11, 17,   true  },
                        { "12 × 18 in",         12,   18,   true  },
                        { "SRA3",               320,  450,  false },
                        { "Super B (13 × 19 in)", 13, 19,   true  },
                        { "B3",                 353,  500,  false },
                        { "A2",                 420,  594,  false },
                        { "SRA2",               450,  640,  false },
                        { "19 × 25 in",         19,   25,   true  },
                        { "B2",                 500,  707,  false },
                        { "23 × 29 in",         23,   29,   true  },
                        { "A1",                 594,  841,  false },
                        { "23 × 35 in",         23,   35,   true  },
                        { "SRA1",               640,  900,  false },
                        { "25 × 38 in",         25,   38,   true  },
                        { "B1",                 707,  1000, false },
                        { "28 × 40 in",         28,   40,   true  },
                        { "A0",                 841,  1189, false },
                        { "SRA0",               900,  1280, false },
                        { "B0",                 1000, 1414, false },
                };
                const int N_SHEETS = int( sizeof(SHEETS) / sizeof(SHEETS[0]) );
                const int CUSTOM_SHEET = N_SHEETS;               // last combo entry
                const QString DEFAULT_SHEET = QStringLiteral( "SRA3" );


                struct SheetColors
                {
                        QColor paper;
                        QColor edge;
                        QColor margin;
                        QColor accent;
                };


                ///
                /// Draw a sheet with its margin and items, scaled to fit an area
                ///
                void drawSheet( QPainter& p, const QRectF& area, const model::SheetLayout& layout,
                                const SheetColors& colors )
                {
                        if ( layout.sheetW() <= 0 || layout.sheetH() <= 0 || area.isEmpty() )
                        {
                                return;
                        }

                        double scale = std::min( area.width() / layout.sheetW(), area.height() / layout.sheetH() );
                        QRectF sheet( 0, 0, layout.sheetW() * scale, layout.sheetH() * scale );
                        sheet.moveCenter( area.center() );

                        p.save();
                        p.setRenderHint( QPainter::Antialiasing, true );
                        p.translate( sheet.topLeft() );
                        p.scale( scale, scale );

                        QRectF paper( 0, 0, layout.sheetW(), layout.sheetH() );
                        QRectF printable = paper.adjusted( layout.margin(), layout.margin(),
                                                           -layout.margin(), -layout.margin() );

                        // Paper, with the non-printable margin shaded
                        p.setPen( Qt::NoPen );
                        p.setBrush( colors.paper );
                        p.drawRect( paper );
                        if ( layout.margin() > 0 )
                        {
                                QPainterPath band;
                                band.addRect( paper );
                                band.addRect( printable );
                                band.setFillRule( Qt::OddEvenFill );
                                QColor shade = colors.margin;
                                shade.setAlpha( 40 );
                                p.setBrush( shade );
                                p.drawPath( band );

                                QPen dashed( colors.margin, 1, Qt::DashLine );
                                dashed.setCosmetic( true );
                                p.setPen( dashed );
                                p.setBrush( Qt::NoBrush );
                                p.drawRect( printable );
                        }

                        // Items: bleed area light, finished (trim) area stronger
                        QColor bleedFill = colors.accent;
                        bleedFill.setAlpha( 45 );
                        QColor trimFill = colors.accent;
                        trimFill.setAlpha( 110 );
                        QColor trimFillRotated = colors.accent;
                        trimFillRotated.setAlpha( 75 );
                        QPen trimPen( colors.accent, 1 );
                        trimPen.setCosmetic( true );

                        for ( const auto& item : layout.items() )
                        {
                                if ( layout.bleed() > 0 )
                                {
                                        p.setPen( Qt::NoPen );
                                        p.setBrush( bleedFill );
                                        p.drawRect( item.bleedBox );
                                }
                                p.setPen( trimPen );
                                p.setBrush( item.rotated ? trimFillRotated : trimFill );
                                p.drawRect( item.trimBox );
                        }

                        // Sheet edge on top
                        QPen edgePen( colors.edge, 1 );
                        edgePen.setCosmetic( true );
                        p.setPen( edgePen );
                        p.setBrush( Qt::NoBrush );
                        p.drawRect( paper );

                        p.restore();
                }


                QString percent( double fraction )
                {
                        return QLocale().toString( fraction * 100.0, 'f', 1 ) + " %";
                }
        }


        ///
        /// Sheet preview widget
        ///
        class SheetPreview : public QWidget
        {
        public:
                explicit SheetPreview( QWidget* parent = nullptr ) : QWidget( parent )
                {
                        setMinimumSize( 320, 320 );
                        setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
                }

                void setLayoutResult( const model::SheetLayout* layout, bool stale )
                {
                        mLayout = layout;
                        mStale  = stale;
                        update();
                }

        protected:
                void paintEvent( QPaintEvent* ) override
                {
                        QPainter p( this );
                        p.fillRect( rect(), palette().color( QPalette::Base ) );

                        if ( !mLayout )
                        {
                                QColor muted = palette().color( QPalette::PlaceholderText );
                                p.setPen( muted );
                                p.drawText( rect().adjusted( 24, 24, -24, -24 ), Qt::AlignCenter | Qt::TextWordWrap,
                                            PrintEstimatorWindow::tr( "Choose a product and a printing sheet." ) );
                                return;
                        }

                        if ( mStale )
                        {
                                p.setOpacity( 0.4 );
                        }

                        SheetColors colors;
                        colors.paper  = Qt::white;
                        colors.edge   = palette().color( QPalette::Text );
                        colors.margin = QColor( "#6e7781" );
                        colors.accent = palette().color( QPalette::Highlight );
                        drawSheet( p, QRectF( rect() ).adjusted( 16, 16, -16, -16 ), *mLayout, colors );
                }

        private:
                const model::SheetLayout* mLayout{ nullptr };
                bool mStale{ false };
        };


        QPointer<PrintEstimatorWindow> PrintEstimatorWindow::sInstance;


        ///
        /// Show the estimator window (creating it once)
        ///
        void PrintEstimatorWindow::showWindow( const model::Template& tmplate )
        {
                if ( !sInstance )
                {
                        sInstance = new PrintEstimatorWindow();
                        sInstance->resize( 1100, 720 );
                }

                if ( !tmplate.isNull() )
                {
                        sInstance->setTemplate( tmplate );
                }

                if ( sInstance->isMinimized() )
                {
                        sInstance->showNormal();
                }
                sInstance->show();
                sInstance->raise();
                sInstance->activateWindow();
        }


        ///
        /// Constructor
        ///
        PrintEstimatorWindow::PrintEstimatorWindow()
                : QMainWindow( nullptr )
        {
                setAttribute( Qt::WA_DeleteOnClose );
                setWindowTitle( tr("Print Sheet Estimator") );
                setWindowIcon( QApplication::windowIcon() );

                // Physical units only; points and picas fall back to millimeters
                auto units = model::Settings::units();
                mUnits = (units.toEnum() == model::Units::IN || units.toEnum() == model::Units::CM) ?
                        units : model::Units::mm();

                buildUi();
                loadSettings();
                onProductTypeChanged();
                onSheetChanged();
                estimate();

                auto* help = new QShortcut( QKeySequence::HelpContents, this );
                connect( help, &QShortcut::activated, this, [this]() {
                        Help::displayContents( this, "estimator" );
                } );
        }


        void PrintEstimatorWindow::closeEvent( QCloseEvent* event )
        {
                saveSettings();
                QMainWindow::closeEvent( event );
        }


        ///
        /// Widgets
        ///
        void PrintEstimatorWindow::buildUi()
        {
                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted = QColor::fromRgbF( (fg.redF()   + bg.redF())   / 2,
                                                 (fg.greenF() + bg.greenF()) / 2,
                                                 (fg.blueF()  + bg.blueF())  / 2 );

                setStyleSheet( QString(
                        "#peMuted { color: %1; }"
                        "#peTile { border: 1px solid palette(mid); border-radius: 8px; background: palette(base); }"
                        "#peTileKey { color: %1; }"
                        "#peTileValue { font-size: 20pt; font-weight: 600; }"
                        "#peError { color: #c62828; }"
                        "#peNote { color: %1; font-style: italic; }" ).arg( muted.name() ) );

                // ---- Left: inputs -------------------------------------------------
                auto* inputs = new QWidget();
                auto* inputsLayout = new QVBoxLayout( inputs );
                inputsLayout->setContentsMargins( 16, 16, 16, 16 );
                inputsLayout->setSpacing( 12 );

                // Units
                auto* unitsRow = new QHBoxLayout();
                unitsRow->addWidget( new QLabel( tr("Units:") ) );
                mUnitsCombo = new QComboBox();
                mUnitsCombo->addItem( tr("Millimeters (mm)"), "mm" );
                mUnitsCombo->addItem( tr("Centimeters (cm)"), "cm" );
                mUnitsCombo->addItem( tr("Inches (in)"),      "in" );
                mUnitsCombo->setCurrentIndex( mUnitsCombo->findData( mUnits.toIdString() ) );
                unitsRow->addWidget( mUnitsCombo, 1 );
                inputsLayout->addLayout( unitsRow );

                // Product
                auto* productBox = new QGroupBox( tr("Product") );
                auto* productLayout = new QVBoxLayout( productBox );

                mLabelRadio  = new QRadioButton( tr("Label") );
                mPageRadio   = new QRadioButton( tr("Page") );
                mCustomRadio = new QRadioButton( tr("Custom size") );
                mLabelRadio->setToolTip( tr("One label of the chosen product, printed and cut out on its own") );
                mPageRadio->setToolTip( tr("The whole page of the chosen product, as one printed piece") );
                mTypeGroup = new QButtonGroup( this );
                mTypeGroup->addButton( mLabelRadio,  LABEL );
                mTypeGroup->addButton( mPageRadio,   PAGE );
                mTypeGroup->addButton( mCustomRadio, CUSTOM );
                auto* typeRow = new QHBoxLayout();
                typeRow->addWidget( mLabelRadio );
                typeRow->addWidget( mPageRadio );
                typeRow->addWidget( mCustomRadio );
                typeRow->addStretch();
                productLayout->addLayout( typeRow );

                auto* chooseRow = new QHBoxLayout();
                mProductLabel = new QLabel();
                mProductLabel->setObjectName( "peMuted" );
                mProductLabel->setWordWrap( true );
                mChooseButton = new QPushButton( tr("Choose...") );
                mChooseButton->setToolTip( tr("Choose a product from the library") );
                mClearButton = new QPushButton( tr("Clear") );
                mClearButton->setToolTip( tr("Remove the chosen product and enter a custom size") );
                mClearButton->hide();
                chooseRow->addWidget( mProductLabel, 1 );
                chooseRow->addWidget( mChooseButton );
                chooseRow->addWidget( mClearButton );
                productLayout->addLayout( chooseRow );

                auto* productForm = new QFormLayout();
                mItemWSpin = new QDoubleSpinBox();
                mItemHSpin = new QDoubleSpinBox();
                mQuantitySpin = new QSpinBox();
                mQuantitySpin->setRange( 1, 100000000 );
                mQuantitySpin->setGroupSeparatorShown( true );
                mQuantitySpin->setValue( 1000 );
                productForm->addRow( tr("Width:"),    mItemWSpin );
                productForm->addRow( tr("Height:"),   mItemHSpin );
                productForm->addRow( tr("Quantity:"), mQuantitySpin );
                productLayout->addLayout( productForm );
                inputsLayout->addWidget( productBox );

                // Printing sheet
                auto* sheetBox = new QGroupBox( tr("Printing sheet") );
                auto* sheetForm = new QFormLayout( sheetBox );
                mSheetCombo = new QComboBox();
                mSheetWSpin = new QDoubleSpinBox();
                mSheetHSpin = new QDoubleSpinBox();
                mMarginSpin = new QDoubleSpinBox();
                mMarginSpin->setToolTip( tr("Edge of the sheet the press cannot print on (gripper), on each side") );
                sheetForm->addRow( tr("Sheet:"),  mSheetCombo );
                sheetForm->addRow( tr("Width:"),  mSheetWSpin );
                sheetForm->addRow( tr("Height:"), mSheetHSpin );
                sheetForm->addRow( tr("Margin:"), mMarginSpin );
                inputsLayout->addWidget( sheetBox );

                // Bleed
                auto* bleedBox = new QGroupBox( tr("Bleed") );
                auto* bleedForm = new QFormLayout( bleedBox );
                mBleedSpin = new QDoubleSpinBox();
                mBleedSpin->setToolTip( tr("Extra printed area around each item, trimmed off after cutting") );
                mBleedHint = new QLabel();
                mBleedHint->setObjectName( "peMuted" );
                mBleedHint->setWordWrap( true );
                bleedForm->addRow( tr("Each side:"), mBleedSpin );
                bleedForm->addRow( mBleedHint );
                inputsLayout->addWidget( bleedBox );

                inputsLayout->addStretch();

                auto* inputsScroll = new QScrollArea();
                inputsScroll->setWidget( inputs );
                inputsScroll->setWidgetResizable( true );
                inputsScroll->setFrameShape( QFrame::NoFrame );
                inputsScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
                inputsScroll->setFixedWidth( 380 );

                // ---- Right: results ----------------------------------------------
                auto* results = new QWidget();
                auto* resultsLayout = new QVBoxLayout( results );
                resultsLayout->setContentsMargins( 0, 16, 16, 16 );
                resultsLayout->setSpacing( 10 );

                auto makeTile = [this]( const QString& key, const QString& tip, QLabel*& value ) {
                        auto* tile = new QFrame();
                        tile->setObjectName( "peTile" );
                        tile->setToolTip( tip );
                        auto* l = new QVBoxLayout( tile );
                        l->setContentsMargins( 14, 10, 14, 10 );
                        auto* keyLabel = new QLabel( key );
                        keyLabel->setObjectName( "peTileKey" );
                        value = new QLabel( QStringLiteral( "–" ) );
                        value->setObjectName( "peTileValue" );
                        value->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        l->addWidget( keyLabel );
                        l->addWidget( value );
                        return tile;
                };
                auto* tiles = new QHBoxLayout();
                tiles->setSpacing( 10 );
                tiles->addWidget( makeTile( tr("Items per sheet"), tr("How many items fit on one sheet"), mPerSheetValue ) );
                tiles->addWidget( makeTile( tr("Sheets needed"), tr("Sheets to print the whole quantity"), mSheetsValue ) );
                tiles->addWidget( makeTile( tr("Waste"),
                                            tr("Share of each full sheet not used by finished items "
                                               "(margin, bleed and offcuts)"), mWasteValue ) );
                resultsLayout->addLayout( tiles );

                mDetailsLabel = new QLabel();
                mDetailsLabel->setObjectName( "peMuted" );
                mDetailsLabel->setWordWrap( true );
                resultsLayout->addWidget( mDetailsLabel );

                mPreview = new SheetPreview();
                resultsLayout->addWidget( mPreview, 1 );

                mStatusLabel = new QLabel();
                mStatusLabel->setWordWrap( true );
                mStatusLabel->hide();
                resultsLayout->addWidget( mStatusLabel );

                auto* buttons = new QHBoxLayout();
                buttons->addStretch();
                mSaveButton = new QPushButton( tr("Save...") );
                mSaveButton->setToolTip( tr("Save the estimate as a PDF, PNG image or text file") );
                mSaveButton->setEnabled( false );
                auto* closeButton = new QPushButton( tr("Close") );
                buttons->addWidget( mSaveButton );
                buttons->addWidget( closeButton );
                resultsLayout->addLayout( buttons );

                auto* central = new QWidget();
                auto* centralLayout = new QHBoxLayout( central );
                centralLayout->setContentsMargins( 0, 0, 0, 0 );
                centralLayout->setSpacing( 0 );
                centralLayout->addWidget( inputsScroll );
                centralLayout->addWidget( results, 1 );
                setCentralWidget( central );

                // ---- Spin boxes in the current units -------------------------------
                for ( auto* spin : { mItemWSpin, mItemHSpin, mSheetWSpin, mSheetHSpin } )
                {
                        configureSpin( spin, MAX_LENGTH_PTS );
                }
                configureSpin( mMarginSpin, model::Distance::mm( 100 ).pt() );
                configureSpin( mBleedSpin,  model::Distance::mm( 20 ).pt() );

                mUpdating = true;
                for ( int i = 0; i < N_SHEETS; i++ )
                {
                        mSheetCombo->addItem( QString() );
                }
                mSheetCombo->addItem( tr("Custom size") );
                mUpdating = false;
                onUnitsChanged();   // sheet names and hints in the current units

                // ---- Signals -----------------------------------------------------
                connect( mUnitsCombo, &QComboBox::currentIndexChanged, this, &PrintEstimatorWindow::onUnitsChanged );
                connect( mTypeGroup, &QButtonGroup::idClicked, this, &PrintEstimatorWindow::onProductTypeChanged );
                connect( mChooseButton, &QPushButton::clicked, this, &PrintEstimatorWindow::chooseProduct );
                connect( mClearButton, &QPushButton::clicked, this, &PrintEstimatorWindow::clearProduct );
                connect( mSheetCombo, &QComboBox::currentIndexChanged, this, &PrintEstimatorWindow::onSheetChanged );
                for ( auto* spin : { mItemWSpin, mItemHSpin, mSheetWSpin, mSheetHSpin, mMarginSpin, mBleedSpin } )
                {
                        connect( spin, &QDoubleSpinBox::valueChanged, this, &PrintEstimatorWindow::scheduleEstimate );
                }
                connect( mQuantitySpin, &QSpinBox::valueChanged, this, &PrintEstimatorWindow::scheduleEstimate );

                // Results follow every change, once typing pauses
                mEstimateTimer = new QTimer( this );
                mEstimateTimer->setSingleShot( true );
                mEstimateTimer->setInterval( 150 );
                connect( mEstimateTimer, &QTimer::timeout, this, &PrintEstimatorWindow::estimate );
                connect( mSaveButton, &QPushButton::clicked, this, &PrintEstimatorWindow::saveEstimate );
                connect( closeButton, &QPushButton::clicked, this, &QWidget::close );
        }


        ///
        /// Restore the last sheet, margin and quantity
        ///
        void PrintEstimatorWindow::loadSettings()
        {
                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );

                mUpdating = true;

                QString sheet = settings.value( "sheet", DEFAULT_SHEET ).toString();
                int sheetIndex = (sheet == "custom") ? CUSTOM_SHEET : -1;
                for ( int i = 0; i < N_SHEETS; i++ )
                {
                        QString name = QString::fromUtf8( SHEETS[i].name );
                        if ( name == sheet || (sheetIndex < 0 && name == DEFAULT_SHEET) ) sheetIndex = i;
                }
                mSheetCombo->setCurrentIndex( std::max( sheetIndex, 0 ) );

                double defaultMargin = (mUnits.toEnum() == model::Units::IN) ?
                        model::Distance::in( DEFAULT_MARGIN_IN ).pt() : model::Distance::mm( DEFAULT_MARGIN_MM ).pt();
                setSpinPts( mSheetWSpin, settings.value( "customSheetW", model::Distance::mm( 320 ).pt() ).toDouble() );
                setSpinPts( mSheetHSpin, settings.value( "customSheetH", model::Distance::mm( 450 ).pt() ).toDouble() );
                setSpinPts( mMarginSpin, settings.value( "margin", defaultMargin ).toDouble() );
                setSpinPts( mItemWSpin,  settings.value( "customItemW", model::Distance::mm( 90 ).pt() ).toDouble() );
                setSpinPts( mItemHSpin,  settings.value( "customItemH", model::Distance::mm( 50 ).pt() ).toDouble() );
                mQuantitySpin->setValue( settings.value( "quantity", 1000 ).toInt() );

                // Without a product from the library only a custom size can be used
                mCustomRadio->setChecked( true );
                mLabelRadio->setEnabled( false );
                mPageRadio->setEnabled( false );

                mUpdating = false;
                settings.endGroup();
        }


        void PrintEstimatorWindow::saveSettings() const
        {
                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );

                int index = mSheetCombo->currentIndex();
                settings.setValue( "sheet", index == CUSTOM_SHEET ? QString( "custom" ) : QString( SHEETS[index].name ) );
                if ( index == CUSTOM_SHEET )
                {
                        settings.setValue( "customSheetW", toPts( mSheetWSpin->value() ) );
                        settings.setValue( "customSheetH", toPts( mSheetHSpin->value() ) );
                }
                if ( productType() == CUSTOM )
                {
                        settings.setValue( "customItemW", toPts( mItemWSpin->value() ) );
                        settings.setValue( "customItemH", toPts( mItemHSpin->value() ) );
                }
                settings.setValue( "margin", toPts( mMarginSpin->value() ) );
                settings.setValue( "quantity", mQuantitySpin->value() );

                settings.endGroup();
        }


        ///
        /// Use a product (from the open label or the library)
        ///
        void PrintEstimatorWindow::setTemplate( const model::Template& tmplate )
        {
                if ( tmplate.isNull() || !tmplate.frame() )
                {
                        return;
                }

                mTemplate = tmplate;
                mClearButton->show();

                // Labels on a roll have no page to print as a whole
                mLabelRadio->setEnabled( true );
                mPageRadio->setEnabled( !mTemplate.isRoll() );
                if ( productType() == CUSTOM || (productType() == PAGE && mTemplate.isRoll()) )
                {
                        mLabelRadio->setChecked( true );
                }
                onProductTypeChanged();
        }


        void PrintEstimatorWindow::chooseProduct()
        {
                SelectProductDialog dialog( this );
                dialog.exec();

                auto tmplate = dialog.tmplate();
                setTemplate( tmplate );
        }


        ///
        /// Forget the chosen product; only a custom size can be used until another is chosen
        ///
        void PrintEstimatorWindow::clearProduct()
        {
                mTemplate = model::Template();
                mClearButton->hide();

                mCustomRadio->setChecked( true );
                mLabelRadio->setEnabled( false );
                mPageRadio->setEnabled( false );
                onProductTypeChanged();
        }


        PrintEstimatorWindow::ProductType PrintEstimatorWindow::productType() const
        {
                int id = mTypeGroup->checkedId();
                return (id == LABEL || id == PAGE) ? ProductType( id ) : CUSTOM;
        }


        void PrintEstimatorWindow::onProductTypeChanged()
        {
                ProductType type = productType();
                bool custom = (type == CUSTOM);

                mUpdating = true;

                if ( !custom && !mTemplate.isNull() )
                {
                        if ( type == LABEL )
                        {
                                setSpinPts( mItemWSpin, mTemplate.frame()->w().pt() );
                                setSpinPts( mItemHSpin, mTemplate.frame()->h().pt() );
                        }
                        else
                        {
                                setSpinPts( mItemWSpin, mTemplate.pageWidth().pt() );
                                setSpinPts( mItemHSpin, mTemplate.pageHeight().pt() );
                        }
                }
                mItemWSpin->setEnabled( custom );
                mItemHSpin->setEnabled( custom );

                // Standard bleed for the kind of product
                setSpinPts( mBleedSpin, standardBleedPts( type ) );

                mUpdating = false;

                mProductLabel->setText( productDescription() );
                QString kind = (type == LABEL) ? tr("labels") : tr("pages and custom items");
                mBleedHint->setText( tr("Standard for %1: %2").arg( kind, formatLength( standardBleedPts( type ) ) ) );

                scheduleEstimate();
        }


        void PrintEstimatorWindow::onSheetChanged()
        {
                if ( mUpdating ) return;

                int index = mSheetCombo->currentIndex();
                bool custom = (index == CUSTOM_SHEET);
                if ( !custom && index >= 0 )
                {
                        mUpdating = true;
                        setSpinPts( mSheetWSpin, SHEETS[index].wPts() );
                        setSpinPts( mSheetHSpin, SHEETS[index].hPts() );
                        mUpdating = false;
                }
                mSheetWSpin->setEnabled( custom );
                mSheetHSpin->setEnabled( custom );

                scheduleEstimate();
        }


        ///
        /// Switch units: convert every value, results stay valid
        ///
        void PrintEstimatorWindow::onUnitsChanged()
        {
                QList<QDoubleSpinBox*> spins = { mItemWSpin, mItemHSpin, mSheetWSpin, mSheetHSpin, mMarginSpin, mBleedSpin };
                QList<double> pts;
                for ( auto* spin : spins )
                {
                        pts << toPts( spin->value() );
                }

                mUnits = model::Units( mUnitsCombo->currentData().toString() );

                bool wasUpdating = mUpdating;
                mUpdating = true;
                for ( int i = 0; i < spins.size(); i++ )
                {
                        double maxPts = (spins[i] == mMarginSpin) ? model::Distance::mm( 100 ).pt() :
                                        (spins[i] == mBleedSpin)  ? model::Distance::mm( 20 ).pt() : MAX_LENGTH_PTS;
                        configureSpin( spins[i], maxPts );
                        setSpinPts( spins[i], pts[i] );
                }
                for ( int i = 0; i < N_SHEETS; i++ )
                {
                        mSheetCombo->setItemText( i, QString( "%1   (%2)" )
                                                  .arg( QString::fromUtf8( SHEETS[i].name ),
                                                        formatSize( SHEETS[i].wPts(), SHEETS[i].hPts() ) ) );
                }
                mUpdating = wasUpdating;

                mProductLabel->setText( productDescription() );
                ProductType type = productType();
                QString kind = (type == LABEL) ? tr("labels") : tr("pages and custom items");
                mBleedHint->setText( tr("Standard for %1: %2").arg( kind, formatLength( standardBleedPts( type ) ) ) );

                // Refresh the shown estimate in the new units
                if ( mHasEstimate )
                {
                        mDetailsLabel->setText( reportLines().last().value );
                }
                scheduleEstimate();
        }


        void PrintEstimatorWindow::scheduleEstimate()
        {
                if ( mUpdating || !mEstimateTimer ) return;

                mEstimateTimer->start();
        }


        void PrintEstimatorWindow::setStatus( const QString& text, bool isError )
        {
                mStatusLabel->setObjectName( isError ? "peError" : "peNote" );
                mStatusLabel->style()->unpolish( mStatusLabel );
                mStatusLabel->style()->polish( mStatusLabel );
                mStatusLabel->setText( text );
                mStatusLabel->show();
        }


        void PrintEstimatorWindow::showError( const QString& message )
        {
                mHasEstimate = false;
                mPerSheetValue->setText( QStringLiteral( "–" ) );
                mSheetsValue->setText( QStringLiteral( "–" ) );
                mWasteValue->setText( QStringLiteral( "–" ) );
                mDetailsLabel->clear();
                mPreview->setLayoutResult( nullptr, false );
                mSaveButton->setEnabled( false );

                setStatus( message, true );
        }


        ///
        /// Work out the layout and the results
        ///
        void PrintEstimatorWindow::estimate()
        {
                ProductType type = productType();
                if ( type != CUSTOM && mTemplate.isNull() )
                {
                        showError( tr("Choose a product first.") );
                        return;
                }

                double itemW  = toPts( mItemWSpin->value() );
                double itemH  = toPts( mItemHSpin->value() );
                double sheetW = toPts( mSheetWSpin->value() );
                double sheetH = toPts( mSheetHSpin->value() );
                double margin = toPts( mMarginSpin->value() );
                double bleed  = toPts( mBleedSpin->value() );

                if ( itemW <= 0 || itemH <= 0 )
                {
                        showError( tr("Enter the width and height of the product.") );
                        return;
                }

                const double tolerance = model::Distance::mm( 0.05 ).pt();
                if ( std::min( sheetW, sheetH ) < MIN_SHEET_SHORT_PTS - tolerance ||
                     std::max( sheetW, sheetH ) < MIN_SHEET_LONG_PTS - tolerance )
                {
                        showError( tr("The printing sheet must be at least A4 (%1).")
                                   .arg( formatSize( MIN_SHEET_SHORT_PTS, MIN_SHEET_LONG_PTS ) ) );
                        return;
                }

                auto layout = model::SheetLayout::compute( sheetW, sheetH, itemW, itemH, bleed, margin );
                if ( layout.perSheet() == 0 )
                {
                        showError( tr("The product with its bleed (%1) does not fit inside the printable "
                                      "area of this sheet. Choose a larger sheet or a smaller margin.")
                                   .arg( formatSize( itemW + 2*bleed, itemH + 2*bleed ) ) );
                        return;
                }

                mLayout      = layout;
                mQuantity    = mQuantitySpin->value();
                mHasEstimate = true;

                QString name = (type == CUSTOM) ? tr("Custom item") :
                        QString( "%1 %2" ).arg( type == LABEL ? tr("Label of") : tr("Page of"), mTemplate.name() );
                mProductText = name;
                int index = mSheetCombo->currentIndex();
                mSheetText = (index == CUSTOM_SHEET) ? tr("Custom sheet") : QString::fromUtf8( SHEETS[index].name );

                QLocale locale;
                mPerSheetValue->setText( locale.toString( mLayout.perSheet() ) );
                mSheetsValue->setText( locale.toString( mLayout.sheetsFor( mQuantity ) ) );
                mWasteValue->setText( percent( mLayout.sheetWaste() ) );
                mDetailsLabel->setText( reportLines().last().value );

                mStatusLabel->hide();
                mSaveButton->setEnabled( true );
                mPreview->setLayoutResult( &mLayout, false );
        }


        ///
        /// Report: one line per fact; the last line is the short summary under the tiles
        ///
        QList<PrintEstimatorWindow::Line> PrintEstimatorWindow::reportLines() const
        {
                QLocale locale;
                QList<Line> lines;

                int perSheet = mLayout.perSheet();
                int sheets   = mLayout.sheetsFor( mQuantity );
                int onLast   = mQuantity - (sheets - 1) * perSheet;
                int rotated  = mLayout.rotatedCount();

                lines << Line{ tr("Product"),          mProductText };
                lines << Line{ tr("Product size"),     formatSize( mLayout.itemW(), mLayout.itemH() ) };
                lines << Line{ tr("Quantity"),         locale.toString( mQuantity ) };
                lines << Line{ tr("Printing sheet"),   QString( "%1 (%2)" ).arg( mSheetText,
                                                                               formatSize( mLayout.sheetW(), mLayout.sheetH() ) ) };
                lines << Line{ tr("Sheet margin"),     formatLength( mLayout.margin() ) };
                lines << Line{ tr("Bleed (each side)"), formatLength( mLayout.bleed() ) };
                lines << Line{ tr("Items per sheet"),  rotated > 0 ?
                                                       tr("%1 (%2 turned 90°)").arg( locale.toString( perSheet ),
                                                                                     locale.toString( rotated ) ) :
                                                       locale.toString( perSheet ) };
                lines << Line{ tr("Sheets needed"),    locale.toString( sheets ) };
                lines << Line{ tr("Waste per sheet"),  percent( mLayout.sheetWaste() ) };
                lines << Line{ tr("Job waste"),        percent( mLayout.jobWaste( mQuantity ) ) };
                lines << Line{ tr("Last sheet"),       tr("%1 of %2 positions used").arg( locale.toString( onLast ),
                                                                                         locale.toString( perSheet ) ) };

                QStringList summary;
                if ( rotated > 0 )
                {
                        summary << tr("%1 of %2 items turned 90°").arg( locale.toString( rotated ),
                                                                        locale.toString( perSheet ) );
                }
                summary << tr("Job waste %1").arg( percent( mLayout.jobWaste( mQuantity ) ) );
                summary << tr("Last sheet: %1 of %2 positions used").arg( locale.toString( onLast ),
                                                                          locale.toString( perSheet ) );
                lines << Line{ QString(), summary.join( QStringLiteral( "   ·   " ) ) };

                return lines;
        }


        QString PrintEstimatorWindow::reportText() const
        {
                auto lines = reportLines();
                lines.removeLast();   // summary repeats the lines above

                int width = 0;
                for ( const auto& line : lines )
                {
                        width = std::max( width, int( line.key.size() ) );
                }

                QString text;
                QTextStream out( &text );
                out << tr("Print Sheet Estimate") << "\n";
                out << QLocale().toString( QDateTime::currentDateTime(), QLocale::LongFormat ) << "\n\n";
                for ( const auto& line : lines )
                {
                        out << (line.key + ":").leftJustified( width + 3 ) << line.value << "\n";
                }
                out << "\n" << tr("Waste is the share of paper not used by finished items "
                                  "(sheet margin, bleed and offcuts).") << "\n";
                return text;
        }


        ///
        /// Draw the report (title, facts and the sheet layout) on a page-shaped canvas
        ///
        void PrintEstimatorWindow::renderReport( QPainter& painter, const QSizeF& canvas ) const
        {
                double w = canvas.width();
                double margin = w * 0.07;
                double y = margin;

                painter.fillRect( QRectF( QPointF( 0, 0 ), canvas ), Qt::white );

                QFont font = QApplication::font();
                font.setPixelSize( int( w * 0.032 ) );
                font.setBold( true );
                painter.setFont( font );
                painter.setPen( EXPORT_INK );
                QFontMetricsF titleMetrics( font, painter.device() );
                y += titleMetrics.ascent();
                painter.drawText( QPointF( margin, y ), tr("Print Sheet Estimate") );
                y += titleMetrics.descent() + w * 0.008;

                font.setBold( false );
                font.setPixelSize( int( w * 0.016 ) );
                painter.setFont( font );
                QFontMetricsF metrics( font, painter.device() );
                painter.setPen( EXPORT_MUTED );
                y += metrics.ascent();
                painter.drawText( QPointF( margin, y ),
                                  QLocale().toString( QDateTime::currentDateTime(), QLocale::LongFormat ) );
                y += metrics.descent() + w * 0.03;

                auto lines = reportLines();
                lines.removeLast();
                double lineHeight = metrics.height() * 1.45;
                double valueX = margin + w * 0.26;
                QFont boldFont = font;
                boldFont.setBold( true );
                for ( const auto& line : lines )
                {
                        y += lineHeight;
                        painter.setFont( font );
                        painter.setPen( EXPORT_MUTED );
                        painter.drawText( QPointF( margin, y ), line.key );
                        painter.setFont( boldFont );
                        painter.setPen( EXPORT_INK );
                        painter.drawText( QPointF( valueX, y ), line.value );
                }
                y += w * 0.04;

                SheetColors colors;
                colors.paper  = Qt::white;
                colors.edge   = EXPORT_INK;
                colors.margin = EXPORT_MUTED;
                colors.accent = EXPORT_ACCENT;
                QRectF area( margin, y, w - 2*margin, canvas.height() - margin - y );
                drawSheet( painter, area, mLayout, colors );
        }


        bool PrintEstimatorWindow::writeText( const QString& fileName ) const
        {
                QSaveFile file( fileName );
                if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
                {
                        return false;
                }
                file.write( reportText().toUtf8() );
                return file.commit();
        }


        bool PrintEstimatorWindow::writeImage( const QString& fileName ) const
        {
                // A4 proportions at 150 dpi
                QImage image( 1240, 1754, QImage::Format_ARGB32_Premultiplied );
                image.setDotsPerMeterX( int( 150 / 0.0254 ) );
                image.setDotsPerMeterY( int( 150 / 0.0254 ) );
                QPainter painter( &image );
                painter.setRenderHint( QPainter::Antialiasing, true );
                painter.setRenderHint( QPainter::TextAntialiasing, true );
                renderReport( painter, image.size() );
                painter.end();
                return image.save( fileName, "PNG" );
        }


        bool PrintEstimatorWindow::writePdf( const QString& fileName ) const
        {
                QPdfWriter writer( fileName );
                writer.setPageSize( QPageSize( QPageSize::A4 ) );
                writer.setPageMargins( QMarginsF( 0, 0, 0, 0 ) );
                writer.setResolution( 150 );
                writer.setTitle( tr("Print Sheet Estimate") );
                writer.setCreator( QApplication::applicationName() );

                QPainter painter;
                if ( !painter.begin( &writer ) )
                {
                        return false;
                }
                renderReport( painter, QSizeF( writer.width(), writer.height() ) );
                return painter.end();
        }


        ///
        /// Save the estimate as PDF, PNG or text (chosen by the file type)
        ///
        void PrintEstimatorWindow::saveEstimate()
        {
                if ( !mHasEstimate ) return;

                const QString pdfFilter = tr("PDF document (*.pdf)");
                const QString pngFilter = tr("PNG image (*.png)");
                const QString txtFilter = tr("Text file (*.txt)");

                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );
                QString dir = settings.value( "saveDir",
                                              QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).toString();
                QString selectedFilter = settings.value( "saveFilter", pdfFilter ).toString();

                QString suggested = QDir( dir ).filePath( tr("print-estimate") );
                QString fileName = QFileDialog::getSaveFileName( this, tr("Save Estimate"), suggested,
                                                                 pdfFilter + ";;" + pngFilter + ";;" + txtFilter,
                                                                 &selectedFilter );
                if ( fileName.isEmpty() )
                {
                        return;
                }

                QString suffix = QFileInfo( fileName ).suffix().toLower();
                if ( suffix != "pdf" && suffix != "png" && suffix != "txt" )
                {
                        suffix = (selectedFilter == pngFilter) ? "png" : (selectedFilter == txtFilter) ? "txt" : "pdf";
                        fileName += "." + suffix;
                }

                bool ok = (suffix == "png") ? writeImage( fileName ) :
                          (suffix == "txt") ? writeText( fileName ) : writePdf( fileName );

                settings.setValue( "saveDir", QFileInfo( fileName ).absolutePath() );
                settings.setValue( "saveFilter", selectedFilter );
                settings.endGroup();

                setStatus( ok ? tr("Saved to %1").arg( QDir::toNativeSeparators( fileName ) ) :
                                tr("Could not save %1").arg( QDir::toNativeSeparators( fileName ) ), !ok );
        }


        ///
        /// Units helpers
        ///
        double PrintEstimatorWindow::standardBleedPts( ProductType type ) const
        {
                bool inches = (mUnits.toEnum() == model::Units::IN);
                if ( type == LABEL )
                {
                        return inches ? model::Distance::in( 0.0625 ).pt() : model::Distance::mm( 2 ).pt();
                }
                return inches ? model::Distance::in( 0.125 ).pt() : model::Distance::mm( 3 ).pt();
        }


        double PrintEstimatorWindow::toPts( double value ) const
        {
                return model::Distance( value, mUnits ).pt();
        }


        double PrintEstimatorWindow::fromPts( double pts ) const
        {
                return model::Distance::pt( pts ).inUnits( mUnits );
        }


        void PrintEstimatorWindow::setSpinPts( QDoubleSpinBox* spin, double pts )
        {
                spin->setValue( fromPts( pts ) );
        }


        void PrintEstimatorWindow::configureSpin( QDoubleSpinBox* spin, double maxPts )
        {
                int decimals = (mUnits.toEnum() == model::Units::MM) ? 2 :
                               (mUnits.toEnum() == model::Units::CM) ? 3 : 4;   // 1/16 in = 0.0625
                spin->setDecimals( decimals );
                spin->setSingleStep( mUnits.toEnum() == model::Units::MM ? 1.0 :
                                     mUnits.toEnum() == model::Units::CM ? 0.1 : 0.0625 );
                spin->setRange( 0, fromPts( maxPts ) );
                spin->setSuffix( " " + mUnits.toIdString() );
                spin->setAccelerated( true );
        }


        QString PrintEstimatorWindow::formatLength( double pts ) const
        {
                int decimals = (mUnits.toEnum() == model::Units::MM) ? 2 :
                               (mUnits.toEnum() == model::Units::CM) ? 3 : 4;   // 1/16 in = 0.0625
                QString number = QLocale().toString( fromPts( pts ), 'f', decimals );
                // Drop trailing zeros: "3.00" -> "3", "2.50" -> "2.5"
                QChar point = QLocale().decimalPoint().at( 0 );
                if ( number.contains( point ) )
                {
                        while ( number.endsWith( '0' ) ) number.chop( 1 );
                        if ( number.endsWith( point ) ) number.chop( 1 );
                }
                return number + " " + mUnits.toIdString();
        }


        QString PrintEstimatorWindow::formatSize( double wPts, double hPts ) const
        {
                QString w = formatLength( wPts );
                w.chop( mUnits.toIdString().size() + 1 );
                return QString( "%1 × %2" ).arg( w, formatLength( hPts ) );
        }


        QString PrintEstimatorWindow::productDescription() const
        {
                if ( productType() == CUSTOM )
                {
                        return mTemplate.isNull() ? tr("Enter the size below, or choose a product.")
                                                  : tr("Enter the size below.");
                }
                if ( mTemplate.isNull() )
                {
                        return tr("No product chosen");
                }

                QString text = "<b>" + mTemplate.name().toHtmlEscaped() + "</b>";
                if ( !mTemplate.description().isEmpty() )
                {
                        text += "<br>" + mTemplate.description().toHtmlEscaped();
                }
                if ( mTemplate.frame()->nLabels() > 1 )
                {
                        text += "<br>" + tr("%1 labels per page").arg( mTemplate.frame()->nLabels() );
                }
                return text;
        }

}
