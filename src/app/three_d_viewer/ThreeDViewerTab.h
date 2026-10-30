#pragma once

#include "../../core/ThreeDViewerLoxLoader.h"
#include "ThreeDViewerLayerListModel.h"

#include <QStringList>
#include <QSet>
#include <QWidget>

#include <optional>

class QSplitter;

namespace TherionStudio
{

class ThreeDViewerInspectorState;
class ThreeDViewerInspectorWidget;
class ThreeDViewerViewportWidget;

class ThreeDViewerTab final : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerTab(QWidget *parent = nullptr);

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    bool loadFile(const QString &filePath, const QString &surveySourcePath, QString *errorMessage = nullptr);
    static QStringList surveySourceCandidatesForTh2(const QString &filePath, const QString &projectRootPath = QString());
    bool reloadFile(QString *errorMessage = nullptr);
    void setSceneModel(const ThreeDViewerSceneModel &sceneModel, const QString &sourceFilePath = QString());
    void setProjectRootPath(const QString &projectRootPath);
    void setTh2SourceSnapshot(const QString &contents, const QSet<int> &hiddenObjectLines);
    QString filePath() const;
    QString displayName() const;
    [[nodiscard]] const ThreeDViewerSceneModel &sceneModel() const;
    bool isDirty() const;
    bool save(QString *errorMessage = nullptr);
    void fitToScene();
    void resetView();
    void setTopView();
    void setSideView();
    void rollViewLeft();
    void rollViewRight();
    void adjustTiltDegrees(double deltaDegrees);
    void setMeasurementMode(bool measurementMode);
    bool measurementMode() const;
    void setAutoRotationEnabled(bool autoRotationEnabled);
    bool autoRotationEnabled() const;
    void setOrthographicProjection(bool orthographicProjection);
    bool orthographicProjection() const;
    void exportImage();
    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    int currentLineNumber() const;

signals:
    void titleChanged();
    void measurementModeChanged(bool measurementMode);
    void autoRotationEnabledChanged(bool autoRotationEnabled);
    void orthographicProjectionChanged(bool orthographicProjection);

private:
    void buildUi();
    void rebuildScene(bool fitToScene = true);
    void updateSceneSummary();
    void loadSceneIntoView(bool fitToScene = true);
    void focusViewportForKeyboardNavigation();

    ThreeDViewerLoxLoader loader_;
    ThreeDViewerSceneModel sceneModel_;
    struct Th2SourceSnapshot
    {
        QString contents;
        QSet<int> hiddenObjectLines;
    };
    std::optional<Th2SourceSnapshot> th2SourceSnapshot_;
    QString filePath_;
    QString displayName_;
    QString projectRootPath_;
    QString surveySourceFilePath_;
    QSplitter *splitter_ = nullptr;
    ThreeDViewerViewportWidget *viewport_ = nullptr;
    ThreeDViewerInspectorState *inspectorState_ = nullptr;
    ThreeDViewerInspectorWidget *inspectorWidget_ = nullptr;
    ThreeDViewerLayerListModel *layerModel_ = nullptr;
    bool orthographicProjection_ = false;
};

} // namespace TherionStudio
