#include "MainWindow.h"

#include "MainWindowValidationFixApplyService.h"
#include "../editor/ValidationSeverityStyle.h"
#include "text_editor/TextEditorValidationCatalog.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPointer>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QScrollBar>

namespace
{
enum ValidationResultRole
{
    ValidationFilePathRole = Qt::UserRole + 90,
    ValidationLineNumberRole,
    ValidationColumnNumberRole,
    ValidationIsFindingRole,
    ValidationDiagnosticIndexRole,
    ValidationSeverityRankRole,
};

bool diagnosticProjectValidationLoggingEnabled()
{
    static const bool enabled = []() {
        const QString value = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
        return value == QStringLiteral("1")
            || value == QStringLiteral("true")
            || value == QStringLiteral("yes")
            || value == QStringLiteral("on");
    }();
    return enabled;
}

bool isAutomaticProjectValidationTrigger(TherionStudio::ProjectValidationController::Trigger trigger)
{
    switch (trigger) {
    case TherionStudio::ProjectValidationController::Trigger::ManualRefresh:
    case TherionStudio::ProjectValidationController::Trigger::FixApplied:
        return false;
    case TherionStudio::ProjectValidationController::Trigger::ProjectOpened:
    case TherionStudio::ProjectValidationController::Trigger::ProjectFilesChanged:
    case TherionStudio::ProjectValidationController::Trigger::DocumentSaved:
    case TherionStudio::ProjectValidationController::Trigger::DocumentChanged:
        return true;
    }
    return true;
}

QString validationTriggerLogName(TherionStudio::ProjectValidationController::Trigger trigger)
{
    switch (trigger) {
    case TherionStudio::ProjectValidationController::Trigger::ManualRefresh:
        return QStringLiteral("manual-refresh");
    case TherionStudio::ProjectValidationController::Trigger::ProjectOpened:
        return QStringLiteral("project-opened");
    case TherionStudio::ProjectValidationController::Trigger::ProjectFilesChanged:
        return QStringLiteral("project-files-changed");
    case TherionStudio::ProjectValidationController::Trigger::DocumentSaved:
        return QStringLiteral("document-saved");
    case TherionStudio::ProjectValidationController::Trigger::DocumentChanged:
        return QStringLiteral("document-changed");
    case TherionStudio::ProjectValidationController::Trigger::FixApplied:
        return QStringLiteral("fix-applied");
    }
    return QStringLiteral("unknown");
}

int severityRank(TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    switch (severity) {
    case TherionStudio::TherionSourceDiagnosticSeverity::Error:
        return 2;
    case TherionStudio::TherionSourceDiagnosticSeverity::Warning:
        return 1;
    }
    return 1;
}

TherionStudio::TherionSourceDiagnosticSeverity higherSeverity(
    TherionStudio::TherionSourceDiagnosticSeverity left,
    TherionStudio::TherionSourceDiagnosticSeverity right)
{
    return severityRank(left) >= severityRank(right) ? left : right;
}

QColor validationSeverityColor(TherionStudio::TherionSourceDiagnosticSeverity severity, const QPalette &palette)
{
    return TherionStudio::validationSeverityForeground(severity, palette);
}

void applyValidationSeverityStyle(QStandardItem *item, TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    if (item == nullptr) {
        return;
    }
    const QPalette palette = qApp != nullptr ? qApp->palette() : QPalette();
    item->setForeground(QBrush(validationSeverityColor(severity, palette)));
    item->setData(severityRank(severity), ValidationSeverityRankRole);
}

void updateValidationFileSeverityStyle(QStandardItem *fileItem, TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    if (fileItem == nullptr) {
        return;
    }
    const int currentRank = fileItem->data(ValidationSeverityRankRole).toInt();
    if (severityRank(severity) > currentRank) {
        applyValidationSeverityStyle(fileItem, severity);
    }
}

TherionStudio::TherionSourceDiagnosticSeverity highestDiagnosticSeverity(
    const QVector<TherionStudio::TherionSourceDiagnostic> &diagnostics)
{
    TherionStudio::TherionSourceDiagnosticSeverity highest =
        TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    for (const TherionStudio::TherionSourceDiagnostic &diagnostic : diagnostics) {
        highest = higherSeverity(highest, diagnostic.severity);
    }
    return highest;
}

bool validationDiagnosticLess(const TherionStudio::TherionSourceDiagnostic &left,
                              const TherionStudio::TherionSourceDiagnostic &right)
{
    const int leftLine = qMax(1, left.lineNumber);
    const int rightLine = qMax(1, right.lineNumber);
    if (leftLine != rightLine) {
        return leftLine < rightLine;
    }

    const int leftColumn = qMax(1, left.columnNumber);
    const int rightColumn = qMax(1, right.columnNumber);
    if (leftColumn != rightColumn) {
        return leftColumn < rightColumn;
    }

    const int leftSeverity = severityRank(left.severity);
    const int rightSeverity = severityRank(right.severity);
    if (leftSeverity != rightSeverity) {
        return leftSeverity > rightSeverity;
    }

    const int codeCompare = left.code.compare(right.code, Qt::CaseInsensitive);
    if (codeCompare != 0) {
        return codeCompare < 0;
    }
    return left.title.compare(right.title, Qt::CaseInsensitive) < 0;
}

QString severityLabel(TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    switch (severity) {
    case TherionStudio::TherionSourceDiagnosticSeverity::Error:
        return QCoreApplication::translate("TherionStudio::MainWindow", "Error");
    case TherionStudio::TherionSourceDiagnosticSeverity::Warning:
        return QCoreApplication::translate("TherionStudio::MainWindow", "Warning");
    }
    return QCoreApplication::translate("TherionStudio::MainWindow", "Warning");
}

QString validationDocumentLabel(const QString &displayName, const QString &filePath)
{
    if (!displayName.isEmpty()) {
        return displayName;
    }
    if (!filePath.isEmpty()) {
        return QFileInfo(filePath).fileName();
    }
    return QCoreApplication::translate("TherionStudio::MainWindow", "Untitled document");
}

QString validationRelativeDisplayPath(const QString &projectRootPath, const QString &filePath)
{
    if (projectRootPath.trimmed().isEmpty() || filePath.trimmed().isEmpty()) {
        return validationDocumentLabel(QFileInfo(filePath).fileName(), filePath);
    }

    const QString relativePath = QDir(projectRootPath).relativeFilePath(filePath);
    if (!relativePath.startsWith(QStringLiteral(".."))) {
        return QDir::toNativeSeparators(relativePath);
    }
    return QFileInfo(filePath).fileName();
}

QString normalizedValidationPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

void configureValidationSourcePreview(QPlainTextEdit *edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setMaximumHeight(72);
}

bool validationFixRemovesSource(const TherionStudio::TherionSourceDiagnostic &diagnostic)
{
    return diagnostic.hasFix
        && diagnostic.fix.length > 0
        && diagnostic.fix.replacementText.isEmpty();
}

QModelIndex validationFindingIndex(QStandardItemModel *model, const QModelIndex &index)
{
    if (model == nullptr || !index.isValid()) {
        return {};
    }

    if (index.data(ValidationIsFindingRole).toBool()) {
        return index;
    }
    if (model->hasChildren(index)) {
        return model->index(0, 0, index);
    }
    return {};
}

int validationDiagnosticIndex(QStandardItemModel *model, const QModelIndex &index)
{
    const QModelIndex findingIndex = validationFindingIndex(model, index);
    if (!findingIndex.isValid()) {
        return -1;
    }
    return findingIndex.data(ValidationDiagnosticIndexRole).toInt();
}

int validationScrollValue(QTreeView *tree)
{
    if (tree == nullptr || tree->verticalScrollBar() == nullptr) {
        return 0;
    }
    return tree->verticalScrollBar()->value();
}

void setValidationScrollValue(QTreeView *tree, int value)
{
    if (tree == nullptr || tree->verticalScrollBar() == nullptr) {
        return;
    }
    QScrollBar *scrollBar = tree->verticalScrollBar();
    scrollBar->setValue(qBound(scrollBar->minimum(), value, scrollBar->maximum()));
}

void restoreValidationScrollValue(QTreeView *tree, int value)
{
    setValidationScrollValue(tree, value);

    QPointer<QTreeView> guardedTree(tree);
    QTimer::singleShot(0, tree, [guardedTree, value]() {
        if (guardedTree == nullptr) {
            return;
        }
        setValidationScrollValue(guardedTree, value);
    });
}
}

void MainWindow::buildValidationSidebar()
{
    if (sidebarPages_ == nullptr || validationResultsModel_ == nullptr) {
        return;
    }

    auto *validationPage = new QWidget(sidebarPages_);
    validationPage->setMinimumWidth(0);
    validationPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *validationLayout = new QVBoxLayout(validationPage);
    validationLayout->setContentsMargins(12, 12, 12, 12);
    validationLayout->setSpacing(8);

    auto *validationHeader = new QLabel(tr("Validate source files and review problems."), validationPage);
    validationHeader->setWordWrap(true);
    validationLayout->addWidget(validationHeader);

    validationScanProjectButton_ = new QPushButton(tr("Validate Project"), validationPage);
    connect(validationScanProjectButton_, &QPushButton::clicked, this, [this]() {
        requestProjectValidation();
    });
    validationLayout->addWidget(validationScanProjectButton_);

    validationStatusLabel_ = new QLabel(validationPage);
    validationStatusLabel_->setWordWrap(true);
    validationLayout->addWidget(validationStatusLabel_);
    updateProjectValidationStatusMessage();

    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});

    validationResultsTree_ = new QTreeView(validationPage);
    validationResultsTree_->setMinimumWidth(0);
    validationResultsTree_->setModel(validationResultsModel_);
    validationResultsTree_->setRootIsDecorated(true);
    validationResultsTree_->setAnimated(true);
    validationResultsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    validationResultsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    validationResultsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validationResultsTree_->setAlternatingRowColors(true);
    validationResultsTree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    validationResultsTree_->header()->setStretchLastSection(true);
    connect(validationResultsTree_, &QTreeView::activated, this, &MainWindow::openValidationResult);
    connect(validationResultsTree_, &QTreeView::clicked, this, &MainWindow::openValidationResult);
    connect(validationResultsTree_, &QTreeView::doubleClicked, this, &MainWindow::openValidationResult);
    connect(validationResultsTree_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::handleValidationSelectionChanged);
    validationLayout->addWidget(validationResultsTree_, 1);

    validationDetailTitleLabel_ = new QLabel(tr("Select a validation finding."), validationPage);
    validationDetailTitleLabel_->setWordWrap(true);
    validationLayout->addWidget(validationDetailTitleLabel_);

    validationDetailMessageLabel_ = new QLabel(validationPage);
    validationDetailMessageLabel_->setWordWrap(true);
    validationLayout->addWidget(validationDetailMessageLabel_);

    validationCurrentSourceLabel_ = new QLabel(tr("Current source line"), validationPage);
    validationLayout->addWidget(validationCurrentSourceLabel_);
    validationCurrentSourceEdit_ = new QPlainTextEdit(validationPage);
    configureValidationSourcePreview(validationCurrentSourceEdit_);
    validationLayout->addWidget(validationCurrentSourceEdit_);

    validationSuggestedSourceLabel_ = new QLabel(tr("Automatic fix preview"), validationPage);
    validationLayout->addWidget(validationSuggestedSourceLabel_);
    validationSuggestedSourceEdit_ = new QPlainTextEdit(validationPage);
    configureValidationSourcePreview(validationSuggestedSourceEdit_);
    validationLayout->addWidget(validationSuggestedSourceEdit_);

    auto *validationActionRow = new QHBoxLayout;
    validationActionRow->setContentsMargins(0, 0, 0, 0);
    validationActionRow->setSpacing(6);
    validationApplyFixButton_ = new QPushButton(tr("Apply Fix"), validationPage);
    validationApplyFixButton_->setEnabled(false);
    connect(validationApplyFixButton_, &QPushButton::clicked, this, &MainWindow::applySelectedValidationFix);
    validationActionRow->addWidget(validationApplyFixButton_);
    validationLayout->addLayout(validationActionRow);

    sidebarPages_->addWidget(validationPage);
}

void MainWindow::triggerValidateDocumentForActiveDocument()
{
    TherionStudio::TherionSourceValidationResult validation;
    QString displayName;
    QString filePath;

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        validation = textTab->validateDocument();
        displayName = textTab->displayName();
        filePath = textTab->filePath();
    } else if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        validation = mapTab->validateDocument();
        displayName = mapTab->displayName();
        filePath = mapTab->filePath();
    } else {
        return;
    }

    if (validationResultsModel_ == nullptr) {
        return;
    }

    const int previousScrollValue = validationScrollValue(validationResultsTree_);
    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
    validationDiagnostics_ = validation.diagnostics;
    validationDiagnosticFilePaths_.clear();
    validationDiagnosticFilePaths_.reserve(validation.diagnostics.size());
    for (qsizetype index = 0; index < validation.diagnostics.size(); ++index) {
        validationDiagnosticFilePaths_.append(filePath);
    }
    validationDocumentPath_ = filePath;
    validationProjectMode_ = false;

    const QString documentLabel = validationDocumentLabel(displayName, filePath);
    if (validation.diagnostics.isEmpty()) {
        clearValidationRailIndicator();
        pendingValidationFixNavigation_ = false;
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("No validation problems found in %1.").arg(documentLabel));
        }
        handleValidationSelectionChanged({}, {});
        showSidebarPane(SidebarPane::Validation);
        return;
    }

    validationProblemCount_ = validation.diagnostics.size();
    validationHighestSeverity_ = highestDiagnosticSeverity(validation.diagnostics);
    updateValidationRailIndicator();

    auto *fileItem = new QStandardItem(tr("%1 (%2)").arg(documentLabel).arg(validation.diagnostics.size()));
    fileItem->setEditable(false);
    fileItem->setData(filePath, ValidationFilePathRole);
    fileItem->setData(false, ValidationIsFindingRole);
    applyValidationSeverityStyle(fileItem, validationHighestSeverity_);

    QVector<int> sortedDiagnosticIndexes;
    sortedDiagnosticIndexes.reserve(validation.diagnostics.size());
    for (int diagnosticIndex = 0; diagnosticIndex < validation.diagnostics.size(); ++diagnosticIndex) {
        sortedDiagnosticIndexes.append(diagnosticIndex);
    }
    std::stable_sort(sortedDiagnosticIndexes.begin(),
                     sortedDiagnosticIndexes.end(),
                     [&validation](int leftIndex, int rightIndex) {
                         return validationDiagnosticLess(validation.diagnostics.at(leftIndex),
                                                         validation.diagnostics.at(rightIndex));
                     });

    for (const int diagnosticIndex : std::as_const(sortedDiagnosticIndexes)) {
        const TherionStudio::TherionSourceDiagnostic &diagnostic = validation.diagnostics.at(diagnosticIndex);
        const QString fixSuffix = diagnostic.hasFix ? tr(" (safe fix available)") : QString();
        const QString label = tr("Line %1: %2: %3%4")
                                  .arg(diagnostic.lineNumber)
                                  .arg(severityLabel(diagnostic.severity))
                                  .arg(diagnostic.title)
                                  .arg(fixSuffix);
        auto *findingItem = new QStandardItem(label);
        findingItem->setEditable(false);
        applyValidationSeverityStyle(findingItem, diagnostic.severity);
        findingItem->setToolTip(diagnostic.message);
        findingItem->setData(filePath, ValidationFilePathRole);
        findingItem->setData(qMax(1, diagnostic.lineNumber), ValidationLineNumberRole);
        findingItem->setData(qMax(1, diagnostic.columnNumber), ValidationColumnNumberRole);
        findingItem->setData(true, ValidationIsFindingRole);
        findingItem->setData(diagnosticIndex, ValidationDiagnosticIndexRole);
        fileItem->appendRow(findingItem);
    }

    validationResultsModel_->appendRow(fileItem);

    if (validationStatusLabel_ != nullptr) {
        validationStatusLabel_->setText(tr("%1 validation problem(s) found in %2.")
                                            .arg(validation.diagnostics.size())
                                            .arg(documentLabel));
    }
    if (validationResultsTree_ != nullptr) {
        validationResultsTree_->expandAll();
        validationResultsTree_->resizeColumnToContents(0);
        const QModelIndex firstFinding = validationResultsModel_->index(0, 0, validationResultsModel_->index(0, 0));
        if (firstFinding.isValid()) {
            validationResultsTree_->setCurrentIndex(firstFinding);
            handleValidationSelectionChanged(firstFinding, {});
            if (pendingValidationFixNavigation_) {
                pendingValidationFixNavigation_ = false;
                openValidationResult(firstFinding);
            }
        }
        restoreValidationScrollValue(validationResultsTree_, previousScrollValue);
    }
    showSidebarPane(SidebarPane::Validation);
}

void MainWindow::requestProjectValidation()
{
    requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ManualRefresh,
                             true);
}

void MainWindow::updateProjectValidationStatusMessage()
{
    if (validationStatusLabel_ == nullptr || sessionStore_ == nullptr) {
        return;
    }

    if (sessionStore_->automaticProjectValidationEnabled()) {
        validationStatusLabel_->setText(
            tr("Automatic project validation is enabled. Use Validate Project to refresh now."));
    } else {
        validationStatusLabel_->setText(
            tr("Automatic project validation is disabled. Use Validate Project to run it manually."));
    }
}

void MainWindow::requestRestoredProjectValidation()
{
    if (!projectRootPath_.trimmed().isEmpty()
        && QDir(projectRootPath_).exists()
        && sessionStore_ != nullptr
        && sessionStore_->automaticProjectValidationEnabled()) {
        requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectOpened, false);
    }
}

void MainWindow::requestProjectValidation(TherionStudio::ProjectValidationController::Trigger trigger,
                                          bool revealPanel)
{
    if (projectValidationController_ == nullptr) {
        return;
    }

    if (isAutomaticProjectValidationTrigger(trigger)
        && (sessionStore_ == nullptr || !sessionStore_->automaticProjectValidationEnabled())) {
        if (diagnosticProjectValidationLoggingEnabled()) {
            qInfo().noquote()
                << QStringLiteral("project-validation-request skipped trigger=%1 reason=automatic-disabled")
                       .arg(validationTriggerLogName(trigger));
        }
        if (revealPanel) {
            updateProjectValidationStatusMessage();
            showSidebarPane(SidebarPane::Validation);
        }
        return;
    }

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        if (revealPanel && validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("Open a project before validating."));
        }
        if (revealPanel) {
            showSidebarPane(SidebarPane::Validation);
        }
        return;
    }

    QHash<QString, QString> inMemoryProjectContentsByPath;
    auto captureInMemorySource = [&inMemoryProjectContentsByPath](QWidget *widget) {
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

        const QFileInfo info(sourceFile);
        const QString normalizedPath = info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath()
            : info.canonicalFilePath();
        if (!normalizedPath.isEmpty()) {
            inMemoryProjectContentsByPath.insert(normalizedPath, sourceText);
        }
    };

    for (int index = 0; editorTabs_ != nullptr && index < editorTabs_->count(); ++index) {
        captureInMemorySource(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        captureInMemorySource(detachedTab);
    }

    TherionStudio::ProjectValidationController::Request request;
    request.trigger = trigger;
    request.projectRootPath = projectRootPath_;
    request.validationCatalog = TherionStudio::validationCatalogFromCommandCatalog(commandCatalogStore_.catalogObject());
    request.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    pendingProjectValidationRevealPanel_ = revealPanel;
    if (revealPanel && validationStatusLabel_ != nullptr) {
        validationStatusLabel_->setText(tr("Validating project..."));
    }
    if (revealPanel && validationScanProjectButton_ != nullptr) {
        validationScanProjectButton_->setEnabled(false);
    }
    if (revealPanel) {
        showSidebarPane(SidebarPane::Validation);
    }
    projectValidationController_->requestValidation(request);
}

void MainWindow::handleProjectValidationStarted(TherionStudio::ProjectValidationController::Trigger trigger,
                                                quint64 generation,
                                                const QString &projectRootPath)
{
    if (normalizedValidationPath(projectRootPath) != normalizedValidationPath(projectRootPath_)) {
        return;
    }

    const bool revealPanel = pendingProjectValidationRevealPanel_;
    validationRevealByGeneration_.insert(generation, revealPanel);
    const bool replaceVisibleResults = revealPanel
        || trigger == TherionStudio::ProjectValidationController::Trigger::ManualRefresh;
    if (replaceVisibleResults) {
        validationDiagnostics_.clear();
        validationDiagnosticFilePaths_.clear();
        validationDocumentPath_.clear();
        clearValidationRailIndicator();
        validationProjectMode_ = true;
        if (validationResultsModel_ != nullptr) {
            validationResultsModel_->clear();
            validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
        }
        handleValidationSelectionChanged({}, {});
    }
    if (replaceVisibleResults && validationStatusLabel_ != nullptr) {
        validationStatusLabel_->setText(tr("Validating project..."));
    }
    if (replaceVisibleResults && validationScanProjectButton_ != nullptr) {
        validationScanProjectButton_->setEnabled(false);
    }
    if (revealPanel) {
        showSidebarPane(SidebarPane::Validation);
    }
}

void MainWindow::updateOpenEditorProjectValidationDiagnostics()
{
    QHash<QString, QVector<TherionStudio::TherionSourceDiagnostic>> diagnosticsByPath;
    for (int index = 0; index < validationDiagnostics_.size(); ++index) {
        if (index >= validationDiagnosticFilePaths_.size()) {
            continue;
        }

        const QString normalizedPath = normalizedValidationPath(validationDiagnosticFilePaths_.at(index));
        if (!normalizedPath.isEmpty()) {
            diagnosticsByPath[normalizedPath].append(validationDiagnostics_.at(index));
        }
    }

    auto applyToDocumentWidget = [&diagnosticsByPath](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }

        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
            textTab->setProjectValidationDiagnostics(diagnosticsByPath.value(normalizedValidationPath(textTab->filePath())));
            return;
        }

        if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
            if (mapTab->workspaceMode() != TherionStudio::MapEditorTab::WorkspaceMode::Raw) {
                return;
            }
            mapTab->setProjectValidationDiagnostics(diagnosticsByPath.value(normalizedValidationPath(mapTab->filePath())));
        }
    };

    if (editorTabs_ != nullptr) {
        for (int index = 0; index < editorTabs_->count(); ++index) {
            applyToDocumentWidget(editorTabs_->widget(index));
        }
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        applyToDocumentWidget(detachedTab);
    }
}

void MainWindow::handleProjectValidationFinished(TherionStudio::ProjectValidationController::Trigger trigger,
                                                 const TherionStudio::ProjectValidationScanner::Result &result)
{
    QElapsedTimer finishTimer;
    finishTimer.start();
    qint64 modelRebuildMs = 0;
    qint64 diagnosticsApplyMs = 0;
    qint64 treeUpdateMs = 0;
    const bool revealPanel = validationRevealByGeneration_.take(result.generation);
    const bool navigateAfterFix = trigger == TherionStudio::ProjectValidationController::Trigger::FixApplied;
    if (validationScanProjectButton_ != nullptr) {
        validationScanProjectButton_->setEnabled(true);
    }
    if (normalizedValidationPath(result.projectRootPath) != normalizedValidationPath(projectRootPath_)) {
        return;
    }
    if (validationResultsModel_ == nullptr) {
        return;
    }
    auto logFinish = [&]() {
        if (!diagnosticProjectValidationLoggingEnabled()) {
            return;
        }
        qInfo().noquote()
            << QStringLiteral("project-validation-ui trigger=%1 generation=%2 files=%3 findings=%4 limit_reached=%5 reveal=%6 model_ms=%7 diagnostics_ms=%8 tree_ms=%9 total_ms=%10")
                   .arg(validationTriggerLogName(trigger))
                   .arg(result.generation)
                   .arg(result.searchedFileCount)
                   .arg(result.findings.size())
                   .arg(result.limitReached ? QStringLiteral("true") : QStringLiteral("false"))
                   .arg(revealPanel ? QStringLiteral("true") : QStringLiteral("false"))
                   .arg(modelRebuildMs)
                   .arg(diagnosticsApplyMs)
                   .arg(treeUpdateMs)
                   .arg(finishTimer.elapsed());
    };

    const int previousScrollValue = validationScrollValue(validationResultsTree_);
    QString selectedFilePath;
    int selectedLineNumber = 0;
    int selectedColumnNumber = 0;
    QString selectedDiagnosticCode;
    if (validationResultsTree_ != nullptr) {
        const QModelIndex currentFinding =
            validationFindingIndex(validationResultsModel_, validationResultsTree_->currentIndex());
        const int selectedDiagnosticIndex = validationDiagnosticIndex(validationResultsModel_, currentFinding);
        if (selectedDiagnosticIndex >= 0 && selectedDiagnosticIndex < validationDiagnostics_.size()) {
            selectedFilePath = normalizedValidationPath(currentFinding.data(ValidationFilePathRole).toString());
            selectedLineNumber = qMax(1, currentFinding.data(ValidationLineNumberRole).toInt());
            selectedColumnNumber = qMax(1, currentFinding.data(ValidationColumnNumberRole).toInt());
            selectedDiagnosticCode = validationDiagnostics_.at(selectedDiagnosticIndex).code;
        }
    }

    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
    validationDiagnostics_.clear();
    validationDiagnosticFilePaths_.clear();
    validationDocumentPath_.clear();
    validationProjectMode_ = true;

    if (!result.errorMessage.isEmpty()) {
        clearValidationRailIndicator();
        QElapsedTimer diagnosticsTimer;
        diagnosticsTimer.start();
        updateOpenEditorProjectValidationDiagnostics();
        diagnosticsApplyMs = diagnosticsTimer.elapsed();
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(result.errorMessage);
        }
        handleValidationSelectionChanged({}, {});
        logFinish();
        return;
    }

    if (result.findings.isEmpty()) {
        clearValidationRailIndicator();
        QElapsedTimer diagnosticsTimer;
        diagnosticsTimer.start();
        updateOpenEditorProjectValidationDiagnostics();
        diagnosticsApplyMs = diagnosticsTimer.elapsed();
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("No validation problems found in %1 searched file(s).")
                                                .arg(result.searchedFileCount));
        }
        handleValidationSelectionChanged({}, {});
        logFinish();
        return;
    }

    validationProblemCount_ = result.findings.size();
    validationHighestSeverity_ = TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    for (const TherionStudio::ProjectValidationScanner::Finding &finding : result.findings) {
        validationHighestSeverity_ = higherSeverity(validationHighestSeverity_, finding.diagnostic.severity);
    }
    updateValidationRailIndicator();

    QHash<QString, QStandardItem *> fileItemsByPath;
    QHash<QString, QVector<TherionStudio::ProjectValidationScanner::Finding>> findingsByPath;
    QVector<QString> orderedFilePaths;
    QElapsedTimer modelTimer;
    modelTimer.start();
    for (const TherionStudio::ProjectValidationScanner::Finding &finding : result.findings) {
        if (!findingsByPath.contains(finding.filePath)) {
            orderedFilePaths.append(finding.filePath);
        }
        findingsByPath[finding.filePath].append(finding);
    }

    QModelIndex restoredFinding;
    for (const QString &filePath : std::as_const(orderedFilePaths)) {
        QVector<TherionStudio::ProjectValidationScanner::Finding> fileFindings =
            findingsByPath.value(filePath);
        std::stable_sort(fileFindings.begin(),
                         fileFindings.end(),
                         [](const TherionStudio::ProjectValidationScanner::Finding &left,
                            const TherionStudio::ProjectValidationScanner::Finding &right) {
                             return validationDiagnosticLess(left.diagnostic, right.diagnostic);
                         });

        for (const TherionStudio::ProjectValidationScanner::Finding &finding : std::as_const(fileFindings)) {
            QStandardItem *fileItem = fileItemsByPath.value(finding.filePath, nullptr);
            if (fileItem == nullptr) {
                fileItem = new QStandardItem(validationRelativeDisplayPath(projectRootPath_, finding.filePath));
                fileItem->setEditable(false);
                fileItem->setData(finding.filePath, ValidationFilePathRole);
                fileItem->setData(false, ValidationIsFindingRole);
                validationResultsModel_->appendRow(fileItem);
                fileItemsByPath.insert(finding.filePath, fileItem);
            }

            const int diagnosticIndex = validationDiagnostics_.size();
            validationDiagnostics_.append(finding.diagnostic);
            validationDiagnosticFilePaths_.append(finding.filePath);

            const QString fixSuffix = finding.diagnostic.hasFix ? tr(" (safe fix available)") : QString();
            const QString label = tr("Line %1: %2: %3%4")
                                      .arg(finding.diagnostic.lineNumber)
                                      .arg(severityLabel(finding.diagnostic.severity))
                                      .arg(finding.diagnostic.title)
                                      .arg(fixSuffix);
            auto *findingItem = new QStandardItem(label);
            findingItem->setEditable(false);
            applyValidationSeverityStyle(findingItem, finding.diagnostic.severity);
            updateValidationFileSeverityStyle(fileItem, finding.diagnostic.severity);
            findingItem->setToolTip(finding.diagnostic.message);
            findingItem->setData(finding.filePath, ValidationFilePathRole);
            findingItem->setData(qMax(1, finding.diagnostic.lineNumber), ValidationLineNumberRole);
            findingItem->setData(qMax(1, finding.diagnostic.columnNumber), ValidationColumnNumberRole);
            findingItem->setData(true, ValidationIsFindingRole);
            findingItem->setData(diagnosticIndex, ValidationDiagnosticIndexRole);
            fileItem->appendRow(findingItem);
            if (!restoredFinding.isValid()
                && normalizedValidationPath(finding.filePath) == selectedFilePath
                && qMax(1, finding.diagnostic.lineNumber) == selectedLineNumber
                && qMax(1, finding.diagnostic.columnNumber) == selectedColumnNumber
                && finding.diagnostic.code == selectedDiagnosticCode) {
                restoredFinding = findingItem->index();
            }
        }
    }
    modelRebuildMs = modelTimer.elapsed();

    QElapsedTimer diagnosticsTimer;
    diagnosticsTimer.start();
    updateOpenEditorProjectValidationDiagnostics();
    diagnosticsApplyMs = diagnosticsTimer.elapsed();

    if (validationStatusLabel_ != nullptr) {
        QString status = tr("%1 validation problem(s) found in %2 searched file(s).")
                             .arg(result.findings.size())
                             .arg(result.searchedFileCount);
        if (result.limitReached) {
            status += QLatin1Char(' ');
            status += tr("Showing the first %1 problem(s).").arg(result.findings.size());
        }
        validationStatusLabel_->setText(status);
    }
    if (validationResultsTree_ != nullptr) {
        QElapsedTimer treeTimer;
        treeTimer.start();
        validationResultsTree_->expandAll();
        validationResultsTree_->resizeColumnToContents(0);
        const QModelIndex firstFinding =
            validationResultsModel_->index(0, 0, validationResultsModel_->index(0, 0));
        const QModelIndex nextFinding = restoredFinding.isValid()
            ? restoredFinding
            : (revealPanel ? firstFinding : QModelIndex());
        if (nextFinding.isValid()) {
            validationResultsTree_->setCurrentIndex(nextFinding);
            handleValidationSelectionChanged(nextFinding, {});
            if (navigateAfterFix) {
                openValidationResult(nextFinding);
            }
        } else {
            handleValidationSelectionChanged({}, {});
        }
        restoreValidationScrollValue(validationResultsTree_, previousScrollValue);
        treeUpdateMs = treeTimer.elapsed();
    }
    if (revealPanel) {
        showSidebarPane(SidebarPane::Validation);
    }
    logFinish();
}

void MainWindow::handleValidationSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)

    const int diagnosticIndex = validationDiagnosticIndex(validationResultsModel_, current);
    if (diagnosticIndex < 0 || diagnosticIndex >= validationDiagnostics_.size()) {
        if (validationDetailTitleLabel_ != nullptr) {
            validationDetailTitleLabel_->setText(tr("Select a validation finding."));
        }
        if (validationDetailMessageLabel_ != nullptr) {
            validationDetailMessageLabel_->clear();
        }
        if (validationCurrentSourceLabel_ != nullptr) {
            validationCurrentSourceLabel_->setText(tr("Current source line"));
        }
        if (validationSuggestedSourceLabel_ != nullptr) {
            validationSuggestedSourceLabel_->setText(tr("Automatic fix preview"));
        }
        if (validationCurrentSourceEdit_ != nullptr) {
            validationCurrentSourceEdit_->clear();
        }
        if (validationSuggestedSourceEdit_ != nullptr) {
            validationSuggestedSourceEdit_->clear();
        }
        if (validationApplyFixButton_ != nullptr) {
            validationApplyFixButton_->setEnabled(false);
            validationApplyFixButton_->setText(tr("Apply Fix"));
        }
        return;
    }

    const TherionStudio::TherionSourceDiagnostic &diagnostic = validationDiagnostics_.at(diagnosticIndex);
    const bool removesSource = validationFixRemovesSource(diagnostic);
    if (validationDetailTitleLabel_ != nullptr) {
        validationDetailTitleLabel_->setText(tr("Line %1: %2").arg(diagnostic.lineNumber).arg(diagnostic.title));
    }
    if (validationDetailMessageLabel_ != nullptr) {
        validationDetailMessageLabel_->setText(diagnostic.message);
    }
    if (validationCurrentSourceLabel_ != nullptr) {
        validationCurrentSourceLabel_->setText(removesSource
                                                   ? tr("Source block to remove")
                                                   : tr("Current source line"));
    }
    if (validationCurrentSourceEdit_ != nullptr) {
        validationCurrentSourceEdit_->setPlainText(diagnostic.currentText);
    }
    if (validationSuggestedSourceLabel_ != nullptr) {
        validationSuggestedSourceLabel_->setText(removesSource
                                                     ? tr("Automatic fix")
                                                     : tr("Automatic fix preview"));
    }
    if (validationSuggestedSourceEdit_ != nullptr) {
        if (!diagnostic.hasFix) {
            validationSuggestedSourceEdit_->setPlainText(tr("No automatic fix is available for this finding."));
        } else if (removesSource) {
            validationSuggestedSourceEdit_->setPlainText(tr("This fix will remove the source block shown above."));
        } else {
            validationSuggestedSourceEdit_->setPlainText(diagnostic.suggestedText);
        }
    }
    if (validationApplyFixButton_ != nullptr) {
        validationApplyFixButton_->setEnabled(diagnostic.hasFix);
        validationApplyFixButton_->setText(diagnostic.hasFix && !diagnostic.fix.description.isEmpty()
                                               ? diagnostic.fix.description
                                               : tr("Apply Fix"));
    }
}

void MainWindow::openValidationResult(const QModelIndex &index)
{
    if (validationResultsModel_ == nullptr || !index.isValid()) {
        return;
    }

    QModelIndex findingIndex = validationFindingIndex(validationResultsModel_, index);
    if (!findingIndex.isValid() || !findingIndex.data(ValidationIsFindingRole).toBool()) {
        return;
    }

    const int lineNumber = qMax(1, findingIndex.data(ValidationLineNumberRole).toInt());
    const int columnNumber = qMax(1, findingIndex.data(ValidationColumnNumberRole).toInt());
    const QString filePath = findingIndex.data(ValidationFilePathRole).toString();
    if (validationResultsTree_ != nullptr && validationResultsTree_->currentIndex() != findingIndex) {
        validationResultsTree_->setCurrentIndex(findingIndex);
    }

    if (!filePath.isEmpty()) {
        QWidget *targetWidget = documentWidgetForFilePath(filePath);
        if (qobject_cast<TherionStudio::TextEditorTab *>(targetWidget) != nullptr) {
            targetWidget = openTextTab(filePath);
        } else if (qobject_cast<TherionStudio::MapEditorTab *>(targetWidget) != nullptr) {
            targetWidget = openMapEditorTab(filePath);
        } else {
            const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenProjectSearchResult(filePath);
            targetWidget = openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenMapDocument
                ? static_cast<QWidget *>(openMapEditorTab(filePath))
                : static_cast<QWidget *>(openTextTab(filePath));
        }
        if (targetWidget == nullptr) {
            return;
        }

        if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(targetWidget)) {
            QPointer<TherionStudio::MapEditorTab> guardedTab(mapTab);
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
            QTimer::singleShot(0, mapTab, [guardedTab, lineNumber]() {
                if (guardedTab == nullptr) {
                    return;
                }
                guardedTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
                guardedTab->goToLine(lineNumber);
            });
            return;
        }

        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(targetWidget)) {
            QPointer<TherionStudio::TextEditorTab> guardedTab(textTab);
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
            QTimer::singleShot(0, textTab, [guardedTab, lineNumber, columnNumber]() {
                if (guardedTab == nullptr) {
                    return;
                }
                guardedTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
                guardedTab->goToLineColumn(lineNumber, columnNumber);
            });
        }
        return;
    }

    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        QPointer<TherionStudio::MapEditorTab> guardedTab(mapTab);
        QTimer::singleShot(0, mapTab, [guardedTab, lineNumber]() {
            if (guardedTab == nullptr) {
                return;
            }
            guardedTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
            guardedTab->goToLine(lineNumber);
        });
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        QPointer<TherionStudio::TextEditorTab> guardedTab(textTab);
        QTimer::singleShot(0, textTab, [guardedTab, lineNumber, columnNumber]() {
            if (guardedTab == nullptr) {
                return;
            }
            guardedTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
            guardedTab->goToLineColumn(lineNumber, columnNumber);
        });
    }
}

void MainWindow::applySelectedValidationFix()
{
    const int diagnosticIndex = validationResultsTree_ != nullptr
        ? validationDiagnosticIndex(validationResultsModel_, validationResultsTree_->currentIndex())
        : -1;
    if (diagnosticIndex < 0 || diagnosticIndex >= validationDiagnostics_.size()) {
        return;
    }

    const TherionStudio::TherionSourceDiagnostic &diagnostic = validationDiagnostics_.at(diagnosticIndex);
    if (!diagnostic.hasFix) {
        return;
    }

    const QString filePath = diagnosticIndex < validationDiagnosticFilePaths_.size()
        ? validationDiagnosticFilePaths_.at(diagnosticIndex)
        : QString();
    if (applyValidationFixesToValidatedDocument(filePath, {diagnostic.fix})) {
        if (validationProjectMode_) {
            requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::FixApplied, true);
        } else {
            pendingValidationFixNavigation_ = true;
            triggerValidateDocumentForActiveDocument();
        }
    }
}

bool MainWindow::applyValidationFixesToValidatedDocument(const QString &filePath,
                                                        const QVector<TherionStudio::TherionSourceDiagnosticFix> &fixes)
{
    TherionStudio::MainWindowValidationFixApplyContext context;
    context.applyFixesToMapPath = [this](const QString &targetPath,
                                         const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *mapTab = openMapEditorTab(targetPath);
        return mapTab != nullptr && mapTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToTextPath = [this](const QString &targetPath,
                                          const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *textTab = openTextTab(targetPath);
        return textTab != nullptr && textTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToCurrentMap = [this](const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *mapTab = currentMapEditorTab();
        return mapTab != nullptr && mapTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToCurrentText = [this](const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *textTab = currentTextTab();
        return textTab != nullptr && textTab->applyValidationFixes(targetFixes);
    };

    return TherionStudio::MainWindowValidationFixApplyService::applyValidationFixes(filePath,
                                                                                     validationDocumentPath_,
                                                                                     fixes,
                                                                                     context);
}
