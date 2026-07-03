#include "MainWindow.h"

#include "ProjectFileDiscovery.h"
#include "../core/TherionFileTypes.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int kOutputArtifactPathRole = Qt::UserRole + 420;
constexpr int kOutputArtifactKindRole = Qt::UserRole + 421;

QString outputKindTitle(TherionStudio::ProjectOutputsScanner::ArtifactKind kind)
{
    switch (kind) {
    case TherionStudio::ProjectOutputsScanner::ArtifactKind::Model:
        return QApplication::translate("MainWindow", "Model");
    case TherionStudio::ProjectOutputsScanner::ArtifactKind::MapAtlas:
        return QApplication::translate("MainWindow", "Map / Atlas");
    case TherionStudio::ProjectOutputsScanner::ArtifactKind::Database:
        return QApplication::translate("MainWindow", "Database");
    }
    return QString();
}

QStandardItem *appendGroup(QStandardItemModel *model, const QString &title)
{
    auto *item = new QStandardItem(title);
    item->setEditable(false);
    item->setSelectable(false);
    model->appendRow(item);
    return item;
}

bool openOutputExternally(QWidget *parent, const QString &filePath)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

    QMessageBox::warning(parent,
                         QApplication::translate("MainWindow", "Open in External App"),
                         QApplication::translate("MainWindow",
                                                 "Failed to open %1 with the system default application.")
                             .arg(QDir::toNativeSeparators(filePath)));
    return false;
}

}

void MainWindow::buildOutputsSidebar()
{
    if (sidebarPages_ == nullptr || outputsModel_ == nullptr) {
        return;
    }

    outputsPage_ = new QWidget(sidebarPages_);
    auto *layout = new QVBoxLayout(outputsPage_);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    outputsStatusLabel_ = new QLabel(tr("Open a project to browse outputs."), outputsPage_);
    outputsStatusLabel_->setWordWrap(true);
    layout->addWidget(outputsStatusLabel_);

    outputsTree_ = new QTreeView(outputsPage_);
    outputsTree_->setModel(outputsModel_);
    outputsTree_->setRootIsDecorated(true);
    outputsTree_->setAnimated(true);
    outputsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    outputsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    outputsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    outputsTree_->setAlternatingRowColors(true);
    outputsTree_->header()->hide();
    connect(outputsTree_, &QTreeView::activated, this, &MainWindow::openProjectOutputArtifact);
    layout->addWidget(outputsTree_, 1);

    sidebarPages_->addWidget(outputsPage_);
    requestProjectOutputsRefresh();
}

void MainWindow::requestProjectOutputsRefresh()
{
    if (projectOutputsScanner_ == nullptr) {
        return;
    }
    projectOutputsScanner_->requestScan(projectRootPath_);
}

void MainWindow::handleProjectOutputsScanFinished(const TherionStudio::ProjectOutputsScanner::Result &result)
{
    if (outputsModel_ == nullptr) {
        return;
    }
    const QString currentProjectRootPath = TherionStudio::ProjectFileDiscovery::canonicalOrAbsolutePath(projectRootPath_);
    if (!projectRootPath_.trimmed().isEmpty()
        && !result.projectRootPath.isEmpty()
        && result.projectRootPath != currentProjectRootPath) {
        return;
    }

    outputsModel_->clear();
    outputsModel_->setHorizontalHeaderLabels({tr("Outputs")});

    if (!result.errorMessage.isEmpty()) {
        if (outputsStatusLabel_ != nullptr) {
            outputsStatusLabel_->setText(result.errorMessage);
        }
        return;
    }

    QStandardItem *modelGroup = appendGroup(outputsModel_, outputKindTitle(TherionStudio::ProjectOutputsScanner::ArtifactKind::Model));
    QStandardItem *mapAtlasGroup = appendGroup(outputsModel_, outputKindTitle(TherionStudio::ProjectOutputsScanner::ArtifactKind::MapAtlas));
    QStandardItem *databaseGroup = appendGroup(outputsModel_, outputKindTitle(TherionStudio::ProjectOutputsScanner::ArtifactKind::Database));

    int modelCount = 0;
    int mapAtlasCount = 0;
    int databaseCount = 0;
    for (const TherionStudio::ProjectOutputsScanner::Artifact &artifact : result.artifacts) {
        auto *item = new QStandardItem(artifact.relativePath);
        item->setEditable(false);
        item->setToolTip(QDir::toNativeSeparators(artifact.filePath));
        item->setData(artifact.filePath, kOutputArtifactPathRole);
        item->setData(static_cast<int>(artifact.kind), kOutputArtifactKindRole);

        switch (artifact.kind) {
        case TherionStudio::ProjectOutputsScanner::ArtifactKind::Model:
            modelGroup->appendRow(item);
            ++modelCount;
            break;
        case TherionStudio::ProjectOutputsScanner::ArtifactKind::MapAtlas:
            mapAtlasGroup->appendRow(item);
            ++mapAtlasCount;
            break;
        case TherionStudio::ProjectOutputsScanner::ArtifactKind::Database:
            databaseGroup->appendRow(item);
            ++databaseCount;
            break;
        }
    }

    modelGroup->setText(tr("Model (%1)").arg(modelCount));
    mapAtlasGroup->setText(tr("Map / Atlas (%1)").arg(mapAtlasCount));
    databaseGroup->setText(tr("Database (%1)").arg(databaseCount));

    if (outputsTree_ != nullptr) {
        outputsTree_->expandAll();
    }
    if (outputsStatusLabel_ != nullptr) {
        outputsStatusLabel_->setText(result.artifacts.isEmpty()
                                         ? tr("No Therion output artifacts found.")
                                         : tr("%1 output artifact(s) found.").arg(result.artifacts.size()));
    }
}

void MainWindow::openProjectOutputArtifact(const QModelIndex &index)
{
    if (outputsModel_ == nullptr || !index.isValid()) {
        return;
    }

    const QString filePath = index.data(kOutputArtifactPathRole).toString();
    if (filePath.isEmpty()) {
        return;
    }

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this,
                             tr("Open Output"),
                             tr("Output file no longer exists:\n%1").arg(QDir::toNativeSeparators(filePath)));
        requestProjectOutputsRefresh();
        return;
    }

    if (TherionStudio::isThreeDViewerArtifactFilePath(filePath)) {
        openThreeDViewerTab(filePath);
        return;
    }
    if (TherionStudio::isTherionSqlExportFilePath(filePath)) {
        openTherionSqlReportTab(filePath);
        return;
    }

    openOutputExternally(this, filePath);
}
