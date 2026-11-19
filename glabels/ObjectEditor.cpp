//  ObjectEditor.cpp
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


#include "ObjectEditor.hpp"

#include "UndoRedoModel.hpp"

#include "model/Model.hpp"
#include "model/ModelObject.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/FileUtil.hpp"
#include "model/Settings.hpp"
#include "model/Size.hpp"

#include "merge/Merge.hpp"
#include "merge/DataSourceUtil.hpp"
#include "VariableHighlighter.hpp"
#include "VariableTypeIcon.hpp"
#include "model/TextNode.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QToolButton>

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QtMath>


namespace glabels
{

        ///
        /// Units suffix for spin boxes (may be hidden by user preference)
        ///
        QString ObjectEditor::unitsSuffix() const
        {
                return model::Settings::propertiesShowUnits() ? " " + mUnits.toIdString() : QString();
        }


        ///
        /// Constructor
        ///
        ObjectEditor::ObjectEditor( QWidget *parent )
                : mModel(nullptr), mObject(nullptr), mImageCwd("."), mBlocked(false)
        {
                setupUi( this );

                textHAlignGroup = new QButtonGroup( this );
                textHAlignGroup->addButton( textHAlignLeftToggle, Qt::AlignLeft );
                textHAlignGroup->addButton( textHAlignCenterToggle, Qt::AlignHCenter );
                textHAlignGroup->addButton( textHAlignRightToggle, Qt::AlignRight );

                textVAlignGroup = new QButtonGroup( this );
                textVAlignGroup->addButton( textVAlignTopToggle, Qt::AlignTop );
                textVAlignGroup->addButton( textVAlignMiddleToggle, Qt::AlignVCenter );
                textVAlignGroup->addButton( textVAlignBottomToggle, Qt::AlignBottom );

                lineColorButton->init( tr("No line"), QColor(0,0,0,0), QColor(0,0,0,255) );
                fillColorButton->init( tr("No fill"), QColor(0,0,0,0), QColor(0,0,0,255) );
                textColorButton->init( tr("Default"), QColor(0,0,0,255), QColor(0,0,0,255) );
                barcodeColorButton->init( tr("Default"), QColor(0,0,0,255), QColor(0,0,0,255) );
                shadowColorButton->init( tr("Default"), QColor(0,0,0,255), QColor(0,0,0,255) );

                textInsertFieldButton->setText( tr("Insert Variable") );
                barcodeInsertFieldButton->setText( tr("Insert Variable") );
                imageFieldButton->setText( tr("Use Variable") );
                imageFieldButton->setVisible( false );   // replaced by the Static | Variable switch

                // {{Variable}} placeholders are shown as highlighted tags while editing
                new VariableHighlighter( textEdit->document() );
                new VariableHighlighter( barcodeDataEdit->document() );
                textEdit->setAcceptRichText( false );
                barcodeDataEdit->setAcceptRichText( false );

                setupVariableModeControls();

                // Quick action next to "Wrap mode": shrink/grow the text box to fit its text
                auto* fitTextButton = new QToolButton( this );
                fitTextButton->setObjectName( "textFitButton" );
                fitTextButton->setIcon( QIcon::fromTheme( "glabels-zoom-to-fit" ) );
                fitTextButton->setToolTip( tr("Fit box to text: resize the text box to the size of its text") );
                fitTextButton->setAccessibleName( tr("Fit box to text") );
                fitTextButton->setAutoRaise( true );
                fitTextButton->setCursor( Qt::PointingHandCursor );
                horizontalLayout_25->insertWidget( 1, fitTextButton );
                connect( fitTextButton, &QToolButton::clicked, this, [this]{
                        if ( mObject && mUndoRedoModel )
                        {
                                mUndoRedoModel->checkpoint( tr("Fit to Text") );
                                mObject->setSize( mObject->naturalSize() );
                        }
                } );

                setEnabled( false );
                hidePages();

                connect( model::Settings::instance(), SIGNAL(changed()),
                         this, SLOT(onSettingsChanged()) );

                onSettingsChanged();
        }


        void ObjectEditor::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                mModel = model;
                mUndoRedoModel = undoRedoModel;

                connect( mModel, SIGNAL(sizeChanged()),
                         this, SLOT(onLabelSizeChanged()) );

                connect( mModel, SIGNAL(selectionChanged()),
                         this, SLOT(onSelectionChanged()) );

                connect( mModel, SIGNAL(mergeSourceChanged()),
                         this, SLOT(onFieldsAvailableChanged()) );

                connect( mModel, SIGNAL(variablesChanged()),
                         this, SLOT(onFieldsAvailableChanged()) );

                onLabelSizeChanged();
                onSelectionChanged();
                onFieldsAvailableChanged();
        }


        void ObjectEditor::hidePages()
        {
                notebook->removeTab( notebook->indexOf(textPage) );
                notebook->removeTab( notebook->indexOf(barcodePage) );
                notebook->removeTab( notebook->indexOf(imagePage) );
                notebook->removeTab( notebook->indexOf(lineFillPage) );
                notebook->removeTab( notebook->indexOf(posSizePage) );
                notebook->removeTab( notebook->indexOf(shadowPage) );
        }


        void ObjectEditor::loadImagePage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        model::TextNode filenameNode = mObject->filenameNode();

                        updateImageModeUi( filenameNode.isField() );

                        if ( filenameNode.isField() )
                        {
                                imageFilenameLineEdit->setText( QString("{{%1}}").arg(filenameNode.data()) );
                        }
                        else
                        {
                                QString fn = model::FileUtil::makeRelativeIfInDir( mModel->dir(), filenameNode.data() );
                                imageFilenameLineEdit->setText( fn );
                        }

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadLineFillPage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        lineWidthSpin->setValue( mObject->lineWidth().pt() );
                        lineColorButton->setColorNode( mObject->lineColorNode() );
                        fillColorButton->setColorNode( mObject->fillColorNode() );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadPositionPage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        posXSpin->setDecimals( mSpinDigits );
                        posXSpin->setSingleStep( mSpinStep );
                        posXSpin->setSuffix( unitsSuffix() );

                        posYSpin->setDecimals( mSpinDigits );
                        posYSpin->setSingleStep( mSpinStep );
                        posYSpin->setSuffix( unitsSuffix() );

                        posXSpin->setValue( mObject->x0().inUnits(mUnits) );
                        posYSpin->setValue( mObject->y0().inUnits(mUnits) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadRectSizePage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        sizeWSpin->setDecimals( mSpinDigits );
                        sizeWSpin->setSingleStep( mSpinStep );
                        sizeWSpin->setSuffix( unitsSuffix() );

                        sizeHSpin->setDecimals( mSpinDigits );
                        sizeHSpin->setSingleStep( mSpinStep );
                        sizeHSpin->setSuffix( unitsSuffix() );

                        sizeWSpin->setValue( mObject->w().inUnits(mUnits) );
                        sizeHSpin->setValue( mObject->h().inUnits(mUnits) );
                        sizeAspectCheck->setChecked( mObject->lockAspectRatio() );

                        model::Size originalSize = mObject->naturalSize();
                        QString originalSizeString = QString( "%1:  %2 x %3 %4" )
                                .arg( tr("Original size") )
                                .arg( originalSize.w().inUnits(mUnits), 0, 'f', mSpinDigits )
                                .arg( originalSize.h().inUnits(mUnits), 0, 'f', mSpinDigits )
                                .arg( mUnits.toIdString() );
                        sizeOriginalSizeLabel->setText( originalSizeString );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadLineSizePage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        sizeLineLengthSpin->setDecimals( mSpinDigits );
                        sizeLineLengthSpin->setSingleStep( mSpinStep );
                        sizeLineLengthSpin->setSuffix( unitsSuffix() );

                        double w = mObject->w().inUnits(mUnits);
                        double h = mObject->h().inUnits(mUnits);
                        sizeLineLengthSpin->setValue( sqrt( w*w + h*h ) );
                        sizeLineAngleSpin->setValue( qRadiansToDegrees( qAtan2( h, w ) ) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadTextPage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        int wrapIndex = 0;
                        switch (mObject->textWrapMode())
                        {
                        case QTextOption::WordWrap:     wrapIndex = 0; break;
                        case QTextOption::WrapAnywhere: wrapIndex = 1; break;
                        default:                        wrapIndex = 2; break;
                        }

                        textFontFamilyCombo->setFamily( mObject->fontFamily() );
                        textFontSizeSpin->setValue( mObject->fontSize() );
                        textFontBoldToggle->setChecked( mObject->fontWeight() == QFont::Bold );
                        textFontItalicToggle->setChecked( mObject->fontItalicFlag() );
                        textFontUnderlineToggle->setChecked( mObject->fontUnderlineFlag() );
                        textColorButton->setColorNode( mObject->textColorNode() );
                        textHAlignGroup->button( mObject->textHAlign() )->setChecked( true );
                        textVAlignGroup->button( mObject->textVAlign() )->setChecked( true );
                        textWrapModeCombo->setCurrentIndex( wrapIndex );
                        textLineSpacingSpin->setValue( mObject->textLineSpacing() );
                        textAutoShrinkCheck->setChecked( mObject->textAutoShrink() );
                        textEdit->setPlainText( merge::DataSourceUtil::toDisplay( mObject->text() ) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadBarcodePage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        barcode::Style bcStyle = mObject->bcStyle();

                        barcodeShowTextCheck->setEnabled( bcStyle.textOptional() );
                        barcodeChecksumCheck->setEnabled( bcStyle.checksumOptional() );

                        // May need to adjust current text flag, if style changed
                        bool textFlag = (mObject->bcTextFlag() && bcStyle.canText())
                                ||  (bcStyle.canText() && !bcStyle.textOptional());
                        mObject->setBcTextFlag( textFlag );

                        // May need to adjust current checksum flag, if style changed
                        bool csFlag = (mObject->bcChecksumFlag() && bcStyle.canChecksum())
                                ||  (bcStyle.canChecksum() && !bcStyle.checksumOptional());
                        mObject->setBcChecksumFlag( csFlag );

                        barcodeStyleButton->setBcStyle( bcStyle );
                        barcodeShowTextCheck->setChecked( textFlag );
                        barcodeChecksumCheck->setChecked( csFlag );
                        barcodeColorButton->setColorNode( mObject->bcColorNode() );
                        barcodeDataEdit->setPlainText( merge::DataSourceUtil::toDisplay( mObject->bcData() ) );
                        updateBarcodeModeUi( !singleVariableOf( mObject->bcData() ).isEmpty() );

                        mBlocked = false;
                }
        }


        void ObjectEditor::loadShadowPage()
        {
                if ( mObject )
                {
                        mBlocked = true;

                        shadowXSpin->setDecimals( mSpinDigits );
                        shadowXSpin->setSingleStep( mSpinStep );
                        shadowXSpin->setSuffix( unitsSuffix() );

                        shadowYSpin->setDecimals( mSpinDigits );
                        shadowYSpin->setSingleStep( mSpinStep );
                        shadowYSpin->setSuffix( unitsSuffix() );

                        shadowEnableCheck->setChecked( mObject->shadow() );
                        shadowXSpin->setValue( mObject->shadowX().inUnits(mUnits) );
                        shadowYSpin->setValue( mObject->shadowY().inUnits(mUnits) );
                        shadowColorButton->setColorNode( mObject->shadowColorNode() );
                        shadowOpacitySpin->setValue( 100*mObject->shadowOpacity() );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onSettingsChanged()
        {
                mUnits = model::Settings::units();
                mSpinDigits = mUnits.resolutionDigits();
                mSpinStep = mUnits.resolution();

                /* Must now update limits and reload any active pages with appropriate units. */
                onLabelSizeChanged();
                onSelectionChanged();
        }


        void ObjectEditor::onLabelSizeChanged()
        {
                if ( mModel )
                {
                        mBlocked = true;

                        model::Distance whMax = std::max( mModel->w(), mModel->h() );

                        posXSpin->setRange( -whMax.inUnits(mUnits), 2*whMax.inUnits(mUnits) );
                        posYSpin->setRange( -whMax.inUnits(mUnits), 2*whMax.inUnits(mUnits) );
                        sizeWSpin->setRange( 0, 2*whMax.inUnits(mUnits) );
                        sizeHSpin->setRange( 0, 2*whMax.inUnits(mUnits) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onSelectionChanged()
        {
                if ( mModel )
                {
                        if ( mObject )
                        {
                                disconnect( mObject, nullptr, this, nullptr );
                        }

                        hidePages();

                        if ( mModel->isSelectionAtomic() )
                        {
                                mObject = mModel->getFirstSelectedObject();

                                if ( dynamic_cast<model::ModelBoxObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-box" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Box object properties") );

                                        notebook->addTab( lineFillPage, tr("line/fill") );
                                        notebook->addTab( posSizePage, tr("position/size") );
                                        notebook->addTab( shadowPage, tr("shadow") );

                                        fillFrame->setVisible( true );
                                        sizeRectFrame->setVisible( true );
                                        sizeOriginalSizeGroup->setVisible( false );
                                        sizeLineFrame->setVisible( false );

                                        loadLineFillPage();
                                        loadPositionPage();
                                        loadRectSizePage();
                                        loadShadowPage();

                                        setEnabled( true );
                                }
                                else if ( dynamic_cast<model::ModelEllipseObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-ellipse" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Ellipse object properties") );

                                        notebook->addTab( lineFillPage, tr("line/fill") );
                                        notebook->addTab( posSizePage, tr("position/size") );
                                        notebook->addTab( shadowPage, tr("shadow") );

                                        fillFrame->setVisible( true );
                                        sizeRectFrame->setVisible( true );
                                        sizeOriginalSizeGroup->setVisible( false );
                                        sizeLineFrame->setVisible( false );

                                        loadLineFillPage();
                                        loadPositionPage();
                                        loadRectSizePage();
                                        loadShadowPage();

                                        setEnabled( true );
                                }
                                else if ( dynamic_cast<model::ModelImageObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-image" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Image object properties") );

                                        notebook->addTab( imagePage, tr("image") );
                                        notebook->addTab( posSizePage, tr("position/size") );
                                        notebook->addTab( shadowPage, tr("shadow") );

                                        sizeRectFrame->setVisible( true );
                                        sizeOriginalSizeGroup->setVisible( true );
                                        sizeLineFrame->setVisible( false );

                                        loadImagePage();
                                        loadPositionPage();
                                        loadRectSizePage();
                                        loadShadowPage();

                                        setEnabled( true );
                                }
                                else if ( dynamic_cast<model::ModelLineObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-line" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Line object properties") );

                                        notebook->addTab( lineFillPage, tr("line/fill") );
                                        notebook->addTab( posSizePage, tr("position/size") );
                                        notebook->addTab( shadowPage, tr("shadow") );

                                        fillFrame->setVisible( false );
                                        sizeRectFrame->setVisible( false );
                                        sizeOriginalSizeGroup->setVisible( false );
                                        sizeLineFrame->setVisible( true );

                                        loadLineFillPage();
                                        loadPositionPage();
                                        loadLineSizePage();
                                        loadShadowPage();

                                        setEnabled( true );
                                }
                                else if ( dynamic_cast<model::ModelTextObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-text" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Text object properties") );

                                        notebook->addTab( textPage, tr("text") );
                                        notebook->addTab( posSizePage, tr("position/size") );
                                        notebook->addTab( shadowPage, tr("shadow") );

                                        sizeRectFrame->setVisible( true );
                                        sizeOriginalSizeGroup->setVisible( false );
                                        sizeLineFrame->setVisible( false );

                                        loadTextPage();
                                        loadPositionPage();
                                        loadRectSizePage();
                                        loadShadowPage();

                                        setEnabled( true );
                                }
                                else if ( dynamic_cast<model::ModelBarcodeObject*>(mObject) )
                                {
                                        titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-barcode" ).pixmap( 24 ) );
                                        titleLabel->setText( tr("Barcode object properties") );

                                        notebook->addTab( barcodePage, tr("barcode") );
                                        notebook->addTab( posSizePage, tr("position/size") );

                                        sizeRectFrame->setVisible( true );
                                        sizeOriginalSizeGroup->setVisible( false );
                                        sizeLineFrame->setVisible( false );

                                        loadBarcodePage();
                                        loadPositionPage();
                                        loadRectSizePage();

                                        setEnabled( true );
                                }
                                else
                                {
                                        Q_ASSERT_X( false, "ObjectEditor::onSelectionChanged", "Invalid object" );
                                }

                                connect( mObject, SIGNAL(changed()), this, SLOT(onObjectChanged()) );
                                connect( mObject, SIGNAL(moved()), this, SLOT(onObjectMoved()) );
                                connect( mObject, SIGNAL(destroyed(QObject*)), this, SLOT(onObjectDestroyed()) );
                        }
                        else
                        {
                                mObject = nullptr;

                                titleImageLabel->setPixmap( QIcon::fromTheme( "glabels-object-properties" ).pixmap( 24 ) );
                                titleLabel->setText( tr("Object properties") );
                                setEnabled( false );
                        }
                }
        }


        void ObjectEditor::onFieldsAvailableChanged()
        {
                if ( !mBlocked )
                {
                        lineColorButton->setKeys( mModel->merge(), mModel->variables() );
                        fillColorButton->setKeys( mModel->merge(), mModel->variables() );
                        textColorButton->setKeys( mModel->merge(), mModel->variables() );
                        barcodeColorButton->setKeys( mModel->merge(), mModel->variables() );
                        shadowColorButton->setKeys( mModel->merge(), mModel->variables() );

                        textInsertFieldButton->setKeys( mModel->merge(), mModel->variables() );
                        barcodeInsertFieldButton->setKeys( mModel->merge(), mModel->variables() );
                        imageFieldButton->setKeys( mModel->merge(), mModel->variables() );

                        fillVariableCombos();
                        if ( dynamic_cast<model::ModelBarcodeObject*>( mObject ) )
                        {
                                loadBarcodePage();
                        }
                        if ( dynamic_cast<model::ModelImageObject*>( mObject ) )
                        {
                                loadImagePage();
                        }
                }
        }


        void ObjectEditor::onObjectChanged()
        {
                if ( !mBlocked )
                {
                        loadLineFillPage();
                        loadRectSizePage();
                        loadLineSizePage();
                        loadImagePage();
                        loadShadowPage();

                        // External edits (in-place editing, variable renames)
                        if ( dynamic_cast<model::ModelTextObject*>( mObject ) &&
                             merge::DataSourceUtil::fromDisplay( textEdit->toPlainText() ) != mObject->text() )
                        {
                                loadTextPage();
                        }
                        if ( dynamic_cast<model::ModelBarcodeObject*>( mObject ) &&
                             merge::DataSourceUtil::fromDisplay( barcodeDataEdit->toPlainText() ) != mObject->bcData() )
                        {
                                loadBarcodePage();
                        }
                }
        }


        void ObjectEditor::onObjectMoved()
        {
                if ( !mBlocked )
                {
                        loadPositionPage();
                }
        }


        void ObjectEditor::onObjectDestroyed()
        {
                disconnect( mObject, nullptr, this, nullptr );
                mObject = nullptr;
        }


        void ObjectEditor::onLineControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Line") );

                        mObject->setLineWidth( model::Distance::pt(lineWidthSpin->value()) );
                        mObject->setLineColorNode( lineColorButton->colorNode() );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onFillControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Fill") );

                        mObject->setFillColorNode( fillColorButton->colorNode() );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onImageFileButtonClicked()
        {
                // Either use saved CWD from a previous open or the current file, if it exists
                QString cwd = mImageCwd;
                if ( !mObject->filenameNode().isField() )
                {
                        QFileInfo fileInfo( mObject->filenameNode().data() );
                        if ( fileInfo.isFile() )
                        {
                                cwd = fileInfo.filePath();
                        }
                }

                QString filters =
                        tr("Image files (*.png *.jpg *.jpeg *.gif *.bmp *.pbm *.pgm *.ppm *.xbm *.xpm *.svg)") + ";;" +
                        tr("All files (*)") + ";;" +
                        tr("PNG - Portable Network Graphics (*.png)") + ";;" +
                        tr("BMP - Windows Bitmap (*.bmp)") + ";;" +
                        tr("GIF - Graphics Interchange Format (*.gif)") + ";;" +
                        tr("JPEG - Joint Photographic Experts Group (*.jpg *.jpeg)") + ";;" +
                        tr("PBM - Portable Bitmap (*.pbm)") + ";;" +
                        tr("PGM - Portable Graymap (*.pgm)") + ";;" +
                        tr("PPM - Portable Pixmap (*.ppm)") + ";;" +
                        tr("SVG - Scalable Vector Graphics (*.svg)") + ";;" +
                        tr("XBM - X11 Bitmap (*.xbm)") + ";;" +
                        tr("XPM - X11 Pixmap (*.xpm)");

                QString filename =
                        QFileDialog::getOpenFileName( this->window(),
                                                      tr("gLabels Plus - Select image file"),
                                                      cwd, filters );

                if ( !filename.isEmpty() )
                {
                        mUndoRedoModel->checkpoint( tr("Set image") );
                        mObject->setFilenameNode( model::TextNode( false, filename ) );

                        // Save CWD for next open
                        QFileInfo fileInfo( filename );
                        mImageCwd = fileInfo.absolutePath();
                }
        }


        void ObjectEditor::onImageKeySelected( QString key )
        {
                if ( mObject )
                {
                        mUndoRedoModel->checkpoint( tr("Set image") );
                        mObject->setFilenameNode( model::TextNode( true, key ) );
                }
        }


        void ObjectEditor::onPositionControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Position") );

                        model::Distance x = model::Distance(posXSpin->value(), mUnits);
                        model::Distance y = model::Distance(posYSpin->value(), mUnits);

                        mObject->setPosition( x, y );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onRectSizeControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Size") );

                        model::Distance spinW = model::Distance(sizeWSpin->value(), mUnits);
                        model::Distance spinH = model::Distance(sizeHSpin->value(), mUnits);

                        if ( sizeAspectCheck->isChecked() )
                        {
                                mObject->setLockAspectRatio( true );
                                if ( fabs(spinW - mObject->w()) > fabs(spinH - mObject->h()) )
                                {
                                        mObject->setWHonorAspect( spinW );
                                        sizeHSpin->setValue( mObject->h().inUnits(mUnits) );
                                }
                                else
                                {
                                        mObject->setHHonorAspect( spinH );
                                        sizeWSpin->setValue( mObject->w().inUnits(mUnits) );
                                }
                        }
                        else
                        {
                                mObject->setLockAspectRatio( false );
                                mObject->setSize( spinW, spinH );
                        }

                        mBlocked = false;
                }
        }


        void ObjectEditor::onLineSizeControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Size") );

                        model::Distance spinLength = model::Distance(sizeLineLengthSpin->value(), mUnits);
                        double spinAngleRads = qDegreesToRadians( sizeLineAngleSpin->value() );

                        mObject->setSize( spinLength*qCos(spinAngleRads),
                                          spinLength*qSin(spinAngleRads) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onTextControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Text") );

                        QTextOption::WrapMode wrapMode;
                        switch (textWrapModeCombo->currentIndex())
                        {
                        case 0:  wrapMode = QTextOption::WordWrap;     break;
                        case 1:  wrapMode = QTextOption::WrapAnywhere; break;
                        default: wrapMode = QTextOption::NoWrap;       break;
                        }

                        mObject->setFontFamily( textFontFamilyCombo->family() );
                        mObject->setFontSize( textFontSizeSpin->value() );
                        mObject->setFontWeight( textFontBoldToggle->isChecked() ? QFont::Bold : QFont::Normal );
                        mObject->setFontItalicFlag( textFontItalicToggle->isChecked() );
                        mObject->setFontUnderlineFlag( textFontUnderlineToggle->isChecked() );
                        mObject->setTextColorNode( textColorButton->colorNode() );
                        mObject->setTextHAlign( Qt::AlignmentFlag( textHAlignGroup->checkedId() ) );
                        mObject->setTextVAlign( Qt::AlignmentFlag( textVAlignGroup->checkedId() ) );
                        mObject->setTextWrapMode( wrapMode );
                        mObject->setTextLineSpacing( textLineSpacingSpin->value() );
                        mObject->setTextAutoShrink( textAutoShrinkCheck->isChecked() );
                        mObject->setText( merge::DataSourceUtil::fromDisplay( textEdit->toPlainText() ) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onTextInsertFieldKeySelected( QString key )
        {
                textEdit->insertPlainText( "{{" + key + "}}" );
                textEdit->setFocus();
        }


        void ObjectEditor::onBarcodeControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Barcode") );

                        barcode::Style bcStyle = barcodeStyleButton->bcStyle();

                        barcodeShowTextCheck->setEnabled( bcStyle.textOptional() );
                        barcodeChecksumCheck->setEnabled( bcStyle.checksumOptional() );

                        // May need to adjust current text flag, if style changed
                        bool textFlag = (barcodeShowTextCheck->isChecked() && bcStyle.canText())
                                ||  (bcStyle.canText() && !bcStyle.textOptional());
                        barcodeShowTextCheck->setChecked( textFlag );

                        // Checksum flag: on a style change start from that style's default
                        // (on only when the check digit is mandatory, because an optional
                        // one is read back as data by most scanners); otherwise keep the
                        // user's choice
                        bool styleChanged = !( bcStyle.id() == mObject->bcStyle().id() &&
                                               bcStyle.backendId() == mObject->bcStyle().backendId() );
                        bool wanted = styleChanged ? false : barcodeChecksumCheck->isChecked();
                        bool csFlag = (wanted && bcStyle.canChecksum())
                                ||  (bcStyle.canChecksum() && !bcStyle.checksumOptional());
                        barcodeChecksumCheck->setChecked( csFlag );

                        mObject->setBcStyle( bcStyle );
                        mObject->setBcTextFlag( textFlag );
                        mObject->setBcChecksumFlag( csFlag );
                        mObject->setBcColorNode( barcodeColorButton->colorNode() );
                        mObject->setBcData( merge::DataSourceUtil::fromDisplay( barcodeDataEdit->toPlainText() ) );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onBarcodeInsertFieldKeySelected( QString key )
        {
                barcodeDataEdit->insertPlainText( "{{" + key + "}}" );
                barcodeDataEdit->setFocus();
        }


        void ObjectEditor::onResetImageSize()
        {
                mUndoRedoModel->checkpoint( tr("Reset") );

                mObject->setSize( mObject->naturalSize() );
        }


        void ObjectEditor::onShadowControlsChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        mUndoRedoModel->checkpoint( tr("Shadow") );

                        mObject->setShadow( shadowEnableCheck->isChecked() );
                        mObject->setShadowX( model::Distance(shadowXSpin->value(), mUnits) );
                        mObject->setShadowY( model::Distance(shadowYSpin->value(), mUnits) );
                        mObject->setShadowColorNode( shadowColorButton->colorNode() );
                        mObject->setShadowOpacity( shadowOpacitySpin->value()/100.0 );

                        mBlocked = false;
                }
        }


        void ObjectEditor::onChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        qDebug() << "Form changed.";

                        mBlocked = false;
                }
        }


        ///
        /// Build the "Static | Variable" switches of the barcode and image pages
        ///
        void ObjectEditor::setupVariableModeControls()
        {
                auto makeSegment = [this]( QToolButton*& left, QToolButton*& right ) {
                        auto* row = new QWidget();
                        row->setObjectName( "modeSegment" );
                        auto* layout = new QHBoxLayout( row );
                        layout->setContentsMargins( 0, 0, 0, 0 );
                        layout->setSpacing( 0 );
                        left = new QToolButton();
                        left->setText( tr("Static") );
                        left->setCheckable( true );
                        left->setObjectName( "segmentLeft" );
                        right = new QToolButton();
                        right->setText( tr("Variable") );
                        right->setCheckable( true );
                        right->setObjectName( "segmentRight" );
                        auto* group = new QButtonGroup( row );
                        group->setExclusive( true );
                        group->addButton( left );
                        group->addButton( right );
                        layout->addWidget( left );
                        layout->addWidget( right );
                        layout->addStretch( 1 );
                        row->setStyleSheet(
                                "#modeSegment QToolButton { padding: 3px 14px; border: 1px solid palette(mid); }"
                                "#modeSegment QToolButton#segmentLeft { border-top-left-radius: 5px; border-bottom-left-radius: 5px; }"
                                "#modeSegment QToolButton#segmentRight { border-left: none;"
                                "  border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"
                                "#modeSegment QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
                                "  border-color: palette(highlight); }"
                                "#modeSegment QToolButton:disabled { color: palette(placeholder-text); }" );
                        return row;
                };

                auto makeVariableRow = [this]( QComboBox*& combo ) {
                        auto* row = new QWidget();
                        auto* layout = new QHBoxLayout( row );
                        layout->setContentsMargins( 0, 0, 0, 0 );
                        layout->addWidget( new QLabel( tr("Variable:") ) );
                        combo = new QComboBox();
                        combo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
                        layout->addWidget( combo, 1 );
                        return row;
                };

                // Barcode page: [Static|Variable] above the data editor
                gridLayout_11->removeItem( gridLayout_10 );
                gridLayout_11->addWidget( makeSegment( mBcStaticButton, mBcVariableButton ), 0, 0 );
                mBcVariableRow = makeVariableRow( mBcVariableCombo );
                gridLayout_11->addWidget( mBcVariableRow, 1, 0 );
                gridLayout_11->addLayout( gridLayout_10, 2, 0 );
                connect( mBcStaticButton, &QToolButton::clicked, this, &ObjectEditor::onBarcodeModeClicked );
                connect( mBcVariableButton, &QToolButton::clicked, this, &ObjectEditor::onBarcodeModeClicked );
                connect( mBcVariableCombo, QOverload<int>::of( &QComboBox::activated ),
                         this, &ObjectEditor::onBarcodeVariableActivated );

                // Image page: [Static|Variable] above the file name
                gridLayout_5->removeItem( horizontalLayout_17 );
                gridLayout_5->removeItem( horizontalLayout_27 );
                gridLayout_5->addWidget( makeSegment( mImageStaticButton, mImageVariableButton ), 0, 0 );
                mImageVariableRow = makeVariableRow( mImageVariableCombo );
                gridLayout_5->addWidget( mImageVariableRow, 1, 0 );
                gridLayout_5->addLayout( horizontalLayout_17, 2, 0 );
                gridLayout_5->addLayout( horizontalLayout_27, 3, 0 );
                connect( mImageStaticButton, &QToolButton::clicked, this, &ObjectEditor::onImageModeClicked );
                connect( mImageVariableButton, &QToolButton::clicked, this, &ObjectEditor::onImageModeClicked );
                connect( mImageVariableCombo, QOverload<int>::of( &QComboBox::activated ),
                         this, &ObjectEditor::onImageVariableActivated );

                updateBarcodeModeUi( false );
                updateImageModeUi( false );
        }


        ///
        /// Fill the variable dropdowns from the data source and custom variables
        ///
        void ObjectEditor::fillVariableCombos()
        {
                for ( QComboBox* combo : { mBcVariableCombo, mImageVariableCombo } )
                {
                        if ( !combo )
                        {
                                continue;
                        }
                        combo->clear();
                        if ( !mModel )
                        {
                                continue;
                        }
                        if ( mModel->merge() )
                        {
                                for ( const auto& key : mModel->merge()->keys() )
                                {
                                        QString type = mModel->merge()->fieldType( key );
                                        combo->addItem( VariableType::icon( type ), "{{" + key + "}}", key );
                                }
                        }
                        for ( const auto& key : mModel->variables().keys() )
                        {
                                combo->addItem( QIcon::fromTheme( "glabels-variables" ), "{{" + key + "}}", key );
                        }
                }

                bool any = mBcVariableCombo && mBcVariableCombo->count() > 0;
                QString tip = any ? QString() : tr("Connect a data source or add a custom variable first");
                for ( QToolButton* b : { mBcVariableButton, mImageVariableButton } )
                {
                        if ( b )
                        {
                                b->setToolTip( tip );
                        }
                }
        }


        ///
        /// Name of the variable if rawText is exactly one "${name}" reference
        ///
        QString ObjectEditor::singleVariableOf( const QString& rawText ) const
        {
                static const QRegularExpression re( "^\\$\\{([^{}:]+)\\}$" );
                auto m = re.match( rawText.trimmed() );
                return m.hasMatch() ? m.captured( 1 ) : QString();
        }


        void ObjectEditor::updateBarcodeModeUi( bool variableMode )
        {
                if ( !mBcStaticButton )
                {
                        return;
                }
                bool any = mBcVariableCombo->count() > 0;
                mBcStaticButton->setChecked( !variableMode );
                mBcVariableButton->setChecked( variableMode );
                mBcVariableButton->setEnabled( any || variableMode );
                mBcVariableRow->setVisible( variableMode );
                barcodeDataEdit->setVisible( !variableMode );
                barcodeInsertFieldButton->setVisible( !variableMode );

                if ( variableMode && mObject )
                {
                        QString name = singleVariableOf( mObject->bcData() );
                        int i = mBcVariableCombo->findData( name );
                        if ( i < 0 )
                        {
                                mBcVariableCombo->addItem( "{{" + name + "}}", name );   // unknown variable: keep showing it
                                i = mBcVariableCombo->count() - 1;
                        }
                        mBcVariableCombo->setCurrentIndex( i );
                }
        }


        void ObjectEditor::updateImageModeUi( bool variableMode )
        {
                if ( !mImageStaticButton )
                {
                        return;
                }
                bool any = mImageVariableCombo->count() > 0;
                mImageStaticButton->setChecked( !variableMode );
                mImageVariableButton->setChecked( variableMode );
                mImageVariableButton->setEnabled( any || variableMode );
                mImageVariableRow->setVisible( variableMode );
                imageFilenameLineEdit->setVisible( !variableMode );
                imageBrowseButton->setVisible( !variableMode );

                if ( variableMode && mObject )
                {
                        QString name = mObject->filenameNode().data();
                        int i = mImageVariableCombo->findData( name );
                        if ( i < 0 )
                        {
                                mImageVariableCombo->addItem( "{{" + name + "}}", name );
                                i = mImageVariableCombo->count() - 1;
                        }
                        mImageVariableCombo->setCurrentIndex( i );
                }
        }


        ///
        /// Barcode Static | Variable clicked
        ///
        void ObjectEditor::onBarcodeModeClicked()
        {
                if ( !mObject || mBlocked )
                {
                        return;
                }
                bool wantVariable = mBcVariableButton->isChecked();
                QString current = singleVariableOf( mObject->bcData() );

                if ( wantVariable && current.isEmpty() )
                {
                        if ( mBcVariableCombo->count() == 0 )
                        {
                                updateBarcodeModeUi( false );
                                return;
                        }
                        // Prefer a barcode-typed variable
                        int index = 0;
                        for ( int i = 0; i < mBcVariableCombo->count(); i++ )
                        {
                                if ( mModel->merge() && mModel->merge()->fieldType( mBcVariableCombo->itemData( i ).toString() )
                                     == merge::DataSourceUtil::TYPE_BARCODE )
                                {
                                        index = i;
                                        break;
                                }
                        }
                        mUndoRedoModel->checkpoint( tr("Barcode") );
                        mObject->setBcData( "${" + mBcVariableCombo->itemData( index ).toString() + "}" );
                }
                else if ( !wantVariable && !current.isEmpty() )
                {
                        // Become static, starting from the value of the first record
                        QString value;
                        if ( mModel->merge() && !mModel->merge()->recordList().isEmpty() )
                        {
                                value = mModel->merge()->recordList().first().value( current );
                        }
                        mUndoRedoModel->checkpoint( tr("Barcode") );
                        mObject->setBcData( value );
                }
                loadBarcodePage();
        }


        void ObjectEditor::onBarcodeVariableActivated( int index )
        {
                if ( !mObject || index < 0 )
                {
                        return;
                }
                QString name = mBcVariableCombo->itemData( index ).toString();
                if ( singleVariableOf( mObject->bcData() ) != name )
                {
                        mUndoRedoModel->checkpoint( tr("Barcode") );
                        mObject->setBcData( "${" + name + "}" );
                        loadBarcodePage();
                }
        }


        ///
        /// Image Static | Variable clicked
        ///
        void ObjectEditor::onImageModeClicked()
        {
                if ( !mObject || mBlocked )
                {
                        return;
                }
                bool wantVariable = mImageVariableButton->isChecked();
                bool isVariable = mObject->filenameNode().isField();

                if ( wantVariable && !isVariable )
                {
                        if ( mImageVariableCombo->count() == 0 )
                        {
                                updateImageModeUi( false );
                                return;
                        }
                        int index = 0;
                        for ( int i = 0; i < mImageVariableCombo->count(); i++ )
                        {
                                if ( mModel->merge() && mModel->merge()->fieldType( mImageVariableCombo->itemData( i ).toString() )
                                     == merge::DataSourceUtil::TYPE_IMAGE )
                                {
                                        index = i;
                                        break;
                                }
                        }
                        onImageKeySelected( mImageVariableCombo->itemData( index ).toString() );
                }
                else if ( !wantVariable && isVariable )
                {
                        mUndoRedoModel->checkpoint( tr("Set image") );
                        mObject->setFilenameNode( model::TextNode( false, "" ) );
                }
                loadImagePage();
        }


        void ObjectEditor::onImageVariableActivated( int index )
        {
                if ( !mObject || index < 0 )
                {
                        return;
                }
                QString name = mImageVariableCombo->itemData( index ).toString();
                if ( !( mObject->filenameNode().isField() && mObject->filenameNode().data() == name ) )
                {
                        onImageKeySelected( name );
                        loadImagePage();
                }
        }

} // namespace glabels
