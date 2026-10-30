#include "MapEditorTab.h"
#include "MapEditorSceneItems.h"
#include "MapEditorXviBackgroundItem.h"
#include "../../../core/ProjectStructureIndex.h"

#include <QFileInfo>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QTreeView>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
QColor xviAltitudePaletteColor(double position, qreal opacity = 1.0)
{
    const double boundedPosition = qBound(0.0, position, 1.0);
    return QColor::fromHslF(0.78 - 0.72 * boundedPosition, 0.62, 0.72, qBound<qreal>(0.0, opacity, 1.0));
}

QStringList stationOwnerNamespaces(const ProjectIndexSnapshot &index, const QString &filePath)
{
    QSet<QString> namespaces;
    const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();
    for (const ProjectStructureEntry &entry : index.entries) {
        if (entry.kind == ProjectStructureEntryKind::Scrap
            && QFileInfo(entry.sourceFile).absoluteFilePath() == normalizedPath) {
            namespaces.insert(entry.namespacePath);
        }
    }
    QStringList result = namespaces.values();
    if (result.isEmpty()) {
        result.append(QString());
    }
    result.sort(Qt::CaseSensitive);
    return result;
}

std::optional<double> resolvedXviStationHeight(const QString &name,
                                               const QStringList &ownerNamespaces,
                                               const ProjectIndexSnapshot &index,
                                               const ScrapAltitudeStations &stations)
{
    if (name.isEmpty()) {
        return std::nullopt;
    }
    QString resolvedReference;
    for (const QString &ownerNamespace : ownerNamespaces) {
        const auto resolution = ProjectStructureIndex::resolveStationReference(index, name, ownerNamespace);
        if (resolution.state != ProjectStationReferenceResolutionState::Unique
            || !stations.heights.contains(resolution.qualifiedReference)
            || (!resolvedReference.isEmpty() && resolvedReference != resolution.qualifiedReference)) {
            return std::nullopt;
        }
        resolvedReference = resolution.qualifiedReference;
    }
    return resolvedReference.isEmpty()
        ? std::nullopt
        : std::optional<double>(stations.heights.value(resolvedReference));
}

double xviAltitudePalettePosition(double height, const ScrapAltitudeStations &stations)
{
    const double range = stations.maximumHeightMetres - stations.minimumHeightMetres;
    return range > 0.0
        ? qBound(0.0, (height - stations.minimumHeightMetres) / range, 1.0)
        : 0.5;
}
}

void MapEditorTab::setMapAppearance(const MapAppearanceSettings &appearance)
{
    if (!appearance.isValid()) {
        qWarning("Invalid map appearance settings; keeping previous values.");
        return;
    }
    mapAppearance_ = appearance;
    if (!mapScene_) {
        refreshXviAltitudeColors();
        updateXviAltitudeLegend();
        return;
    }
    for (auto *item : unsortedSceneItems(mapScene_)) {
        if (item->data(kMapAltitudeUnderlayRole).toBool()) {
            if (auto *path = dynamic_cast<QGraphicsPathItem *>(item)) {
                QColor color = path->brush().color();
                color.setAlphaF(appearance.altitudeUnderlayStrengthPercent / 100.0);
                path->setBrush(color);
            }
        } else if (auto *xvi = dynamic_cast<MapEditorXviBackgroundItem *>(item)) {
            xvi->setCenterlineAppearance(QColor(appearance.xviCenterlineColor), appearance.xviCenterlineWidth);
        }
    }
    refreshXviAltitudeColors();
    updateXviAltitudeLegend();
}

void MapEditorTab::setAltitudeUnderlayData(bool enabled, const ProjectIndexSnapshot &index,
                                         const ScrapAltitudeStations &stations)
{
    altitudeUnderlayEnabled_ = enabled;
    altitudeProjectIndex_ = index;
    altitudeStations_ = stations;
    refreshAltitudeUnderlay();
}

void MapEditorTab::refreshAltitudeUnderlay()
{
    altitudeUnderlayHint_.clear();
    refreshXviAltitudeColors();
    updateXviAltitudeLegend();
    const auto snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (!snapshot || !mapScene_) {
        return;
    }
    const auto scraps = altitudeUnderlayEnabled_
        ? mapEditorAltitudeScraps(*snapshot, filePath(), altitudeProjectIndex_, altitudeStations_,
                                 mapAppearance_.altitudeUnderlayStrengthPercent)
        : QHash<int, MapEditorAltitudeScrap>{};
    int approximate = 0;
    const int rendered = renderMapEditorAltitudeUnderlay(mapScene_, *snapshot, scraps, &approximate);
    // Underlay items carry scrap line numbers and were replaced without a scene generation change.
    sceneInteractionIndex_.invalidate();
    applyInspectorObjectVisibility();
    if (altitudeUnderlayEnabled_) {
        altitudeUnderlayHint_ = tr("Underlay: mean relative heights; categorical colors where altitude is unavailable.");
        if (!altitudeStations_.entranceHeightMetres) {
            altitudeUnderlayHint_ = tr("Underlay: categorical scrap colors — altitude unavailable (not altitude).");
        }
        altitudeUnderlayHint_ += QLatin1Char(' ')
            + tr("Passage outlines: %1 precise, %2 approximate, %3 unfilled.")
                .arg(rendered - approximate).arg(approximate).arg(scraps.size() - rendered);
    }
    emit statusHintChanged();
}

void MapEditorTab::refreshXviAltitudeColors()
{
    if (!mapScene_) {
        return;
    }
    const bool byAltitude = mapAppearance_.xviCenterlineColorMode
        == MapAppearanceSettings::XviCenterlineColorMode::ByAltitude;
    const bool hasDatum = altitudeStations_.entranceHeightMetres.has_value();
    const QStringList ownerNamespaces = stationOwnerNamespaces(altitudeProjectIndex_, filePath());
    QHash<QString, double> resolvedHeights;
    QSet<QString> resolvedNames;
    QSet<QString> unresolvedNames;
    const auto stationHeight = [&](const QString &name) -> std::optional<double> {
        if (resolvedNames.contains(name)) {
            return resolvedHeights.value(name);
        }
        if (unresolvedNames.contains(name)) {
            return std::nullopt;
        }
        const auto height = resolvedXviStationHeight(name, ownerNamespaces, altitudeProjectIndex_, altitudeStations_);
        if (height) {
            resolvedNames.insert(name);
            resolvedHeights.insert(name, *height);
        } else {
            unresolvedNames.insert(name);
        }
        return height;
    };
    const qreal opacity = QColor(mapAppearance_.xviCenterlineColor).alphaF();

    for (QGraphicsItem *item : unsortedSceneItems(mapScene_)) {
        auto *xvi = dynamic_cast<MapEditorXviBackgroundItem *>(item);
        if (xvi == nullptr) {
            continue;
        }
        const auto &stationPairs = xvi->geometryData().traverseShotStations;
        QVector<MapEditorXviTraverseColor> colors;
        colors.reserve(xvi->geometryData().traverseShotLines.size());
        for (int index = 0; index < xvi->geometryData().traverseShotLines.size(); ++index) {
            MapEditorXviTraverseColor color;
            if (byAltitude && hasDatum && index < stationPairs.size()) {
                const auto fromHeight = stationHeight(stationPairs.at(index).from);
                const auto toHeight = stationHeight(stationPairs.at(index).to);
                if (fromHeight && toHeight) {
                    color.from = xviAltitudePaletteColor(xviAltitudePalettePosition(*fromHeight, altitudeStations_), opacity);
                    color.to = xviAltitudePaletteColor(xviAltitudePalettePosition(*toHeight, altitudeStations_), opacity);
                    color.hasAltitude = true;
                }
            }
            colors.append(color);
        }
        xvi->setCenterlineAltitudeColors(colors, byAltitude);
    }
}

void MapEditorTab::updateXviAltitudeLegend()
{
    if (xviAltitudeLegend_ == nullptr) {
        if (mapObjectsTree_ == nullptr || mapObjectsTree_->parentWidget() == nullptr) {
            return;
        }
        auto *objectsTab = mapObjectsTree_->parentWidget();
        xviAltitudeLegend_ = new QWidget(objectsTab);
        xviAltitudeLegend_->setObjectName(QStringLiteral("xviAltitudeLegend"));
        auto *layout = new QVBoxLayout(xviAltitudeLegend_);
        layout->setContentsMargins(4, 4, 4, 2);
        layout->setSpacing(2);
        const auto label = [](QWidget *parent, const char *name) {
            auto *result = new QLabel(parent);
            result->setObjectName(QString::fromLatin1(name));
            return result;
        };
        xviAltitudeLegendTitle_ = label(xviAltitudeLegend_, "xviAltitudeLegendTitle");
        xviAltitudeLegendTitle_->setWordWrap(true);
        layout->addWidget(xviAltitudeLegendTitle_);
        xviAltitudeLegendBar_ = label(xviAltitudeLegend_, "xviAltitudeLegendBar");
        xviAltitudeLegendBar_->setFixedHeight(8);
        xviAltitudeLegendBar_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(xviAltitudeLegendBar_);
        xviAltitudeLegendTicks_ = new QWidget(xviAltitudeLegend_);
        auto *ticks = new QHBoxLayout(xviAltitudeLegendTicks_);
        ticks->setContentsMargins(0, 0, 0, 0);
        ticks->setSpacing(0);
        xviAltitudeLegendLow_ = label(xviAltitudeLegendTicks_, "xviAltitudeLegendLow");
        xviAltitudeLegendMiddle_ = label(xviAltitudeLegendTicks_, "xviAltitudeLegendMiddle");
        xviAltitudeLegendHigh_ = label(xviAltitudeLegendTicks_, "xviAltitudeLegendHigh");
        xviAltitudeLegendLow_->setAlignment(Qt::AlignLeft);
        xviAltitudeLegendMiddle_->setAlignment(Qt::AlignHCenter);
        xviAltitudeLegendHigh_->setAlignment(Qt::AlignRight);
        ticks->addWidget(xviAltitudeLegendLow_, 1);
        ticks->addWidget(xviAltitudeLegendMiddle_, 1);
        ticks->addWidget(xviAltitudeLegendHigh_, 1);
        layout->addWidget(xviAltitudeLegendTicks_);
        xviAltitudeLegendFallback_ = label(xviAltitudeLegend_, "xviAltitudeLegendFallback");
        xviAltitudeLegendFallback_->setWordWrap(true);
        layout->addWidget(xviAltitudeLegendFallback_);
        xviAltitudeLegendScrapFallback_ = label(xviAltitudeLegend_, "xviAltitudeLegendScrapFallback");
        xviAltitudeLegendScrapFallback_->setWordWrap(true);
        layout->addWidget(xviAltitudeLegendScrapFallback_);
        objectsTab->layout()->addWidget(xviAltitudeLegend_);
    }
    const bool byAltitude = mapAppearance_.xviCenterlineColorMode
        == MapAppearanceSettings::XviCenterlineColorMode::ByAltitude;
    xviAltitudeLegend_->setVisible(byAltitude);
    if (!byAltitude) {
        return;
    }

    const bool hasRange = altitudeStations_.entranceHeightMetres.has_value()
        && !altitudeStations_.heights.isEmpty();
    const QString entrance = altitudeStations_.entranceReference.isEmpty()
        ? tr("flagged entrance") : altitudeStations_.entranceReference;
    xviAltitudeLegendTitle_->setText(hasRange
        ? tr("XVI height relative to %1 (LOX; not absolute altitude)").arg(entrance)
        : tr("XVI height relative to a flagged LOX entrance"));
    xviAltitudeLegendBar_->setVisible(hasRange);
    xviAltitudeLegendTicks_->setVisible(hasRange);
    if (hasRange) {
        const double low = altitudeStations_.minimumHeightMetres - *altitudeStations_.entranceHeightMetres;
        const double high = altitudeStations_.maximumHeightMetres - *altitudeStations_.entranceHeightMetres;
        const double middle = (low + high) / 2.0;
        const QLocale locale;
        xviAltitudeLegendLow_->setText(locale.toString(low, 'f', 1) + QStringLiteral(" m"));
        xviAltitudeLegendMiddle_->setText(locale.toString(middle, 'f', 1) + QStringLiteral(" m"));
        xviAltitudeLegendHigh_->setText(locale.toString(high, 'f', 1) + QStringLiteral(" m"));

        QPixmap bar(240, 8);
        bar.fill(Qt::transparent);
        QPainter painter(&bar);
        QLinearGradient gradient(0.0, 0.0, bar.width(), 0.0);
        gradient.setColorAt(0.0, xviAltitudePaletteColor(0.0));
        gradient.setColorAt(1.0, xviAltitudePaletteColor(1.0));
        painter.fillRect(bar.rect(), gradient);
        xviAltitudeLegendBar_->setPixmap(bar);
    }
    int total = 0;
    int colored = 0;
    if (mapScene_) {
        for (QGraphicsItem *item : unsortedSceneItems(mapScene_)) {
            if (auto *xvi = dynamic_cast<MapEditorXviBackgroundItem *>(item)) {
                total += xvi->geometryData().traverseShotLines.size();
                for (const auto &color : xvi->centerlineAltitudeColors()) {
                    colored += color.hasAltitude ? 1 : 0;
                }
            }
        }
    }
    QString reason;
    if (total == 0) {
        reason = tr("No XVI centerline is loaded; TH2 survey lines are not XVI backgrounds.");
    } else if (altitudeStations_.sourceState == ScrapAltitudeStations::SourceState::AmbiguousOutputs) {
        reason = tr("Multiple LOX outputs (%1); a unique output is required.").arg(altitudeStations_.outputCount);
    } else if (altitudeStations_.sourceState == ScrapAltitudeStations::SourceState::LoadFailed) {
        reason = tr("LOX station metadata could not be loaded.");
    } else if (altitudeStations_.heights.isEmpty()) {
        reason = tr("LOX station heights are unavailable.");
    } else if (!altitudeStations_.entranceHeightMetres) {
        reason = tr("No flagged LOX entrance is available for the relative height datum.");
    } else {
        reason = tr("Missing or ambiguous endpoint heights use the configured XVI color.");
    }
    xviAltitudeLegendFallback_->setText(
        tr("XVI legs: %1 altitude-colored, %2 fallback.").arg(colored).arg(total - colored)
        + QLatin1Char('\n') + reason);
    xviAltitudeLegendScrapFallback_->setVisible(altitudeUnderlayEnabled_);
    if (altitudeUnderlayEnabled_) {
        xviAltitudeLegendScrapFallback_->setText(tr("Categorical scrap fallback colors are not altitude."));
    }
}
}
