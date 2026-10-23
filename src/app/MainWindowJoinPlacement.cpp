#include "MainWindow.h"

#include "ProjectConnectionDataStatus.h"
#include "ProjectJoinsPanel.h"
#include "ProjectStationPositionService.h"
#include "TherionCompileResult.h"
#include "text_editor/map_editor/MapEditorJoinPairs.h"
#include "text_editor/map_editor/MapEditorJoinViewLink.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>

namespace
{
QHash<int, QString> stationKeysByLine(const TherionStudio::ProjectIndexSnapshot &snapshot, const QString &scrapObjectId)
{
    QHash<int, QString> keys;
    for (const TherionStudio::ProjectScrapStationPoint &point : snapshot.stationPointsByScrapObjectId.value(scrapObjectId)) {
        keys.insert(point.lineNumber, point.stationKey);
    }
    return keys;
}

// The project's sources that station positions depend on (the export ignores drawings itself).
QStringList stationPositionSourceFiles(const TherionStudio::ProjectIndexSnapshot &snapshot)
{
    QSet<QString> sourceFiles = {snapshot.rootConfigPath};
    for (const TherionStudio::ProjectStructureEntry &entry : snapshot.entries) {
        if (!entry.sourceFile.isEmpty()) {
            sourceFiles.insert(entry.sourceFile);
        }
    }
    return QStringList(sourceFiles.cbegin(), sourceFiles.cend());
}

std::optional<TherionStudio::ProjectStationPositionRequest> stationPositionRequest(
    const TherionStudio::ProjectIndexSnapshot &snapshot,
    const QString &executableInput)
{
    if (snapshot.rootConfigPath.isEmpty()) {
        return std::nullopt;
    }
    return TherionStudio::ProjectStationPositionRequest{
        executableInput.isEmpty() ? QStringLiteral("therion") : executableInput,
        snapshot.rootConfigPath,
        stationPositionSourceFiles(snapshot)};
}

enum class RunChoice
{
    Run,
    Open,
    Closed,
};

RunChoice askToRunTherion(QWidget *parent, bool outOfDate, bool *dontAskAgain)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(MainWindow::tr("Station Positions"));
    box.setText(outOfDate ? MainWindow::tr("The station positions are out of date. Run Therion to update them?")
                          : MainWindow::tr("This connection needs station positions. Run Therion now?"));
    box.setInformativeText(MainWindow::tr("On a large project this can take a while."));
    QPushButton *run = box.addButton(MainWindow::tr("Run Therion"), QMessageBox::AcceptRole);
    QPushButton *open = box.addButton(outOfDate ? MainWindow::tr("Use Them As They Are")
                                                : MainWindow::tr("Open Without Alignment"),
                                      QMessageBox::RejectRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(run);
    box.setCheckBox(new QCheckBox(MainWindow::tr("Don't ask again"), &box));
    box.exec();
    *dontAskAgain = box.checkBox()->isChecked();
    if (box.clickedButton() == run) {
        return RunChoice::Run;
    }
    return box.clickedButton() == open ? RunChoice::Open : RunChoice::Closed;
}

struct JoinDrawing
{
    QString sourceFile;
    int lineNumber = 0;
    QString scrapObjectId;
};

// The side's join-station point, or the scrap of its survey nearest the station; none without computed positions.
std::optional<JoinDrawing> joinDrawing(const TherionStudio::ProjectIndexSnapshot &snapshot,
                                       const TherionStudio::ProjectJoinSide &side,
                                       const QHash<QString, TherionStudio::Th2StationPosition> *positions)
{
    if (side.drawing.has_value()) {
        return JoinDrawing{side.drawing->sourceFile, side.drawing->lineNumber, side.drawing->scrapObjectId};
    }
    if (positions == nullptr) {
        return std::nullopt;
    }
    const std::optional<TherionStudio::ProjectStructureEntry> scrap = nearestSurveyScrap(snapshot, side, *positions);
    return scrap.has_value() ? std::optional<JoinDrawing>(JoinDrawing{scrap->sourceFile, scrap->lineNumber, scrap->objectId})
                             : std::nullopt;
}
}

void MainWindow::setupJoinStationPositions()
{
    // Temporary files go in the system temporary folder, never in the project.
    stationPositionService_ = new TherionStudio::ProjectStationPositionService(QDir::tempPath(), this);
    connect(stationPositionService_, &TherionStudio::ProjectStationPositionService::runningChanged, this, [this](bool running) {
        if (running) {
            statusBar()->showMessage(tr("Computing station positions with Therion for connected drawings..."));
        } else {
            statusBar()->clearMessage();
        }
        refreshConnectionDataStatus();
    });
    connect(stationPositionService_,
            &TherionStudio::ProjectStationPositionService::positionsFailed,
            this,
            [this](const QString &message) {
                statusBar()->showMessage(message, 8000);
                openPendingConnection();
                refreshConnectionDataStatus();
            });
    connect(stationPositionService_, &TherionStudio::ProjectStationPositionService::positionsReady, this, [this]() {
        refreshConnectionDataStatus();
        openPendingConnection();
    });
    connect(projectJoinsPanel_,
            &TherionStudio::ProjectJoinsPanel::runCancelRequested,
            this,
            &MainWindow::cancelStationPositionRun);
    refreshConnectionDataStatus();
}

void MainWindow::loadJoinStationPositionsFromCompile()
{
    const TherionStudio::TherionCompileResult &result = compileResultRecorder_->latest();
    if (stationPositionService_ != nullptr && result.isValid() && !result.databasePaths.isEmpty()) {
        stationPositionService_->loadCompileExport(result.databasePaths.first(), result.startedAt);
    }
    refreshConnectionDataStatus();
}

bool MainWindow::prepareConnectionPositions(const TherionStudio::ProjectConnectionPendingOpen &open)
{
    if (stationPositionService_ == nullptr) {
        return true;
    }
    if (stationPositionService_->isRunning()) {
        pendingConnection_ = open;
        refreshConnectionDataStatus();
        return false;
    }
    const std::optional<TherionStudio::ProjectStationPositionRequest> request =
        stationPositionRequest(lastStructureSidebarProjectIndex_, therionExecutableInput());
    // Without a target config, or after a run that failed for unchanged sources, open with what there is.
    if (!request.has_value() || stationPositionService_->attemptFailed(*request)) {
        return true;
    }
    const bool runPositionsOutOfDate =
        stationPositionService_->dataSource() == TherionStudio::ProjectStationPositionService::DataSource::Run
        && stationPositionService_->isOutOfDate(request->sourceFiles);
    const TherionStudio::ConnectionPositionRunMode mode = TherionStudio::connectionPositionRunModeFromSetting(
        sessionStore_ != nullptr ? sessionStore_->connectionPositionRunMode() : QString());
    const TherionStudio::ConnectionPositionRunAction action =
        TherionStudio::connectionPositionRunAction(mode, currentStationPositions() != nullptr, runPositionsOutOfDate);
    if (action == TherionStudio::ConnectionPositionRunAction::OpenNow) {
        return true;
    }
    if (action == TherionStudio::ConnectionPositionRunAction::AskFirst) {
        bool dontAskAgain = false;
        const RunChoice choice = askToRunTherion(this, runPositionsOutOfDate, &dontAskAgain);
        if (choice == RunChoice::Closed) {
            return false;
        }
        if (dontAskAgain && sessionStore_ != nullptr) {
            sessionStore_->setConnectionPositionRunMode(TherionStudio::connectionPositionRunModeSetting(
                choice == RunChoice::Run ? TherionStudio::ConnectionPositionRunMode::Run
                                         : TherionStudio::ConnectionPositionRunMode::Never));
        }
        if (choice == RunChoice::Open) {
            return true;
        }
    }
    pendingConnection_ = open;
    stationPositionService_->run(*request);
    refreshConnectionDataStatus();
    return false;
}

void MainWindow::openPendingConnection()
{
    if (!pendingConnection_.has_value()) {
        return;
    }
    const TherionStudio::ProjectConnectionPendingOpen open = *pendingConnection_;
    pendingConnection_.reset();
    if (open.pair.has_value()) {
        openProjectJoinNow(*open.pair, open.separateWindow);
    } else if (open.joinOnly.has_value()) {
        openProjectJoinOnlyNow(*open.joinOnly, open.separateWindow);
    }
}

void MainWindow::cancelStationPositionRun()
{
    if (stationPositionService_ != nullptr) {
        stationPositionService_->cancel();
    }
    // The connection still opens, with the positions there were or without alignment.
    openPendingConnection();
    refreshConnectionDataStatus();
}

void MainWindow::refreshConnectionDataStatus()
{
    if (projectJoinsPanel_ == nullptr || stationPositionService_ == nullptr) {
        return;
    }
    TherionStudio::ProjectConnectionDataStatus::State state;
    state.source = stationPositionService_->dataSource();
    state.takenAt = stationPositionService_->dataTakenAt();
    state.running = stationPositionService_->isRunning();
    state.outOfDate = stationPositionService_->isOutOfDate(stationPositionSourceFiles(lastStructureSidebarProjectIndex_));
    state.noTargetConfig = lastStructureSidebarProjectIndex_.rootConfigPath.isEmpty();
    // The sidebar is built before the compiler console that owns the recorder.
    if (compileResultRecorder_ != nullptr) {
        const TherionStudio::TherionCompileResult &compile = compileResultRecorder_->latest();
        state.compileWroteNoDatabase = compile.isValid() && compile.databasePaths.isEmpty();
    }
    state.openPending = pendingConnection_.has_value();
    projectJoinsPanel_->setDataStatus(TherionStudio::ProjectConnectionDataStatus::text(state, QDateTime::currentDateTime()),
                                      state.noTargetConfig);
    projectJoinsPanel_->setRunInProgress(state.running);
    // Rows show what they need; the fit pass is quick (milliseconds on a large project).
    projectJoinsPanel_->setConnectionData(
        currentStationPositions() != nullptr
            ? std::optional<QSet<QString>>(TherionStudio::projectScrapsFittedByMeasuredLegs(
                  lastStructureSidebarProjectIndex_, stationPositionService_->legs()))
            : std::nullopt,
        state.outOfDate);
}

TherionStudio::MapEditorJoinPlacement MainWindow::joinPlacement(const QString &firstScrapObjectId,
                                                                const QString &secondScrapObjectId,
                                                                const QString &projection,
                                                                bool sameStation) const
{
    TherionStudio::MapEditorJoinPlacement placement;
    placement.firstStationKeysByLine = stationKeysByLine(lastStructureSidebarProjectIndex_, firstScrapObjectId);
    placement.secondStationKeysByLine = stationKeysByLine(lastStructureSidebarProjectIndex_, secondScrapObjectId);
    placement.projection = projection;
    placement.sameStation = sameStation;
    placement.positions = currentStationPositions();
    if (placement.positions != nullptr) {
        placement.legs = &stationPositionService_->legs();
    }
    return placement;
}

const QHash<QString, TherionStudio::Th2StationPosition> *MainWindow::currentStationPositions() const
{
    return stationPositionService_ != nullptr && !stationPositionService_->isRunning()
            && !stationPositionService_->positions().isEmpty()
        ? &stationPositionService_->positions()
        : nullptr;
}

void MainWindow::openProjectJoin(const TherionStudio::ProjectJoinPair &pair, bool separateWindow)
{
    if (!pair.canOpen()) {
        return;
    }
    if (pair.needsComputedPositions()
        && !prepareConnectionPositions(TherionStudio::ProjectConnectionPendingOpen{pair, std::nullopt, separateWindow})) {
        return;
    }
    openProjectJoinNow(pair, separateWindow);
}

void MainWindow::openProjectJoinNow(const TherionStudio::ProjectJoinPair &pair, bool separateWindow)
{
    // Each drawing opens at its join-station point, or at the scrap of its survey that stands in for it.
    const QHash<QString, TherionStudio::Th2StationPosition> *positions = currentStationPositions();
    const std::optional<JoinDrawing> first = joinDrawing(lastStructureSidebarProjectIndex_, pair.first, positions);
    const std::optional<JoinDrawing> second = joinDrawing(lastStructureSidebarProjectIndex_, pair.second, positions);
    if (!first.has_value() || !second.has_value()) {
        const QString station = (first.has_value() ? pair.second : pair.first).station.reference;
        statusBar()->showMessage(tr("%1 has no station point in this view, and Therion has computed no position to choose "
                                    "its survey's nearest drawing.")
                                     .arg(station),
                                 8000);
        return;
    }
    openProjectJoinDrawings(first->sourceFile,
                            first->lineNumber,
                            second->sourceFile,
                            second->lineNumber,
                            separateWindow,
                            joinPlacement(first->scrapObjectId,
                                          second->scrapObjectId,
                                          pair.projection,
                                          pair.linkKind == TherionStudio::ProjectStationLinkKind::Equate));
}

void MainWindow::openProjectJoinOnly(const TherionStudio::ProjectJoinGroup &group, bool separateWindow)
{
    if (group.joinOnlyScraps.size() != 2
        || !prepareConnectionPositions(TherionStudio::ProjectConnectionPendingOpen{std::nullopt, group, separateWindow})) {
        return;
    }
    openProjectJoinOnlyNow(group, separateWindow);
}

void MainWindow::openProjectJoinOnlyNow(const TherionStudio::ProjectJoinGroup &group, bool separateWindow)
{
    const TherionStudio::ProjectStructureEntry &first = group.joinOnlyScraps.at(0);
    const TherionStudio::ProjectStructureEntry &second = group.joinOnlyScraps.at(1);
    const QString projection = lastStructureSidebarProjectIndex_.scrapProjectionByObjectId.value(first.objectId);
    openProjectJoinDrawings(first.sourceFile,
                            first.lineNumber,
                            second.sourceFile,
                            second.lineNumber,
                            separateWindow,
                            joinPlacement(first.objectId, second.objectId, projection, false));
}

QString MainWindow::joinFailureMessage(TherionStudio::MapEditorJoinFailure failure, const QString &projection) const
{
    if (failure == TherionStudio::MapEditorJoinFailure::NoFit) {
        return tr("Cannot align the connected drawings because a scrap has neither enough measured legs between its "
                  "station points nor an XVI background.");
    }
    if (projection == QStringLiteral("extended")) {
        return tr("No station is shared by these drawings, and drawings in an extended elevation can only be "
                  "aligned at a shared station.");
    }
    return tr("No station is shared by these drawings, so they open without alignment.");
}

QString MainWindow::joinPlacementNotes(const TherionStudio::MapEditorJoinResult &result,
                                       const QString &firstFile,
                                       const QString &secondFile) const
{
    QStringList notes;
    const auto addNotes = [this, &notes](const TherionStudio::Th2CentrelineFit &fit, bool offBackground, const QString &file) {
        const QString name = QFileInfo(file).fileName();
        if (fit.source != TherionStudio::Th2CentrelineFitSource::MeasuredLegs) {
            notes.append(tr("%1 has too few measured legs between its station points, so it is placed by its XVI "
                            "background, which may be turned by the declination.")
                             .arg(name));
        } else if (offBackground) {
            notes.append(tr("The scale of %1 differs from its XVI background by more than a tenth.").arg(name));
        }
    };
    addNotes(result.firstFit, result.firstOffBackground, firstFile);
    addNotes(result.secondFit, result.secondOffBackground, secondFile);
    return notes.join(QLatin1Char(' '));
}

void MainWindow::openProjectJoinDrawings(const QString &firstFile,
                                         int firstLine,
                                         const QString &secondFile,
                                         int secondLine,
                                         bool separateWindow,
                                         const TherionStudio::MapEditorJoinPlacement &placement)
{
    auto *firstTab = qobject_cast<TherionStudio::MapEditorTab *>(openProjectSourceLocation(firstFile, firstLine));
    auto *secondTab = qobject_cast<TherionStudio::MapEditorTab *>(openProjectSourceLocation(secondFile, secondLine));
    if (firstTab == nullptr || secondTab == nullptr || firstTab == secondTab) {
        return;
    }

    // Outline each drawing in the other and link their views.
    if (separateWindow) {
        // The main window shows the first drawing; the second drawing's map is in its separate window.
        editorTabs_->setCurrentWidget(firstTab);
    }
    const TherionStudio::MapEditorJoinResult result =
        TherionStudio::openMapEditorJoinPair(firstTab, firstLine, secondTab, secondLine, separateWindow, placement);
    if (!result.opened()) {
        statusBar()->showMessage(joinFailureMessage(result.failure, placement.projection), 5000);
    } else if (const QString notes = joinPlacementNotes(result, firstFile, secondFile); !notes.isEmpty()) {
        statusBar()->showMessage(notes, 8000);
    }
    // Keep the Joined indicator and link icons current as the join is removed or its drawing closed.
    for (const TherionStudio::MapEditorJoinPair &joinPair : TherionStudio::mapEditorJoinPairsFor(firstTab)) {
        if (joinPair.partner == secondTab && joinPair.link != nullptr) {
            connect(joinPair.link, &QObject::destroyed, this, &MainWindow::refreshDocumentStatusWidgets, Qt::QueuedConnection);
        }
    }
    refreshDocumentStatusWidgets();
}
