/***************************************************************************
  testqgsbatchedlayeraddcontroller.cpp
  --------------------------------
  begin                : August 2026
***************************************************************************/

#include "layers/qgsbatchedlayeraddcontroller.h"
#include "qgsapplication.h"
#include "qgslayertree.h"
#include "qgsproject.h"
#include "qgsprovidersublayerdetails.h"
#include "qgssettingsentryimpl.h"
#include "qgstaskmanager.h"
#include "qgstest.h"

#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

class TestQgsBatchedLayerAddController : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void addsQueuedLayersInBatches();
    void cancelKeepsAlreadyAddedLayers();
    void addsLayersIntoASharedGroup();
    void unchecksHeavyLayers();

  private:
    //! Builds \a count sublayer details all pointing at a copied points.shp fixture.
    QList<QgsProviderSublayerDetails> makeShapefileDetails( int count );

    std::unique_ptr<QTemporaryDir> mTempDir;
    QString mShapefilePath;
};

void TestQgsBatchedLayerAddController::initTestCase()
{
  QgsApplication::initQgis();

  mTempDir = std::make_unique<QTemporaryDir>();
  QVERIFY( mTempDir->isValid() );
  const QString source = QStringLiteral( TEST_DATA_DIR );
  for ( const QString &extension : { u"shp"_s, u"shx"_s, u"dbf"_s, u"prj"_s } )
    QFile::copy( source + u"/points."_s + extension, mTempDir->path() + u"/points."_s + extension );
  mShapefilePath = mTempDir->path() + u"/points.shp"_s;
}

void TestQgsBatchedLayerAddController::cleanupTestCase()
{
  mTempDir.reset();
  QgsApplication::exitQgis();
}

void TestQgsBatchedLayerAddController::cleanup()
{
  QgsProject::instance()->clear();
  QgsBatchedLayerAddController::settingsBatchSize->setValue( QgsBatchedLayerAddController::settingsBatchSize->defaultValue() );
  QgsBatchedLayerAddController::settingsBatchThreshold->setValue( QgsBatchedLayerAddController::settingsBatchThreshold->defaultValue() );
  QgsBatchedLayerAddController::settingsHeavyLayerFeatureCount->setValue( QgsBatchedLayerAddController::settingsHeavyLayerFeatureCount->defaultValue() );
}

QList<QgsProviderSublayerDetails> TestQgsBatchedLayerAddController::makeShapefileDetails( int count )
{
  QList<QgsProviderSublayerDetails> details;
  details.reserve( count );
  for ( int i = 0; i < count; i++ )
  {
    QgsProviderSublayerDetails d;
    d.setType( Qgis::LayerType::Vector );
    d.setProviderKey( u"ogr"_s );
    d.setUri( mShapefilePath );
    d.setName( u"copy_%1"_s.arg( i ) );
    details.append( d );
  }
  return details;
}

void TestQgsBatchedLayerAddController::addsQueuedLayersInBatches()
{
  QgsBatchedLayerAddController controller;
  QSignalSpy progressSpy( &controller, &QgsBatchedLayerAddController::batchProgress );
  QSignalSpy completedSpy( &controller, &QgsBatchedLayerAddController::completed );

  controller.enqueue( makeShapefileDetails( 17 ), u"base"_s, QString() );
  controller.start();
  QVERIFY( controller.isActive() );

  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 30000 );
  QVERIFY( !controller.isActive() );

  // 17 layers at the default batch size of 5 means 4 batches
  QCOMPARE( progressSpy.count(), 4 );
  QCOMPARE( progressSpy.last().at( 0 ).toInt(), 17 );
  QCOMPARE( progressSpy.last().at( 1 ).toInt(), 17 );

  QCOMPARE( QgsProject::instance()->mapLayers().size(), 17 );
  QCOMPARE( completedSpy.last().at( 1 ).toBool(), false );
}

void TestQgsBatchedLayerAddController::cancelKeepsAlreadyAddedLayers()
{
  QgsBatchedLayerAddController controller;
  QSignalSpy completedSpy( &controller, &QgsBatchedLayerAddController::completed );

  // cancel the run (through the task manager, as the user would) after the second batch
  connect( &controller, &QgsBatchedLayerAddController::batchProgress, this, []( int added, int ) {
    if ( added == 10 )
      QgsApplication::taskManager()->cancelAll();
  } );

  controller.enqueue( makeShapefileDetails( 17 ), u"base"_s, QString() );
  controller.start();

  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 30000 );

  // the layers added before the cancellation are kept
  QCOMPARE( QgsProject::instance()->mapLayers().size(), 10 );
  QCOMPARE( completedSpy.last().at( 1 ).toBool(), true );
}

void TestQgsBatchedLayerAddController::addsLayersIntoASharedGroup()
{
  QgsBatchedLayerAddController controller;
  QSignalSpy completedSpy( &controller, &QgsBatchedLayerAddController::completed );

  controller.enqueue( makeShapefileDetails( 12 ), u"base"_s, u"My Folder"_s );
  controller.start();

  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 30000 );

  // every batch lands in the same group
  QgsLayerTreeGroup *group = QgsProject::instance()->layerTreeRoot()->findGroup( u"My Folder"_s );
  QVERIFY( group );
  QCOMPARE( group->findLayers().size(), 12 );
  QCOMPARE( QgsProject::instance()->layerTreeRoot()->findGroups().size(), 1 );
}

void TestQgsBatchedLayerAddController::unchecksHeavyLayers()
{
  // points.shp has 17 features: with the threshold at 5, every added layer is "heavy"
  QgsBatchedLayerAddController::settingsHeavyLayerFeatureCount->setValue( 5 );

  QgsBatchedLayerAddController controller;
  QSignalSpy completedSpy( &controller, &QgsBatchedLayerAddController::completed );

  controller.enqueue( makeShapefileDetails( 12 ), u"base"_s, QString() );
  controller.start();

  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 30000 );
  QCOMPARE( QgsProject::instance()->mapLayers().size(), 12 );

  const QList<QgsLayerTreeLayer *> nodes = QgsProject::instance()->layerTreeRoot()->findLayers();
  QCOMPARE( nodes.size(), 12 );
  for ( QgsLayerTreeLayer *node : nodes )
    QVERIFY( !node->itemVisibilityChecked() );

  // with a very large threshold nothing is unchecked
  cleanup();
  QgsBatchedLayerAddController::settingsHeavyLayerFeatureCount->setValue( 1000000 );

  QgsBatchedLayerAddController lightController;
  QSignalSpy lightCompletedSpy( &lightController, &QgsBatchedLayerAddController::completed );
  lightController.enqueue( makeShapefileDetails( 12 ), u"base"_s, QString() );
  lightController.start();

  QTRY_COMPARE_WITH_TIMEOUT( lightCompletedSpy.count(), 1, 30000 );
  const QList<QgsLayerTreeLayer *> lightNodes = QgsProject::instance()->layerTreeRoot()->findLayers();
  QCOMPARE( lightNodes.size(), 12 );
  for ( QgsLayerTreeLayer *node : lightNodes )
    QVERIFY( node->itemVisibilityChecked() );
}

QGSTEST_MAIN( TestQgsBatchedLayerAddController )
#include "testqgsbatchedlayeraddcontroller.moc"
