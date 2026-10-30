#include "ThreeDViewerLayerListModel.h"

#include "../LucideIconFactory.h"

#include <QApplication>

namespace TherionStudio
{

ThreeDViewerLayerListModel::ThreeDViewerLayerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    rebuildEntries();
}

int ThreeDViewerLayerListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

QVariant ThreeDViewerLayerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return {};
    }

    const Entry &entry = entries_.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return entryLabel(entry);
    case Qt::DecorationRole:
        return entry.kind == EntryKind::Layer ? themedLucideIcon(layerIconName(entry.layer), QApplication::palette(), 16, 1.0) : QIcon();
    case Qt::CheckStateRole:
    case VisibleRole:
        return entryCheckState(entry);
    case LayerRole:
        return static_cast<int>(entry.layer);
    case CountRole:
        return entryItemCount(entry);
    case IconNameRole:
        return entry.kind == EntryKind::Layer ? layerIconName(entry.layer) : QString();
    case IndentRole:
        return entry.kind == EntryKind::Layer ? 0 : 1;
    case HasChildrenRole:
        return entryHasChildren(entry);
    case RowRole:
        return index.row();
    default:
        return {};
    }
}

Qt::ItemFlags ThreeDViewerLayerListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

bool ThreeDViewerLayerListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return false;
    }

    if (role != Qt::CheckStateRole && role != VisibleRole) {
        return false;
    }

    const bool visible = value.toInt() == Qt::Checked || value.toBool();
    const Entry entry = entries_.at(index.row());
    if (entryVisible(entry) == visible && entryCheckState(entry) != Qt::PartiallyChecked) {
        return false;
    }

    setEntryVisible(entry, visible);
    emit dataChanged(this->index(0, 0), this->index(entries_.size() - 1, 0), {Qt::DisplayRole, Qt::DecorationRole, Qt::CheckStateRole, VisibleRole});
    emit layerVisibilityChanged(index.row(), visible);
    return true;
}

QHash<int, QByteArray> ThreeDViewerLayerListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {Qt::DecorationRole, "decoration"},
        {Qt::CheckStateRole, "layerChecked"},
        {IconNameRole, "iconName"},
        {LayerRole, "layerIndex"},
        {VisibleRole, "visible"},
        {CountRole, "count"},
        {LabelRole, "label"},
        {IndentRole, "indent"},
        {HasChildrenRole, "hasChildren"},
        {RowRole, "rowIndex"},
    };
}

void ThreeDViewerLayerListModel::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    beginResetModel();
    sceneModel_ = sceneModel;
    rebuildEntries();
    endResetModel();
}

const ThreeDViewerSceneModel &ThreeDViewerLayerListModel::sceneModel() const
{
    return sceneModel_;
}

bool ThreeDViewerLayerListModel::layerVisible(Layer layer) const
{
    const int index = layerIndex(layer);
    return index >= 0 && index < layerCount() ? layerVisibility_[index] : false;
}

bool ThreeDViewerLayerListModel::layerVisible(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= layerCount()) {
        return false;
    }
    return layerVisible(static_cast<Layer>(layerIndex));
}

void ThreeDViewerLayerListModel::setLayerVisible(Layer layer, bool visible)
{
    const int index = layerIndex(layer);
    if (index < 0 || index >= layerCount() || layerVisible(layer) == visible) {
        return;
    }

    layerVisibility_[index] = visible;
    const QModelIndex modelIndex = layerModelIndex(layer);
    emit dataChanged(modelIndex, modelIndex, {Qt::DisplayRole, Qt::DecorationRole, Qt::CheckStateRole, VisibleRole});
    emit layerVisibilityChanged(index, visible);
}

void ThreeDViewerLayerListModel::setLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex < 0 || layerIndex >= entries_.size()) {
        return;
    }
    const Entry entry = entries_.at(layerIndex);
    if (entryVisible(entry) == visible && entryCheckState(entry) != Qt::PartiallyChecked) {
        return;
    }
    setEntryVisible(entry, visible);
    emit dataChanged(index(0, 0), index(entries_.size() - 1, 0), {Qt::DisplayRole, Qt::DecorationRole, Qt::CheckStateRole, VisibleRole});
    emit layerVisibilityChanged(layerIndex, visible);
}

std::array<bool, static_cast<int>(ThreeDViewerLayerListModel::Layer::Count)> ThreeDViewerLayerListModel::layerVisibility() const
{
    return layerVisibility_;
}

ThreeDViewerLayerListModel::FeatureVisibility ThreeDViewerLayerListModel::featureVisibility() const
{
    return featureVisibility_;
}

void ThreeDViewerLayerListModel::rebuildEntries()
{
    entries_.clear();
    for (int index = 0; index < layerCount(); ++index) {
        const Layer layer = static_cast<Layer>(index);
        if (layer == Layer::Surfaces) {
            continue;
        }
        entries_.append(Entry{EntryKind::Layer, layer, Feature::Regular});

        if (layer != Layer::Centerline && layer != Layer::Stations) {
            continue;
        }

        const QVector<Feature> centerlineFeatures = {
            Feature::Surface,
            Feature::Regular,
            Feature::Splay,
            Feature::Duplicate,
            Feature::Hidden,
        };
        const QVector<Feature> stationFeatures = {
            Feature::Entrance,
            Feature::Fixed,
            Feature::OtherStation,
        };
        const QVector<Feature> &features = layer == Layer::Centerline ? centerlineFeatures : stationFeatures;
        QVector<Feature> presentFeatures;
        presentFeatures.reserve(features.size());
        for (Feature feature : features) {
            if (!featurePresentForLayer(layer, feature)) {
                continue;
            }
            presentFeatures.append(feature);
        }

        if (layer == Layer::Stations && presentFeatures.size() < 2) {
            continue;
        }

        for (Feature feature : presentFeatures) {
            const EntryKind kind = layer == Layer::Centerline
                ? EntryKind::CenterlineFeature
                : EntryKind::StationFeature;
            entries_.append(Entry{kind, layer, feature});
        }
    }
}

int ThreeDViewerLayerListModel::entryIndexForLayer(Layer layer) const
{
    for (int index = 0; index < entries_.size(); ++index) {
        const Entry &entry = entries_.at(index);
        if (entry.kind == EntryKind::Layer && entry.layer == layer) {
            return index;
        }
    }
    return -1;
}

bool ThreeDViewerLayerListModel::entryHasChildren(const Entry &entry) const
{
    if (entry.kind != EntryKind::Layer) {
        return false;
    }

    for (const Entry &candidate : entries_) {
        if (candidate.kind != EntryKind::Layer && candidate.layer == entry.layer) {
            return true;
        }
    }
    return false;
}

Qt::CheckState ThreeDViewerLayerListModel::entryCheckState(const Entry &entry) const
{
    if (entry.kind != EntryKind::Layer) {
        return entryVisible(entry) ? Qt::Checked : Qt::Unchecked;
    }

    if (!layerVisible(entry.layer)) {
        return Qt::Unchecked;
    }

    bool hasFeature = false;
    bool hasVisibleFeature = false;
    bool hasHiddenFeature = false;
    for (const Entry &candidate : entries_) {
        if (candidate.kind == EntryKind::Layer || candidate.layer != entry.layer) {
            continue;
        }
        hasFeature = true;
        if (entryVisible(candidate)) {
            hasVisibleFeature = true;
        } else {
            hasHiddenFeature = true;
        }
    }
    if (hasFeature && hasVisibleFeature && hasHiddenFeature) {
        return Qt::PartiallyChecked;
    }
    return Qt::Checked;
}

void ThreeDViewerLayerListModel::setEntryVisible(const Entry &entry, bool visible)
{
    if (entry.kind == EntryKind::Layer) {
        layerVisibility_[layerIndex(entry.layer)] = visible;
        for (const Entry &candidate : entries_) {
            if (candidate.kind == EntryKind::Layer || candidate.layer != entry.layer) {
                continue;
            }
            if (candidate.kind == EntryKind::CenterlineFeature) {
                featureVisibility_.centerline[static_cast<int>(candidate.feature)] = visible;
            } else if (candidate.kind == EntryKind::StationFeature) {
                featureVisibility_.stations[static_cast<int>(candidate.feature)] = visible;
            } else if (candidate.kind == EntryKind::LabelFeature) {
                featureVisibility_.labels[static_cast<int>(candidate.feature)] = visible;
            }
        }
        return;
    }

    std::array<bool, kFeatureCount> *visibility = nullptr;
    if (entry.kind == EntryKind::CenterlineFeature) {
        visibility = &featureVisibility_.centerline;
    } else if (entry.kind == EntryKind::StationFeature) {
        visibility = &featureVisibility_.stations;
    } else if (entry.kind == EntryKind::LabelFeature) {
        visibility = &featureVisibility_.labels;
    }
    if (visibility == nullptr) {
        return;
    }
    (*visibility)[static_cast<int>(entry.feature)] = visible;
    layerVisibility_[layerIndex(entry.layer)] = true;
}

bool ThreeDViewerLayerListModel::entryVisible(const Entry &entry) const
{
    if (entry.kind == EntryKind::Layer) {
        return layerVisible(entry.layer);
    }
    if (entry.kind == EntryKind::CenterlineFeature) {
        return featureVisibility_.centerline[static_cast<int>(entry.feature)];
    }
    if (entry.kind == EntryKind::StationFeature) {
        return featureVisibility_.stations[static_cast<int>(entry.feature)];
    }
    if (entry.kind == EntryKind::LabelFeature) {
        return featureVisibility_.labels[static_cast<int>(entry.feature)];
    }
    return false;
}

int ThreeDViewerLayerListModel::entryItemCount(const Entry &entry) const
{
    return entry.kind == EntryKind::Layer ? layerItemCount(entry.layer) : featureItemCount(entry.layer, entry.feature);
}

QString ThreeDViewerLayerListModel::entryLabel(const Entry &entry) const
{
    return entry.kind == EntryKind::Layer ? layerLabel(entry.layer) : featureLabel(entry.layer, entry.feature);
}

QString ThreeDViewerLayerListModel::featureLabel(Layer layer, Feature feature) const
{
    switch (feature) {
    case Feature::Regular:
        return layer == Layer::Centerline ? tr("Underground") : tr("All");
    case Feature::Surface:
        return tr("Surface");
    case Feature::Splay:
        return tr("Splay");
    case Feature::Duplicate:
        return tr("Duplicate");
    case Feature::Hidden:
        return tr("Hidden");
    case Feature::Entrance:
        return tr("Entrances");
    case Feature::Fixed:
        return layer == Layer::Centerline ? tr("Fixed") : tr("Fixed Stations");
    case Feature::Continuation:
        return tr("Continuations");
    case Feature::OtherStation:
        return tr("Other Stations");
    case Feature::Count:
        break;
    }
    return {};
}

bool ThreeDViewerLayerListModel::featurePresentForLayer(Layer layer, Feature feature) const
{
    return featureItemCount(layer, feature) > 0;
}

int ThreeDViewerLayerListModel::featureItemCount(Layer layer, Feature feature) const
{
    if (layer == Layer::Centerline) {
        int count = 0;
        for (const ThreeDViewerShot &shot : sceneModel_.shots) {
            const Feature shotFeature = shot.duplicate ? Feature::Duplicate
                : shot.splay ? Feature::Splay
                : shot.surface ? Feature::Surface
                : shot.hidden ? Feature::Hidden
                : Feature::Regular;
            if (shotFeature == feature) {
                ++count;
            }
        }
        return count;
    }

    if (layer != Layer::Stations && layer != Layer::Labels) {
        return 0;
    }

    int count = 0;
    for (const ThreeDViewerStation &station : sceneModel_.stations) {
        const Feature stationFeature = station.entrance ? Feature::Entrance
            : station.fixed ? Feature::Fixed
            : Feature::OtherStation;
        const bool matches = stationFeature == feature;
        if (matches) {
            ++count;
        }
    }
    return count;
}

QString ThreeDViewerLayerListModel::layerLabel(Layer layer) const
{
    switch (layer) {
    case Layer::Centerline:
        return tr("Centerline");
    case Layer::Stations:
        return tr("Stations");
    case Layer::Labels:
        return tr("Labels");
    case Layer::Meshes:
        return tr("Walls");
    case Layer::Surfaces:
        return tr("Surfaces");
    case Layer::Count:
        break;
    }
    return {};
}

QString ThreeDViewerLayerListModel::layerIconName(Layer layer) const
{
    return layerVisible(layer) ? QStringLiteral("eye") : QStringLiteral("eye-off");
}

int ThreeDViewerLayerListModel::layerItemCount(Layer layer) const
{
    switch (layer) {
    case Layer::Centerline:
        return sceneModel_.shots.size();
    case Layer::Stations:
        return sceneModel_.stations.size();
    case Layer::Labels:
        return sceneModel_.stations.size();
    case Layer::Meshes: {
        return sceneModel_.scrapLinework.size() + sceneModel_.scrapPoints.size();
    }
    case Layer::Surfaces:
        return sceneModel_.surfaces.size();
    case Layer::Count:
        break;
    }
    return 0;
}

QModelIndex ThreeDViewerLayerListModel::layerModelIndex(Layer layer) const
{
    const int row = entryIndexForLayer(layer);
    return row >= 0 && row < entries_.size() ? index(row, 0) : QModelIndex();
}

} // namespace TherionStudio
