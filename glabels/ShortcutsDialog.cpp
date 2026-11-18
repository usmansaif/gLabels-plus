//  ShortcutsDialog.cpp
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

#include "ShortcutsDialog.hpp"

#include "DialogUtil.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSettings>
#include <QVBoxLayout>


namespace
{
        const int ActionRole = Qt::UserRole + 1;
}


namespace glabels
{

        void ShortcutsDialog::registerAction( QAction* action, const QString& id )
        {
                action->setObjectName( id );
                action->setProperty( "defaultShortcut", QVariant::fromValue( action->shortcut() ) );

                QSettings settings;
                settings.beginGroup( "Shortcuts" );
                if ( settings.contains( id ) )
                {
                        action->setShortcut( QKeySequence::fromString( settings.value( id ).toString(),
                                                                       QKeySequence::PortableText ) );
                }
                settings.endGroup();
        }


        void ShortcutsDialog::applySaved( const QList<QAction*>& actions )
        {
                QSettings settings;
                settings.beginGroup( "Shortcuts" );
                for ( auto* action : actions )
                {
                        QString id = action->objectName();
                        if ( id.isEmpty() )
                        {
                                continue;
                        }
                        if ( settings.contains( id ) )
                        {
                                action->setShortcut( QKeySequence::fromString( settings.value( id ).toString(),
                                                                               QKeySequence::PortableText ) );
                        }
                        else
                        {
                                action->setShortcut( action->property( "defaultShortcut" ).value<QKeySequence>() );
                        }
                }
                settings.endGroup();
        }


        QString ShortcutsDialog::cleanText( const QAction* action )
        {
                QString text = action->text();
                text.remove( "..." );
                text.remove( QChar( 0x2026 ) );
                // Remove mnemonic markers, but keep literal "&&"
                text.replace( "&&", QChar( 0x1 ) );
                text.remove( '&' );
                text.replace( QChar( 0x1 ), "&" );
                return text.trimmed();
        }


        ShortcutsDialog::ShortcutsDialog( const QList<QAction*>& actions, QWidget* parent )
                : QDialog(parent), mActions(actions)
        {
                setWindowTitle( tr("Keyboard Shortcuts") );
                DialogUtil::makeResizableWindow( this );
                resize( 560, 520 );

                for ( auto* action : mActions )
                {
                        mPending[action] = action->shortcut();
                }

                auto* intro = new QLabel( tr("Select a command, then press the new key combination. "
                                             "Changes take effect when you click OK.") );
                intro->setWordWrap( true );

                mFilterEdit = new QLineEdit();
                mFilterEdit->setPlaceholderText( tr("Search commands or shortcuts...") );
                mFilterEdit->setClearButtonEnabled( true );

                mTree = new QTreeWidget();
                mTree->setColumnCount( 2 );
                mTree->setHeaderLabels( { tr("Command"), tr("Shortcut") } );
                mTree->setRootIsDecorated( false );
                mTree->setUniformRowHeights( true );
                mTree->setSortingEnabled( true );
                mTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
                mTree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );

                for ( auto* action : mActions )
                {
                        auto* item = new QTreeWidgetItem();
                        item->setText( 0, cleanText( action ) );
                        item->setIcon( 0, action->icon() );
                        item->setData( 0, ActionRole, QVariant::fromValue( static_cast<QObject*>( action ) ) );
                        if ( !action->statusTip().isEmpty() )
                        {
                                item->setToolTip( 0, action->statusTip() );
                        }
                        mTree->addTopLevelItem( item );
                        refreshItem( item );
                }
                mTree->sortItems( 0, Qt::AscendingOrder );

                mSequenceEdit = new QKeySequenceEdit();
#if QT_VERSION >= QT_VERSION_CHECK(6,4,0)
                mSequenceEdit->setMaximumSequenceLength( 1 );
                mSequenceEdit->setClearButtonEnabled( true );
#endif
                mClearButton = new QPushButton( tr("Clear") );
                mClearButton->setToolTip( tr("Remove the shortcut from this command") );
                mResetButton = new QPushButton( tr("Default") );
                mResetButton->setToolTip( tr("Restore this command's original shortcut") );

                auto* editRow = new QHBoxLayout();
                editRow->addWidget( new QLabel( tr("Shortcut:") ) );
                editRow->addWidget( mSequenceEdit, 1 );
                editRow->addWidget( mClearButton );
                editRow->addWidget( mResetButton );

                mConflictLabel = new QLabel();
                mConflictLabel->setWordWrap( true );
                mConflictLabel->setStyleSheet( "color: #c4320a;" );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults );
                buttons->button( QDialogButtonBox::RestoreDefaults )->setText( tr("Reset All") );

                auto* layout = new QVBoxLayout();
                layout->addWidget( intro );
                layout->addWidget( mFilterEdit );
                layout->addWidget( mTree, 1 );
                layout->addLayout( editRow );
                layout->addWidget( mConflictLabel );
                layout->addWidget( buttons );
                setLayout( layout );

                connect( mFilterEdit, &QLineEdit::textChanged, this, &ShortcutsDialog::onFilterChanged );
                connect( mTree, &QTreeWidget::currentItemChanged, this, &ShortcutsDialog::onCurrentItemChanged );
                connect( mSequenceEdit, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutsDialog::onKeySequenceChanged );
                connect( mClearButton, &QPushButton::clicked, this, &ShortcutsDialog::onClearClicked );
                connect( mResetButton, &QPushButton::clicked, this, &ShortcutsDialog::onResetClicked );
                connect( buttons->button( QDialogButtonBox::RestoreDefaults ), &QPushButton::clicked,
                         this, &ShortcutsDialog::onResetAllClicked );
                connect( buttons, &QDialogButtonBox::accepted, this, &ShortcutsDialog::onAccepted );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

                if ( mTree->topLevelItemCount() )
                {
                        mTree->setCurrentItem( mTree->topLevelItem( 0 ) );
                }
                onCurrentItemChanged();
        }


        QTreeWidgetItem* ShortcutsDialog::itemForAction( QAction* action ) const
        {
                for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
                {
                        auto* item = mTree->topLevelItem( i );
                        if ( item->data( 0, ActionRole ).value<QObject*>() == action )
                        {
                                return item;
                        }
                }
                return nullptr;
        }


        void ShortcutsDialog::refreshItem( QTreeWidgetItem* item )
        {
                auto* action = qobject_cast<QAction*>( item->data( 0, ActionRole ).value<QObject*>() );
                if ( !action )
                {
                        return;
                }
                QKeySequence seq = mPending.value( action );
                item->setText( 1, seq.toString( QKeySequence::NativeText ) );

                QFont f = item->font( 1 );
                f.setBold( seq != action->property( "defaultShortcut" ).value<QKeySequence>() );
                item->setFont( 1, f );
        }


        void ShortcutsDialog::onCurrentItemChanged()
        {
                auto* item = mTree->currentItem();
                bool has = item != nullptr;
                mSequenceEdit->setEnabled( has );
                mClearButton->setEnabled( has );
                mResetButton->setEnabled( has );

                mBlocked = true;
                if ( has )
                {
                        auto* action = qobject_cast<QAction*>( item->data( 0, ActionRole ).value<QObject*>() );
                        mSequenceEdit->setKeySequence( mPending.value( action ) );
                }
                else
                {
                        mSequenceEdit->clear();
                }
                mBlocked = false;

                updateConflictLabel();
        }


        void ShortcutsDialog::onKeySequenceChanged()
        {
                if ( mBlocked )
                {
                        return;
                }
                auto* item = mTree->currentItem();
                if ( !item )
                {
                        return;
                }
                auto* action = qobject_cast<QAction*>( item->data( 0, ActionRole ).value<QObject*>() );
                mPending[action] = mSequenceEdit->keySequence();
                refreshItem( item );
                updateConflictLabel();
        }


        void ShortcutsDialog::onClearClicked()
        {
                mSequenceEdit->clear();
                onKeySequenceChanged();
        }


        void ShortcutsDialog::onResetClicked()
        {
                auto* item = mTree->currentItem();
                if ( !item )
                {
                        return;
                }
                auto* action = qobject_cast<QAction*>( item->data( 0, ActionRole ).value<QObject*>() );
                mSequenceEdit->setKeySequence( action->property( "defaultShortcut" ).value<QKeySequence>() );
                onKeySequenceChanged();
        }


        void ShortcutsDialog::onResetAllClicked()
        {
                for ( auto* action : mActions )
                {
                        mPending[action] = action->property( "defaultShortcut" ).value<QKeySequence>();
                        if ( auto* item = itemForAction( action ) )
                        {
                                refreshItem( item );
                        }
                }
                onCurrentItemChanged();
        }


        void ShortcutsDialog::onFilterChanged( const QString& text )
        {
                for ( int i = 0; i < mTree->topLevelItemCount(); i++ )
                {
                        auto* item = mTree->topLevelItem( i );
                        bool match = text.isEmpty() ||
                                item->text( 0 ).contains( text, Qt::CaseInsensitive ) ||
                                item->text( 1 ).contains( text, Qt::CaseInsensitive );
                        item->setHidden( !match );
                }
        }


        void ShortcutsDialog::updateConflictLabel()
        {
                mConflictLabel->clear();

                auto* item = mTree->currentItem();
                if ( !item )
                {
                        return;
                }
                auto* action = qobject_cast<QAction*>( item->data( 0, ActionRole ).value<QObject*>() );
                QKeySequence seq = mPending.value( action );
                if ( seq.isEmpty() )
                {
                        return;
                }

                QStringList conflicts;
                for ( auto* other : mActions )
                {
                        if ( other != action && mPending.value( other ) == seq )
                        {
                                conflicts << cleanText( other );
                        }
                }
                if ( !conflicts.isEmpty() )
                {
                        mConflictLabel->setText( tr("Warning: %1 is also used by: %2")
                                                 .arg( seq.toString( QKeySequence::NativeText ), conflicts.join( ", " ) ) );
                }
        }


        void ShortcutsDialog::onAccepted()
        {
                QSettings settings;
                settings.beginGroup( "Shortcuts" );
                for ( auto* action : mActions )
                {
                        QKeySequence seq = mPending.value( action );
                        QKeySequence def = action->property( "defaultShortcut" ).value<QKeySequence>();
                        if ( seq == def )
                        {
                                settings.remove( action->objectName() );
                        }
                        else
                        {
                                settings.setValue( action->objectName(), seq.toString( QKeySequence::PortableText ) );
                        }
                        action->setShortcut( seq );
                }
                settings.endGroup();

                accept();
        }

}
