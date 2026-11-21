//  ObjectEditor.hpp
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

#ifndef ObjectEditor_hpp
#define ObjectEditor_hpp


#include "ui_ObjectEditor.h"

#include "model/Model.hpp"
#include "model/ModelObject.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QToolButton>


namespace glabels
{

        // Forward references
        class UndoRedoModel;


        ///
        /// Object Editor Widget
        ///
        class ObjectEditor : public QWidget, public Ui_ObjectEditor
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                ObjectEditor( QWidget *parent = nullptr );


                /////////////////////////////////
                // Public methods
                /////////////////////////////////
                void setModel( model::Model* model, UndoRedoModel* undoRedoModel );


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                QString unitsSuffix() const;
                void hidePages();
                void loadImagePage();
                void loadLineFillPage();
                void loadPositionPage();
                void loadRectSizePage();
                void loadLineSizePage();
                void loadTextPage();
                void updateTextPathControls();
                void loadBarcodePage();
                void loadShadowPage();

                void setupVariableModeControls();
                void fillVariableCombos();
                void updateBarcodeModeUi( bool variableMode );
                void updateImageModeUi( bool variableMode );
                QString singleVariableOf( const QString& rawText ) const;


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onSettingsChanged();
                void onLabelSizeChanged();
                void onSelectionChanged();
                void onFieldsAvailableChanged();
                void onObjectChanged();
                void onObjectMoved();
                void onObjectDestroyed();
                void onLineControlsChanged();
                void onFillControlsChanged();
                void onImageFileButtonClicked();
                void onImageKeySelected( QString key );
                void onPositionControlsChanged();
                void onRectSizeControlsChanged();
                void onLineSizeControlsChanged();
                void onTextControlsChanged();
                void onTextInsertFieldKeySelected( QString key );
                void onBarcodeControlsChanged();
                void onBarcodeInsertFieldKeySelected( QString key );
                void onResetImageSize();
                void onShadowControlsChanged();
                void onChanged();
                void onBarcodeModeClicked();
                void onBarcodeVariableActivated( int index );
                void onImageModeClicked();
                void onImageVariableActivated( int index );


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                model::Model*            mModel;
                model::ModelObject*      mObject;
                UndoRedoModel*           mUndoRedoModel;

                model::Units             mUnits;
                int                      mSpinDigits;
                double                   mSpinStep;

                QButtonGroup*            textHAlignGroup;
                QButtonGroup*            textVAlignGroup;

                QString                  mImageCwd;

                bool                     mBlocked;

                // Static | Variable switches for barcode data and image file
                QToolButton*             mBcStaticButton{ nullptr };
                QToolButton*             mBcVariableButton{ nullptr };
                QComboBox*               mBcVariableCombo{ nullptr };
                QWidget*                 mBcVariableRow{ nullptr };
                QToolButton*             mImageStaticButton{ nullptr };
                QToolButton*             mImageVariableButton{ nullptr };
                QComboBox*               mImageVariableCombo{ nullptr };
                QWidget*                 mImageVariableRow{ nullptr };

        };

}


#endif // ObjectEditor_hpp
