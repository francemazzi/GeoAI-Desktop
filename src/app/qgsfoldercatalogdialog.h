/***************************************************************************
    qgsfoldercatalogdialog.h
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

#ifndef QGSFOLDERCATALOGDIALOG_H
#define QGSFOLDERCATALOGDIALOG_H

#include "qgis_app.h"
#include "qgsprovidersublayerdetails.h"
#include "qgsprovidersublayermodel.h"

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QTreeView;
class QgsFilterLineEdit;
class QgsProviderSublayerProxyModel;

/**
 * A catalog of the datasets found inside a dropped folder.
 *
 * Lists every dataset discovered by QgsFolderScanTask as a tree (grouped by
 * subfolder and container), with a text filter. No layer is added to the map
 * until the user confirms a selection.
 */
class APP_EXPORT QgsFolderCatalogDialog : public QDialog
{
    Q_OBJECT

  public:
    QgsFolderCatalogDialog(
      const QString &folderPath, const QList<QgsProviderSublayerDetails> &details, const QList<QgsProviderSublayerModel::NonLayerItem> &projectItems, bool truncated, QWidget *parent = nullptr
    );
    ~QgsFolderCatalogDialog() override;

    //! Returns the layers selected by the user.
    QList<QgsProviderSublayerDetails> selectedLayers() const;

    //! Returns the non-layer items (project files) selected by the user.
    QList<QgsProviderSublayerModel::NonLayerItem> selectedNonLayerItems() const;

    //! Returns the name of the layer tree group to add layers to, or an empty string for no group.
    QString groupName() const;

  private slots:
    void selectAll();

  private:
    void updateOkButton();

    QgsProviderSublayerModel *mModel = nullptr;
    QgsProviderSublayerProxyModel *mProxyModel = nullptr;
    QTreeView *mTree = nullptr;
    QgsFilterLineEdit *mSearchLineEdit = nullptr;
    QCheckBox *mCbxAddToGroup = nullptr;
    QDialogButtonBox *mButtonBox = nullptr;
    QString mGroupName;
};

#endif // QGSFOLDERCATALOGDIALOG_H
