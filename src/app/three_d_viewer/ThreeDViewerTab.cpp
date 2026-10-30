#include "ThreeDViewerTab.h"

#include "ThreeDViewerInspectorState.h"
#include "ThreeDViewerInspectorWidget.h"
#include "ThreeDViewerImageExportDialog.h"
#include "ThreeDViewerLayerListModel.h"
#include "ThreeDViewerViewportWidget.h"

#include "../ExportFileName.h"
#include "../text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../text_editor/map_editor/MapEditorPointSymbolGeometry.h"
#include "../../core/TherionCentreline.h"
#include "../../core/TherionFileTypes.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace TherionStudio
{
namespace
{
constexpr double kKeyboardTiltStepDegrees = 5.0;


QString normalizedFilePathKey(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return QDir::cleanPath(canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath);
}

QStringList findSurveySourceCandidatesForTh2(const QString &filePath, const QString &projectRootPath)
{
    QStringList directories;
    const auto appendDirectory = [&directories](const QString &path) {
        if (path.isEmpty()) {
            return;
        }
        const QFileInfo info(path);
        const QString directoryPath = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        const QString normalized = QDir::cleanPath(directoryPath);
        if (!directories.contains(normalized)) {
            directories.append(normalized);
        }
    };

    appendDirectory(projectRootPath);
    QDir sourceDirectory = QFileInfo(filePath).dir();
    for (int depth = 0; depth < 4; ++depth) {
        appendDirectory(sourceDirectory.absolutePath());
        if (!sourceDirectory.cdUp()) {
            break;
        }
    }

    QStringList configPaths;
    QStringList surveyPaths;
    QSet<QString> seenPaths;
    const auto appendCandidates = [&seenPaths](const QStringList &directoriesToScan,
                                               const QString &pattern,
                                               QStringList *destination) {
        for (const QString &directoryPath : directoriesToScan) {
            const QFileInfoList files = QDir(directoryPath).entryInfoList({pattern}, QDir::Files, QDir::Name);
            for (const QFileInfo &fileInfo : files) {
                const QString path = fileInfo.absoluteFilePath();
                const QString key = normalizedFilePathKey(path);
                if (!seenPaths.contains(key)) {
                    seenPaths.insert(key);
                    destination->append(path);
                }
            }
        }
    };
    appendCandidates(directories, QStringLiteral("*.thconfig"), &configPaths);
    appendCandidates(directories, QStringLiteral("*.th"), &surveyPaths);

    const QString targetPath = normalizedFilePathKey(filePath);
    QStringList matchingConfigs;
    QStringList matchingSurveys;
    const TherionCentrelineFileReader readFile = defaultTherionCentrelineFileReader();
    const auto referencesTarget = [&targetPath, &readFile](const QString &candidatePath) {
        const TherionCentrelineObservations observations = parseTherionCentreline({candidatePath}, readFile);
        for (const TherionCentrelineTh2Reference &reference : observations.th2References) {
            if (normalizedFilePathKey(reference.filePath) == targetPath) {
                return true;
            }
        }
        return false;
    };

    for (const QString &path : configPaths) {
        if (referencesTarget(path)) {
            matchingConfigs.append(path);
        }
    }
    if (!matchingConfigs.isEmpty()) {
        return matchingConfigs;
    }
    for (const QString &path : surveyPaths) {
        if (referencesTarget(path)) {
            matchingSurveys.append(path);
        }
    }
    return matchingSurveys;
}

void appendReferencedScrapObjects(const TherionCentrelineObservations &observations,
                                  TherionCentrelineResult *result,
                                  const TherionCentrelineFileReader &readFile,
                                  QStringList *warnings,
                                  const QString &requestedTh2Path = QString(),
                                  const QSet<int> &hiddenObjectLines = {})
{
    if (result == nullptr) {
        return;
    }

    const MapEditorObjectStyleCatalog styleCatalog = loadDefaultMapEditorObjectStyleCatalog();
    const TherionScrap3DOptions options{hiddenObjectLines, false, true};
    const qreal wallWidth = resolveMapEditorLineStyle(styleCatalog, QStringLiteral("wall"), {}).strokeWidth;
    for (const TherionCentrelineTh2Reference &reference : observations.th2References) {
        if (!requestedTh2Path.isEmpty() && normalizedFilePathKey(reference.filePath) != requestedTh2Path) {
            continue;
        }
        QString contents;
        if (!readFile(reference.filePath, &contents)) {
            if (warnings != nullptr) {
                warnings->append(QCoreApplication::translate("TherionStudio::ThreeDViewerTab",
                                                            "Unable to read referenced scrap file: %1")
                                     .arg(reference.filePath));
            }
            continue;
        }

        const auto scrapModels = TherionScrap3D::projectTh2File(
            reference.filePath, contents, reference.surveyPath, result->stationPositions, 0, 0, options);
        for (const TherionScrap3DModel &scrapModel : scrapModels) {
            if (warnings != nullptr && !scrapModel.transform.valid) {
                warnings->append(QCoreApplication::translate("TherionStudio::ThreeDViewerTab",
                                                            "Scrap '%1' omitted: no matching centreline station anchor.")
                                     .arg(scrapModel.scrapId));
            } else if (warnings != nullptr && scrapModel.transform.approximate) {
                warnings->append(QCoreApplication::translate("TherionStudio::ThreeDViewerTab",
                                                            "Scrap '%1' is approximate: its station anchors do not determine a unique orientation and scale.")
                                     .arg(scrapModel.scrapId));
            }

            for (ThreeDViewerScrapPolyline line : scrapModel.linework) {
                const MapEditorResolvedLineStyle style = resolveMapEditorLineStyle(styleCatalog, line.type, line.subtype);
                line.strokeVisible = style.strokeVisible;
                line.strokeWidth = line.type.compare(QStringLiteral("wall"), Qt::CaseInsensitive) == 0
                    ? style.strokeWidth : std::min(style.strokeWidth, wallWidth * 0.45);
                line.penStyle = style.penStyle;
                line.dashPattern = style.dashPattern;
                if (style.strokeColor.has_value()) {
                    line.strokeColorRgba = style.strokeColor->rgba();
                    line.hasStrokeColor = true;
                }
                result->scene.scrapLinework.append(std::move(line));
            }
            for (ThreeDViewerScrapPoint point : scrapModel.pointFeatures) {
                const auto style = resolveMapEditorPointStyle(styleCatalog, point.type, point.subtype);
                point.size = style.size;
                point.strokeWidth = std::min(style.outlineWidth, wallWidth * 0.45);
                const qreal halfSize = point.size * 0.5;
                const auto path = mapEditorPointRenderPath(
                    style.symbol, style.symbolParts, QRectF(-halfSize, -halfSize, point.size, point.size),
                    style.size, {}, point.orientationDegrees);
                for (const auto &polygon : path.toSubpathPolygons()) {
                    point.symbolStrokes.append(polygon);
                }
                if (style.fillColor.has_value()) {
                    point.fillColorRgba = style.fillColor->rgba();
                    point.hasFillColor = true;
                }
                if (style.strokeColor.has_value()) {
                    point.strokeColorRgba = style.strokeColor->rgba();
                    point.hasStrokeColor = true;
                }
                result->scene.scrapPoints.append(std::move(point));
            }
        }
    }
}
void appendLoxMeshGroupsAsScrapLinework(ThreeDViewerSceneModel *scene)
{
    if (scene == nullptr || !scene->scrapLinework.isEmpty() || scene->meshGroups.isEmpty()) {
        return;
    }

    const MapEditorObjectStyleCatalog styleCatalog = loadDefaultMapEditorObjectStyleCatalog();
    const MapEditorResolvedLineStyle wallStyle = resolveMapEditorLineStyle(styleCatalog, QStringLiteral("wall"), {});
    for (const ThreeDViewerMeshGroup &mesh : scene->meshGroups) {
        if (mesh.vertices.isEmpty() || mesh.triangles.isEmpty()) {
            continue;
        }
        QHash<quint64, int> edgeUseCounts;
        edgeUseCounts.reserve(mesh.triangles.size() * 3);
        const auto edgeKey = [](quint32 a, quint32 b) -> quint64 {
            const quint32 lo = std::min(a, b);
            const quint32 hi = std::max(a, b);
            return (quint64(lo) << 32) | quint64(hi);
        };
        for (const auto &tri : mesh.triangles) {
            ++edgeUseCounts[edgeKey(tri[0], tri[1])];
            ++edgeUseCounts[edgeKey(tri[1], tri[2])];
            ++edgeUseCounts[edgeKey(tri[2], tri[0])];
        }
        for (auto it = edgeUseCounts.cbegin(); it != edgeUseCounts.cend(); ++it) {
            const quint32 a = quint32(it.key() >> 32);
            const quint32 b = quint32(it.key() & 0xFFFFFFFFu);
            if (a >= quint32(mesh.vertices.size()) || b >= quint32(mesh.vertices.size())) {
                continue;
            }
            ThreeDViewerScrapPolyline line;
            line.type = QStringLiteral("wall");
            line.points = {mesh.vertices.at(qsizetype(a)), mesh.vertices.at(qsizetype(b))};
            line.strokeVisible = true;
            line.strokeWidth = it.value() == 1 ? wallStyle.strokeWidth : std::max<qreal>(0.9, wallStyle.strokeWidth * 0.45);
            if (wallStyle.strokeColor.has_value()) {
                line.strokeColorRgba = wallStyle.strokeColor->rgba();
                line.hasStrokeColor = true;
            }
            scene->scrapLinework.append(std::move(line));
        }
    }
}

void applyCompiledLoxStationPositionsIfPresent(const QString &sourcePath,
                                               const ThreeDViewerLoxLoader &loader,
                                               TherionCentrelineResult *result)
{
    if (result == nullptr || sourcePath.isEmpty()) {
        return;
    }
    const QFileInfo sourceInfo(sourcePath);
    const QDir dir = sourceInfo.dir();
    const QFileInfoList loxFiles = dir.entryInfoList({QStringLiteral("*.lox")}, QDir::Files, QDir::Time);
    if (loxFiles.size() != 1) {
        return;
    }
    const ThreeDViewerLoxLoader::Result loxResult = loader.loadStationMetadataFile(loxFiles.first().absoluteFilePath());
    if (!loxResult.ok() || loxResult.scene.stations.isEmpty()) {
        return;
    }
    QHash<QString, ThreeDViewerVec3> loxPositions;
    loxPositions.reserve(loxResult.scene.stations.size());
    for (const ThreeDViewerStation &station : loxResult.scene.stations) {
        const QString qualified = loxResult.scene.stationQualifiedName(station);
        if (!qualified.isEmpty()) {
            loxPositions.insert(qualified, station.position);
        }
    }
    for (auto it = result->stationPositions.begin(); it != result->stationPositions.end(); ++it) {
        const auto loxIt = loxPositions.constFind(it.key());
        if (loxIt != loxPositions.cend()) {
            it.value() = loxIt.value();
        }
    }
    for (ThreeDViewerStation &station : result->scene.stations) {
        const QString qualified = result->scene.stationQualifiedName(station);
        const auto loxIt = loxPositions.constFind(qualified);
        if (loxIt != loxPositions.cend()) {
            station.position = loxIt.value();
        }
    }
}
}


QStringList ThreeDViewerTab::surveySourceCandidatesForTh2(const QString &filePath,
                                                            const QString &projectRootPath)
{
    return findSurveySourceCandidatesForTh2(filePath, projectRootPath);
}

ThreeDViewerTab::ThreeDViewerTab(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

bool ThreeDViewerTab::loadFile(const QString &filePath, QString *errorMessage)
{
    return loadFile(filePath, QString(), errorMessage);
}

bool ThreeDViewerTab::loadFile(const QString &filePath,
                               const QString &surveySourcePath,
                               QString *errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    ThreeDViewerSceneModel model;
    QStringList sceneWarnings;
    QString resolvedSurveySourcePath;


    if (suffix == QStringLiteral("lox")) {
        const ThreeDViewerLoxLoader::Result result = loader_.loadFile(filePath);
        if (!result.ok()) {
            if (errorMessage != nullptr) {
                *errorMessage = result.error;
            }
            return false;
        }
        model = result.scene;
        appendLoxMeshGroupsAsScrapLinework(&model);
    } else if (suffix == QStringLiteral("th") || suffix == QStringLiteral("thconfig")) {
        const TherionCentrelineFileReader readFile = defaultTherionCentrelineFileReader();
        const TherionCentrelineObservations observations = parseTherionCentreline({filePath}, readFile);
        TherionCentrelineResult result = solveTherionCentreline(observations);
        applyCompiledLoxStationPositionsIfPresent(filePath, loader_, &result);
        if (result.stationPositions.isEmpty() && !result.diagnostics.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = result.diagnostics.first().message;
            }
            return false;
        }
        appendReferencedScrapObjects(observations, &result, readFile, &sceneWarnings);
        model = result.scene;
    } else if (suffix == QStringLiteral("th2")) {
        if (!QFileInfo(filePath).isFile()) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Unable to read the selected .th2 file.");
            }
            return false;
        }
        resolvedSurveySourcePath = surveySourcePath;
        if (resolvedSurveySourcePath.isEmpty()) {
            const QStringList candidates = surveySourceCandidatesForTh2(filePath, projectRootPath_);
            if (candidates.isEmpty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = tr("No survey source in the project references this .th2 file. Open the .th or .thconfig file that includes it.");
                }
                return false;
            }
            if (candidates.size() > 1) {
                if (errorMessage != nullptr) {
                    *errorMessage = tr("Multiple survey sources reference this .th2 file. Select one to build its 3D scene.");
                }
                return false;
            }
            resolvedSurveySourcePath = candidates.first();
        }

        const QString requestedPath = normalizedFilePathKey(filePath);
        const TherionCentrelineFileReader diskReader = defaultTherionCentrelineFileReader();
        const TherionCentrelineFileReader readFile = [this, requestedPath, diskReader](const QString &path, QString *contents) {
            if (th2SourceSnapshot_.has_value() && normalizedFilePathKey(path) == requestedPath) {
                *contents = th2SourceSnapshot_->contents;
                return true;
            }
            return diskReader(path, contents);
        };
        const TherionCentrelineObservations observations = parseTherionCentreline({resolvedSurveySourcePath}, readFile);
        const bool isReferenced = std::any_of(observations.th2References.cbegin(),
                                              observations.th2References.cend(),
                                              [&requestedPath](const TherionCentrelineTh2Reference &reference) {
                                                  return normalizedFilePathKey(reference.filePath) == requestedPath;
                                              });
        if (!isReferenced) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("The selected survey source does not reference this .th2 file.");
            }
            return false;
        }

        TherionCentrelineResult result = solveTherionCentreline(observations);
        applyCompiledLoxStationPositionsIfPresent(resolvedSurveySourcePath, loader_, &result);
        if (result.stationPositions.isEmpty() && !result.diagnostics.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = result.diagnostics.first().message;
            }
            return false;
        }
        appendReferencedScrapObjects(observations, &result, readFile, &sceneWarnings, requestedPath,
                                    th2SourceSnapshot_.has_value() ? th2SourceSnapshot_->hiddenObjectLines : QSet<int>{});
        model = result.scene;
    } else {
        const ThreeDViewerLoxLoader::Result result = loader_.loadFile(filePath);
        if (!result.ok()) {
            if (errorMessage != nullptr) {
                *errorMessage = result.error;
            }
            return false;
        }
        model = result.scene;
        appendLoxMeshGroupsAsScrapLinework(&model);
    }

    setSceneModel(model, filePath);
    surveySourceFilePath_ = resolvedSurveySourcePath;
    if (inspectorState_ != nullptr) {
        inspectorState_->setSceneWarnings(sceneWarnings);
    }
    return true;
}

bool ThreeDViewerTab::reloadFile(QString *errorMessage)
{
    if (filePath_.isEmpty()) {
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        return false;
    }

    return loadFile(filePath_, surveySourceFilePath_, errorMessage);
}

void ThreeDViewerTab::setSceneModel(const ThreeDViewerSceneModel &sceneModel, const QString &sourceFilePath)
{
    sceneModel_ = sceneModel;
    sceneModel_.meshGroups.clear();
    sceneModel_.surfaces.clear();
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
    }

    if (!sourceFilePath.isEmpty()) {
        const QFileInfo fileInfo(sourceFilePath);
        const QString canonicalPath = fileInfo.canonicalFilePath();
        filePath_ = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
        displayName_ = fileInfo.fileName();
        if (displayName_.isEmpty()) {
            displayName_ = filePath_;
        }
    }

    updateSceneSummary();
    rebuildScene();
    emit titleChanged();
}

void ThreeDViewerTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
}

void ThreeDViewerTab::setTh2SourceSnapshot(const QString &contents, const QSet<int> &hiddenObjectLines)
{
    th2SourceSnapshot_ = Th2SourceSnapshot{contents, hiddenObjectLines};
}

QString ThreeDViewerTab::filePath() const
{
    return filePath_;
}

QString ThreeDViewerTab::displayName() const
{
    return displayName_;
}

const ThreeDViewerSceneModel &ThreeDViewerTab::sceneModel() const
{
    return sceneModel_;
}

bool ThreeDViewerTab::isDirty() const
{
    return false;
}

bool ThreeDViewerTab::save(QString *errorMessage)
{
    if (errorMessage != nullptr) {
        *errorMessage = tr("The 3D viewer is read-only.");
    }
    return false;
}

void ThreeDViewerTab::fitToScene()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->fitToScene();
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::resetView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->resetView();
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::setTopView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Top);
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::setSideView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Side);
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::rollViewLeft()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollLeft();
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::rollViewRight()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollRight();
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::adjustTiltDegrees(double deltaDegrees)
{
    if (viewport_ == nullptr || inspectorState_ == nullptr) {
        return;
    }
    viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees() + deltaDegrees);
    focusViewportForKeyboardNavigation();
}

void ThreeDViewerTab::setMeasurementMode(bool measurementMode)
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setMeasurementMode(measurementMode);
    }
    if (viewport_ != nullptr) {
        viewport_->setMeasurementMode(measurementMode);
        if (measurementMode) {
            viewport_->focusViewport(Qt::ShortcutFocusReason);
        }
    }
}

bool ThreeDViewerTab::measurementMode() const
{
    return inspectorState_ != nullptr ? inspectorState_->measurementMode() : false;
}

void ThreeDViewerTab::setAutoRotationEnabled(bool autoRotationEnabled)
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setAutoRotationEnabled(autoRotationEnabled);
    }
    if (viewport_ != nullptr) {
        viewport_->setAutoRotationEnabled(autoRotationEnabled);
    }
}

bool ThreeDViewerTab::autoRotationEnabled() const
{
    return inspectorState_ != nullptr ? inspectorState_->autoRotationEnabled() : false;
}

void ThreeDViewerTab::setOrthographicProjection(bool orthographicProjection)
{
    if (orthographicProjection_ == orthographicProjection) {
        return;
    }

    orthographicProjection_ = orthographicProjection;
    if (inspectorState_ != nullptr) {
        inspectorState_->setOrthographicProjection(orthographicProjection_);
    }
    if (viewport_ != nullptr) {
        viewport_->setOrthographicProjection(orthographicProjection_);
    }
    emit orthographicProjectionChanged(orthographicProjection_);
}

bool ThreeDViewerTab::orthographicProjection() const
{
    return orthographicProjection_;
}

void ThreeDViewerTab::exportImage()
{
    if (viewport_ == nullptr) {
        return;
    }

    ThreeDViewerImageExportDialog dialog(viewport_->viewportPixelSize(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString initialDirectory = !filePath_.isEmpty()
        ? QFileInfo(filePath_).absolutePath()
        : projectRootPath_;
    const QString defaultName = defaultArtifactExportFileName(filePath_,
                                                              QStringLiteral("therion-studio-3d"),
                                                              QStringLiteral("png"),
                                                              QDateTime::currentDateTime());
    QString outputPath = QFileDialog::getSaveFileName(this,
                                                      tr("Export 3D Image"),
                                                      QDir(initialDirectory).filePath(defaultName),
                                                      tr("PNG images (*.png);;All files (*)"));
    if (outputPath.isEmpty()) {
        return;
    }
    if (QFileInfo(outputPath).suffix().isEmpty()) {
        outputPath += QStringLiteral(".png");
    }

    const QPointer<ThreeDViewerTab> guardedThis(this);
    viewport_->grabImage(dialog.exportSize(), [guardedThis, outputPath](const QImage &image) {
        if (guardedThis == nullptr) {
            return;
        }
        QImage outputImage = image;
        outputImage.setDevicePixelRatio(1.0);
        if (!outputImage.isNull()) {
            outputImage = outputImage.convertToFormat(QImage::Format_RGB888);
        }
        if (outputImage.isNull() || !outputImage.save(outputPath, "PNG")) {
            QMessageBox::warning(guardedThis,
                                 guardedThis->tr("Export 3D Image"),
                                 guardedThis->tr("Could not write %1.").arg(QDir::toNativeSeparators(outputPath)));
            return;
        }
    });
}

void ThreeDViewerTab::showFindBar(bool)
{
}

void ThreeDViewerTab::hideFindBar()
{
}

void ThreeDViewerTab::goToLine(int)
{
}

int ThreeDViewerTab::currentLineNumber() const
{
    return 0;
}

void ThreeDViewerTab::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter_, 1);

    auto *viewportFrame = new QFrame(splitter_);
    viewportFrame->setFrameStyle(QFrame::StyledPanel);
    auto *viewportLayout = new QVBoxLayout(viewportFrame);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);
    viewport_ = new ThreeDViewerViewportWidget(viewportFrame);
    viewportLayout->addWidget(viewport_);
    splitter_->addWidget(viewportFrame);

    auto *measurementEscapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    measurementEscapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(measurementEscapeShortcut, &QShortcut::activated, this, [this] {
        if (measurementMode()) {
            setMeasurementMode(false);
        }
    });
    auto *rotateLeftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    rotateLeftShortcut->setContext(Qt::WindowShortcut);
    connect(rotateLeftShortcut, &QShortcut::activated, this, &ThreeDViewerTab::rollViewLeft);
    auto *rotateRightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    rotateRightShortcut->setContext(Qt::WindowShortcut);
    connect(rotateRightShortcut, &QShortcut::activated, this, &ThreeDViewerTab::rollViewRight);
    auto *tiltUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    tiltUpShortcut->setContext(Qt::WindowShortcut);
    connect(tiltUpShortcut, &QShortcut::activated, this, [this] {
        adjustTiltDegrees(-kKeyboardTiltStepDegrees);
    });
    auto *tiltDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    tiltDownShortcut->setContext(Qt::WindowShortcut);
    connect(tiltDownShortcut, &QShortcut::activated, this, [this] {
        adjustTiltDegrees(kKeyboardTiltStepDegrees);
    });

    auto *inspector = new QWidget(splitter_);
    auto *inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(8, 8, 8, 8);
    inspectorLayout->setSpacing(8);

    inspectorState_ = new ThreeDViewerInspectorState(this);
    layerModel_ = new ThreeDViewerLayerListModel(this);
    connect(layerModel_, &ThreeDViewerLayerListModel::layerVisibilityChanged, this, [this](int, bool) {
        if (viewport_ != nullptr && layerModel_ != nullptr) {
            viewport_->setLayerVisibility(layerModel_->layerVisibility());
            viewport_->setFeatureVisibility(layerModel_->featureVisibility());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::meshColorModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::centerlineColorModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCenterlineColorMode(static_cast<ThreeDViewerCenterlineColorMode>(inspectorState_->centerlineColorMode()));
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::backgroundModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setBackgroundMode(static_cast<ThreeDViewerBackgroundMode>(inspectorState_->backgroundMode()));
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::measurementModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeasurementMode(inspectorState_->measurementMode());
        }
        emit measurementModeChanged(inspectorState_ != nullptr ? inspectorState_->measurementMode() : false);
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::autoRotationEnabledChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        }
        emit autoRotationEnabledChanged(inspectorState_ != nullptr ? inspectorState_->autoRotationEnabled() : false);
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::autoRotationSpeedChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        }
    });
    const auto syncSceneOverlayVisibility = [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                                 inspectorState_->showHud(),
                                                 inspectorState_->showInfo());
        }
    };
    connect(inspectorState_, &ThreeDViewerInspectorState::showBoundingBoxChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::showHudChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::showInfoChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraFacingDegreesChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraTiltDegreesChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraDistanceMetersChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraFocalLengthMmChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        }
    });
    connect(viewport_, &ThreeDViewerViewportWidget::cameraSettingsChanged, this, [this](double facingDegrees, double tiltDegrees, double distanceMeters, double focalLengthMm) {
        if (inspectorState_ == nullptr) {
            return;
        }
        inspectorState_->setCameraFacingDegrees(facingDegrees);
        inspectorState_->setCameraTiltDegrees(tiltDegrees);
        inspectorState_->setCameraDistanceMeters(distanceMeters);
        inspectorState_->setCameraFocalLengthMm(focalLengthMm);
    });
    connect(viewport_, &ThreeDViewerViewportWidget::measurementModeExitRequested, this, [this] {
        setMeasurementMode(false);
    });

    inspectorWidget_ = new ThreeDViewerInspectorWidget(inspectorState_, layerModel_, inspector);
    inspectorLayout->addWidget(inspectorWidget_, 1);

    if (viewport_ != nullptr && inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setCenterlineColorMode(static_cast<ThreeDViewerCenterlineColorMode>(inspectorState_->centerlineColorMode()));
        viewport_->setBackgroundMode(static_cast<ThreeDViewerBackgroundMode>(inspectorState_->backgroundMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
        viewport_->setOrthographicProjection(orthographicProjection_);
        inspectorState_->setOrthographicProjection(orthographicProjection_);
        viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                             inspectorState_->showHud(),
                                             inspectorState_->showInfo());
    }

    splitter_->addWidget(inspector);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setSizes({900, 320});
}

void ThreeDViewerTab::rebuildScene(bool fitToScene)
{
    loadSceneIntoView(fitToScene);
}

void ThreeDViewerTab::updateSceneSummary()
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setFilePath(filePath_);
        inspectorState_->setSceneModel(sceneModel_);
    }
}

void ThreeDViewerTab::loadSceneIntoView(bool fitToScene)
{
    if (viewport_ == nullptr) {
        return;
    }

    viewport_->setSceneModel(sceneModel_, fitToScene);
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
        viewport_->setLayerVisibility(layerModel_->layerVisibility());
        viewport_->setFeatureVisibility(layerModel_->featureVisibility());
    }
    if (inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setCenterlineColorMode(static_cast<ThreeDViewerCenterlineColorMode>(inspectorState_->centerlineColorMode()));
        viewport_->setBackgroundMode(static_cast<ThreeDViewerBackgroundMode>(inspectorState_->backgroundMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
        viewport_->setOrthographicProjection(orthographicProjection_);
        inspectorState_->setOrthographicProjection(orthographicProjection_);
        viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                             inspectorState_->showHud(),
                                             inspectorState_->showInfo());
    }
    if (fitToScene) {
        viewport_->fitToScene();
    }
}

void ThreeDViewerTab::focusViewportForKeyboardNavigation()
{
    if (viewport_ == nullptr) {
        return;
    }

    viewport_->focusViewport(Qt::ShortcutFocusReason);

    const QPointer<ThreeDViewerViewportWidget> viewport(viewport_);
    QTimer::singleShot(0, this, [viewport] {
        if (viewport != nullptr) {
            viewport->focusViewport(Qt::ShortcutFocusReason);
        }
    });
}

} // namespace TherionStudio
