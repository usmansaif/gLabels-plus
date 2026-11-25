//  SymbolsDialog.hpp
//
//  Copyright (C) 2026  gLabels Plus contributors
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

#ifndef SymbolsDialog_hpp
#define SymbolsDialog_hpp


#include <QDialog>

class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;


namespace glabels
{

        ///
        /// Tools > Insert Symbol
        ///
        /// A non-modal picker for the built-in symbol set (glabels/symbols): solid
        /// black drawings for handling marks, food, contact details, shop and
        /// delivery, and simple marks and arrows, made to print sharply on direct
        /// thermal label printers.  Insert adds the chosen symbol to the label and
        /// the dialog stays open until the user closes it.  Users can add their own
        /// SVG drawings (Add Your Own, or drop .svg files on the window); these are
        /// validated and kept on the computer by CustomSymbols.
        ///
        class SymbolsDialog : public QDialog
        {
                Q_OBJECT

        public:
                struct Symbol
                {
                        QString id;          // e.g. "fragile"; resource ":/symbols/<id>.svg"
                        QString name;        // translated
                        QString category;    // translated
                        QString keywords;
                        QString path;        // ":/symbols/<id>.svg", or the stored file of a custom symbol
                        bool    custom = false;
                };

                explicit SymbolsDialog( QWidget* parent = nullptr );

                /// The built-in symbols
                static QList<Symbol> symbols();

                /// The built-in symbols followed by the user's own (see CustomSymbols)
                static QList<Symbol> allSymbols();

        protected:
                void dragEnterEvent( QDragEnterEvent* event ) override;
                void dropEvent( QDropEvent* event ) override;

        signals:
                /// Insert the symbol: svg is the drawing, size its width and height in points
                void insertRequested( const QString& id, const QString& name, const QByteArray& svg, double size );

        private slots:
                void onFilterChanged();
                void onSelectionChanged();
                void onInsert();
                void onAddCustom();
                void onRemoveCustom();

        private:
                void addCustomFromFile( const QString& fileName );
                void populate( const QString& selectId, const QString& showCategory = QString() );
                const Symbol* selectedSymbol() const;

                QList<Symbol>   mSymbols;
                QLineEdit*      mSearch;
                QComboBox*      mCategory;
                QListWidget*    mList;
                QDoubleSpinBox* mSize;
                QLabel*         mStatus;
                QPushButton*    mInsertButton;
                QPushButton*    mRemoveButton;
        };

}


#endif // SymbolsDialog_hpp
