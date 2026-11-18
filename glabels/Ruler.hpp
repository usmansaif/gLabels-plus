//  Ruler.hpp
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

#ifndef Ruler_hpp
#define Ruler_hpp


#include "model/Point.hpp"

#include <QWidget>


namespace glabels
{

        // Forward References
        class LabelEditor;


        ///
        /// Ruler Widget
        ///
        /// Displays a horizontal or vertical measurement scale aligned with the label
        /// shown in a LabelEditor, in the user's preferred units.
        ///
        class Ruler : public QWidget
        {
                Q_OBJECT

        public:
                Ruler( Qt::Orientation orientation, LabelEditor* editor, QWidget* parent = nullptr );

                QSize sizeHint() const override;
                QSize minimumSizeHint() const override;

        public slots:
                void setPointer( model::Point p );
                void clearPointer();

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private:
                Qt::Orientation mOrientation;
                LabelEditor*    mEditor;
                bool            mPointerValid{ false };
                model::Point    mPointer;
        };

}


#endif // Ruler_hpp
