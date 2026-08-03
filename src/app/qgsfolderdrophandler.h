/***************************************************************************
    qgsfolderdrophandler.h
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

#ifndef QGSFOLDERDROPHANDLER_H
#define QGSFOLDERDROPHANDLER_H

#include "qgis_app.h"
#include "qgscustomdrophandler.h"

class QgsSettingsEntryInteger;

/**
 * Handles dropping a plain folder onto the QGIS window.
 *
 * Instead of failing with "Invalid Data Source", the folder is scanned in a
 * background task and a catalog dialog lists the datasets found, letting the
 * user choose what to load. Folders which are themselves OGR datasets
 * (e.g. ESRI File Geodatabases) are left to the regular OGR opening path.
 */
class APP_EXPORT QgsFolderDropHandler : public QgsCustomDropHandler
{
    Q_OBJECT

  public:
    // Strata: maximum number of candidate datasets scanned when cataloguing a dropped folder
    static const QgsSettingsEntryInteger *settingsMaxScannedFiles;

    QString customUriProviderKey() const override;
    bool handleFileDrop( const QString &file ) override;

    /**
     * Scans \a path in a background task and shows the catalog dialog when the
     * scan completes. Returns FALSE if \a path is not a plain folder.
     */
    static bool openFolderAsCatalog( const QString &path );
};

#endif // QGSFOLDERDROPHANDLER_H
