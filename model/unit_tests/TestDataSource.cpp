//  TestDataSource.cpp
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

#include "TestDataSource.hpp"

#include "barcode/Backends.hpp"
#include "merge/DataSourceUtil.hpp"
#include "merge/Factory.hpp"
#include "merge/Merge.hpp"
#include "merge/Text.hpp"

#include "model/Db.hpp"
#include "model/FrameRect.hpp"
#include "model/Template.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/TextNode.hpp"
#include "model/Version.hpp"
#include "model/XmlLabelCreator.hpp"
#include "model/XmlLabelParser.hpp"

#include <QTemporaryDir>

#include <memory>


QTEST_MAIN(TestDataSource)

using namespace glabels;
using namespace glabels::merge;


namespace
{
        QString writeFile( const QTemporaryDir& dir, const QString& name, const QByteArray& content )
        {
                QString path = dir.filePath( name );
                QFile file( path );
                file.open( QIODevice::WriteOnly );
                file.write( content );
                file.close();
                return path;
        }
}


void TestDataSource::initTestCase()
{
        QCoreApplication::setOrganizationName( model::Version::ORGANIZATION_NAME );
        model::Settings::init();
        model::Db::init();
        Factory::init();
        barcode::Backends::init();
}


void TestDataSource::detectDelimiter_data()
{
        QTest::addColumn<QByteArray>( "sample" );
        QTest::addColumn<QChar>( "expected" );

        QTest::newRow( "comma" )     << QByteArray( "a,b,c\n1,2,3\n4,5,6\n" ) << QChar( ',' );
        QTest::newRow( "tab" )       << QByteArray( "a\tb\tc\n1\t2\t3\n" ) << QChar( '\t' );
        QTest::newRow( "semicolon" ) << QByteArray( "name;price\nTea;1,50\nCoffee;2,75\n" ) << QChar( ';' );
        QTest::newRow( "pipe" )      << QByteArray( "a|b\n1|2\n" ) << QChar( '|' );
        QTest::newRow( "quoted" )    << QByteArray( "\"a;x\",b\n\"1;2;3\",2\n" ) << QChar( ',' );
        QTest::newRow( "single" )    << QByteArray( "name\nTea\n" ) << QChar( ',' );
}


void TestDataSource::detectDelimiter()
{
        QFETCH( QByteArray, sample );
        QFETCH( QChar, expected );
        QCOMPARE( Text::detectDelimiter( sample ), expected );
}


void TestDataSource::autoBackend()
{
        QTemporaryDir dir;
        QString fn = writeFile( dir, "data.tsv", "prod_sku_01\tName\nSKU-1\tTea\nSKU-2\tCoffee\n" );

        std::unique_ptr<Merge> merge( Factory::createMerge( "Text/Auto/Line1Keys" ) );
        merge->setSource( fn );
        QCOMPARE( merge->keys(), QStringList( { "prod_sku_01", "Name" } ) );
        QCOMPARE( merge->recordList().size(), 2 );
        QCOMPARE( merge->recordList()[1].value( "Name" ), QString( "Coffee" ) );
        QCOMPARE( dynamic_cast<Text*>( merge.get() )->delimiter(), QChar( '\t' ) );
}


void TestDataSource::bomAndLatin1()
{
        QTemporaryDir dir;

        QString utf8 = writeFile( dir, "bom.csv", "\xEF\xBB\xBFname,city\nAli,Lahore\n" );
        std::unique_ptr<Merge> m1( Factory::createMerge( "Text/Auto/Line1Keys" ) );
        m1->setSource( utf8 );
        QCOMPARE( m1->keys().first(), QString( "name" ) );   // BOM stripped

        QString latin1 = writeFile( dir, "latin1.csv", "name\nJos\xE9\n" );
        std::unique_ptr<Merge> m2( Factory::createMerge( "Text/Auto/Line1Keys" ) );
        m2->setSource( latin1 );
        QCOMPARE( m2->recordList()[0].value( "name" ), QString::fromUtf8( "Jos\xC3\xA9" ) );
}


void TestDataSource::json()
{
        QTemporaryDir dir;
        QString fn = writeFile( dir, "data.json",
                                "{ \"records\": [ { \"sku\": \"A1\", \"price\": 12.5, \"qty\": 3, \"ok\": true },"
                                "                 { \"sku\": \"B2\", \"extra\": null } ] }" );

        std::unique_ptr<Merge> merge( Factory::createMerge( "Json" ) );
        merge->setSource( fn );
        QCOMPARE( merge->keys(), QStringList( { "sku", "price", "qty", "ok", "extra" } ) );   // file order
        QCOMPARE( merge->recordList().size(), 2 );
        QCOMPARE( merge->recordList()[0].value( "price" ), QString( "12.5" ) );
        QCOMPARE( merge->recordList()[0].value( "qty" ), QString( "3" ) );
        QCOMPARE( merge->recordList()[0].value( "ok" ), QString( "true" ) );
        QCOMPARE( merge->recordList()[1].value( "extra" ), QString() );
}


void TestDataSource::fieldMapping()
{
        QTemporaryDir dir;
        QString fn = writeFile( dir, "data.csv", "prod_sku_01,name,secret\nS1,Tea,x\nS2,Coffee,y\n" );

        std::unique_ptr<Merge> merge( Factory::createMerge( "Text/Comma/Line1Keys" ) );
        merge->setSource( fn );
        merge->setSelected( 1, false );

        QList<Field> fields = DataSourceUtil::defaultFields( merge.get() );
        QCOMPARE( fields.size(), 3 );
        QCOMPARE( fields[0].name, QString( "Product_SKU_01" ) );
        QCOMPARE( fields[0].type, DataSourceUtil::TYPE_BARCODE );
        fields[2].include = false;
        merge->setFields( fields );

        QCOMPARE( merge->keys(), QStringList( { "Product_SKU_01", "Name" } ) );
        QCOMPARE( merge->sourceColumns(), QStringList( { "prod_sku_01", "name", "secret" } ) );
        QCOMPARE( merge->recordList()[0].value( "Product_SKU_01" ), QString( "S1" ) );
        QVERIFY( !merge->recordList()[0].contains( "secret" ) );
        QCOMPARE( merge->fieldType( "Product_SKU_01" ), DataSourceUtil::TYPE_BARCODE );

        // Selection survives a mapping change and a clone
        QCOMPARE( merge->nSelectedRecords(), 1 );
        std::unique_ptr<Merge> clone( merge->clone() );
        QCOMPARE( clone->keys(), merge->keys() );
        QCOMPARE( clone->unselectedIndices(), QList<int>( { 1 } ) );
}


void TestDataSource::suggestName_data()
{
        QTest::addColumn<QString>( "column" );
        QTest::addColumn<QString>( "expected" );

        QTest::newRow( "snake" )   << "prod_sku_01" << "Product_SKU_01";
        QTest::newRow( "spaces" )  << "Product name" << "Product_Name";
        QTest::newRow( "camel" )   << "unitPrice" << "Unit_Price";
        QTest::newRow( "symbols" ) << "Price (PKR)" << "Price_Pkr";
        QTest::newRow( "digit" )   << "1" << "Column_1";
        QTest::newRow( "empty" )   << "" << "Column_4";
}


void TestDataSource::suggestName()
{
        QFETCH( QString, column );
        QFETCH( QString, expected );
        QCOMPARE( DataSourceUtil::suggestName( column, 3 ), expected );
        QVERIFY( DataSourceUtil::isValidName( expected ) );
}


void TestDataSource::inferType()
{
        QCOMPARE( DataSourceUtil::inferType( "photo", { "a.png", "b.JPG" } ), DataSourceUtil::TYPE_IMAGE );
        QCOMPARE( DataSourceUtil::inferType( "file", { "img/a.png" } ), DataSourceUtil::TYPE_IMAGE );
        QCOMPARE( DataSourceUtil::inferType( "ean", { "123" } ), DataSourceUtil::TYPE_BARCODE );
        QCOMPARE( DataSourceUtil::inferType( "code", { "5012345678900", "4006381333931" } ), DataSourceUtil::TYPE_BARCODE );
        QCOMPARE( DataSourceUtil::inferType( "price", { "1,250.50", "99" } ), DataSourceUtil::TYPE_NUMBER );
        QCOMPARE( DataSourceUtil::inferType( "name", { "Tea", "Coffee" } ), DataSourceUtil::TYPE_TEXT );
        QCOMPARE( DataSourceUtil::makeUnique( { "Name", "name", "Name" } ), QStringList( { "Name", "name_2", "Name_3" } ) );
        QVERIFY( !DataSourceUtil::isValidName( "bad name" ) );
        QVERIFY( !DataSourceUtil::isValidName( "" ) );
}


void TestDataSource::displayConversion()
{
        QString raw = "Price: ${Price:%.2f} for ${Name}";
        QString display = DataSourceUtil::toDisplay( raw );
        QCOMPARE( display, QString( "Price: {{Price:%.2f}} for {{Name}}" ) );
        QCOMPARE( DataSourceUtil::fromDisplay( display ), raw );
        QCOMPARE( DataSourceUtil::fromDisplay( "{{A}}{{B}}" ), QString( "${A}${B}" ) );
}


void TestDataSource::renameReferences()
{
        QCOMPARE( DataSourceUtil::renameReferences( "${Old} ${Older} ${Old:=x}", "Old", "New" ),
                  QString( "${New} ${Older} ${New:=x}" ) );
}


void TestDataSource::modelRename()
{
        model::Model label;

        auto* text = new model::ModelTextObject();
        text->setText( "Name: ${Old}" );
        label.addObject( text );

        auto* barcode = new model::ModelBarcodeObject();
        barcode->setBcData( "${Old}" );
        label.addObject( barcode );

        auto* image = new model::ModelImageObject();
        image->setFilenameNode( model::TextNode( true, "Old" ) );
        label.addObject( image );

        QCOMPARE( label.renameVariableReferences( "Old", "New" ), 3 );
        QCOMPARE( text->text(), QString( "Name: ${New}" ) );
        QCOMPARE( barcode->bcData(), QString( "${New}" ) );
        QCOMPARE( image->filenameNode().data(), QString( "New" ) );
}


void TestDataSource::xmlRoundTrip()
{
        QTemporaryDir dir;
        QString csv = writeFile( dir, "data.csv", "prod_sku,name\nS1,Tea\nS2,Coffee\nS3,Milk\n" );

        auto* label = new model::Model();
        model::Template tmplate( "Test Brand", "part", "desc", "testPaperId", 110, 410 );
        model::FrameRect frame( 120, 220, 5, 0, 0, "rect1" );
        tmplate.addFrame( frame );
        label->setTmplate( tmplate );

        Merge* merge = Factory::createMerge( "Text/Auto/Line1Keys" );
        merge->setSource( csv );
        QList<Field> fields = DataSourceUtil::defaultFields( merge );
        fields[1].name = "Product_Name";
        merge->setFields( fields );
        merge->setSelected( 2, false );
        label->setMerge( merge );

        QString fn = dir.filePath( "label.glabels" );
        model::XmlLabelCreator::writeFile( label, fn );

        std::unique_ptr<model::Model> read( model::XmlLabelParser::readFile( fn ) );
        QVERIFY( read );
        QCOMPARE( read->merge()->id(), QString( "Text/Auto/Line1Keys" ) );
        QCOMPARE( read->merge()->fields(), fields );
        QCOMPARE( read->merge()->keys(), QStringList( { "Product_SKU", "Product_Name" } ) );
        QCOMPARE( read->merge()->unselectedIndices(), QList<int>( { 2 } ) );
        QCOMPARE( read->merge()->recordList()[0].value( "Product_Name" ), QString( "Tea" ) );

        delete label;
}
