//  RollCostWindow.cpp
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

#include "RollCostWindow.hpp"

#include "Help.hpp"
#include "SelectProductDialog.hpp"

#include "model/Db.hpp"
#include "model/Distance.hpp"
#include "model/Frame.hpp"
#include "model/Settings.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace glabels
{

        namespace
        {
                const QString SETTINGS_GROUP = QStringLiteral( "RollCostEstimator" );
                const QString CUSTOM_CURRENCY = QStringLiteral( "custom" );

                // Largest label accepted (2 m) and longest ribbon (10 km)
                const double MAX_LABEL_PTS  = model::Distance::mm( 2000 ).pt();
                const double MAX_GAP_PTS    = model::Distance::mm( 100 ).pt();
                const double MAX_RIBBON_PTS = model::Distance::mm( 10000000 ).pt();
                const double MAX_PRICE      = 1e12;

                // Colors used in saved images and PDFs (ink and a single accent)
                const QColor EXPORT_INK( "#1f2328" );
                const QColor EXPORT_MUTED( "#6e7781" );
                const QColor EXPORT_ACCENT( "#e8590c" );

                // Common currencies (ISO 4217 codes)
                const char* CURRENCIES[] = {
                        "USD", "EUR", "GBP", "PKR", "INR", "BDT", "LKR", "NPR", "AED", "SAR",
                        "QAR", "KWD", "OMR", "BHD", "EGP", "TRY", "CNY", "JPY", "KRW", "HKD",
                        "SGD", "MYR", "IDR", "THB", "PHP", "VND", "AUD", "NZD", "CAD", "MXN",
                        "BRL", "ARS", "CLP", "COP", "ZAR", "NGN", "KES", "GHS", "MAD", "CHF",
                        "SEK", "NOK", "DKK", "PLN", "CZK", "HUF", "RON", "UAH", "RUB", "ILS",
                };


                double ptsPerRibbonUnit( const QString& unit )
                {
                        return (unit == "ft") ? model::Distance::in( 12 ).pt() : model::Distance::mm( 1000 ).pt();
                }


                // Number without trailing zeros: "3.00" -> "3", "2.50" -> "2.5"
                QString trimmed( double value, int decimals )
                {
                        QString number = QLocale().toString( value, 'f', decimals );
                        QChar point = QLocale().decimalPoint().at( 0 );
                        if ( number.contains( point ) )
                        {
                                while ( number.endsWith( '0' ) ) number.chop( 1 );
                                if ( number.endsWith( point ) ) number.chop( 1 );
                        }
                        return number;
                }


                QDoubleSpinBox* priceSpin()
                {
                        auto* spin = new QDoubleSpinBox();
                        spin->setRange( 0, MAX_PRICE );
                        spin->setDecimals( 2 );
                        spin->setGroupSeparatorShown( true );
                        spin->setAccelerated( true );
                        return spin;
                }
        }


        QPointer<RollCostWindow> RollCostWindow::sInstance;


        void RollCostWindow::showWindow( const model::Template& tmplate )
        {
                if ( !sInstance )
                {
                        sInstance = new RollCostWindow();
                        sInstance->resize( 1080, 760 );
                }
                sInstance->setTemplate( tmplate );

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
        RollCostWindow::RollCostWindow()
                : QMainWindow( nullptr )
        {
                setAttribute( Qt::WA_DeleteOnClose );
                setWindowTitle( tr("Ribbon & Label Cost Estimator") );
                setWindowIcon( QApplication::windowIcon() );

                // Physical units only; points and picas fall back to millimeters
                auto units = model::Settings::units();
                mUnits = (units.toEnum() == model::Units::IN || units.toEnum() == model::Units::CM) ?
                        units : model::Units::mm();
                mRibbonUnit = (mUnits.toEnum() == model::Units::IN) ? "ft" : "m";

                buildUi();
                loadSettings();
                onCurrencyChanged();
                onRibbonToggled();
                updateProductLabel();
                calculate();

                auto* help = new QShortcut( QKeySequence::HelpContents, this );
                connect( help, &QShortcut::activated, this, [this]() {
                        Help::displayContents( this, "rollcost" );
                } );
        }


        void RollCostWindow::closeEvent( QCloseEvent* event )
        {
                saveSettings();
                QMainWindow::closeEvent( event );
        }


        ///
        /// Widgets
        ///
        void RollCostWindow::buildUi()
        {
                QColor fg = palette().color( QPalette::WindowText );
                QColor bg = palette().color( QPalette::Window );
                QColor muted = QColor::fromRgbF( (fg.redF()   + bg.redF())   / 2,
                                                 (fg.greenF() + bg.greenF()) / 2,
                                                 (fg.blueF()  + bg.blueF())  / 2 );

                setStyleSheet( QString(
                        "#rcMuted { color: %1; }"
                        "#rcTile { border: 1px solid palette(mid); border-radius: 8px; background: palette(base); }"
                        "#rcTileKey { color: %1; }"
                        "#rcTileValue { font-size: 20pt; font-weight: 600; }"
                        "#rcTileNote { color: %1; }"
                        "#rcError { color: #c62828; }"
                        "#rcCard { border: 1px solid palette(mid); border-radius: 8px; background: palette(base); }"
                        "#rcCardTotal { border: 2px solid #e8590c; border-radius: 8px; background: palette(base); }"
                        "#rcCardTitle { font-weight: 700; }"
                        "#rcKey { color: %1; }"
                        "#rcStrong { font-weight: 700; }"
                        "#rcNote { color: %1; font-style: italic; }" ).arg( muted.name() ) );

                // ---- Left: inputs -------------------------------------------------
                auto* inputs = new QWidget();
                auto* inputsLayout = new QVBoxLayout( inputs );
                inputsLayout->setContentsMargins( 16, 16, 16, 16 );
                inputsLayout->setSpacing( 12 );

                // Currency and units
                auto* topForm = new QFormLayout();
                mCurrencyCombo = new QComboBox();
                mCurrencyCombo->setToolTip( tr("Currency of the prices. Amounts are not converted.") );
                QString localCode = QLocale().currencySymbol( QLocale::CurrencyIsoCode );
                QStringList codes;
                for ( const char* code : CURRENCIES )
                {
                        codes << QString::fromLatin1( code );
                }
                if ( !localCode.isEmpty() && !codes.contains( localCode ) )
                {
                        codes.prepend( localCode );
                }
                for ( const auto& code : codes )
                {
                        mCurrencyCombo->addItem( code, code );
                }
                mCurrencyCombo->addItem( tr("Other..."), CUSTOM_CURRENCY );
                mCurrencyEdit = new QLineEdit();
                mCurrencyEdit->setMaxLength( 8 );
                mCurrencyEdit->setPlaceholderText( tr("Symbol, e.g. Rs") );
                mCurrencyEdit->setMaximumWidth( 110 );
                auto* currencyRow = new QHBoxLayout();
                currencyRow->addWidget( mCurrencyCombo, 1 );
                currencyRow->addWidget( mCurrencyEdit );
                topForm->addRow( tr("Currency:"), currencyRow );

                mUnitsCombo = new QComboBox();
                mUnitsCombo->addItem( tr("Millimeters (mm)"), "mm" );
                mUnitsCombo->addItem( tr("Centimeters (cm)"), "cm" );
                mUnitsCombo->addItem( tr("Inches (in)"),      "in" );
                mUnitsCombo->setCurrentIndex( mUnitsCombo->findData( mUnits.toIdString() ) );
                topForm->addRow( tr("Units:"), mUnitsCombo );
                inputsLayout->addLayout( topForm );

                // Labels
                auto* labelBox = new QGroupBox( tr("Labels") );
                auto* labelLayout = new QVBoxLayout( labelBox );
                auto* productRow = new QHBoxLayout();
                mProductLabel = new QLabel();
                mProductLabel->setWordWrap( true );
                mProductLabel->setTextFormat( Qt::RichText );
                mChooseButton = new QPushButton( tr("Choose...") );
                mChooseButton->setToolTip( tr("Fill in the label size from a product in the library") );
                mClearButton = new QPushButton( tr("Clear") );
                mClearButton->setToolTip( tr("Stop using the chosen product. The values stay so you can edit them.") );
                labelLayout->addWidget( mProductLabel );
                productRow->addWidget( mChooseButton );
                productRow->addWidget( mClearButton );
                productRow->addStretch( 1 );
                labelLayout->addLayout( productRow );
                auto* labelForm = new QFormLayout();
                mLabelWSpin = new QDoubleSpinBox();
                mLabelWSpin->setToolTip( tr("Label size across the roll") );
                mLabelLSpin = new QDoubleSpinBox();
                mLabelLSpin->setToolTip( tr("Label size along the roll, in the direction it feeds through the printer") );
                mGapSpin = new QDoubleSpinBox();
                mGapSpin->setToolTip( tr("Gap between labels along the roll. The ribbon moves past it too.") );
                mAcrossSpin = new QSpinBox();
                mAcrossSpin->setRange( 1, 20 );
                mAcrossSpin->setToolTip( tr("Labels side by side on the roll. The ribbon moves once for each row.") );
                mPerRollSpin = new QSpinBox();
                mPerRollSpin->setRange( 1, 10000000 );
                mPerRollSpin->setGroupSeparatorShown( true );
                mPerRollSpin->setToolTip( tr("Total labels on one roll") );
                mRollPriceSpin = priceSpin();
                mRollPriceSpin->setToolTip( tr("Price of one roll of labels") );
                labelForm->addRow( tr("Width:"),          mLabelWSpin );
                labelForm->addRow( tr("Length:"),         mLabelLSpin );
                labelForm->addRow( tr("Gap:"),            mGapSpin );
                labelForm->addRow( tr("Labels across:"),  mAcrossSpin );
                labelForm->addRow( tr("Labels per roll:"), mPerRollSpin );
                labelForm->addRow( tr("Roll price:"),     mRollPriceSpin );
                labelLayout->addLayout( labelForm );
                inputsLayout->addWidget( labelBox );

                // Ribbon
                mNoRibbonCheck = new QCheckBox( tr("No ribbon (direct thermal)") );
                mNoRibbonCheck->setToolTip( tr("Direct thermal printers print on heat-sensitive labels without a ribbon") );
                inputsLayout->addWidget( mNoRibbonCheck );

                mRibbonBox = new QGroupBox( tr("Ribbon") );
                auto* ribbonForm = new QFormLayout( mRibbonBox );
                mRibbonLSpin = new QDoubleSpinBox();
                mRibbonLSpin->setDecimals( 1 );
                mRibbonLSpin->setGroupSeparatorShown( true );
                mRibbonLSpin->setAccelerated( true );
                mRibbonLSpin->setToolTip( tr("Length of one ribbon, e.g. 300 m or 1,476 ft") );
                mRibbonUnitCombo = new QComboBox();
                mRibbonUnitCombo->addItem( tr("m"),  "m" );
                mRibbonUnitCombo->addItem( tr("ft"), "ft" );
                mRibbonUnitCombo->setCurrentIndex( mRibbonUnitCombo->findData( mRibbonUnit ) );
                auto* ribbonLRow = new QHBoxLayout();
                ribbonLRow->addWidget( mRibbonLSpin, 1 );
                ribbonLRow->addWidget( mRibbonUnitCombo );
                mRibbonWSpin = new QDoubleSpinBox();
                mRibbonWSpin->setToolTip( tr("Ribbon width. It should be at least as wide as the labels across the roll.") );
                mRibbonPriceSpin = priceSpin();
                mRibbonPriceSpin->setToolTip( tr("Price of one ribbon") );
                ribbonForm->addRow( tr("Length:"), ribbonLRow );
                ribbonForm->addRow( tr("Width:"),  mRibbonWSpin );
                ribbonForm->addRow( tr("Price:"),  mRibbonPriceSpin );
                inputsLayout->addWidget( mRibbonBox );

                // Job
                auto* jobBox = new QGroupBox( tr("Job") );
                auto* jobForm = new QFormLayout( jobBox );
                mQuantitySpin = new QSpinBox();
                mQuantitySpin->setRange( 1, 100000000 );
                mQuantitySpin->setGroupSeparatorShown( true );
                mQuantitySpin->setToolTip( tr("Number of good labels you need") );
                mWasteSpin = new QDoubleSpinBox();
                mWasteSpin->setRange( 0, 100 );
                mWasteSpin->setDecimals( 1 );
                mWasteSpin->setSuffix( " %" );
                mWasteSpin->setToolTip( tr("Extra labels for setup, test prints and misprints") );
                jobForm->addRow( tr("Labels to print:"), mQuantitySpin );
                jobForm->addRow( tr("Waste:"),           mWasteSpin );
                inputsLayout->addWidget( jobBox );

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

                auto makeTile = []( const QString& key, const QString& tip, QLabel*& value, QLabel*& note ) {
                        auto* tile = new QFrame();
                        tile->setObjectName( "rcTile" );
                        tile->setToolTip( tip );
                        auto* l = new QVBoxLayout( tile );
                        l->setContentsMargins( 14, 10, 14, 10 );
                        auto* keyLabel = new QLabel( key );
                        keyLabel->setObjectName( "rcTileKey" );
                        value = new QLabel( QStringLiteral( "–" ) );
                        value->setObjectName( "rcTileValue" );
                        value->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        note = new QLabel( QStringLiteral( " " ) );
                        note->setObjectName( "rcTileNote" );
                        note->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        l->addWidget( keyLabel );
                        l->addWidget( value );
                        l->addWidget( note );
                        return tile;
                };
                auto* tiles = new QHBoxLayout();
                tiles->setSpacing( 10 );
                tiles->addWidget( makeTile( tr("Ribbons"), tr("Whole ribbons to buy and their amount"),
                                            mRibbonsValue, mRibbonsAmount ) );
                tiles->addWidget( makeTile( tr("Label rolls"), tr("Whole rolls of labels to buy and their amount"),
                                            mRollsValue, mRollsAmount ) );
                tiles->addWidget( makeTile( tr("Total to buy"), tr("Ribbons and label rolls together"),
                                            mTotalValue, mTotalNote ) );
                resultsLayout->addLayout( tiles );

                mSections = new QWidget();
                mSectionsLayout = new QGridLayout( mSections );
                mSectionsLayout->setContentsMargins( 0, 0, 0, 0 );
                mSectionsLayout->setSpacing( 10 );
                mSectionsLayout->setColumnStretch( 0, 1 );
                mSectionsLayout->setColumnStretch( 1, 1 );
                auto* detailsScroll = new QScrollArea();
                detailsScroll->setWidget( mSections );
                detailsScroll->setWidgetResizable( true );
                detailsScroll->setFrameShape( QFrame::NoFrame );
                resultsLayout->addWidget( detailsScroll, 1 );

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
                configureSpin( mLabelWSpin,  MAX_LABEL_PTS );
                configureSpin( mLabelLSpin,  MAX_LABEL_PTS );
                configureSpin( mGapSpin,     MAX_GAP_PTS );
                configureSpin( mRibbonWSpin, MAX_LABEL_PTS );
                mRibbonLSpin->setRange( 0, ribbonFromPts( MAX_RIBBON_PTS ) );

                // ---- Signals -----------------------------------------------------
                connect( mCurrencyCombo, &QComboBox::currentIndexChanged, this, &RollCostWindow::onCurrencyChanged );
                connect( mCurrencyEdit, &QLineEdit::textChanged, this, &RollCostWindow::onCurrencyChanged );
                connect( mUnitsCombo, &QComboBox::currentIndexChanged, this, &RollCostWindow::onUnitsChanged );
                connect( mRibbonUnitCombo, &QComboBox::currentIndexChanged, this, &RollCostWindow::onRibbonUnitChanged );
                connect( mNoRibbonCheck, &QCheckBox::toggled, this, &RollCostWindow::onRibbonToggled );
                for ( auto* spin : { mLabelWSpin, mLabelLSpin, mGapSpin, mRollPriceSpin,
                                     mRibbonLSpin, mRibbonWSpin, mRibbonPriceSpin, mWasteSpin } )
                {
                        connect( spin, &QDoubleSpinBox::valueChanged, this, &RollCostWindow::calculate );
                }
                for ( auto* spin : { mAcrossSpin, mPerRollSpin, mQuantitySpin } )
                {
                        connect( spin, &QSpinBox::valueChanged, this, &RollCostWindow::calculate );
                }
                for ( auto* spin : { mLabelWSpin, mLabelLSpin } )
                {
                        connect( spin, &QDoubleSpinBox::valueChanged, this, &RollCostWindow::updateProductLabel );
                }
                connect( mChooseButton, &QPushButton::clicked, this, &RollCostWindow::chooseProduct );
                connect( mClearButton, &QPushButton::clicked, this, &RollCostWindow::clearProduct );
                connect( mSaveButton, &QPushButton::clicked, this, &RollCostWindow::saveEstimate );
                connect( closeButton, &QPushButton::clicked, this, &QWidget::close );
        }


        ///
        /// Restore the last values; defaults are a common 4 x 6 in shipping label
        /// or 100 x 150 mm, and a 300 m (or 1,476 ft) ribbon
        ///
        void RollCostWindow::loadSettings()
        {
                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );

                mUpdating = true;

                QString code = settings.value( "currency", QLocale().currencySymbol( QLocale::CurrencyIsoCode ) ).toString();
                int index = mCurrencyCombo->findData( code );
                mCurrencyCombo->setCurrentIndex( index >= 0 ? index : mCurrencyCombo->findData( "USD" ) );
                mCurrencyEdit->setText( settings.value( "customCurrency" ).toString() );

                QString ribbonUnit = settings.value( "ribbonUnit", mRibbonUnit ).toString();
                if ( ribbonUnit == "m" || ribbonUnit == "ft" )
                {
                        mRibbonUnit = ribbonUnit;
                        mRibbonUnitCombo->setCurrentIndex( mRibbonUnitCombo->findData( mRibbonUnit ) );
                        mRibbonLSpin->setRange( 0, ribbonFromPts( MAX_RIBBON_PTS ) );
                }

                bool inches = (mUnits.toEnum() == model::Units::IN);
                double defaultW   = inches ? model::Distance::in( 4 ).pt()     : model::Distance::mm( 100 ).pt();
                double defaultL   = inches ? model::Distance::in( 6 ).pt()     : model::Distance::mm( 150 ).pt();
                double defaultGap = inches ? model::Distance::in( 0.125 ).pt() : model::Distance::mm( 3 ).pt();
                double defaultRibbonW = inches ? model::Distance::in( 4.33 ).pt() : model::Distance::mm( 110 ).pt();
                double defaultRibbonL = (mRibbonUnit == "ft") ? model::Distance::in( 1476 * 12 ).pt()
                                                              : model::Distance::mm( 300000 ).pt();

                mLabelWSpin->setValue( fromPts( settings.value( "labelW", defaultW ).toDouble() ) );
                mLabelLSpin->setValue( fromPts( settings.value( "labelL", defaultL ).toDouble() ) );
                mGapSpin->setValue( fromPts( settings.value( "gap", defaultGap ).toDouble() ) );
                mAcrossSpin->setValue( settings.value( "across", 1 ).toInt() );
                mPerRollSpin->setValue( settings.value( "perRoll", 500 ).toInt() );
                mRollPriceSpin->setValue( settings.value( "rollPrice", 0.0 ).toDouble() );

                mNoRibbonCheck->setChecked( settings.value( "noRibbon", false ).toBool() );
                mRibbonLSpin->setValue( ribbonFromPts( settings.value( "ribbonL", defaultRibbonL ).toDouble() ) );
                mRibbonWSpin->setValue( fromPts( settings.value( "ribbonW", defaultRibbonW ).toDouble() ) );
                mRibbonPriceSpin->setValue( settings.value( "ribbonPrice", 0.0 ).toDouble() );

                mQuantitySpin->setValue( settings.value( "quantity", 1000 ).toInt() );
                mWasteSpin->setValue( settings.value( "waste", 0.0 ).toDouble() );

                // The product last chosen; the saved values (maybe edited) are kept
                QString product = settings.value( "product" ).toString();
                if ( !product.isEmpty() )
                {
                        auto tmplate = model::Db::lookupTemplateFromName( product );
                        if ( !tmplate.isNull() && tmplate.frame() )
                        {
                                mTemplate = tmplate;
                                mProductW = tmplate.frame()->w().pt();
                                mProductL = tmplate.frame()->h().pt();
                        }
                }

                mUpdating = false;
                settings.endGroup();
        }


        void RollCostWindow::saveSettings() const
        {
                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );

                settings.setValue( "currency", mCurrencyCombo->currentData().toString() );
                settings.setValue( "customCurrency", mCurrencyEdit->text().trimmed() );
                settings.setValue( "ribbonUnit", mRibbonUnit );

                settings.setValue( "labelW", toPts( mLabelWSpin->value() ) );
                settings.setValue( "labelL", toPts( mLabelLSpin->value() ) );
                settings.setValue( "gap", toPts( mGapSpin->value() ) );
                settings.setValue( "across", mAcrossSpin->value() );
                settings.setValue( "perRoll", mPerRollSpin->value() );
                settings.setValue( "rollPrice", mRollPriceSpin->value() );

                settings.setValue( "noRibbon", mNoRibbonCheck->isChecked() );
                settings.setValue( "ribbonL", ribbonToPts( mRibbonLSpin->value() ) );
                settings.setValue( "ribbonW", toPts( mRibbonWSpin->value() ) );
                settings.setValue( "ribbonPrice", mRibbonPriceSpin->value() );

                settings.setValue( "quantity", mQuantitySpin->value() );
                settings.setValue( "waste", mWasteSpin->value() );
                settings.setValue( "product", mTemplate.isNull() ? QString() : mTemplate.name() );

                settings.endGroup();
        }


        ///
        /// The open label: a roll product fills in the label
        ///
        void RollCostWindow::setTemplate( const model::Template& tmplate )
        {
                if ( !tmplate.isNull() && tmplate.isRoll() )
                {
                        applyTemplate( tmplate );
                }
        }


        ///
        /// Use a product as the label: size, and gap and labels across where the
        /// product has them.  Every value can still be changed by hand.
        ///
        void RollCostWindow::applyTemplate( const model::Template& tmplate )
        {
                if ( tmplate.isNull() || !tmplate.frame() )
                {
                        return;
                }

                const auto* frame = tmplate.frame();
                double w = frame->w().pt();
                double h = frame->h().pt();
                if ( w <= 0 || h <= 0 )
                {
                        return;
                }

                mTemplate = tmplate;
                mProductW = w;
                mProductL = h;

                bool wasUpdating = mUpdating;
                mUpdating = true;
                mLabelWSpin->setValue( fromPts( w ) );
                mLabelLSpin->setValue( fromPts( h ) );

                const auto& layouts = frame->layouts();
                if ( !layouts.empty() )
                {
                        const auto& layout = layouts.front();
                        if ( tmplate.isRoll() )
                        {
                                mAcrossSpin->setValue( std::max( 1, layout.nx() ) );
                        }
                        if ( layout.ny() > 1 && layout.dy().pt() > h )
                        {
                                mGapSpin->setValue( fromPts( layout.dy().pt() - h ) );
                        }
                }
                mUpdating = wasUpdating;

                updateProductLabel();
                calculate();
        }


        void RollCostWindow::chooseProduct()
        {
                SelectProductDialog dialog( this );
                dialog.exec();
                applyTemplate( dialog.tmplate() );
        }


        void RollCostWindow::clearProduct()
        {
                mTemplate = model::Template();
                updateProductLabel();
        }


        void RollCostWindow::updateProductLabel()
        {
                if ( mTemplate.isNull() )
                {
                        mProductLabel->setText( "<span style='color:gray'>" +
                                                tr("Choose a product, or enter the label size below.").toHtmlEscaped() +
                                                "</span>" );
                        mClearButton->hide();
                        return;
                }

                QString text = "<b>" + mTemplate.name().toHtmlEscaped() + "</b>";
                if ( !mTemplate.description().isEmpty() )
                {
                        text += "<br>" + mTemplate.description().toHtmlEscaped();
                }
                const double tolerance = model::Distance::mm( 0.01 ).pt();
                if ( std::abs( toPts( mLabelWSpin->value() ) - mProductW ) > tolerance ||
                     std::abs( toPts( mLabelLSpin->value() ) - mProductL ) > tolerance )
                {
                        text += "<br><i>" + tr("Size changed from the product") + "</i>";
                }
                mProductLabel->setText( text );
                mClearButton->show();
        }


        ///
        /// Switch units: convert every length
        ///
        void RollCostWindow::onUnitsChanged()
        {
                QList<QDoubleSpinBox*> spins = { mLabelWSpin, mLabelLSpin, mGapSpin, mRibbonWSpin };
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
                        configureSpin( spins[i], spins[i] == mGapSpin ? MAX_GAP_PTS : MAX_LABEL_PTS );
                        spins[i]->setValue( fromPts( pts[i] ) );
                }
                mUpdating = wasUpdating;

                calculate();
        }


        void RollCostWindow::onRibbonUnitChanged()
        {
                double pts = ribbonToPts( mRibbonLSpin->value() );
                mRibbonUnit = mRibbonUnitCombo->currentData().toString();

                bool wasUpdating = mUpdating;
                mUpdating = true;
                mRibbonLSpin->setRange( 0, ribbonFromPts( MAX_RIBBON_PTS ) );
                mRibbonLSpin->setValue( ribbonFromPts( pts ) );
                mUpdating = wasUpdating;

                calculate();
        }


        void RollCostWindow::onCurrencyChanged()
        {
                bool custom = (mCurrencyCombo->currentData().toString() == CUSTOM_CURRENCY);
                mCurrencyEdit->setVisible( custom );
                if ( custom && !mUpdating )
                {
                        mCurrencyEdit->setFocus();
                }

                QString prefix = currency() + " ";
                for ( auto* spin : { mRollPriceSpin, mRibbonPriceSpin } )
                {
                        spin->setPrefix( currency().isEmpty() ? QString() : prefix );
                }
                calculate();
        }


        void RollCostWindow::onRibbonToggled()
        {
                mRibbonBox->setEnabled( !mNoRibbonCheck->isChecked() );
                calculate();
        }


        void RollCostWindow::setStatus( const QString& text, bool isError )
        {
                mStatusLabel->setObjectName( isError ? "rcError" : "rcNote" );
                mStatusLabel->style()->unpolish( mStatusLabel );
                mStatusLabel->style()->polish( mStatusLabel );
                mStatusLabel->setText( text );
                mStatusLabel->show();
        }


        void RollCostWindow::showError( const QString& message )
        {
                mHasResult = false;
                for ( auto* label : { mRibbonsValue, mRollsValue, mTotalValue } )
                {
                        label->setText( QStringLiteral( "–" ) );
                }
                for ( auto* label : { mRibbonsAmount, mRollsAmount, mTotalNote } )
                {
                        label->setText( QStringLiteral( " " ) );
                }
                showSections();
                mSaveButton->setEnabled( false );

                setStatus( message, true );
        }


        ///
        /// Work out the job and show the results
        ///
        void RollCostWindow::calculate()
        {
                if ( mUpdating || !mSaveButton ) return;

                model::RollCost::Input in;
                in.quantity      = mQuantitySpin->value();
                in.wastePercent  = mWasteSpin->value();
                in.labelLength   = toPts( mLabelLSpin->value() );
                in.gap           = toPts( mGapSpin->value() );
                in.across        = mAcrossSpin->value();
                in.labelsPerRoll = mPerRollSpin->value();
                in.rollPrice     = mRollPriceSpin->value();
                in.useRibbon     = !mNoRibbonCheck->isChecked();
                in.ribbonLength  = ribbonToPts( mRibbonLSpin->value() );
                in.ribbonPrice   = mRibbonPriceSpin->value();

                double labelW = toPts( mLabelWSpin->value() );

                if ( mCurrencyCombo->currentData().toString() == CUSTOM_CURRENCY && currency().isEmpty() )
                {
                        showError( tr("Enter a currency symbol, or choose a currency from the list.") );
                        return;
                }
                if ( labelW <= 0 || in.labelLength <= 0 )
                {
                        showError( tr("Enter the width and length of one label.") );
                        return;
                }
                if ( in.useRibbon && in.ribbonLength <= 0 )
                {
                        showError( tr("Enter the length of one ribbon, or tick No ribbon (direct thermal).") );
                        return;
                }
                if ( in.useRibbon && in.ribbonLength < in.labelLength + in.gap )
                {
                        showError( tr("The ribbon is shorter than one label. Check the ribbon length and its unit.") );
                        return;
                }

                auto result = model::RollCost::compute( in );
                if ( !result.isValid() )
                {
                        showError( tr("Check the values: every quantity must be more than zero.") );
                        return;
                }

                mResult    = result;
                mLabelW    = labelW;
                mRibbonW   = toPts( mRibbonWSpin->value() );
                mHasResult = true;

                // Ribbon narrower than the labels it must cover
                mWarning.clear();
                double rowW = in.across * labelW;
                if ( in.useRibbon && mRibbonW > 0 && mRibbonW + model::Distance::mm( 0.05 ).pt() < rowW )
                {
                        mWarning = tr("The ribbon (%1) is narrower than the labels across the roll (%2). "
                                      "Part of each label will not print.")
                                .arg( formatLength( mRibbonW ), formatLength( rowW ) );
                }

                QLocale locale;
                if ( in.useRibbon )
                {
                        mRibbonsValue->setText( locale.toString( mResult.ribbons() ) );
                        mRibbonsAmount->setText( money( mResult.ribbonCost() ) );
                }
                else
                {
                        mRibbonsValue->setText( tr("None") );
                        mRibbonsAmount->setText( tr("Direct thermal") );
                }
                mRollsValue->setText( locale.toString( mResult.rolls() ) );
                mRollsAmount->setText( money( mResult.labelCost() ) );
                mTotalValue->setText( money( mResult.totalCost() ) );
                mTotalNote->setText( tr("%1 per label").arg( money( mResult.costPerLabel() ) ) );
                showSections();

                if ( mWarning.isEmpty() )
                {
                        mStatusLabel->hide();
                }
                else
                {
                        setStatus( mWarning, true );
                }
                mSaveButton->setEnabled( true );
        }


        ///
        /// Report: sections of facts, shared by the window, text, image and PDF
        ///
        QList<RollCostWindow::Line> RollCostWindow::reportLines() const
        {
                QLocale locale;
                QList<Line> lines;
                const auto& in = mResult.input();

                lines << Line{ tr("Job"), QString(), true };
                lines << Line{ tr("Labels to print"), locale.toString( in.quantity ) };
                if ( mResult.wasteLabels() > 0 )
                {
                        lines << Line{ tr("Extra for waste"), tr("%1 (%2 %)").arg( locale.toString( mResult.wasteLabels() ),
                                                                                  trimmed( in.wastePercent, 1 ) ) };
                        lines << Line{ tr("Labels printed"), locale.toString( mResult.printedLabels() ) };
                }
                lines << Line{ tr("Label size"), tr("%1 wide × %2 long").arg( formatLength( mLabelW ),
                                                                              formatLength( in.labelLength ) ) };
                lines << Line{ tr("Gap"), formatLength( in.gap ) };
                if ( in.across > 1 )
                {
                        lines << Line{ tr("Labels across"), tr("%1 (%2 rows)").arg( locale.toString( in.across ),
                                                                                    locale.toString( mResult.rows() ) ) };
                }

                lines << Line{ tr("Labels"), QString(), true };
                lines << Line{ tr("Labels per roll"), locale.toString( in.labelsPerRoll ) };
                lines << Line{ tr("Roll price"), money( in.rollPrice ) };
                lines << Line{ tr("Rolls to buy"), locale.toString( mResult.rolls() ) };
                lines << Line{ tr("Left on last roll"), tr("%1 labels").arg( locale.toString( mResult.labelsLeft() ) ) };
                lines << Line{ tr("Labels amount"), money( mResult.labelCost() ), false, true };

                lines << Line{ tr("Ribbon"), QString(), true };
                if ( in.useRibbon )
                {
                        QString ribbon = formatRibbon( in.ribbonLength );
                        if ( mRibbonW > 0 )
                        {
                                ribbon = tr("%1 × %2 wide").arg( ribbon, formatLength( mRibbonW ) );
                        }
                        lines << Line{ tr("Ribbon size"), ribbon };
                        lines << Line{ tr("Ribbon price"), money( in.ribbonPrice ) };
                        lines << Line{ tr("Ribbon needed"), formatRibbon( mResult.ribbonUsed() ) };
                        lines << Line{ tr("Ribbons to buy"), locale.toString( mResult.ribbons() ) };
                        lines << Line{ tr("Left on last ribbon"), formatRibbon( mResult.ribbonLeft() ) };
                        lines << Line{ tr("Ribbons amount"), money( mResult.ribbonCost() ), false, true };
                }
                else
                {
                        lines << Line{ tr("Ribbon"), tr("None (direct thermal)") };
                }

                lines << Line{ tr("Cost"), QString(), true };
                if ( in.useRibbon )
                {
                        lines << Line{ tr("Labels used"), money( mResult.labelUsedCost() ) };
                        lines << Line{ tr("Ribbon used"), money( mResult.ribbonUsedCost() ) };
                }
                lines << Line{ tr("Used by this job"), money( mResult.usedCost() ) };
                lines << Line{ tr("Cost per label"), money( mResult.costPerLabel() ), false, true };

                lines << Line{ tr("Totals"), QString(), true, false, true };
                lines << Line{ tr("Label rolls (%1)").arg( locale.toString( mResult.rolls() ) ),
                               money( mResult.labelCost() ) };
                if ( in.useRibbon )
                {
                        lines << Line{ tr("Ribbons (%1)").arg( locale.toString( mResult.ribbons() ) ),
                                       money( mResult.ribbonCost() ) };
                }
                lines << Line{ tr("Total to buy"), money( mResult.totalCost() ), false, true };

                return lines;
        }


        ///
        /// Results as bordered cards: Job, Labels, Ribbon, Cost and Totals
        ///
        void RollCostWindow::showSections()
        {
                while ( auto* item = mSectionsLayout->takeAt( 0 ) )
                {
                        delete item->widget();
                        delete item;
                }

                if ( !mHasResult )
                {
                        return;
                }

                QList<QFrame*> cards;
                QGridLayout* grid = nullptr;
                int row = 0;
                for ( const auto& line : reportLines() )
                {
                        if ( line.heading )
                        {
                                auto* card = new QFrame();
                                card->setObjectName( line.total ? "rcCardTotal" : "rcCard" );
                                grid = new QGridLayout( card );
                                grid->setContentsMargins( 14, 10, 14, 12 );
                                grid->setHorizontalSpacing( 16 );
                                grid->setVerticalSpacing( 6 );
                                grid->setColumnStretch( 1, 1 );
                                auto* title = new QLabel( line.key );
                                title->setObjectName( "rcCardTitle" );
                                grid->addWidget( title, 0, 0, 1, 2 );
                                row = 1;
                                cards << card;
                                continue;
                        }
                        if ( !grid ) continue;

                        auto* key = new QLabel( line.key );
                        key->setObjectName( "rcKey" );
                        auto* value = new QLabel( line.value );
                        value->setObjectName( line.strong ? "rcStrong" : "" );
                        value->setWordWrap( true );
                        value->setTextInteractionFlags( Qt::TextSelectableByMouse );
                        value->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                        grid->addWidget( key, row, 0, Qt::AlignTop );
                        grid->addWidget( value, row, 1 );
                        row++;
                        grid->setRowStretch( row, 1 );   // keep rows at the top of a taller card
                        if ( row > 1 ) grid->setRowStretch( row - 1, 0 );
                }

                // Two columns; the last card (Totals) spans both
                for ( int i = 0; i < cards.size(); i++ )
                {
                        bool last = (i == cards.size() - 1);
                        if ( last && i % 2 == 0 )
                        {
                                mSectionsLayout->addWidget( cards[i], i / 2, 0, 1, 2 );
                        }
                        else
                        {
                                mSectionsLayout->addWidget( cards[i], i / 2, i % 2 );
                        }
                }

                auto* note = new QLabel( tr("Total to buy counts whole ribbons and rolls. Cost counts only "
                                            "the labels and ribbon length this job uses.") );
                note->setObjectName( "rcNote" );
                note->setWordWrap( true );
                mSectionsLayout->addWidget( note, (cards.size() + 1) / 2, 0, 1, 2 );
                mSectionsLayout->setRowStretch( (cards.size() + 1) / 2 + 1, 1 );
        }


        QString RollCostWindow::reportText() const
        {
                auto lines = reportLines();
                int width = 0;
                for ( const auto& line : lines )
                {
                        if ( !line.heading ) width = std::max( width, int( line.key.size() ) );
                }

                QString text;
                QTextStream out( &text );
                out << tr("Ribbon & Label Cost Estimate") << "\n";
                out << QLocale().toString( QDateTime::currentDateTime(), QLocale::LongFormat ) << "\n";
                for ( const auto& line : lines )
                {
                        if ( line.heading )
                        {
                                out << "\n" << line.key << "\n";
                                continue;
                        }
                        out << "  " << (line.key + ":").leftJustified( width + 3 ) << line.value << "\n";
                }
                if ( !mWarning.isEmpty() )
                {
                        out << "\n" << mWarning << "\n";
                }
                out << "\n" << tr("Total to buy counts whole ribbons and rolls. Cost counts only "
                                  "the labels and ribbon length this job uses.") << "\n";
                return text;
        }


        ///
        /// Draw the report on a page-shaped canvas
        ///
        void RollCostWindow::renderReport( QPainter& painter, const QSizeF& canvas ) const
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
                painter.drawText( QPointF( margin, y ), tr("Ribbon & Label Cost Estimate") );
                y += titleMetrics.descent() + w * 0.008;

                font.setBold( false );
                font.setPixelSize( int( w * 0.016 ) );
                painter.setFont( font );
                QFontMetricsF metrics( font, painter.device() );
                painter.setPen( EXPORT_MUTED );
                y += metrics.ascent();
                painter.drawText( QPointF( margin, y ),
                                  QLocale().toString( QDateTime::currentDateTime(), QLocale::LongFormat ) );
                y += metrics.descent() + w * 0.02;

                double lineHeight = metrics.height() * 1.45;
                double valueX = margin + w * 0.30;
                QFont boldFont = font;
                boldFont.setBold( true );
                QFont headingFont = font;
                headingFont.setBold( true );
                headingFont.setPixelSize( int( w * 0.019 ) );

                for ( const auto& line : reportLines() )
                {
                        if ( line.heading )
                        {
                                y += lineHeight * 1.6;
                                painter.setFont( headingFont );
                                painter.setPen( EXPORT_ACCENT );
                                painter.drawText( QPointF( margin, y ), line.key );
                                y += lineHeight * 0.3;
                                painter.setPen( QPen( EXPORT_ACCENT, std::max( 1.0, w * 0.0012 ) ) );
                                painter.drawLine( QPointF( margin, y ), QPointF( w - margin, y ) );
                                continue;
                        }
                        y += lineHeight;
                        painter.setFont( font );
                        painter.setPen( EXPORT_MUTED );
                        painter.drawText( QPointF( margin, y ), line.key );
                        painter.setFont( line.strong ? boldFont : font );
                        painter.setPen( EXPORT_INK );
                        painter.drawText( QPointF( valueX, y ), line.value );
                }

                QStringList notes;
                if ( !mWarning.isEmpty() ) notes << mWarning;
                notes << tr("Total to buy counts whole ribbons and rolls. Cost counts only "
                            "the labels and ribbon length this job uses.");
                y += lineHeight * 2;
                painter.setFont( font );
                painter.setPen( EXPORT_MUTED );
                for ( const auto& note : notes )
                {
                        QRectF box( margin, y, w - 2*margin, canvas.height() - y - margin );
                        QRectF used;
                        painter.drawText( box, Qt::TextWordWrap, note, &used );
                        y += used.height() + lineHeight * 0.5;
                }
        }


        bool RollCostWindow::writeText( const QString& fileName ) const
        {
                QSaveFile file( fileName );
                if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
                {
                        return false;
                }
                file.write( reportText().toUtf8() );
                return file.commit();
        }


        bool RollCostWindow::writeImage( const QString& fileName ) const
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


        bool RollCostWindow::writePdf( const QString& fileName ) const
        {
                QPdfWriter writer( fileName );
                writer.setPageSize( QPageSize( QPageSize::A4 ) );
                writer.setPageMargins( QMarginsF( 0, 0, 0, 0 ) );
                writer.setResolution( 150 );
                writer.setTitle( tr("Ribbon & Label Cost Estimate") );
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
        void RollCostWindow::saveEstimate()
        {
                if ( !mHasResult ) return;

                const QString pdfFilter = tr("PDF document (*.pdf)");
                const QString pngFilter = tr("PNG image (*.png)");
                const QString txtFilter = tr("Text file (*.txt)");

                QSettings settings;
                settings.beginGroup( SETTINGS_GROUP );
                QString dir = settings.value( "saveDir",
                                              QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).toString();
                QString selectedFilter = settings.value( "saveFilter", pdfFilter ).toString();

                QString suggested = QDir( dir ).filePath( tr("ribbon-label-cost") );
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
        /// Currency and units helpers
        ///
        QString RollCostWindow::currency() const
        {
                QString code = mCurrencyCombo->currentData().toString();
                return (code == CUSTOM_CURRENCY) ? mCurrencyEdit->text().trimmed() : code;
        }


        QString RollCostWindow::money( double amount ) const
        {
                // Amounts under 1 (cost per label) show up to 4 decimals, at least 2
                QString number = QLocale().toString( amount, 'f', 2 );
                if ( std::abs( amount ) > 0 && std::abs( amount ) < 1 )
                {
                        number = QLocale().toString( amount, 'f', 4 );
                        for ( int i = 0; i < 2 && number.endsWith( '0' ); i++ )
                        {
                                number.chop( 1 );
                        }
                }
                QString symbol = currency();
                return symbol.isEmpty() ? number : symbol + " " + number;
        }


        double RollCostWindow::toPts( double value ) const
        {
                return model::Distance( value, mUnits ).pt();
        }


        double RollCostWindow::fromPts( double pts ) const
        {
                return model::Distance::pt( pts ).inUnits( mUnits );
        }


        double RollCostWindow::ribbonToPts( double value ) const
        {
                return value * ptsPerRibbonUnit( mRibbonUnit );
        }


        double RollCostWindow::ribbonFromPts( double pts ) const
        {
                return pts / ptsPerRibbonUnit( mRibbonUnit );
        }


        void RollCostWindow::configureSpin( QDoubleSpinBox* spin, double maxPts )
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


        QString RollCostWindow::formatLength( double pts ) const
        {
                int decimals = (mUnits.toEnum() == model::Units::MM) ? 2 :
                               (mUnits.toEnum() == model::Units::CM) ? 3 : 4;
                return trimmed( fromPts( pts ), decimals ) + " " + mUnits.toIdString();
        }


        QString RollCostWindow::formatRibbon( double pts ) const
        {
                return trimmed( ribbonFromPts( pts ), 2 ) + " " + mRibbonUnit;
        }

}
