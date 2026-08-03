/***************************************************************************
    qgsfolderscantask.cpp
    ---------------------
    begin                : August 2026
    copyright            : (C) 2026 by Francesco Mazzi
    email                : francemazzi at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qgsfolderscantask.h"

#include "qgsapplication.h"
#include "qgsfeedback.h"
#include "qgsogrproviderutils.h"
#include "qgsproviderregistry.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include "moc_qgsfolderscantask.cpp"

using namespace Qt::StringLiterals;

namespace
{
  // Extensions worth resolving through querySublayers. Sidecar files (.dbf, .shx, .prj, …)
  // are excluded implicitly by not being listed here.
  const QSet<QString> &candidateFileExtensions()
  {
    static const QSet<QString> sExtensions {
      u"shp"_s, u"gpkg"_s, u"geojson"_s, u"json"_s, u"kml"_s, u"kmz"_s,
      u"tab"_s, u"mif"_s, u"gml"_s, u"sqlite"_s, u"fgb"_s,
      u"tif"_s, u"tiff"_s, u"asc"_s, u"img"_s, u"jp2"_s, u"vrt"_s
    };
    return sExtensions;
  }

  const QSet<QString> &projectFileExtensions()
  {
    static const QSet<QString> sExtensions { u"qgs"_s, u"qgz"_s };
    return sExtensions;
  }

  bool isInsideContainerDirectory( const QString &relativePath, const QStringList &containerDirExtensions )
  {
    const QStringList parts = relativePath.split( '/', Qt::SkipEmptyParts );
    for ( int i = 0; i < parts.size() - 1; i++ )
    {
      const QString suffix = QFileInfo( parts.at( i ) ).suffix().toLower();
      if ( !suffix.isEmpty() && containerDirExtensions.contains( suffix ) )
        return true;
    }
    return false;
  }
} // namespace

QgsFolderScanTask::QgsFolderScanTask( const QString &directory, int maxFiles )
  : QgsTask( tr( "Scanning folder %1" ).arg( QDir::toNativeSeparators( directory ) ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
  , mDirectory( directory )
  , mMaxFiles( maxFiles )
{
}

QgsFolderScanTask::QgsFolderScanTask( const QStringList &files )
  : QgsTask( tr( "Scanning %n file(s)", nullptr, files.size() ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
  , mFiles( files )
{
}

void QgsFolderScanTask::cancel()
{
  if ( mFeedback )
    mFeedback->cancel();
  QgsTask::cancel();
}

QList<QgsProviderSublayerDetails> QgsFolderScanTask::results() const
{
  const QMutexLocker locker( &mMutex );
  return mResults;
}

QList<QgsProviderSublayerModel::NonLayerItem> QgsFolderScanTask::projectItems() const
{
  const QMutexLocker locker( &mMutex );
  return mProjectItems;
}

bool QgsFolderScanTask::truncated() const
{
  const QMutexLocker locker( &mMutex );
  return mTruncated;
}

int QgsFolderScanTask::scannedFileCount() const
{
  const QMutexLocker locker( &mMutex );
  return mScannedFileCount;
}

bool QgsFolderScanTask::run()
{
  mFeedback = std::make_unique<QgsFeedback>();

  const QStringList containerDirExtensions = QgsOgrProviderUtils::directoryExtensions();

  // first pass: collect candidate dataset paths (cheap, name-based only)
  QStringList candidateFiles;
  QStringList candidateContainerDirs;
  QStringList projectFiles;
  bool truncated = false;

  const QDir baseDir( mDirectory.isEmpty() ? QDir::rootPath() : mDirectory );

  if ( !mDirectory.isEmpty() )
  {
    QDirIterator it( mDirectory, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories );
    while ( it.hasNext() )
    {
      if ( isCanceled() )
        return false;

      const QString path = it.next();
      const QFileInfo info = it.fileInfo();
      const QString suffix = info.suffix().toLower();
      const QString relativePath = baseDir.relativeFilePath( path );

      // entries inside a container directory (e.g. the internal files of a .gdb) are
      // part of that dataset, not datasets of their own
      if ( isInsideContainerDirectory( relativePath, containerDirExtensions ) )
        continue;

      if ( info.isDir() )
      {
        if ( containerDirExtensions.contains( suffix ) )
          candidateContainerDirs.append( path );
        continue;
      }

      if ( projectFileExtensions().contains( suffix ) )
      {
        projectFiles.append( path );
        continue;
      }

      if ( !candidateFileExtensions().contains( suffix ) )
        continue;

      if ( mMaxFiles > 0 && candidateFiles.size() + candidateContainerDirs.size() >= mMaxFiles )
      {
        truncated = true;
        break;
      }
      candidateFiles.append( path );
    }
  }
  else
  {
    for ( const QString &file : std::as_const( mFiles ) )
    {
      const QFileInfo info( file );
      const QString suffix = info.suffix().toLower();
      if ( info.isDir() && containerDirExtensions.contains( suffix ) )
        candidateContainerDirs.append( file );
      else if ( projectFileExtensions().contains( suffix ) )
        projectFiles.append( file );
      else if ( info.isFile() )
        candidateFiles.append( file );
    }
  }

  // second pass: resolve each candidate through the provider registry
  QList<QgsProviderSublayerDetails> results;
  const int total = candidateFiles.size() + candidateContainerDirs.size();
  int processed = 0;

  for ( const QString &path : std::as_const( candidateContainerDirs ) )
  {
    if ( isCanceled() )
      return false;
    scanDataset( path, baseDir, true, results );
    processed++;
    setProgress( total > 0 ? 100.0 * processed / total : 100 );
  }

  for ( const QString &path : std::as_const( candidateFiles ) )
  {
    if ( isCanceled() )
      return false;
    scanDataset( path, baseDir, false, results );
    processed++;
    setProgress( total > 0 ? 100.0 * processed / total : 100 );
  }

  QList<QgsProviderSublayerModel::NonLayerItem> projectItems;
  projectItems.reserve( projectFiles.size() );
  for ( const QString &path : std::as_const( projectFiles ) )
  {
    QgsProviderSublayerModel::NonLayerItem item;
    item.setType( u"project"_s );
    item.setName( mDirectory.isEmpty() ? QFileInfo( path ).fileName() : baseDir.relativeFilePath( path ) );
    item.setDescription( tr( "QGIS project file" ) );
    item.setUri( path );
    item.setIcon( QgsApplication::getThemeIcon( u"mIconQgsProjectFile.svg"_s ) );
    projectItems.append( item );
  }

  const QMutexLocker locker( &mMutex );
  mResults = results;
  mProjectItems = projectItems;
  mTruncated = truncated;
  mScannedFileCount = processed;
  return true;
}

void QgsFolderScanTask::scanDataset( const QString &path, const QDir &baseDir, bool isContainerDirectory, QList<QgsProviderSublayerDetails> &results )
{
  const Qgis::SublayerQueryFlags flags = isContainerDirectory ? Qgis::SublayerQueryFlags() : Qgis::SublayerQueryFlags( Qgis::SublayerQueryFlag::FastScan );
  QList<QgsProviderSublayerDetails> sublayers = QgsProviderRegistry::instance()->querySublayers( path, flags, mFeedback.get() );

  // fast scan leaves multi-layer containers unresolved: scan those in depth so the
  // catalog lists their internal layers individually
  bool hasSkippedContainer = false;
  for ( const QgsProviderSublayerDetails &sublayer : std::as_const( sublayers ) )
  {
    if ( sublayer.skippedContainerScan() )
    {
      hasSkippedContainer = true;
      break;
    }
  }
  if ( hasSkippedContainer )
    sublayers = QgsProviderRegistry::instance()->querySublayers( path, Qgis::SublayerQueryFlags(), mFeedback.get() );

  const bool isContainer = isContainerDirectory || sublayers.size() > 1;
  const QString relativePath = mDirectory.isEmpty() ? QFileInfo( path ).fileName() : baseDir.relativeFilePath( path );

  // group results under their location relative to the scanned folder; layers of a
  // container dataset are additionally grouped under the container's own name
  QStringList pathComponents = relativePath.split( '/', Qt::SkipEmptyParts );
  if ( !isContainer && !pathComponents.isEmpty() )
    pathComponents.removeLast();

  for ( QgsProviderSublayerDetails &sublayer : sublayers )
  {
    sublayer.setPath( pathComponents );
    results.append( sublayer );
  }
}
