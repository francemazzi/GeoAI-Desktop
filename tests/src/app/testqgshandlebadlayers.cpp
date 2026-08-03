/***************************************************************************
  testqgshandlebadlayers.cpp
  --------------------------------
  begin                : August 2026
***************************************************************************/

#include "qgshandlebadlayers.h"
#include "qgsproject.h"
#include "qgstest.h"
#include "qgsvectorlayer.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QTableWidget>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

class TestQgsHandleBadLayers : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void autoSuggestRepairsPathFromSiblingFolder();

  private:
    //! Builds a <maplayer> DOM node as found in a project file with a broken datasource.
    static QDomNode makeBadLayerNode( QDomDocument &doc, const QString &layerId, const QString &name, const QString &datasource );
};

void TestQgsHandleBadLayers::initTestCase()
{
  QgsApplication::initQgis();
}

void TestQgsHandleBadLayers::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsHandleBadLayers::cleanup()
{
  QgsProject::instance()->clear();
}

QDomNode TestQgsHandleBadLayers::makeBadLayerNode( QDomDocument &doc, const QString &layerId, const QString &name, const QString &datasource )
{
  QDomElement layerElement = doc.createElement( u"maplayer"_s );
  layerElement.setAttribute( u"type"_s, u"vector"_s );

  QDomElement idElement = doc.createElement( u"id"_s );
  idElement.appendChild( doc.createTextNode( layerId ) );
  layerElement.appendChild( idElement );

  QDomElement datasourceElement = doc.createElement( u"datasource"_s );
  datasourceElement.appendChild( doc.createTextNode( datasource ) );
  layerElement.appendChild( datasourceElement );

  QDomElement nameElement = doc.createElement( u"layername"_s );
  nameElement.appendChild( doc.createTextNode( name ) );
  layerElement.appendChild( nameElement );

  QDomElement providerElement = doc.createElement( u"provider"_s );
  providerElement.appendChild( doc.createTextNode( u"ogr"_s ) );
  layerElement.appendChild( providerElement );

  doc.appendChild( layerElement );
  return layerElement;
}

void TestQgsHandleBadLayers::autoSuggestRepairsPathFromSiblingFolder()
{
  // layout: parent/project/ holds the project, parent/DBT SHAPE/ holds the data;
  // the project references the broken sibling-relative layout ./SHAPE/points.shp
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  const QDir parentDir( tempDir.path() );
  QVERIFY( parentDir.mkdir( u"project"_s ) );
  QVERIFY( parentDir.mkdir( u"DBT SHAPE"_s ) );

  const QString dataDir = parentDir.filePath( u"DBT SHAPE"_s );
  const QString source = QStringLiteral( TEST_DATA_DIR );
  for ( const QString &extension : { u"shp"_s, u"shx"_s, u"dbf"_s, u"prj"_s } )
    QFile::copy( source + u"/points."_s + extension, dataDir + u"/points."_s + extension );

  const QString projectDir = parentDir.filePath( u"project"_s );
  QgsProject::instance()->setFileName( projectDir + u"/test.qgs"_s );

  const QString brokenPath = projectDir + u"/SHAPE/points.shp"_s;

  // the broken layer is kept in the project as an invalid placeholder, as during project load
  QgsVectorLayer *badLayer = new QgsVectorLayer( brokenPath, u"points"_s, u"ogr"_s );
  QVERIFY( !badLayer->isValid() );
  QgsProject::instance()->addMapLayer( badLayer );

  QDomDocument doc;
  const QList<QDomNode> badNodes { makeBadLayerNode( doc, badLayer->id(), u"points"_s, brokenPath ) };

  QgsHandleBadLayers dialog( badNodes );

  // the constructor's sibling pass finds parent/DBT SHAPE/points.shp and repairs the live layer
  QCOMPARE( dialog.layerCount(), 1 );
  QCOMPARE( dialog.unrepairedLayerCount(), 0 );
  QVERIFY( badLayer->isValid() );
  QVERIFY( badLayer->source().contains( u"DBT SHAPE"_s ) );
}

QGSTEST_MAIN( TestQgsHandleBadLayers )
#include "testqgshandlebadlayers.moc"
