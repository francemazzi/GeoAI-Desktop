/***************************************************************************
    qgsbatchedlayeraddcontroller.cpp
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

#include "qgsbatchedlayeraddcontroller.h"

#include "canvas/qgscanvasrefreshblocker.h"
#include "layers/qgsapplayerhandling.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgslayertree.h"
#include "qgsmessagebar.h"
#include "qgsmessagebaritem.h"
#include "qgsmessagelog.h"
#include "qgsmessageoutput.h"
#include "qgsmessageviewer.h"
#include "qgsproject.h"
#include "qgsproxyprogresstask.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingstree.h"
#include "qgsvectorlayer.h"

#include <QCoreApplication>
#include <QPushButton>

#include "moc_qgsbatchedlayeraddcontroller.cpp"

using namespace Qt::StringLiterals;

const QgsSettingsEntryInteger *QgsBatchedLayerAddController::settingsBatchSize
  = new QgsSettingsEntryInteger( u"bulk-import-batch-size"_s, QgsSettingsTree::sTreeApp, 5, u"Number of layers added per main-thread batch during a bulk import"_s, Qgis::SettingsOption(), 1 );
const QgsSettingsEntryInteger *QgsBatchedLayerAddController::settingsBatchThreshold
  = new QgsSettingsEntryInteger( u"bulk-import-threshold"_s, QgsSettingsTree::sTreeApp, 10, u"Minimum number of layers for the batched bulk import to engage instead of the synchronous path"_s, Qgis::SettingsOption(), 0 );
const QgsSettingsEntryInteger64 *QgsBatchedLayerAddController::settingsHeavyLayerFeatureCount
  = new QgsSettingsEntryInteger64( u"bulk-import-heavy-feature-count"_s, QgsSettingsTree::sTreeApp, 50000, u"Layers with at least this many features are added unchecked after a bulk import, to keep the first render responsive. 0 disables."_s, Qgis::SettingsOption(), 0 );

QgsBatchedLayerAddController::QgsBatchedLayerAddController( QObject *parent )
  : QObject( parent )
{
  mTick.setSingleShot( true );
  mTick.setInterval( 0 );
  connect( &mTick, &QTimer::timeout, this, &QgsBatchedLayerAddController::processNextBatch );
}

QgsBatchedLayerAddController::~QgsBatchedLayerAddController() = default;

QgsBatchedLayerAddController *QgsBatchedLayerAddController::instance()
{
  static QgsBatchedLayerAddController *sInstance = new QgsBatchedLayerAddController( QCoreApplication::instance() );
  return sInstance;
}

void QgsBatchedLayerAddController::enqueue( const QList<QgsProviderSublayerDetails> &details, const QString &baseName, const QString &groupName )
{
  mPending.append( details );
  mTotal += details.size();
  if ( !mActive )
  {
    mBaseName = baseName;
    mGroupName = groupName;
  }
  else if ( mProgressTask )
  {
    mProgressTask->setDescription( tr( "Adding layers (%1/%2)…" ).arg( mAdded.size() ).arg( mTotal ) );
  }
}

void QgsBatchedLayerAddController::start()
{
  if ( mActive || mPending.isEmpty() )
    return;

  mActive = true;
  mCancelRequested = false;
  mElapsed.start();

  if ( !mGroupName.isEmpty() )
    mGroup = QgsAppLayerHandling::createGroupForSublayers( mGroupName );

  // freeze the canvas for the whole run: one render at the end, the filling legend
  // and the task progress are the visible feedback in the meantime
  if ( QgisApp::instance() )
    mRefreshBlocker = std::make_unique<QgsCanvasRefreshBlocker>();

  mProgressTask = new QgsProxyProgressTask( tr( "Adding layers (0/%1)…" ).arg( mTotal ), true );
  connect( mProgressTask, &QgsProxyProgressTask::canceled, this, [this] { mCancelRequested = true; } );
  if ( QgsTaskManager *taskManager = QgsApplication::taskManager() )
    taskManager->addTask( mProgressTask.data() );

  emit aboutToStart();
  mTick.start();
}

void QgsBatchedLayerAddController::processNextBatch()
{
  if ( !mActive )
    return;

  if ( mCancelRequested )
  {
    finish( true );
    return;
  }

  const int batchSize = std::max( 1, static_cast<int>( settingsBatchSize->value() ) );
  const QList<QgsProviderSublayerDetails> batch = mPending.mid( 0, batchSize );
  mPending.erase( mPending.begin(), mPending.begin() + batch.size() );

  QgsAppLayerHandling::SublayerAddOptions options;
  options.deferDatumTransformPrompts = true;
  options.existingGroup = mGroup;
  mAdded.append( QgsAppLayerHandling::addSublayers( batch, mBaseName, QString(), true, options ) );

  if ( mProgressTask )
  {
    mProgressTask->setProxyProgress( mTotal > 0 ? 100.0 * mAdded.size() / mTotal : 100 );
    mProgressTask->setDescription( tr( "Adding layers (%1/%2)…" ).arg( mAdded.size() ).arg( mTotal ) );
  }
  emit batchProgress( mAdded.size(), mTotal );

  if ( mPending.isEmpty() )
    finish( false );
  else
    mTick.start(); // 0 ms single-shot: yields the event loop before the next batch
}

void QgsBatchedLayerAddController::finish( bool cancelled )
{
  askDeferredDatumTransforms();
  QgsAppLayerHandling::postProcessAddedLayers( mAdded );
  uncheckHeavyLayers();

  if ( mProgressTask )
    mProgressTask->finalize( true );
  mProgressTask = nullptr;

  // releasing the blocker triggers the single deferred canvas render
  mRefreshBlocker.reset();

  logImportSummary( cancelled );

  if ( QgisApp::instance() )
  {
    QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );

    if ( cancelled )
      QgisApp::instance()->visibleMessageBar()->pushInfo( tr( "Layer import" ), tr( "Import cancelled after %1 of %2 layers — layers already added were kept" ).arg( mAdded.size() ).arg( mTotal ) );
    else
      QgisApp::instance()->visibleMessageBar()->pushSuccess( tr( "Layer import" ), tr( "Added %1 layer(s) in %2 s" ).arg( mAdded.size() ).arg( mElapsed.elapsed() / 1000.0, 0, 'f', 1 ) );
  }

  const QList<QgsMapLayer *> added = mAdded;
  mAdded.clear();
  mPending.clear();
  mGroup = nullptr;
  mBaseName.clear();
  mGroupName.clear();
  mTotal = 0;
  mActive = false;
  mCancelRequested = false;

  emit completed( added, cancelled );
}

void QgsBatchedLayerAddController::askDeferredDatumTransforms()
{
  if ( !QgisApp::instance() )
    return;

  const QgsCoordinateReferenceSystem projectCrs = QgsProject::instance()->crs();
  QSet<QString> handledCrs;
  for ( QgsMapLayer *layer : std::as_const( mAdded ) )
  {
    if ( !layer )
      continue;
    const QString authId = layer->crs().authid().isEmpty() ? layer->crs().toWkt() : layer->crs().authid();
    if ( handledCrs.contains( authId ) )
      continue;
    handledCrs.insert( authId );
    QgisApp::instance()->askUserForDatumTransform( layer->crs(), projectCrs, layer );
  }
}

void QgsBatchedLayerAddController::uncheckHeavyLayers()
{
  const long long heavyThreshold = settingsHeavyLayerFeatureCount->value();
  if ( heavyThreshold <= 0 || mTotal < settingsBatchThreshold->value() )
    return;

  QStringList heavyLayerNames;
  for ( QgsMapLayer *layer : std::as_const( mAdded ) )
  {
    QgsVectorLayer *vectorLayer = qobject_cast<QgsVectorLayer *>( layer );
    if ( !vectorLayer )
      continue;
    // featureCount is cheap for file-based OGR sources (header / metadata read);
    // unknown counts (-1) are treated as light so unknowns are never punished
    const long long count = vectorLayer->featureCount();
    if ( count < heavyThreshold )
      continue;

    if ( QgsLayerTreeLayer *node = QgsProject::instance()->layerTreeRoot()->findLayer( layer->id() ) )
    {
      node->setItemVisibilityChecked( false );
      heavyLayerNames.append( layer->name() );
    }
  }

  if ( heavyLayerNames.isEmpty() || !QgisApp::instance() )
    return;

  QgsMessageBarItem *messageWidget = QgsMessageBar::createMessage( tr( "Layer import" ), tr( "%n large layer(s) were added unchecked to keep the map responsive. Tick them in the Layers panel to display.", nullptr, heavyLayerNames.size() ) );
  QPushButton *detailsButton = new QPushButton( tr( "Details" ) );
  const QString detailsText = heavyLayerNames.join( u"<br>"_s );
  QObject::connect( detailsButton, &QPushButton::clicked, QgisApp::instance(), [detailsText] {
    if ( QgsMessageViewer *dialog = dynamic_cast<QgsMessageViewer *>( QgsMessageOutput::createMessageOutput() ) )
    {
      dialog->setTitle( tr( "Large layers added unchecked" ) );
      dialog->setMessage( detailsText, Qgis::StringFormat::Html );
      dialog->showMessage();
    }
  } );
  messageWidget->layout()->addWidget( detailsButton );
  QgisApp::instance()->visibleMessageBar()->pushWidget( messageWidget, Qgis::MessageLevel::Info, 0 );
}

void QgsBatchedLayerAddController::logImportSummary( bool cancelled ) const
{
  QgsMessageLog::logMessage( u"{\"event\": \"bulk-import\", \"layersRequested\": %1, \"layersAdded\": %2, \"durationMs\": %3, \"cancelled\": %4}"_s.arg( mTotal ).arg( mAdded.size() ).arg( mElapsed.elapsed() ).arg( cancelled ? u"true"_s : u"false"_s ), QObject::tr( "Layer Import" ), Qgis::MessageLevel::Info );
}
