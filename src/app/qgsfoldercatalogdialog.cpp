/***************************************************************************
    qgsfoldercatalogdialog.cpp
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

#include "qgsfoldercatalogdialog.h"

#include <functional>

#include "qgsfilterlineedit.h"
#include "qgsgui.h"
#include "qgsmaplayer.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingsregistrygui.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTreeView>
#include <QVBoxLayout>

#include "moc_qgsfoldercatalogdialog.cpp"

using namespace Qt::StringLiterals;

QgsFolderCatalogDialog::QgsFolderCatalogDialog(
  const QString &folderPath, const QList<QgsProviderSublayerDetails> &details, const QList<QgsProviderSublayerModel::NonLayerItem> &projectItems, bool truncated, QWidget *parent
)
  : QDialog( parent )
{
  const QString folderName = QDir( folderPath ).dirName();
  setWindowTitle( tr( "Select Items to Add | %1" ).arg( folderName ) );
  setObjectName( u"QgsFolderCatalogDialog"_s );
  QgsGui::enableAutoGeometryRestore( this );

  mGroupName = folderName;
  if ( QgsSettingsRegistryGui::settingsFormatLayerName->value() )
    mGroupName = QgsMapLayer::formatLayerName( mGroupName );

  QVBoxLayout *layout = new QVBoxLayout();

  QLabel *summaryLabel = new QLabel( tr( "%n dataset(s) found in \"%1\". No layer is loaded until you add a selection.", nullptr, details.size() ).arg( QDir::toNativeSeparators( folderPath ) ) );
  summaryLabel->setWordWrap( true );
  layout->addWidget( summaryLabel );

  if ( truncated )
  {
    QLabel *truncatedLabel = new QLabel( tr( "The scan stopped at the configured maximum number of files — some datasets may not be listed." ) );
    truncatedLabel->setWordWrap( true );
    layout->addWidget( truncatedLabel );
  }

  mModel = new QgsProviderSublayerModel( this );
  mModel->setSublayerDetails( details );
  for ( const QgsProviderSublayerModel::NonLayerItem &item : projectItems )
    mModel->addNonLayerItem( item );

  mProxyModel = new QgsProviderSublayerProxyModel( this );
  mProxyModel->setSourceModel( mModel );

  mSearchLineEdit = new QgsFilterLineEdit();
  mSearchLineEdit->setShowSearchIcon( true );
  mSearchLineEdit->setPlaceholderText( tr( "Search…" ) );
  layout->addWidget( mSearchLineEdit );

  mTree = new QTreeView();
  mTree->setModel( mProxyModel );
  mTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
  mTree->expandAll();
  layout->addWidget( mTree, 1 );

  QHBoxLayout *selectionButtonsLayout = new QHBoxLayout();
  QPushButton *selectAllButton = new QPushButton( tr( "Select All" ) );
  QPushButton *deselectAllButton = new QPushButton( tr( "Deselect All" ) );
  selectionButtonsLayout->addWidget( selectAllButton );
  selectionButtonsLayout->addWidget( deselectAllButton );
  selectionButtonsLayout->addStretch();
  layout->addLayout( selectionButtonsLayout );

  mCbxAddToGroup = new QCheckBox( tr( "Add layers to a group" ) );
  mCbxAddToGroup->setChecked( QgsSettingsRegistryGui::settingsOpenSublayersInGroup->value() );
  layout->addWidget( mCbxAddToGroup );

  mButtonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
  mButtonBox->button( QDialogButtonBox::Ok )->setText( tr( "Add Selected" ) );
  mButtonBox->button( QDialogButtonBox::Ok )->setEnabled( false );
  layout->addWidget( mButtonBox );

  setLayout( layout );

  connect( selectAllButton, &QAbstractButton::pressed, this, &QgsFolderCatalogDialog::selectAll );
  connect( deselectAllButton, &QAbstractButton::pressed, this, [this] { mTree->selectionModel()->clear(); } );
  connect( mTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QgsFolderCatalogDialog::updateOkButton );
  connect( mSearchLineEdit, &QgsFilterLineEdit::textChanged, mProxyModel, &QgsProviderSublayerProxyModel::setFilterString );
  connect( mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );
  connect( mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
  connect( mTree, &QTreeView::doubleClicked, this, [this]( const QModelIndex &index ) {
    const QModelIndex left = mTree->model()->index( index.row(), 0, index.parent() );
    if ( !( left.flags() & Qt::ItemIsSelectable ) )
      return;
    mTree->selectionModel()
      ->select( QItemSelection( left, mTree->model()->index( index.row(), mTree->model()->columnCount() - 1, index.parent() ) ), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows );
    accept();
  } );
}

QgsFolderCatalogDialog::~QgsFolderCatalogDialog()
{
  QgsSettingsRegistryGui::settingsOpenSublayersInGroup->setValue( mCbxAddToGroup->isChecked() );
}

QList<QgsProviderSublayerDetails> QgsFolderCatalogDialog::selectedLayers() const
{
  const QModelIndexList selection = mTree->selectionModel()->selectedRows();
  QList<QgsProviderSublayerDetails> selectedSublayers;
  for ( const QModelIndex &index : selection )
  {
    const QModelIndex sourceIndex = mProxyModel->mapToSource( index );
    if ( !mModel->data( sourceIndex, static_cast<int>( QgsProviderSublayerModel::Role::IsNonLayerItem ) ).toBool() )
      selectedSublayers << mModel->indexToSublayer( sourceIndex );
  }
  return selectedSublayers;
}

QList<QgsProviderSublayerModel::NonLayerItem> QgsFolderCatalogDialog::selectedNonLayerItems() const
{
  const QModelIndexList selection = mTree->selectionModel()->selectedRows();
  QList<QgsProviderSublayerModel::NonLayerItem> selectedItems;
  for ( const QModelIndex &index : selection )
  {
    const QModelIndex sourceIndex = mProxyModel->mapToSource( index );
    if ( mModel->data( sourceIndex, static_cast<int>( QgsProviderSublayerModel::Role::IsNonLayerItem ) ).toBool() )
      selectedItems << mModel->indexToNonLayerItem( sourceIndex );
  }
  return selectedItems;
}

QString QgsFolderCatalogDialog::groupName() const
{
  if ( !mCbxAddToGroup->isChecked() )
    return QString();
  return mGroupName;
}

void QgsFolderCatalogDialog::selectAll()
{
  // recursively select every selectable row currently visible through the proxy
  std::function<void( const QModelIndex & )> selectChildren = [this, &selectChildren]( const QModelIndex &parent ) {
    const int rows = mProxyModel->rowCount( parent );
    for ( int row = 0; row < rows; row++ )
    {
      const QModelIndex index = mProxyModel->index( row, 0, parent );
      if ( index.flags() & Qt::ItemIsSelectable )
        mTree->selectionModel()->select( QItemSelection( index, mProxyModel->index( row, mProxyModel->columnCount() - 1, parent ) ), QItemSelectionModel::Select | QItemSelectionModel::Rows );
      selectChildren( index );
    }
  };
  selectChildren( QModelIndex() );
}

void QgsFolderCatalogDialog::updateOkButton()
{
  mButtonBox->button( QDialogButtonBox::Ok )->setEnabled( mTree->selectionModel()->hasSelection() );
}
