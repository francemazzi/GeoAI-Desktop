/***************************************************************************
    qgsfolderscantask.h
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
 *                                                                         *
 ***************************************************************************/

#ifndef QGSFOLDERSCANTASK_H
#define QGSFOLDERSCANTASK_H

#include "qgis_app.h"
#include "qgsprovidersublayerdetails.h"
#include "qgsprovidersublayermodel.h"
#include "qgstaskmanager.h"

#include <QMutex>
#include <QStringList>
#include <memory>

class QDir;
class QgsFeedback;

/**
 * Background task which enumerates the GIS datasets contained in a folder (recursively)
 * or in an explicit list of files, without creating any map layer.
 *
 * Candidate files are pre-filtered by extension, then resolved through
 * QgsProviderRegistry::querySublayers() with a fast scan; container datasets
 * (e.g. GeoPackages, ESRI File Geodatabases) are scanned in depth so their internal
 * layers are listed individually. QGIS project files are reported separately as
 * non-layer items.
 *
 * Each result's path() is set to the components of its location relative to the
 * scanned folder, so QgsProviderSublayerModel displays the results as a tree.
 */
class APP_EXPORT QgsFolderScanTask : public QgsTask
{
    Q_OBJECT

  public:
    //! Scans \a directory recursively, stopping after \a maxFiles candidate datasets.
    QgsFolderScanTask( const QString &directory, int maxFiles );

    //! Scans an explicit list of \a files (no recursion, no cap).
    explicit QgsFolderScanTask( const QStringList &files );

    void cancel() override;

    //! Returns the datasets found. Only valid after the task has completed.
    QList<QgsProviderSublayerDetails> results() const;

    //! Returns the QGIS project files found, as non-layer items.
    QList<QgsProviderSublayerModel::NonLayerItem> projectItems() const;

    //! Returns TRUE if the scan stopped at the maximum file cap before exhausting the folder.
    bool truncated() const;

    //! Returns the number of candidate datasets which were scanned.
    int scannedFileCount() const;

    //! Returns the folder being scanned (empty in explicit file-list mode).
    QString directory() const { return mDirectory; }

  protected:
    bool run() override;

  private:
    //! Resolves one candidate dataset, appending its sublayers to \a results.
    void scanDataset( const QString &path, const QDir &baseDir, bool isContainerDirectory, QList<QgsProviderSublayerDetails> &results );

    QString mDirectory;
    QStringList mFiles;
    int mMaxFiles = 0;

    std::unique_ptr<QgsFeedback> mFeedback;

    mutable QMutex mMutex;
    QList<QgsProviderSublayerDetails> mResults;
    QList<QgsProviderSublayerModel::NonLayerItem> mProjectItems;
    bool mTruncated = false;
    int mScannedFileCount = 0;
};

#endif // QGSFOLDERSCANTASK_H
