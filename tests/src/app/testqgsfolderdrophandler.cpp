/***************************************************************************
  testqgsfolderdrophandler.cpp
  --------------------------------
  begin                : August 2026
***************************************************************************/

#include "qgsapplication.h"
#include "qgsfolderdrophandler.h"
#include "qgsfolderscantask.h"
#include "qgstest.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

class TestQgsFolderDropHandler : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void scanFindsDatasetsAndExcludesSidecars();
    void scanRespectsMaxFilesCap();
    void handleFileDropRefusesFilesAndOgrDirectories();

  private:
    //! Copies the points.shp fixture (+ sidecars) into \a destDir with base name \a baseName.
    static QString copyPointsShapefile( const QString &destDir, const QString &baseName );
    //! Seeds a folder with a shapefile, a GeoPackage, a text file, a subfolder shapefile and a project file.
    static void seedFolder( const QString &root );
    //! Runs \a task on the task manager and waits for completion.
    static bool runTask( QgsFolderScanTask *task );
};

void TestQgsFolderDropHandler::initTestCase()
{
  QgsApplication::initQgis();
}

void TestQgsFolderDropHandler::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

QString TestQgsFolderDropHandler::copyPointsShapefile( const QString &destDir, const QString &baseName )
{
  const QString source = QStringLiteral( TEST_DATA_DIR );
  for ( const QString &extension : { u"shp"_s, u"shx"_s, u"dbf"_s, u"prj"_s } )
  {
    QFile::copy( source + u"/points."_s + extension, destDir + '/' + baseName + '.' + extension );
  }
  return destDir + '/' + baseName + u".shp"_s;
}

void TestQgsFolderDropHandler::seedFolder( const QString &root )
{
  copyPointsShapefile( root, u"points"_s );
  QFile::copy( QStringLiteral( TEST_DATA_DIR ) + u"/points_gpkg.gpkg"_s, root + u"/data.gpkg"_s );

  QFile textFile( root + u"/readme.txt"_s );
  QVERIFY( textFile.open( QIODevice::WriteOnly ) );
  textFile.write( "not a dataset" );
  textFile.close();

  QVERIFY( QDir( root ).mkdir( u"sub"_s ) );
  copyPointsShapefile( root + u"/sub"_s, u"other_points"_s );

  QFile projectFile( root + u"/project.qgz"_s );
  QVERIFY( projectFile.open( QIODevice::WriteOnly ) );
  projectFile.write( "dummy" );
  projectFile.close();
}

bool TestQgsFolderDropHandler::runTask( QgsFolderScanTask *task )
{
  QSignalSpy completedSpy( task, &QgsTask::taskCompleted );
  QgsApplication::taskManager()->addTask( task );
  return completedSpy.wait( 30000 );
}

void TestQgsFolderDropHandler::scanFindsDatasetsAndExcludesSidecars()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  seedFolder( tempDir.path() );

  QgsFolderScanTask *task = new QgsFolderScanTask( tempDir.path(), 1000 );
  QVERIFY( runTask( task ) );

  const QList<QgsProviderSublayerDetails> results = task->results();
  QVERIFY( !results.isEmpty() );

  bool foundRootShapefile = false;
  bool foundSubfolderShapefile = false;
  bool foundGeoPackageLayer = false;
  for ( const QgsProviderSublayerDetails &detail : results )
  {
    // sidecars and plain files must never surface as datasets
    QVERIFY( !detail.uri().endsWith( ".dbf"_L1 ) );
    QVERIFY( !detail.uri().endsWith( ".txt"_L1 ) );

    if ( detail.name() == "points"_L1 && detail.path().isEmpty() )
      foundRootShapefile = true;
    if ( !detail.path().isEmpty() && detail.path().constFirst() == "sub"_L1 )
      foundSubfolderShapefile = true;
    if ( detail.uri().contains( "data.gpkg"_L1 ) )
      foundGeoPackageLayer = true;
  }
  QVERIFY( foundRootShapefile );
  QVERIFY( foundSubfolderShapefile );
  QVERIFY( foundGeoPackageLayer );

  const QList<QgsProviderSublayerModel::NonLayerItem> projects = task->projectItems();
  QCOMPARE( projects.size(), 1 );
  QCOMPARE( projects.at( 0 ).name(), u"project.qgz"_s );

  QVERIFY( !task->truncated() );
}

void TestQgsFolderDropHandler::scanRespectsMaxFilesCap()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  seedFolder( tempDir.path() );

  QgsFolderScanTask *task = new QgsFolderScanTask( tempDir.path(), 1 );
  QVERIFY( runTask( task ) );

  QVERIFY( task->truncated() );
  QCOMPARE( task->scannedFileCount(), 1 );
}

void TestQgsFolderDropHandler::handleFileDropRefusesFilesAndOgrDirectories()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  const QString shapefile = copyPointsShapefile( tempDir.path(), u"points"_s );

  QgsFolderDropHandler handler;

  // plain files are left to the regular opening path
  QVERIFY( !handler.handleFileDrop( shapefile ) );

  // OGR dataset directories (.gdb) are left to the regular OGR path, where the
  // sublayer prompt and its guardrail apply
  QVERIFY( QDir( tempDir.path() ).mkdir( u"data.gdb"_s ) );
  QVERIFY( !handler.handleFileDrop( tempDir.path() + u"/data.gdb"_s ) );
}

QGSTEST_MAIN( TestQgsFolderDropHandler )
#include "testqgsfolderdrophandler.moc"
