//  MergeView.cpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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


#include "MergeView.hpp"

#include "UndoRedoModel.hpp"

#include <QLabel>
#include <QPushButton>

#include "MergeTableModel.hpp"

#include "merge/Factory.hpp"

#include "model/FileUtil.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>


namespace glabels
{
        const QChar NEWLINE_CHAR = QChar(0x23CE);

        ///
        /// Constructor
        ///
        MergeView::MergeView( QWidget *parent )
                : QWidget(parent), mModel(nullptr), mUndoRedoModel(nullptr), mOldFormatComboIndex(0)
        {
                setupUi( this );

                titleLabel->setText( QString( "<span style='font-size:18pt;'>%1</span>" ).arg( tr("Data Source") ) );

                mMergeFormatNames = merge::Factory::nameList();
                formatCombo->addItems( mMergeFormatNames );

                // The format is chosen (or detected) in the Connect Data Source dialog
                label->setVisible( false );
                formatCombo->setVisible( false );
                groupBox->setTitle( tr("Data file") );
                groupBox_2->setTitle( tr("Records to print") );
                locationLabel->setText( tr("File:") );
                locationLineEdit->setPlaceholderText( tr("No data source connected") );

                auto* disconnectButton = new QPushButton( tr("Disconnect") );
                disconnectButton->setObjectName( "disconnectButton" );
                disconnectButton->setToolTip( tr("Stop using a data source for this label") );
                horizontalLayout_2->addWidget( disconnectButton );
                connect( disconnectButton, &QPushButton::clicked, this, [this]{
                        if ( mModel && mUndoRedoModel )
                        {
                                mUndoRedoModel->checkpoint( tr("Disconnect Data Source") );
                                mModel->setMerge( merge::Factory::createMerge( "None" ) );
                        }
                } );

                mCountLabel = new QLabel();
                horizontalLayout->insertWidget( 3, mCountLabel );
        }


        ///
        /// Set Model
        ///
        void MergeView::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                mModel = model;
                mUndoRedoModel = undoRedoModel;

                // Initialize CWD
                mCwd = mModel->dirPath();

                onMergeChanged();
                connect( mModel, SIGNAL(mergeChanged()), this, SLOT(onMergeChanged()) );
        }


        ///
        /// Merge changed handler
        ///
        void MergeView::onMergeChanged()
        {
                QString name = merge::Factory::idToName( mModel->merge()->id() );
                int index = mMergeFormatNames.indexOf( name );
                mOldFormatComboIndex = index;
                formatCombo->setCurrentIndex( index );

                QString fn;

                switch ( merge::Factory::idToType( mModel->merge()->id() ) )
                {
                case merge::Factory::NONE:
                case merge::Factory::FIXED:
                        locationLabel->setEnabled( false );
                        locationLineEdit->setText( "" );
                        locationBrowseButton->setText( tr("Connect Data Source...") );
                        findChild<QPushButton*>( "disconnectButton" )->setVisible( false );
                        break;

                case merge::Factory::FILE:
                        locationLabel->setEnabled( true );
                        fn = model::FileUtil::makeRelativeIfInDir( mModel->dir(), mModel->merge()->source() );
                        locationLineEdit->setText( fn );
                        locationBrowseButton->setText( tr("Change File or Variables...") );
                        findChild<QPushButton*>( "disconnectButton" )->setVisible( true );
                        break;

                default:
                        qWarning( "MergeView::onMergeChanged()::Should not be reached!" );
                        break;
                }

                recordsTableView->setModel( new MergeTableModel( mModel->merge() ) );
                recordsTableView->resizeColumnsToContents();
                updateCount();

                connect( mModel->merge(), SIGNAL(sourceChanged()),
                         this, SLOT(onMergeSourceChanged()) );
                connect( mModel->merge(), &merge::Merge::selectionChanged, this, &MergeView::updateCount );
        }


        ///
        /// Merge source changed handler
        ///
        void MergeView::onMergeSourceChanged()
        {
                QString fn = model::FileUtil::makeRelativeIfInDir( mModel->dir(), mModel->merge()->source() );
                locationLineEdit->setText( fn );

                recordsTableView->setModel( new MergeTableModel( mModel->merge() ) );
                recordsTableView->resizeColumnsToContents();
                updateCount();
        }


        ///
        /// Selected record count
        ///
        void MergeView::updateCount()
        {
                if ( !mModel || !mModel->merge() || mModel->merge()->recordList().isEmpty() )
                {
                        mCountLabel->clear();
                        return;
                }
                mCountLabel->setText( tr("%1 of %2 records selected")
                                      .arg( mModel->merge()->nSelectedRecords() )
                                      .arg( mModel->merge()->recordList().size() ) );
        }


        ///
        /// Format combo changed handler
        ///
        void MergeView::onFormatComboActivated()
        {
                int index = formatCombo->currentIndex();
                if ( index != mOldFormatComboIndex )
                {
                        mOldFormatComboIndex = index;

                        QString id = merge::Factory::indexToId(index);
                        mModel->setMerge( merge::Factory::createMerge( id ) );
                }
        }


        ///
        /// Location button clicked handler
        ///
        void MergeView::onLocationBrowseButtonClicked()
        {
                emit connectRequested();
        }


        ///
        /// Select all button clicked handler
        ///
        void MergeView::onSelectAllButtonClicked()
        {
                mModel->merge()->selectAll();
        }


        ///
        /// Unselect all button clicked handler
        ///
        void MergeView::onUnselectAllButtonClicked()
        {
                mModel->merge()->unselectAll();
        }


        ///
        /// Reload button clicked handler
        ///
        void MergeView::onReloadButtonClicked()
        {
                mModel->merge()->reloadSource();
        }


        ///
        /// modify text to be printable e.g. replace newlines
        ///
        QString MergeView::printableTextForView( QString text )
        {
                // Replace windows style newlines
                text.replace("\r\n", NEWLINE_CHAR);

                // Replace unix style newlines
                text.replace("\n", NEWLINE_CHAR);

                return text;
        }


} // namespace glabels
