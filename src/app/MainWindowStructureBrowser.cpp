#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowStructureRoles.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "../core/TherionFileTypes.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QSignalBlocker>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyle>
#include <QStringList>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <functional>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "../core/ProjectStructureIndex.h"

namespace
{
constexpr auto kStructureActionFocusTargetConfig = "focus-target-config";

QString normalizedStructurePathKey(const QString &path);
QString relativeStructurePath(const QString &projectRootPath, const QString &path);

QString structureObjectKindLabel(const QString &category)
{
    if (category == QStringLiteral("Stations")) {
        return QStringLiteral("station");
    }
    if (category == QStringLiteral("Points")) {
        return QStringLiteral("point");
    }
    if (category == QStringLiteral("Lines")) {
        return QStringLiteral("line");
    }
    if (category == QStringLiteral("Areas")) {
        return QStringLiteral("area");
    }
    if (category == QStringLiteral("Scraps")) {
        return QStringLiteral("scrap");
    }
    if (category == QStringLiteral("Maps")) {
        return QStringLiteral("map");
    }
    if (category == QStringLiteral("Surveys")) {
        return QStringLiteral("survey");
    }
    if (category == QStringLiteral("Centrelines")) {
        return QStringLiteral("centerline");
    }

    return category;
}

QString formatProjectStructureSummary(const QHash<QString, int> &categoryCounts, int totalItems, int rootSurveyCount)
{
    const int surveyCount = categoryCounts.value(QStringLiteral("Surveys"));
    const int centrelineCount = categoryCounts.value(QStringLiteral("Centrelines"));
    const int mapCount = categoryCounts.value(QStringLiteral("Maps"));
    const int scrapCount = categoryCounts.value(QStringLiteral("Scraps"));
    const int stationCount = categoryCounts.value(QStringLiteral("Stations"));
    const int pointCount = categoryCounts.value(QStringLiteral("Points"));
    const int lineCount = categoryCounts.value(QStringLiteral("Lines"));
    const int areaCount = categoryCounts.value(QStringLiteral("Areas"));

    return QObject::tr("Structure items: %1; survey roots: %2; surveys: %3; centerlines: %4; maps: %5; scraps: %6; stations: %7; points: %8; lines: %9; areas: %10")
        .arg(totalItems)
        .arg(rootSurveyCount)
        .arg(surveyCount)
        .arg(centrelineCount)
        .arg(mapCount)
        .arg(scrapCount)
        .arg(stationCount)
        .arg(pointCount)
        .arg(lineCount)
        .arg(areaCount);
}

QStandardItem *createIndexedItem(const QString &text,
                                 const QString &sourceFile,
                                 int lineNumber,
                                 const QString &category,
                                 const QString &name,
                                 const QString &objectId = QString())
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(sourceFile, SourceFileRole);
    item->setData(lineNumber, LineNumberRole);
    item->setData(category, CategoryRole);
    item->setData(name, NameRole);
    item->setData(objectId, ObjectIdRole);
    return item;
}

QString structureExpansionKeyForIndex(const QModelIndex &index)
{
    if (!index.isValid()) {
        return QString();
    }

    const QString objectId = index.data(ObjectIdRole).toString();
    if (!objectId.isEmpty()) {
        return QStringLiteral("object:%1").arg(objectId);
    }

    const QString diagnosticKey = index.data(DiagnosticKeyRole).toString();
    if (!diagnosticKey.isEmpty()) {
        return QStringLiteral("diagnostic:%1").arg(diagnosticKey);
    }

    const QString action = index.data(ActionRole).toString();
    if (!action.isEmpty()) {
        return QStringLiteral("action:%1").arg(action);
    }

    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();
    const QString category = index.data(CategoryRole).toString();
    const QString name = index.data(NameRole).toString();
    if (!sourceFile.isEmpty() || !category.isEmpty() || !name.isEmpty()) {
        return QStringLiteral("source:%1:%2:%3:%4")
            .arg(normalizedStructurePathKey(sourceFile),
                 QString::number(lineNumber),
                 category,
                 name);
    }

    return QString();
}

QSet<QString> expandedStructureNodeKeys(QTreeView *tree)
{
    QSet<QString> expandedKeys;
    if (tree == nullptr || tree->model() == nullptr) {
        return expandedKeys;
    }

    const QAbstractItemModel *model = tree->model();
    std::function<void(const QModelIndex &)> visit = [&](const QModelIndex &parentIndex) {
        const int rowCount = model->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex index = model->index(row, 0, parentIndex);
            if (!index.isValid()) {
                continue;
            }

            const QString expansionKey = structureExpansionKeyForIndex(index);
            if (!expansionKey.isEmpty() && tree->isExpanded(index)) {
                expandedKeys.insert(expansionKey);
            }
            visit(index);
        }
    };
    visit(QModelIndex());
    return expandedKeys;
}

void restoreStructureNodeExpansion(QTreeView *tree, const QSet<QString> &expandedKeys)
{
    if (tree == nullptr || tree->model() == nullptr) {
        return;
    }

    const QAbstractItemModel *model = tree->model();
    std::function<void(const QModelIndex &)> visit = [&](const QModelIndex &parentIndex) {
        const int rowCount = model->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex index = model->index(row, 0, parentIndex);
            if (!index.isValid()) {
                continue;
            }

            const QString expansionKey = structureExpansionKeyForIndex(index);
            if (!expansionKey.isEmpty()) {
                tree->setExpanded(index, expandedKeys.contains(expansionKey));
            }
            visit(index);
        }
    };
    visit(QModelIndex());
}

QString diagnosticStructureKey(const TherionStudio::ProjectIndexDiagnostic &diagnostic)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(QString::number(static_cast<int>(diagnostic.kind)),
             diagnostic.sourceObjectId,
             normalizedStructurePathKey(diagnostic.sourceFile),
             diagnostic.referencedName,
             QString::number(diagnostic.candidateCount));
}

bool isStructureRelationshipDiagnostic(const TherionStudio::ProjectIndexDiagnostic &diagnostic)
{
    switch (diagnostic.kind) {
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapReference:
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapScrapReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownMapReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownMapScrapReference:
    case TherionStudio::ProjectIndexDiagnosticKind::MixedMapAndScrapReferences:
        return true;
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownJoinReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownJoinLinePointMark:
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousJoinReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownStationReference:
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousStationReference:
    case TherionStudio::ProjectIndexDiagnosticKind::DuplicateObjectId:
        return false;
    }
    return false;
}

QString diagnosticStructureItemText(const TherionStudio::ProjectIndexDiagnostic &diagnostic)
{
    switch (diagnostic.kind) {
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapReference:
        return QObject::tr("Ambiguous map: %1").arg(diagnostic.referencedName);
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapScrapReference:
        return QObject::tr("Ambiguous scrap: %1").arg(diagnostic.referencedName);
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownMapReference:
        return QObject::tr("Unresolved map: %1").arg(diagnostic.referencedName);
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownMapScrapReference:
        return QObject::tr("Unresolved scrap: %1").arg(diagnostic.referencedName);
    case TherionStudio::ProjectIndexDiagnosticKind::MixedMapAndScrapReferences:
        return QObject::tr("Mixed map/scrap content near: %1").arg(diagnostic.referencedName);
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownJoinReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownJoinLinePointMark:
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousJoinReference:
    case TherionStudio::ProjectIndexDiagnosticKind::UnknownStationReference:
    case TherionStudio::ProjectIndexDiagnosticKind::AmbiguousStationReference:
    case TherionStudio::ProjectIndexDiagnosticKind::DuplicateObjectId:
        break;
    }

    return QObject::tr("Unresolved reference: %1").arg(diagnostic.referencedName);
}

QString diagnosticStructureToolTip(const TherionStudio::ProjectIndexDiagnostic &diagnostic,
                                   const QString &projectRootPath)
{
    const QString relativePath = relativeStructurePath(projectRootPath, diagnostic.sourceFile);
    const QString sourceText = relativePath.isEmpty()
        ? QDir::toNativeSeparators(diagnostic.sourceFile)
        : relativePath;

    if (diagnostic.kind == TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapReference
        || diagnostic.kind == TherionStudio::ProjectIndexDiagnosticKind::AmbiguousMapScrapReference) {
        return QObject::tr("%1\nSource: %2:%3\nCandidates: %4")
            .arg(diagnosticStructureItemText(diagnostic),
                 sourceText,
                 QString::number(diagnostic.lineNumber),
                 QString::number(diagnostic.candidateCount));
    }

    return QObject::tr("%1\nSource: %2:%3")
        .arg(diagnosticStructureItemText(diagnostic),
             sourceText,
             QString::number(diagnostic.lineNumber));
}

QStandardItem *createDiagnosticItem(const TherionStudio::ProjectIndexDiagnostic &diagnostic,
                                    const QString &projectRootPath)
{
    auto *item = createIndexedItem(diagnosticStructureItemText(diagnostic),
                                   diagnostic.sourceFile,
                                   diagnostic.lineNumber,
                                   QStringLiteral("Diagnostics"),
                                   diagnostic.referencedName);
    item->setData(diagnosticStructureKey(diagnostic), DiagnosticKeyRole);

    item->setToolTip(diagnosticStructureToolTip(diagnostic, projectRootPath));

    if (QApplication::style() != nullptr) {
        item->setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
    item->setData(QColor(156, 96, 0), Qt::ForegroundRole);
    return item;
}

QString structureBrowserItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    const QString displayName = entry.category == QStringLiteral("Scraps")
            && entry.name == QStringLiteral("Unnamed Scrap")
        ? QObject::tr("Unnamed Scrap")
        : entry.name;
    if (entry.category == QStringLiteral("Surveys") && !entry.createsNamespace) {
        return QObject::tr("%1 (namespace off)").arg(displayName);
    }

    return displayName;
}

QString mapObjectItemText(const TherionStudio::ProjectStructureEntry &entry)
{
    const QString displayName = entry.category == QStringLiteral("Scraps")
            && entry.name == QStringLiteral("Unnamed Scrap")
        ? QObject::tr("Unnamed Scrap")
        : entry.name;
    if (entry.category == QStringLiteral("Scraps")) {
        return displayName;
    }

    return QStringLiteral("%1: %2").arg(structureObjectKindLabel(entry.category), displayName);
}

QPixmap renderStructureLucidePixmap(const QString &iconName, const QColor &color, int extent)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return QPixmap();
    }

    const qreal devicePixelRatio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    return pixmap;
}

QIcon structureLucideIcon(const QString &iconName)
{
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(renderStructureLucidePixmap(iconName, palette.color(QPalette::Text), 16), QIcon::Normal);
    icon.addPixmap(renderStructureLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::Text), 16), QIcon::Disabled);
    return icon;
}

QIcon structureItemIconForCategory(const QString &category)
{
    if (category == QStringLiteral("Surveys")) {
        return structureLucideIcon(QStringLiteral("compass"));
    }
    if (category == QStringLiteral("Maps")) {
        return structureLucideIcon(QStringLiteral("map"));
    }
    if (category == QStringLiteral("Scraps")) {
        return structureLucideIcon(QStringLiteral("puzzle"));
    }

    return QIcon();
}

QString normalizedStructurePathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QString relativeStructurePath(const QString &projectRootPath, const QString &path)
{
    const QString normalizedPath = normalizedStructurePathKey(path);
    if (normalizedPath.isEmpty()) {
        return QString();
    }

    const QString normalizedProjectRoot = normalizedStructurePathKey(projectRootPath);
    if (normalizedProjectRoot.isEmpty()) {
        return QDir::toNativeSeparators(normalizedPath);
    }

    const QString relativePath = QDir(normalizedProjectRoot).relativeFilePath(normalizedPath);
    return QDir::toNativeSeparators(relativePath);
}

QString projectIndexStructuralSignature(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    QStringList parts;
    parts.reserve(projectIndex.entries.size() * 9
                  + projectIndex.mapScrapReferencesByMapKey.size() * 4
                  + projectIndex.diagnostics.size() * 4);

    parts.append(QStringLiteral("root"));
    parts.append(normalizedStructurePathKey(projectIndex.projectRootPath));
    parts.append(normalizedStructurePathKey(projectIndex.rootConfigPath));
    QStringList rootFiles;
    rootFiles.reserve(projectIndex.rootFilePaths.size());
    for (const QString &rootFilePath : projectIndex.rootFilePaths) {
        rootFiles.append(normalizedStructurePathKey(rootFilePath));
    }
    std::sort(rootFiles.begin(), rootFiles.end());
    parts.append(rootFiles.join(QLatin1Char(',')));

    parts.append(QStringLiteral("entries"));
    parts.append(QString::number(projectIndex.entries.size()));
    for (const TherionStudio::ProjectStructureEntry &entry : projectIndex.entries) {
        parts.append(QString::number(static_cast<int>(entry.kind)));
        parts.append(entry.objectId);
        parts.append(entry.parentObjectId);
        parts.append(entry.category);
        parts.append(entry.name);
        parts.append(entry.namespacePath);
        parts.append(normalizedStructurePathKey(entry.sourceFile));
        parts.append(QString::number(entry.depth));
        parts.append(entry.createsNamespace ? QStringLiteral("1") : QStringLiteral("0"));
    }

    QStringList mapKeys = projectIndex.mapScrapReferencesByMapKey.keys();
    std::sort(mapKeys.begin(), mapKeys.end());
    parts.append(QStringLiteral("map-scrap-refs"));
    parts.append(QString::number(mapKeys.size()));
    for (const QString &mapKey : mapKeys) {
        QStringList scrapKeys = projectIndex.mapScrapReferencesByMapKey.value(mapKey).values();
        std::sort(scrapKeys.begin(), scrapKeys.end());
        parts.append(mapKey);
        parts.append(scrapKeys.join(QLatin1Char(',')));
    }

    QStringList parentMapKeys = projectIndex.mapChildReferencesByMapKey.keys();
    std::sort(parentMapKeys.begin(), parentMapKeys.end());
    parts.append(QStringLiteral("map-child-refs"));
    parts.append(QString::number(parentMapKeys.size()));
    for (const QString &mapKey : parentMapKeys) {
        QStringList childMapKeys = projectIndex.mapChildReferencesByMapKey.value(mapKey).values();
        std::sort(childMapKeys.begin(), childMapKeys.end());
        parts.append(mapKey);
        parts.append(childMapKeys.join(QLatin1Char(',')));
    }

    QVector<TherionStudio::ProjectIndexDiagnostic> structureDiagnostics;
    for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : projectIndex.diagnostics) {
        if (isStructureRelationshipDiagnostic(diagnostic)) {
            structureDiagnostics.append(diagnostic);
        }
    }

    parts.append(QStringLiteral("diagnostics"));
    parts.append(QString::number(structureDiagnostics.size()));
    for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : structureDiagnostics) {
        parts.append(QString::number(static_cast<int>(diagnostic.kind)));
        parts.append(diagnostic.sourceObjectId);
        parts.append(normalizedStructurePathKey(diagnostic.sourceFile));
        parts.append(diagnostic.referencedName);
        parts.append(QString::number(diagnostic.candidateCount));
    }

    return parts.join(QChar(0x1f));
}

int structureCategorySortRank(const QString &category)
{
    if (category == QStringLiteral("Surveys")) {
        return 0;
    }
    if (category == QStringLiteral("Maps")) {
        return 1;
    }
    if (category == QStringLiteral("Scraps")) {
        return 2;
    }
    if (category == QStringLiteral("Diagnostics")) {
        return 3;
    }

    return 4;
}

void sortStructureSiblingRows(QStandardItem *parentItem)
{
    if (parentItem == nullptr || parentItem->rowCount() <= 0) {
        return;
    }

    struct StructureSiblingRow
    {
        QList<QStandardItem *> items;
        int originalRow = 0;
    };

    QVector<StructureSiblingRow> rows;
    rows.reserve(parentItem->rowCount());
    const int rowCount = parentItem->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        rows.append(StructureSiblingRow{parentItem->takeRow(0), row});
    }

    std::stable_sort(rows.begin(), rows.end(), [](const StructureSiblingRow &left, const StructureSiblingRow &right) {
        const QStandardItem *leftItem = left.items.value(0);
        const QStandardItem *rightItem = right.items.value(0);
        const int leftRank = structureCategorySortRank(leftItem != nullptr ? leftItem->data(CategoryRole).toString() : QString());
        const int rightRank = structureCategorySortRank(rightItem != nullptr ? rightItem->data(CategoryRole).toString() : QString());
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }

        const QString leftName = leftItem != nullptr ? leftItem->data(NameRole).toString() : QString();
        const QString rightName = rightItem != nullptr ? rightItem->data(NameRole).toString() : QString();
        const int nameComparison = QString::localeAwareCompare(leftName.toLower(), rightName.toLower());
        if (nameComparison != 0) {
            return nameComparison < 0;
        }

        return left.originalRow < right.originalRow;
    });

    for (StructureSiblingRow &row : rows) {
        parentItem->appendRow(row.items);
    }
    for (int row = 0; row < parentItem->rowCount(); ++row) {
        sortStructureSiblingRows(parentItem->child(row));
    }
}

void updateStructureSourceLocationRoles(QStandardItemModel *model,
                                        const QString &projectRootPath,
                                        const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    if (model == nullptr) {
        return;
    }

    QHash<QString, TherionStudio::ProjectStructureEntry> entriesByObjectId;
    for (const TherionStudio::ProjectStructureEntry &entry : projectIndex.entries) {
        if (!entry.objectId.isEmpty()) {
            entriesByObjectId.insert(entry.objectId, entry);
        }
    }
    QHash<QString, TherionStudio::ProjectIndexDiagnostic> diagnosticsByKey;
    for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : projectIndex.diagnostics) {
        if (!isStructureRelationshipDiagnostic(diagnostic)) {
            continue;
        }
        diagnosticsByKey.insert(diagnosticStructureKey(diagnostic), diagnostic);
    }

    std::function<void(QStandardItem *)> visitItem = [&](QStandardItem *item) {
        if (item == nullptr) {
            return;
        }

        const QString objectId = item->data(ObjectIdRole).toString();
        const auto entryIt = entriesByObjectId.constFind(objectId);
        if (entryIt != entriesByObjectId.constEnd()) {
            item->setData(entryIt->sourceFile, SourceFileRole);
            item->setData(entryIt->lineNumber, LineNumberRole);
            if (entryIt->lineNumber > 0 && entryIt->category != QStringLiteral("File")) {
                item->setData(QStringLiteral("%1|%2|%3").arg(QDir(projectRootPath).absolutePath(),
                                                              entryIt->sourceFile)
                                                   .arg(entryIt->lineNumber),
                              OverrideKeyRole);
            }
        }
        const QString diagnosticKey = item->data(DiagnosticKeyRole).toString();
        const auto diagnosticIt = diagnosticsByKey.constFind(diagnosticKey);
        if (diagnosticIt != diagnosticsByKey.constEnd()) {
            item->setData(diagnosticIt->sourceFile, SourceFileRole);
            item->setData(diagnosticIt->lineNumber, LineNumberRole);
            item->setToolTip(diagnosticStructureToolTip(*diagnosticIt, projectRootPath));
        }

        for (int row = 0; row < item->rowCount(); ++row) {
            visitItem(item->child(row));
        }
    };

    for (int row = 0; row < model->rowCount(); ++row) {
        visitItem(model->item(row));
    }
}
}

void MainWindow::requestStructureSidebarRebuild()
{
    if (structureSidebarScanner_ == nullptr) {
        return;
    }

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        return;
    }

    QHash<QString, QString> inMemoryProjectContentsByPath;
    auto captureInMemoryStructureSource = [&inMemoryProjectContentsByPath](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }

        QString sourceFile;
        QString sourceText;
        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
            sourceFile = textTab->filePath();
            sourceText = textTab->text();
        } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
            sourceFile = mapTab->filePath();
            sourceText = mapTab->text();
        } else {
            return;
        }

        const QString normalizedPath = normalizedStructurePathKey(sourceFile);
        if (normalizedPath.isEmpty()) {
            return;
        }
        inMemoryProjectContentsByPath.insert(normalizedPath, sourceText);
    };

    for (int index = 0; editorTabs_ != nullptr && index < editorTabs_->count(); ++index) {
        captureInMemoryStructureSource(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        captureInMemoryStructureSource(detachedTab);
    }

    const QString preferredConfigPath = resolvedTherionTargetConfigPath();
    structureSidebarScanner_->requestScan(projectRootPath_,
                                          inMemoryProjectContentsByPath,
                                          preferredConfigPath);
}

void MainWindow::handleStructureSidebarScanFinished(const TherionStudio::ProjectStructureScanner::Result &result)
{
    if (structureSidebarScanner_ == nullptr
        || !structureSidebarScanner_->isLatestRequestResult(result)
        || normalizedStructurePathKey(result.projectRootPath) != normalizedStructurePathKey(projectRootPath_)) {
        return;
    }

    if (!result.errorMessage.isEmpty()) {
        appendConsoleLine(result.errorMessage);
    }

    if (!projectRootPath_.isEmpty() && QDir(projectRootPath_).exists()) {
        if (!result.errorMessage.isEmpty()) {
            showStructureSidebarMessage(result.errorMessage);
            return;
        }
        applyStructureSidebarIndex(result.projectIndex);
    }
}

void MainWindow::rebuildStructureSidebar()
{
    storeCurrentStructureExpansionState();

    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        structureExpandedNodeKeysByMode_.clear();
        structureExpansionProjectRootPath_.clear();
        hasStructureExpansionStateByMode_.clear();

        auto *rootItem = new QStandardItem(tr("Open a project to populate the survey hierarchy"));
        rootItem->setEditable(false);
        structureModel_->appendRow(rootItem);
        projectStructureSummary_ = tr("Open a project to view its survey hierarchy summary.");
        structureTree_->expandAll();
        return;
    }

    requestStructureSidebarRebuild();
}

void MainWindow::storeCurrentStructureExpansionState()
{
    if (!hasAppliedStructureSidebarIndex_ || structureTree_ == nullptr) {
        return;
    }

    const int modeKey = static_cast<int>(structureViewMode_);
    structureExpandedNodeKeysByMode_.insert(modeKey, expandedStructureNodeKeys(structureTree_));
    hasStructureExpansionStateByMode_.insert(modeKey, true);
}

void MainWindow::applyStructureSidebarIndex(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    lastStructureSidebarProjectIndex_ = projectIndex;

    const QString currentExpansionProjectRootPath = normalizedStructurePathKey(projectRootPath_);
    if (structureExpansionProjectRootPath_ != currentExpansionProjectRootPath) {
        structureExpansionProjectRootPath_ = currentExpansionProjectRootPath;
        structureExpandedNodeKeysByMode_.clear();
        hasStructureExpansionStateByMode_.clear();
    }

    const QString nextSignature = QStringLiteral("%1|%2")
                                      .arg(QString::number(static_cast<int>(structureViewMode_)),
                                           projectIndexStructuralSignature(projectIndex));
    if (hasAppliedStructureSidebarIndex_ && nextSignature == lastAppliedStructureSidebarSignature_) {
        updateStructureSidebarSourceLocations(projectIndex);
        return;
    }

    const int modeKey = static_cast<int>(structureViewMode_);
    QSet<QString> expandedNodeKeys = structureExpandedNodeKeysByMode_.value(modeKey);
    bool hasExpansionState = hasStructureExpansionStateByMode_.value(modeKey, false);
    if (hasAppliedStructureSidebarIndex_ && structureTree_ != nullptr) {
        expandedNodeKeys = expandedStructureNodeKeys(structureTree_);
        hasExpansionState = true;
    }

    hasAppliedStructureSidebarIndex_ = true;
    lastAppliedStructureSidebarSignature_ = nextSignature;

    const QVector<TherionStudio::ProjectStructureEntry> &entries = projectIndex.entries;

    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});

    QHash<QString, int> categoryCounts;
    int totalItems = 0;
    int rootSurveyCount = 0;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        categoryCounts[entry.category] += 1;
        ++totalItems;
        if (entry.category == QStringLiteral("Surveys") && entry.depth == 0) {
            ++rootSurveyCount;
        }
    }

    projectStructureSummary_ = tr("Project structure summary: %1")
                                   .arg(formatProjectStructureSummary(categoryCounts, totalItems, rootSurveyCount));
    const QString rootSummary = [&]() {
        if (!projectIndex.rootConfigPath.isEmpty()) {
            return tr("Root config: %1")
                .arg(relativeStructurePath(projectRootPath_, projectIndex.rootConfigPath));
        }
        if (!projectIndex.rootFilePaths.isEmpty()) {
            QStringList rootPaths;
            rootPaths.reserve(projectIndex.rootFilePaths.size());
            for (const QString &rootFilePath : projectIndex.rootFilePaths) {
                rootPaths.append(relativeStructurePath(projectRootPath_, rootFilePath));
            }
            return tr("Inferred root file(s): %1").arg(rootPaths.join(QStringLiteral(", ")));
        }

        return tr("No root config or source file resolved.");
    }();
    projectStructureSummary_ = QStringLiteral("%1\n%2").arg(projectStructureSummary_, rootSummary);
    if (structureTree_ != nullptr) {
        structureTree_->setToolTip(projectStructureSummary_);
    }
    structureModel_->setHeaderData(0, Qt::Horizontal, projectStructureSummary_, Qt::ToolTipRole);
    const auto includeInStructureView = [](const QString &category) {
        return category == QStringLiteral("Surveys")
            || category == QStringLiteral("Maps")
            || category == QStringLiteral("Scraps");
    };

    if (entries.isEmpty()) {
        auto *emptyItem = new QStandardItem(tr("No survey hierarchy was found in the selected project"));
        emptyItem->setEditable(false);
        structureModel_->appendRow(emptyItem);
    } else {
        const auto createStructureItem = [&](const TherionStudio::ProjectStructureEntry &entry) {
            QString entryName = entry.name;
            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryName = structureNameOverrides_.value(overrideKey, entry.name);
            }

            TherionStudio::ProjectStructureEntry displayEntry = entry;
            displayEntry.name = entryName;

            auto *entryItem = createIndexedItem(structureBrowserItemText(displayEntry),
                                                entry.sourceFile,
                                                entry.lineNumber,
                                                entry.category,
                                                entryName,
                                                entry.objectId);
            const QIcon entryIcon = structureItemIconForCategory(entry.category);
            if (!entryIcon.isNull()) {
                entryItem->setIcon(entryIcon);
            }

            if (entry.lineNumber > 0 && entry.category != QStringLiteral("File")) {
                const QString overrideKey = structureOverrideKey(entry.sourceFile, entry.lineNumber);
                entryItem->setData(overrideKey, OverrideKeyRole);
            }

            return entryItem;
        };

        if (structureViewMode_ == StructureViewMode::Map) {
            QHash<QString, TherionStudio::ProjectStructureEntry> entriesByKey;
            QStringList mapKeys;
            QSet<QString> referencedMapKeys;
            for (const TherionStudio::ProjectStructureEntry &entry : entries) {
                const QString key = TherionStudio::ProjectStructureIndex::structureEntryNodeKey(entry);
                entriesByKey.insert(key, entry);
                if (entry.category == QStringLiteral("Maps")) {
                    mapKeys.append(key);
                }
            }

            for (auto it = projectIndex.mapChildReferencesByMapKey.constBegin();
                 it != projectIndex.mapChildReferencesByMapKey.constEnd();
                 ++it) {
                for (const QString &childKey : it.value()) {
                    referencedMapKeys.insert(childKey);
                }
            }

            QHash<QString, QVector<TherionStudio::ProjectIndexDiagnostic>> diagnosticsByMapKey;
            for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : projectIndex.diagnostics) {
                if (isStructureRelationshipDiagnostic(diagnostic)) {
                    diagnosticsByMapKey[diagnostic.sourceObjectId].append(diagnostic);
                }
            }

            const auto sortedStructureKeys = [&](QSet<QString> keys) {
                QStringList sortedKeys = keys.values();
                std::sort(sortedKeys.begin(), sortedKeys.end(), [&](const QString &leftKey, const QString &rightKey) {
                    const TherionStudio::ProjectStructureEntry leftEntry = entriesByKey.value(leftKey);
                    const TherionStudio::ProjectStructureEntry rightEntry = entriesByKey.value(rightKey);
                    const int leftRank = structureCategorySortRank(leftEntry.category);
                    const int rightRank = structureCategorySortRank(rightEntry.category);
                    if (leftRank != rightRank) {
                        return leftRank < rightRank;
                    }
                    const int nameComparison = QString::localeAwareCompare(leftEntry.name.toLower(), rightEntry.name.toLower());
                    if (nameComparison != 0) {
                        return nameComparison < 0;
                    }
                    return leftKey < rightKey;
                });
                return sortedKeys;
            };

            const auto appendDiagnosticsForMap = [&](QStandardItem *parentItem, const QString &mapKey) {
                if (parentItem == nullptr) {
                    return;
                }
                const QVector<TherionStudio::ProjectIndexDiagnostic> diagnostics = diagnosticsByMapKey.value(mapKey);
                for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : diagnostics) {
                    parentItem->appendRow(createDiagnosticItem(diagnostic, projectRootPath_));
                }
            };

            std::function<QStandardItem *(const QString &, QSet<QString>)> createMapCompositionItem =
                [&](const QString &key, QSet<QString> ancestorKeys) -> QStandardItem * {
                const auto entryIt = entriesByKey.constFind(key);
                if (entryIt == entriesByKey.constEnd()) {
                    return nullptr;
                }

                QStandardItem *item = createStructureItem(entryIt.value());
                if (ancestorKeys.contains(key)) {
                    auto *cycleItem = new QStandardItem(tr("Cycle: %1").arg(entryIt->name));
                    cycleItem->setEditable(false);
                    cycleItem->setData(QStringLiteral("Diagnostics"), CategoryRole);
                    item->appendRow(cycleItem);
                    return item;
                }

                ancestorKeys.insert(key);

                for (const QString &childMapKey : sortedStructureKeys(projectIndex.mapChildReferencesByMapKey.value(key))) {
                    QStandardItem *childItem = createMapCompositionItem(childMapKey, ancestorKeys);
                    if (childItem != nullptr) {
                        item->appendRow(childItem);
                    }
                }

                for (const QString &scrapKey : sortedStructureKeys(projectIndex.mapScrapReferencesByMapKey.value(key))) {
                    const auto scrapIt = entriesByKey.constFind(scrapKey);
                    if (scrapIt != entriesByKey.constEnd()) {
                        item->appendRow(createStructureItem(scrapIt.value()));
                    }
                }

                appendDiagnosticsForMap(item, key);
                return item;
            };

            QStringList rootMapKeys;
            for (const QString &mapKey : std::as_const(mapKeys)) {
                if (!referencedMapKeys.contains(mapKey)) {
                    rootMapKeys.append(mapKey);
                }
            }
            if (rootMapKeys.isEmpty()) {
                rootMapKeys = mapKeys;
            }
            std::sort(rootMapKeys.begin(), rootMapKeys.end(), [&](const QString &leftKey, const QString &rightKey) {
                const TherionStudio::ProjectStructureEntry leftEntry = entriesByKey.value(leftKey);
                const TherionStudio::ProjectStructureEntry rightEntry = entriesByKey.value(rightKey);
                const int nameComparison = QString::localeAwareCompare(leftEntry.name.toLower(), rightEntry.name.toLower());
                if (nameComparison != 0) {
                    return nameComparison < 0;
                }
                return leftKey < rightKey;
            });

            if (rootMapKeys.isEmpty()) {
                auto *emptyItem = new QStandardItem(tr("No map composition was found in the selected project"));
                emptyItem->setEditable(false);
                structureModel_->appendRow(emptyItem);
            } else {
                for (const QString &mapKey : std::as_const(rootMapKeys)) {
                    QStandardItem *mapItem = createMapCompositionItem(mapKey, {});
                    if (mapItem != nullptr) {
                        structureModel_->appendRow(mapItem);
                    }
                }
            }
            sortStructureSiblingRows(structureModel_->invisibleRootItem());
        } else {
        struct VisibleStructureNode
        {
            TherionStudio::ProjectStructureEntry entry;
            QString entryName;
            QString nodeKey;
            QStandardItem *item = nullptr;
        };

        QVector<VisibleStructureNode> nodes;
        nodes.reserve(entries.size());

        for (const TherionStudio::ProjectStructureEntry &entry : entries) {
            if (!includeInStructureView(entry.category)) {
                continue;
            }

            QStandardItem *entryItem = createStructureItem(entry);

            VisibleStructureNode node;
            node.entry = entry;
            node.entryName = entryItem->data(NameRole).toString();
            node.nodeKey = TherionStudio::ProjectStructureIndex::structureEntryNodeKey(entry);
            node.item = entryItem;

            nodes.append(node);
        }

        if (nodes.isEmpty()) {
            auto *emptyItem = new QStandardItem(tr("No surveys, maps, or scraps were found in the selected project"));
            emptyItem->setEditable(false);
            structureModel_->appendRow(emptyItem);
            if (hasExpansionState) {
                restoreStructureNodeExpansion(structureTree_, expandedNodeKeys);
            } else {
                structureTree_->expandAll();
            }
            structureExpandedNodeKeysByMode_.insert(modeKey, expandedStructureNodeKeys(structureTree_));
            hasStructureExpansionStateByMode_.insert(modeKey, true);
            return;
        }

        QHash<QString, QStandardItem *> mapItemByKey;
        for (const VisibleStructureNode &node : std::as_const(nodes)) {
            if (node.entry.category == QStringLiteral("Maps")) {
                mapItemByKey.insert(node.nodeKey, node.item);
            }
        }

        QVector<int> visibleNodeIndexByDepth;
        for (int nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            VisibleStructureNode &node = nodes[nodeIndex];

            while (visibleNodeIndexByDepth.size() <= node.entry.depth) {
                visibleNodeIndexByDepth.append(-1);
            }
            for (int depth = node.entry.depth; depth < visibleNodeIndexByDepth.size(); ++depth) {
                visibleNodeIndexByDepth[depth] = -1;
            }

            QStandardItem *parentItem = nullptr;
            for (int parentDepth = node.entry.depth - 1; parentDepth >= 0; --parentDepth) {
                if (parentDepth >= visibleNodeIndexByDepth.size()) {
                    continue;
                }
                const int parentNodeIndex = visibleNodeIndexByDepth.at(parentDepth);
                if (parentNodeIndex >= 0 && parentNodeIndex < nodes.size()) {
                    parentItem = nodes.at(parentNodeIndex).item;
                    break;
                }
            }

            if (parentItem != nullptr) {
                parentItem->appendRow(node.item);
            } else {
                structureModel_->appendRow(node.item);
            }

            visibleNodeIndexByDepth[node.entry.depth] = nodeIndex;
        }

        for (const TherionStudio::ProjectIndexDiagnostic &diagnostic : projectIndex.diagnostics) {
            if (!isStructureRelationshipDiagnostic(diagnostic)) {
                continue;
            }
            QStandardItem *parentItem = mapItemByKey.value(diagnostic.sourceObjectId, nullptr);
            if (parentItem == nullptr) {
                parentItem = structureModel_->invisibleRootItem();
            }

            parentItem->appendRow(createDiagnosticItem(diagnostic, projectRootPath_));
        }
        sortStructureSiblingRows(structureModel_->invisibleRootItem());
        }
    }

    if (hasExpansionState) {
        restoreStructureNodeExpansion(structureTree_, expandedNodeKeys);
    } else {
        structureTree_->expandAll();
    }
    structureExpandedNodeKeysByMode_.insert(modeKey, expandedStructureNodeKeys(structureTree_));
    hasStructureExpansionStateByMode_.insert(modeKey, true);
}

void MainWindow::showStructureSidebarMessage(const QString &message)
{
    structureModel_->clear();
    structureModel_->setHorizontalHeaderLabels({tr("Name")});
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();

    auto *messageItem = new QStandardItem(message);
    messageItem->setEditable(false);
    messageItem->setToolTip(message);
    structureModel_->appendRow(messageItem);

    auto *actionItem = new QStandardItem(tr("Select Target Config in Compiler"));
    actionItem->setEditable(false);
    actionItem->setData(QString::fromLatin1(kStructureActionFocusTargetConfig), ActionRole);
    actionItem->setToolTip(tr("Open the Compiler pane and focus the Target Config field."));
    structureModel_->appendRow(actionItem);

    projectStructureSummary_ = message;
    structureTree_->expandAll();
}

bool MainWindow::activateStructureSidebarAction(const QString &action)
{
    if (action != QString::fromLatin1(kStructureActionFocusTargetConfig)) {
        return false;
    }

    showSidebarPane(SidebarPane::Console);
    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex = therionRunTargetCombo_->findData(QStringLiteral("project"));
        if (projectIndex >= 0) {
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }
    if (therionTargetConfigEdit_ != nullptr) {
        therionTargetConfigEdit_->setFocus(Qt::ShortcutFocusReason);
        therionTargetConfigEdit_->selectAll();
    }
    return true;
}

void MainWindow::updateStructureSidebarSourceLocations(const TherionStudio::ProjectIndexSnapshot &projectIndex)
{
    updateStructureSourceLocationRoles(structureModel_, projectRootPath_, projectIndex);
}

void MainWindow::rebuildMapObjectsTree()
{
    if (mapObjectsModel_ == nullptr) {
        return;
    }

    mapObjectsModel_->clear();
    mapObjectsModel_->setHorizontalHeaderLabels({tr("Objects")});

    QWidget *tabWidget = currentDocumentWidget();
    const QString filePath = tabWidget != nullptr ? documentPathForWidget(tabWidget) : QString();
    if (tabWidget == nullptr || !filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        refreshMapBackgroundPanel();
        return;
    }

    QString text;
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        text = textTab->text();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        text = mapTab->text();
    }

    const QVector<TherionStudio::ProjectStructureEntry> entries = TherionStudio::ProjectStructureIndex::scanTh2Objects(filePath, text);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        mapObjectsModel_->appendRow(placeholderItem);
        refreshMapBackgroundPanel();
        return;
    }

    QVector<QStandardItem *> parentStack;
    for (const TherionStudio::ProjectStructureEntry &entry : entries) {
        while (parentStack.size() > entry.depth) {
            parentStack.removeLast();
        }

        auto *entryItem = createIndexedItem(mapObjectItemText(entry), entry.sourceFile, entry.lineNumber, entry.category, entry.name);
        QStandardItem *parentItem = parentStack.isEmpty() ? mapObjectsModel_->invisibleRootItem() : parentStack.last();
        parentItem->appendRow(entryItem);
        parentStack.append(entryItem);
    }

    if (mapObjectsTree_ != nullptr) {
        mapObjectsTree_->expandAll();
    }
    updateMapObjectSelectionFromEditorLocation(filePath, documentCurrentLineNumberForWidget(tabWidget));
    refreshMapBackgroundPanel();
}

void MainWindow::openStructureSourceIndex(const QModelIndex &current, QTreeView *)
{
    if (!current.isValid()) {
        return;
    }

    if (activateStructureSidebarAction(current.data(ActionRole).toString())) {
        return;
    }

    if (isStructureContainerIndex(current)) {
        return;
    }

    const QString sourceFile = current.data(SourceFileRole).toString();
    const int lineNumber = current.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = nullptr;
    if (TherionStudio::isThreeDViewerArtifactFilePath(sourceFile)) {
        tabWidget = openThreeDViewerTab(sourceFile);
    } else if (QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")) {
        tabWidget = static_cast<QWidget *>(openMapEditorTab(sourceFile));
    } else {
        tabWidget = static_cast<QWidget *>(openTextTab(sourceFile));
    }
    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}

void MainWindow::handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree)
{
    QTreeView *tree = sourceTree != nullptr ? sourceTree : structureTree_;
    if (!index.isValid()) {
        return;
    }

    if (activateStructureSidebarAction(index.data(ActionRole).toString())) {
        return;
    }

    if (isStructureContainerIndex(index)) {
        if (tree != nullptr && tree->isExpanded(index)) {
            tree->collapse(index);
        } else {
            if (tree != nullptr) {
                tree->expand(index);
            }
        }

        const QModelIndex sourceIndex = firstStructureSourceIndex(index);
        if (sourceIndex.isValid()) {
            if (tree != nullptr) {
                tree->setCurrentIndex(sourceIndex);
            }
        }
        return;
    }

    openStructureSourceIndex(index, tree);
}

void MainWindow::updateMapEditorActionState()
{
    refreshMapBackgroundPanel();
}

QModelIndex MainWindow::firstStructureSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() == nullptr) {
        return QModelIndex();
    }

    if (!index.data(SourceFileRole).toString().isEmpty() && index.data(LineNumberRole).toInt() > 0) {
        return index;
    }

    const QAbstractItemModel *model = index.model();
    const int rowCount = model->rowCount(index);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, 0, index);
        const QModelIndex sourceIndex = firstStructureSourceIndex(childIndex);
        if (sourceIndex.isValid()) {
            return sourceIndex;
        }
    }

    return QModelIndex();
}

bool MainWindow::isStructureContainerIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();
    return category == QStringLiteral("Project") || sourceFile.isEmpty() || lineNumber <= 0;
}

QModelIndex MainWindow::findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (structureModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = structureModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = structureModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    const QModelIndex targetIndex = findStructureIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == structureTree_->currentIndex()) {
        return;
    }

    if (structureTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(structureTree_->selectionModel());
        structureTree_->setCurrentIndex(targetIndex);
    } else {
        structureTree_->setCurrentIndex(targetIndex);
    }

    structureTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
}

QModelIndex MainWindow::findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const
{
    if (mapObjectsModel_ == nullptr || filePath.isEmpty() || lineNumber <= 0) {
        return QModelIndex();
    }

    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
                                      ? QFileInfo(filePath).absoluteFilePath()
                                      : QFileInfo(filePath).canonicalFilePath();

    QModelIndex bestIndex;
    int bestLineNumber = -1;

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = mapObjectsModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = mapObjectsModel_->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const QString childSourceFile = childIndex.data(SourceFileRole).toString();
            const QString candidateSourceFile = QFileInfo(childSourceFile).canonicalFilePath().isEmpty()
                                                    ? QFileInfo(childSourceFile).absoluteFilePath()
                                                    : QFileInfo(childSourceFile).canonicalFilePath();
            const int candidateLineNumber = childIndex.data(LineNumberRole).toInt();

            if (candidateSourceFile == canonicalPath && candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };

    visitNode(QModelIndex());
    return bestIndex;
}

void MainWindow::updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber)
{
    if (mapObjectsTree_ == nullptr) {
        return;
    }

    const QModelIndex targetIndex = findMapObjectIndexForSourceLocation(filePath, lineNumber);
    if (!targetIndex.isValid() || targetIndex == mapObjectsTree_->currentIndex()) {
        return;
    }

    if (mapObjectsTree_->selectionModel() != nullptr) {
        QSignalBlocker blocker(mapObjectsTree_->selectionModel());
        mapObjectsTree_->setCurrentIndex(targetIndex);
    } else {
        mapObjectsTree_->setCurrentIndex(targetIndex);
    }

    mapObjectsTree_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
}
