/***************************************************************************
    qgsaimessagelogbuffer.cpp
    ---------------------
    begin                : June 2026
    copyright            : (C) 2026 by Francesco Mazzi
    email                : francemazzi at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsaimessagelogbuffer.h"

#include <algorithm>

#include "qgsapplication.h"
#include "qgsmessagelog.h"
#include "qgssettings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QString>

#include "moc_qgsaimessagelogbuffer.cpp"

using namespace Qt::StringLiterals;

namespace
{
  bool tagMatches( const QString &entryTag, const QStringList &filterTags )
  {
    if ( filterTags.isEmpty() )
      return true;

    const QString normalizedEntryTag = entryTag.isEmpty() ? u"General"_s : entryTag;
    for ( const QString &filterTag : filterTags )
    {
      if ( normalizedEntryTag.compare( filterTag, Qt::CaseInsensitive ) == 0 )
        return true;
    }
    return false;
  }

  bool levelMatches( const QList<Qgis::MessageLevel> &levels, Qgis::MessageLevel level )
  {
    if ( levels.isEmpty() )
      return true;
    return levels.contains( level );
  }

  QString levelName( Qgis::MessageLevel level )
  {
    switch ( level )
    {
      case Qgis::MessageLevel::Info:
        return u"info"_s;
      case Qgis::MessageLevel::Warning:
        return u"warning"_s;
      case Qgis::MessageLevel::Critical:
        return u"critical"_s;
      case Qgis::MessageLevel::Success:
        return u"success"_s;
      default:
        return u"unknown"_s;
    }
  }
} // namespace

QgsAiMessageLogBuffer::QgsAiMessageLogBuffer( QObject *parent, int capacity )
  : QObject( parent )
  , mCapacity( capacity > 0 ? capacity : DEFAULT_CAPACITY )
{
  mEntries.reserve( mCapacity );

  if ( QgsMessageLog *log = QgsApplication::messageLog() )
  {
    connect( log, static_cast<void ( QgsMessageLog::* )( const QString &, const QString &, Qgis::MessageLevel, Qgis::StringFormat )>( &QgsMessageLog::messageReceivedWithFormat ), this, &QgsAiMessageLogBuffer::onMessageReceived );
  }
}

void QgsAiMessageLogBuffer::onMessageReceived( const QString &message, const QString &tag, Qgis::MessageLevel level, Qgis::StringFormat format )
{
  Q_UNUSED( format )

  Entry entry;
  entry.timestamp = QDateTime::currentDateTimeUtc();
  entry.tag = tag;
  entry.level = level;
  entry.message = message;

  QMutexLocker locker( &mMutex );
  if ( mEntries.size() >= mCapacity )
    mEntries.removeFirst();
  mEntries.append( entry );
  locker.unlock();
  appendDiagnosticsEntry( entry );
}

QString QgsAiMessageLogBuffer::diagnosticsFilePath()
{
  return QDir( QgsApplication::qgisSettingsDirPath() ).filePath( u"strata_ai_diagnostics.jsonl"_s );
}

void QgsAiMessageLogBuffer::setDiagnosticsFileEnabled( bool enabled )
{
  QgsSettings settings;
  settings.setValue( u"strata/ai/diagnostics_jsonl_enabled"_s, enabled );
}

void QgsAiMessageLogBuffer::appendDiagnosticsEntry( const Entry &entry ) const
{
  QgsSettings settings;
  if ( !settings.value( u"strata/ai/diagnostics_jsonl_enabled"_s, true ).toBool() )
    return;

  QFile file( diagnosticsFilePath() );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
    return;
  QJsonObject object;
  object.insert( u"timestamp"_s, entry.timestamp.toString( Qt::ISODateWithMs ) );
  object.insert( u"tag"_s, entry.tag.isEmpty() ? u"General"_s : entry.tag );
  object.insert( u"level"_s, levelName( entry.level ) );
  object.insert( u"message"_s, entry.message.left( 16384 ) );
  file.write( QJsonDocument( object ).toJson( QJsonDocument::Compact ) );
  file.write( "\n" );
}

QgsAiMessageLogBuffer::QueryResult QgsAiMessageLogBuffer::query( const Query &query ) const
{
  QueryResult result;
  const int effectiveLimit = std::clamp( query.limit > 0 ? query.limit : 50, 1, MAX_QUERY_LIMIT );

  QMutexLocker locker( &mMutex );
  result.totalBuffered = mEntries.size();

  const QDateTime cutoff = query.sinceSeconds > 0 ? QDateTime::currentDateTimeUtc().addSecs( -query.sinceSeconds ) : QDateTime();

  auto entryMatches = [&]( const Entry &entry ) {
    if ( query.sinceSeconds > 0 && entry.timestamp < cutoff )
      return false;
    if ( !levelMatches( query.levels, entry.level ) )
      return false;
    if ( !tagMatches( entry.tag, query.tags ) )
      return false;
    if ( !query.search.isEmpty() && !entry.message.contains( query.search, Qt::CaseInsensitive ) )
      return false;
    return true;
  };

  for ( int i = mEntries.size() - 1; i >= 0; --i )
  {
    const Entry &entry = mEntries.at( i );
    if ( !entryMatches( entry ) )
      continue;

    result.entries.append( entry );
    if ( result.entries.size() >= effectiveLimit )
    {
      for ( int j = i - 1; j >= 0; --j )
      {
        if ( entryMatches( mEntries.at( j ) ) )
        {
          result.truncated = true;
          break;
        }
      }
      break;
    }
  }

  return result;
}

QStringList QgsAiMessageLogBuffer::distinctTags() const
{
  QMutexLocker locker( &mMutex );
  QStringList tags;
  for ( const Entry &entry : mEntries )
  {
    const QString tag = entry.tag.isEmpty() ? u"General"_s : entry.tag;
    if ( !tags.contains( tag, Qt::CaseInsensitive ) )
      tags.append( tag );
  }
  tags.sort( Qt::CaseInsensitive );
  return tags;
}

int QgsAiMessageLogBuffer::entryCount() const
{
  QMutexLocker locker( &mMutex );
  return mEntries.size();
}
