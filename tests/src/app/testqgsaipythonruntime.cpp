/***************************************************************************
  testqgsaipythonruntime.cpp
  --------------------------
  begin                : August 2026
  copyright            : (C) 2026
***************************************************************************/

#include "ai/tools/qgsaipythonruntime.h"
#include "qgsapplication.h"
#include "qgstest.h"

using namespace Qt::StringLiterals;

class TestQgsAiPythonRuntime : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void bootstrapProvidesPyQgisNamespace();
    void pipWrapperUsesIsolatedTargetAndBundledInterpreter();
    void pipWrapperPreflightsAndCachesPackages();
    void externallyManagedErrorsAreRecognized();
};

void TestQgsAiPythonRuntime::initTestCase()
{
  QgsApplication::initQgis();
}

void TestQgsAiPythonRuntime::bootstrapProvidesPyQgisNamespace()
{
  const QString source = QgsAiPythonRuntime::bootstrapSource( u"/tmp/profile's packages"_s );

  QVERIFY( source.contains( u"sys.path.insert(0, __qgsai_package_path)"_s ) );
  QVERIFY( source.contains( u"from qgis.utils import iface"_s ) );
  QVERIFY( source.contains( u"import processing"_s ) );
  QVERIFY( source.contains( u"import qgis.PyQt"_s ) );
  QVERIFY( source.contains( u"PyQt = qgis.PyQt"_s ) );
  QVERIFY( source.contains( u"/tmp/profile\\'s packages"_s ) );
}

void TestQgsAiPythonRuntime::pipWrapperUsesIsolatedTargetAndBundledInterpreter()
{
  const QString source = QgsAiPythonRuntime::pipWrapperSource( u"/tmp/out.json"_s, u"/tmp/args.json"_s, u"/tmp/ai-target"_s, u"/tmp/cache.json"_s, 42 );

  QVERIFY( source.contains( uR"("--target")"_s ) );
  QVERIFY( !source.contains( uR"("--user")"_s ) );
  QVERIFY( !source.contains( u"--break-system-packages"_s ) );
  QVERIFY( !source.contains( u"shutil.which"_s ) );
  QVERIFY( source.contains( u"os.path.dirname(os.path.abspath(sys.executable))"_s ) );
  QVERIFY( source.contains( u"os.path.dirname(real_path) != real_application_dir"_s ) );
  QVERIFY( source.contains( u"os.path.realpath(data.get(\"executable\", \"\"))"_s ) );
  QVERIFY( source.contains( uR"(["python", "python3", "python.exe", "python3.exe"])"_s ) );
  QVERIFY( source.contains( uR"(pop("PIP_BREAK_SYSTEM_PACKAGES", None))"_s ) );
}

void TestQgsAiPythonRuntime::pipWrapperPreflightsAndCachesPackages()
{
  const QString source = QgsAiPythonRuntime::pipWrapperSource( u"/tmp/out.json"_s, u"/tmp/args.json"_s, u"/tmp/ai-target"_s, u"/tmp/cache.json"_s, 42 );

  QVERIFY( source.contains( u"importlib.util.find_spec(module_name)"_s ) );
  QVERIFY( source.contains( u"importlib_metadata.distribution(package_name)"_s ) );
  QVERIFY( source.contains( u"successful_specs"_s ) );
  QVERIFY( source.contains( u"__qgsai_cached.append"_s ) );
  QVERIFY( source.contains( uR"(== "duckdb")"_s ) );
  QVERIFY( source.contains( u"bundled_package_missing"_s ) );
}

void TestQgsAiPythonRuntime::externallyManagedErrorsAreRecognized()
{
  QVERIFY( QgsAiPythonRuntime::isExternallyManagedError( u"error: externally-managed-environment"_s ) );
  QVERIFY( QgsAiPythonRuntime::isExternallyManagedError( u"Blocked by PEP 668"_s ) );
  QVERIFY( !QgsAiPythonRuntime::isExternallyManagedError( u"temporary network failure"_s ) );

  const QString source = QgsAiPythonRuntime::pipWrapperSource( u"/tmp/out.json"_s, u"/tmp/args.json"_s, u"/tmp/target"_s, u"/tmp/cache.json"_s, 42 );
  QVERIFY( source.contains( uR"(__qgsai_error_code = "externally_managed_environment")"_s ) );
  QVERIFY( source.contains( u"__qgsai_retryable = False"_s ) );
}

QGSTEST_MAIN( TestQgsAiPythonRuntime )
#include "testqgsaipythonruntime.moc"
