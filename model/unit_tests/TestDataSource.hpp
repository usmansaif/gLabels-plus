//  TestDataSource.hpp
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

#include <QtTest/QtTest>


class TestDataSource : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();
        void detectDelimiter_data();
        void detectDelimiter();
        void autoBackend();
        void bomAndLatin1();
        void json();
        void fieldMapping();
        void suggestName_data();
        void suggestName();
        void inferType();
        void displayConversion();
        void renameReferences();
        void modelRename();
        void xmlRoundTrip();
};
