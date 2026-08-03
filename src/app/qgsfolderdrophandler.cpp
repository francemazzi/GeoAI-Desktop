/***************************************************************************
    qgsfolderdrophandler.cpp
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

#include "qgsfolderdrophandler.h"

#include "canvas/qgscanvasrefreshblocker.h"
#include "layers/qgsapplayerhandling.h"
#include "layers/qgsbatchedlayeraddcontroller.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsfoldercatalogdialog.h"
#include "qgsfolderscantask.h"
#include "qgsmessagebar.h"
#include "qgsmessagelog.h"
#include "qgsogrproviderutils.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingsregistrygui.h"
#include "qgssettingstree.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

#include "moc_qgsfolderdrophandler.cpp"

using namespace Qt::StringLiterals;

const QgsSettingsEntryInteger *QgsFolderDropHandler::settingsMaxScannedFiles
  = new QgsSettingsEntryInteger( u"folder-import-max-files"_s, QgsSettingsTree::sTreeApp, 1000, u"Maximum number of candidate datasets scanned when cataloguing a dropped folder"_s, Qgis::SettingsOption(), 0 );

QString QgsFolderDropHandler::customUriProviderKey() const
{
  return u"folder"_s;
}

bool QgsFolderDropHandler::handleFileDrop( const QString &file )
{
  return openFolderAsCatalog( file );
}

bool QgsFolderDropHandler::openFolderAsCatalog( const QString &path )
{
  const QFileInfo info( path );
  if ( !info.isDir() )
    return false;

  // folders which are themselves OGR datasets (e.g. .gdb) stay on the regular opening
  // path, where the sublayer prompt and its guardrail apply
  if ( QgsOgrProviderUtils::directoryExtensions().contains( info.suffix().toLower() ) )
    return false;

  QgsFolderScanTask *task = new QgsFolderScanTask( path, static_cast<int>( settingsMaxScannedFiles->value() ) );

  QObject::connect( task, &QgsTask::taskCompleted, QgisApp::instance(), [task, path] {
    const QList<QgsProviderSublayerDetails> details = task->results();
    const QList<QgsProviderSublayerModel::NonLayerItem> projectItems = task->projectItems();

    QgsMessageLog::logMessage(
      QObject::tr( "Folder catalog: %1 dataset(s) and %2 project file(s) found in \"%3\"%4" )
        .arg( details.size() )
        .arg( projectItems.size() )
        .arg( path, task->truncated() ? QObject::tr( " (scan truncated)" ) : QString() ),
      QObject::tr( "Layer Import" ),
      Qgis::MessageLevel::Info
    );

    if ( details.empty() && projectItems.empty() )
    {
      QgisApp::instance()->visibleMessageBar()->pushWarning( QObject::tr( "Folder catalog" ), QObject::tr( "No supported datasets were found in \"%1\"" ).arg( QDir::toNativeSeparators( path ) ) );
      return;
    }

    QgsFolderCatalogDialog dlg( path, details, projectItems, task->truncated(), QgisApp::instance() );
    if ( dlg.exec() != QDialog::Accepted )
      return;

    const QList<QgsProviderSublayerDetails> selectedLayers = dlg.selectedLayers();
    const QList<QgsProviderSublayerModel::NonLayerItem> selectedProjects = dlg.selectedNonLayerItems();

    if ( !selectedLayers.empty() )
    {
      QString base = QDir( path ).dirName();
      if ( QgsSettingsRegistryGui::settingsFormatLayerName->value() )
        base = QgsMapLayer::formatLayerName( base );

      // large selections go through the batched controller (progress + cancel); small
      // ones follow the regular synchronous path
      const int batchThreshold = QgsBatchedLayerAddController::settingsBatchThreshold->value();
      if ( batchThreshold > 0 && selectedLayers.size() >= batchThreshold )
      {
        QgsBatchedLayerAddController::instance()->enqueue( selectedLayers, base, dlg.groupName() );
        QgsBatchedLayerAddController::instance()->start();
      }
      else
      {
        QgsCanvasRefreshBlocker refreshBlocker;
        const QList<QgsMapLayer *> added = QgsAppLayerHandling::addSublayers( selectedLayers, base, dlg.groupName() );
        QgsAppLayerHandling::postProcessAddedLayers( added );
      }
    }
    else if ( !selectedProjects.empty() )
    {
      QgisApp::instance()->openProject( selectedProjects.at( 0 ).uri() );
    }
  } );

  QgsApplication::taskManager()->addTask( task );
  return true;
}
