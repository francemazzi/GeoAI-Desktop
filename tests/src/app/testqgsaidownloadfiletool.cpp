/***************************************************************************
  testqgsaidownloadfiletool.cpp
  --------------------------
  begin                : July 2026
  copyright            : (C) 2026
***************************************************************************/

#include "ai/qgsaifilecontextprovider.h"
#include "ai/qgsaiworkspacetrust.h"
#include "ai/tools/qgsaidownloadfiletool.h"
#include "qgstest.h"

#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

class TestQgsAiDownloadFileTool : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void unencodedOverpassQueryIsNormalizedAndAccepted();
    void unfixableUrlStillFailsWithHint();
    void alreadyEncodedUrlIsUnaffected();
};

void TestQgsAiDownloadFileTool::initTestCase()
{
}

void TestQgsAiDownloadFileTool::cleanupTestCase()
{
}

// A raw Overpass API query contains '[', ']' and '"' unescaped in the query
// string. The tool should percent-encode and re-validate the URL rather than
// rejecting it outright. We prove this indirectly: URL validation happens
// before the destination-path check, so pointing dest_path outside the
// workspace surfaces the *destination* error instead of "URL is not valid"
// only if the URL was successfully normalized. This avoids triggering the
// network request / approval dialog in the test.
void TestQgsAiDownloadFileTool::unencodedOverpassQueryIsNormalizedAndAccepted()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  QgsAiWorkspaceTrust::setState( tempDir.path(), QgsAiWorkspaceTrust::State::Trusted );

  QgsAiFileContextProvider contextProvider( tempDir.path() );
  QgsAiDownloadFileTool tool( &contextProvider, nullptr );
  QVERIFY( tool.isAvailable() );

  QJsonObject args;
  args.insert( u"url"_s, u"https://overpass-api.de/api/interpreter?data=[out:geojson][timeout:25];(nwr[name~\"Garzaia\",i](around:5000,44.9333,10.6167););out body;>;out skel qt;"_s );
  args.insert( u"dest_path"_s, u"../outside-workspace.geojson"_s );

  const QgsAiToolResult result = tool.execute( args );
  QVERIFY( !result.success );
  QVERIFY2( !result.errorMessage.contains( u"URL is not valid"_s ), qPrintable( result.errorMessage ) );
  QVERIFY2( result.errorMessage.contains( u"outside the workspace"_s ), qPrintable( result.errorMessage ) );
}

// A URL with no query string to repair (e.g. malformed scheme) must still be
// rejected, with the error message hinting at percent-encoding.
void TestQgsAiDownloadFileTool::unfixableUrlStillFailsWithHint()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  QgsAiWorkspaceTrust::setState( tempDir.path(), QgsAiWorkspaceTrust::State::Trusted );

  QgsAiFileContextProvider contextProvider( tempDir.path() );
  QgsAiDownloadFileTool tool( &contextProvider, nullptr );

  QJsonObject args;
  args.insert( u"url"_s, u"ht!tp://[invalid"_s );
  args.insert( u"dest_path"_s, u"data.geojson"_s );

  const QgsAiToolResult result = tool.execute( args );
  QVERIFY( !result.success );
  QVERIFY2( result.errorMessage.contains( u"URL is not valid"_s ), qPrintable( result.errorMessage ) );
  QVERIFY2( result.errorMessage.contains( u"percent-encode"_s ), qPrintable( result.errorMessage ) );
}

// An already correctly percent-encoded URL must pass validation unchanged
// (no double-encoding), again proven via the destination-path error.
void TestQgsAiDownloadFileTool::alreadyEncodedUrlIsUnaffected()
{
  QTemporaryDir tempDir;
  QVERIFY( tempDir.isValid() );
  QgsAiWorkspaceTrust::setState( tempDir.path(), QgsAiWorkspaceTrust::State::Trusted );

  QgsAiFileContextProvider contextProvider( tempDir.path() );
  QgsAiDownloadFileTool tool( &contextProvider, nullptr );

  QJsonObject args;
  args.insert( u"url"_s, u"https://overpass-api.de/api/interpreter?data=%5Bout%3Ageojson%5D%3B"_s );
  args.insert( u"dest_path"_s, u"../outside-workspace.geojson"_s );

  const QgsAiToolResult result = tool.execute( args );
  QVERIFY( !result.success );
  QVERIFY2( !result.errorMessage.contains( u"URL is not valid"_s ), qPrintable( result.errorMessage ) );
  QVERIFY2( result.errorMessage.contains( u"outside the workspace"_s ), qPrintable( result.errorMessage ) );
}

QGSTEST_MAIN( TestQgsAiDownloadFileTool )
#include "testqgsaidownloadfiletool.moc"
