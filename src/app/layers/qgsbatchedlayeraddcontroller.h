/***************************************************************************
    qgsbatchedlayeraddcontroller.h
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

#ifndef QGSBATCHEDLAYERADDCONTROLLER_H
#define QGSBATCHEDLAYERADDCONTROLLER_H

#include "qgis_app.h"
#include "qgsprovidersublayerdetails.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>

class QgsCanvasRefreshBlocker;
class QgsLayerTreeGroup;
class QgsMapLayer;
class QgsProxyProgressTask;
class QgsSettingsEntryInteger;
class QgsSettingsEntryInteger64;

/**
 * Adds a large selection of sublayers to the project in small main-thread batches.
 *
 * Layer creation must stay on the main thread (QgsMapLayer is not thread-safe), so
 * responsiveness comes from slicing the work: each timer tick adds a handful of layers
 * and then yields the event loop before the next batch. The canvas stays frozen for
 * the whole run (one render at the end), progress is reported through the task manager
 * with a cancellable proxy task, and cancelling keeps the layers already added.
 *
 * Per-layer datum-transform prompts are deferred and asked once per unique CRS at the
 * end; bulk post-processing runs once on completion. Very large layers are added
 * unchecked so the final render stays responsive.
 */
class APP_EXPORT QgsBatchedLayerAddController : public QObject
{
    Q_OBJECT

  public:
    // Strata: layers added per main-thread timer tick
    static const QgsSettingsEntryInteger *settingsBatchSize;
    // Strata: minimum number of layers for the batched controller to engage
    static const QgsSettingsEntryInteger *settingsBatchThreshold;
    // Strata: layers at/above this feature count are added unchecked after a mass add
    static const QgsSettingsEntryInteger64 *settingsHeavyLayerFeatureCount;

    explicit QgsBatchedLayerAddController( QObject *parent = nullptr );
    ~QgsBatchedLayerAddController() override;

    //! Returns the application-wide controller instance.
    static QgsBatchedLayerAddController *instance();

    /**
     * Queues \a details for batched addition. \a baseName and \a groupName follow
     * QgsAppLayerHandling::addSublayers() semantics. May be called while a run is
     * active to extend the current queue (the first enqueue's names win).
     */
    void enqueue( const QList<QgsProviderSublayerDetails> &details, const QString &baseName, const QString &groupName );

    //! Starts processing the queue. No-op if a run is already active.
    void start();

    //! Returns TRUE while a batched add is running.
    bool isActive() const { return mActive; }

  signals:
    //! Emitted before the first batch, so heavy background activity (e.g. AI indexing) can pause.
    void aboutToStart();

    //! Emitted after each batch with the number of layers added so far and the total queued.
    void batchProgress( int added, int total );

    //! Emitted when the run ends. \a cancelled is TRUE if the user cancelled; added layers are kept.
    void completed( const QList<QgsMapLayer *> &addedLayers, bool cancelled );

  private slots:
    void processNextBatch();

  private:
    void finish( bool cancelled );
    void uncheckHeavyLayers();
    void askDeferredDatumTransforms();
    void logImportSummary( bool cancelled ) const;

    QList<QgsProviderSublayerDetails> mPending;
    QList<QgsMapLayer *> mAdded;
    QString mBaseName;
    QString mGroupName;
    QgsLayerTreeGroup *mGroup = nullptr;
    QPointer<QgsProxyProgressTask> mProgressTask;
    std::unique_ptr<QgsCanvasRefreshBlocker> mRefreshBlocker;
    QTimer mTick;
    QElapsedTimer mElapsed;
    int mTotal = 0;
    bool mActive = false;
    bool mCancelRequested = false;
};

#endif // QGSBATCHEDLAYERADDCONTROLLER_H
