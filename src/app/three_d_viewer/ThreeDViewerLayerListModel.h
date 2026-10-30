#pragma once

#include "../../core/ThreeDViewerSceneModel.h"

#include <QAbstractListModel>
#include <QIcon>
#include <QVector>

#include <array>

namespace TherionStudio
{

class ThreeDViewerLayerListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    static constexpr int kLayerCount = 5;

    enum class Layer
    {
        Centerline = 0,
        Stations = 1,
        Labels = 2,
        Meshes = 3,
        Surfaces = 4,
        Count = kLayerCount
    };

    enum Role
    {
        IconNameRole = Qt::UserRole + 1,
        LayerRole,
        VisibleRole,
        CountRole,
        LabelRole,
        IndentRole,
        HasChildrenRole,
        RowRole
    };

    enum class Feature
    {
        Regular = 0,
        Surface,
        Splay,
        Duplicate,
        Hidden,
        Entrance,
        Fixed,
        Continuation,
        OtherStation,
        Count
    };

    static constexpr int kFeatureCount = static_cast<int>(Feature::Count);

    struct FeatureVisibility
    {
        std::array<bool, kFeatureCount> centerline = {
            true, false, false, false, false, false, false, false, false
        };
        std::array<bool, kFeatureCount> stations = {
            false, false, false, false, false, false, true, false, true
        };
        std::array<bool, kFeatureCount> labels = {
            false, false, false, false, false, false, false, false, false
        };
    };

    explicit ThreeDViewerLayerListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);
    const ThreeDViewerSceneModel &sceneModel() const;

    bool layerVisible(Layer layer) const;
    Q_INVOKABLE bool layerVisible(int layerIndex) const;
    void setLayerVisible(Layer layer, bool visible);
    Q_INVOKABLE void setLayerVisible(int layerIndex, bool visible);
    std::array<bool, static_cast<int>(Layer::Count)> layerVisibility() const;
    FeatureVisibility featureVisibility() const;

signals:
    void layerVisibilityChanged(int layerIndex, bool visible);

private:
    enum class EntryKind
    {
        Layer,
        CenterlineFeature,
        StationFeature,
        LabelFeature
    };

    struct Entry
    {
        EntryKind kind = EntryKind::Layer;
        Layer layer = Layer::Centerline;
        Feature feature = Feature::Regular;
    };

    static constexpr int layerCount()
    {
        return kLayerCount;
    }

    static int layerIndex(Layer layer)
    {
        return static_cast<int>(layer);
    }

    QModelIndex layerModelIndex(Layer layer) const;

    void rebuildEntries();
    int entryIndexForLayer(Layer layer) const;
    bool entryHasChildren(const Entry &entry) const;
    Qt::CheckState entryCheckState(const Entry &entry) const;
    void setEntryVisible(const Entry &entry, bool visible);
    bool entryVisible(const Entry &entry) const;
    int entryItemCount(const Entry &entry) const;
    QString entryLabel(const Entry &entry) const;
    QString featureLabel(Layer layer, Feature feature) const;
    bool featurePresentForLayer(Layer layer, Feature feature) const;
    int featureItemCount(Layer layer, Feature feature) const;

    QString layerLabel(Layer layer) const;
    QString layerIconName(Layer layer) const;
    int layerItemCount(Layer layer) const;

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, kLayerCount> layerVisibility_ = {true, true, true, true, false};
    FeatureVisibility featureVisibility_;
    QVector<Entry> entries_;
};

} // namespace TherionStudio
