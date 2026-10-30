#include "MainWindowSettingsDialog.h"
#include "ProjectVersioningWidget.h"

#include "../platform/DiagnosticLogging.h"
#include "TherionExecutableDetector.h"
#include "TherionExecutableSelectionController.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace TherionStudio
{
MainWindowSettingsDialog::MainWindowSettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent)
    , projectRootPath_(settings.projectRootPath)
{
    setWindowTitle(tr("Settings"));
    setModal(true);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(10);

    languageCombo_ = new QComboBox(this);
    languageCombo_->addItem(tr("System Default"), QStringLiteral("system"));
    languageCombo_->addItem(tr("English"), QStringLiteral("en"));
    languageCombo_->addItem(tr("Czech"), QStringLiteral("cs"));
    languageCombo_->addItem(tr("French"), QStringLiteral("fr"));
    languageCombo_->addItem(tr("Slovak"), QStringLiteral("sk"));
    selectComboData(languageCombo_, settings.applicationLanguage, QStringLiteral("system"));
    formLayout->addRow(tr("Language"), languageCombo_);

    auto *languageNote = new QLabel(
        tr("Language changes take effect after restarting Therion Studio."), this);
    languageNote->setWordWrap(true);
    languageNote->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); font-size: 11px; }"));
    formLayout->addRow(QString(), languageNote);

    therionExecutableEdit_ = new QLineEdit(this);
    therionExecutableEdit_->setPlaceholderText(QStringLiteral("therion"));
    therionExecutableEdit_->setText(settings.therionExecutablePath.trimmed());
    therionExecutableEdit_->setCursorPosition(therionExecutableEdit_->text().size());
    therionExecutableBrowseButton_ = new QPushButton(tr("Browse..."), this);
    connect(therionExecutableBrowseButton_,
            &QPushButton::clicked,
            this,
            &MainWindowSettingsDialog::browseTherionExecutable);

    therionExecutableAutoDetectButton_ = new QPushButton(tr("Auto-detect"), this);
    connect(therionExecutableAutoDetectButton_,
            &QPushButton::clicked,
            this,
            &MainWindowSettingsDialog::autoDetectTherionExecutable);

    auto *executableRow = new QWidget(this);
    auto *executableLayout = new QHBoxLayout(executableRow);
    executableLayout->setContentsMargins(0, 0, 0, 0);
    executableLayout->setSpacing(6);
    executableLayout->addWidget(therionExecutableEdit_, 1);
    executableLayout->addWidget(therionExecutableBrowseButton_);
    executableLayout->addWidget(therionExecutableAutoDetectButton_);
    formLayout->addRow(tr("Therion executable"), executableRow);

    defaultTextEditorModeCombo_ = new QComboBox(this);
    defaultTextEditorModeCombo_->addItem(tr("Raw"), QStringLiteral("raw"));
    defaultTextEditorModeCombo_->addItem(tr("Blocks"), QStringLiteral("blocks"));
    selectComboData(defaultTextEditorModeCombo_,
                    settings.defaultTextEditorMode,
                    QStringLiteral("raw"));
    formLayout->addRow(tr("Default .th / config editor"), defaultTextEditorModeCombo_);

    automaticProjectValidationCheckBox_ =
        new QCheckBox(tr("Run full project validation automatically"), this);
    automaticProjectValidationCheckBox_->setChecked(settings.automaticProjectValidationEnabled);
    automaticProjectValidationCheckBox_->setToolTip(
        tr("When enabled, the Validation panel refreshes the whole project after project, document, and file changes."));
    formLayout->addRow(QString(), automaticProjectValidationCheckBox_);

    mapAltitudeUnderlayCheckBox_ = new QCheckBox(tr("Pastel scrap altitude underlay"), this);
    mapAltitudeUnderlayCheckBox_->setObjectName(QStringLiteral("mapAltitudeUnderlayCheckBox"));
    mapAltitudeUnderlayCheckBox_->setChecked(settings.mapAltitudeUnderlayEnabled);
    formLayout->addRow(QString(), mapAltitudeUnderlayCheckBox_);
    auto *altitudeNote = new QLabel(tr("Mean mapped LOX station heights relative to a flagged entrance; "
        "otherwise stable scrap colors (not altitude). Editor only; off by default."), this);
    altitudeNote->setWordWrap(true);
    formLayout->addRow(QString(), altitudeNote);

    mapAltitudeUnderlayStrengthSpin_ = new QSpinBox(this);
    mapAltitudeUnderlayStrengthSpin_->setObjectName(QStringLiteral("mapAltitudeUnderlayStrengthSpin"));
    mapAltitudeUnderlayStrengthSpin_->setRange(0, 100);
    mapAltitudeUnderlayStrengthSpin_->setSuffix(tr(" %"));
    mapAltitudeUnderlayStrengthSpin_->setValue(settings.mapAppearance.altitudeUnderlayStrengthPercent);
    mapAltitudeUnderlayStrengthSpin_->setEnabled(settings.mapAltitudeUnderlayEnabled);
    connect(mapAltitudeUnderlayCheckBox_, &QCheckBox::toggled,
            mapAltitudeUnderlayStrengthSpin_, &QWidget::setEnabled);
    formLayout->addRow(tr("Scrap underlay strength"), mapAltitudeUnderlayStrengthSpin_);

    xviCenterlineColor_ = QColor(settings.mapAppearance.xviCenterlineColor);
    xviCenterlineColorButton_ = new QPushButton(xviCenterlineColor_.name(QColor::HexArgb), this);
    xviCenterlineColorButton_->setObjectName(QStringLiteral("xviCenterlineColorButton"));
    xviCenterlineColorButton_->setToolTip(tr("Choose XVI centerline color"));
    connect(xviCenterlineColorButton_, &QPushButton::clicked, this, [this] {
        const QColor selected = QColorDialog::getColor(xviCenterlineColor_, this,
            tr("XVI centerline color"), QColorDialog::ShowAlphaChannel);
        if (selected.isValid()) {
            xviCenterlineColor_ = selected;
            xviCenterlineColorButton_->setText(selected.name(QColor::HexArgb));
        }
    });
    formLayout->addRow(tr("XVI centerline color"), xviCenterlineColorButton_);

    xviCenterlineColorModeCombo_ = new QComboBox(this);
    xviCenterlineColorModeCombo_->setObjectName(QStringLiteral("xviCenterlineColorModeCombo"));
    xviCenterlineColorModeCombo_->addItem(tr("One color"), QStringLiteral("single-color"));
    xviCenterlineColorModeCombo_->addItem(tr("By altitude"), QStringLiteral("by-altitude"));
    xviCenterlineColorModeCombo_->setCurrentIndex(xviCenterlineColorModeCombo_->findData(
        settings.mapAppearance.xviCenterlineColorMode == MapAppearanceSettings::XviCenterlineColorMode::ByAltitude
            ? QStringLiteral("by-altitude") : QStringLiteral("single-color")));
    formLayout->addRow(tr("XVI centerline coloring"), xviCenterlineColorModeCombo_);

    xviCenterlineWidthSpin_ = new QDoubleSpinBox(this);
    xviCenterlineWidthSpin_->setObjectName(QStringLiteral("xviCenterlineWidthSpin"));
    xviCenterlineWidthSpin_->setRange(MapAppearanceSettings::minimumCenterlineWidth,
                                     MapAppearanceSettings::maximumCenterlineWidth);
    xviCenterlineWidthSpin_->setDecimals(2);
    xviCenterlineWidthSpin_->setSingleStep(0.1);
    xviCenterlineWidthSpin_->setSuffix(tr(" px"));
    xviCenterlineWidthSpin_->setValue(settings.mapAppearance.xviCenterlineWidth);
    xviCenterlineWidthSpin_->setToolTip(
        tr("Cosmetic stroke width; retains the existing thinning when zoomed out. "
           "Only XVI station-to-station centerline legs change, not passage outlines or other layers."));
    formLayout->addRow(tr("XVI centerline thickness"), xviCenterlineWidthSpin_);

    troubleshootingLogsCheckBox_ =
        new QCheckBox(tr("Enable troubleshooting logs for 24 hours"), this);
    troubleshootingLogsCheckBox_->setChecked(settings.troubleshootingLogsEnabled);
    troubleshootingLogsCheckBox_->setToolTip(
        tr("Diagnostic logging writes timing and input diagnostics to the application log folder and takes effect after restarting Therion Studio."));
    formLayout->addRow(QString(), troubleshootingLogsCheckBox_);

    auto *loggingNote = new QLabel(
        tr("Troubleshooting logs are rotated automatically and the preference expires after 24 hours."), this);
    loggingNote->setWordWrap(true);
    loggingNote->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); font-size: 11px; }"));
    formLayout->addRow(QString(), loggingNote);

    auto *logActionsRow = new QWidget(this);
    auto *logActionsLayout = new QHBoxLayout(logActionsRow);
    logActionsLayout->setContentsMargins(0, 0, 0, 0);
    logActionsLayout->setSpacing(6);
    openLogFolderButton_ = new QPushButton(tr("Open Log Folder"), this);
    connect(openLogFolderButton_,
            &QPushButton::clicked,
            this,
            &MainWindowSettingsDialog::openDiagnosticLogFolder);
    clearLogsButton_ = new QPushButton(tr("Clear Logs"), this);
    connect(clearLogsButton_,
            &QPushButton::clicked,
            this,
            &MainWindowSettingsDialog::clearDiagnosticLogs);
    logActionsLayout->addWidget(openLogFolderButton_);
    logActionsLayout->addWidget(clearLogsButton_);
    logActionsLayout->addStretch(1);
    formLayout->addRow(QString(), logActionsRow);

    auto *generalPage = new QWidget(this);
    generalPage->setLayout(formLayout);
    auto *generalScroll = new QScrollArea(this);
    generalScroll->setWidgetResizable(true);
    generalScroll->setFrameShape(QFrame::NoFrame);
    generalScroll->setWidget(generalPage);

    auto *backups = new ProjectVersioningWidget(settings.projectRootPath, this);
    backupPage_ = backups;
    connect(backups, &ProjectVersioningWidget::preferencesChanged,
            this, &MainWindowSettingsDialog::projectVersioningChanged);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("settingsTabs"));
    tabs_->addTab(generalScroll, tr("General"));
    tabs_->addTab(backups, tr("Backups and versioning"));
    rootLayout->addWidget(tabs_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
    resize(760, 700);
}

void MainWindowSettingsDialog::showBackupPage()
{
    tabs_->setCurrentWidget(backupPage_);
}

void MainWindowSettingsDialog::showGitHubPage()
{
    showBackupPage();
    qobject_cast<ProjectVersioningWidget *>(backupPage_)->showGitHubPage();
}

MainWindowSettingsDialog::Settings MainWindowSettingsDialog::settings() const
{
    Settings result;
    result.projectRootPath = projectRootPath_;
    result.mapAltitudeUnderlayEnabled = mapAltitudeUnderlayCheckBox_->isChecked();
    result.mapAppearance.altitudeUnderlayStrengthPercent = mapAltitudeUnderlayStrengthSpin_->value();
    result.mapAppearance.xviCenterlineColor = xviCenterlineColor_.name(QColor::HexArgb);
    result.mapAppearance.xviCenterlineWidth = xviCenterlineWidthSpin_->value();
    result.mapAppearance.xviCenterlineColorMode = xviCenterlineColorModeCombo_->currentData().toString()
        == QStringLiteral("single-color")
        ? MapAppearanceSettings::XviCenterlineColorMode::SingleColor
        : MapAppearanceSettings::XviCenterlineColorMode::ByAltitude;
    result.applicationLanguage = languageCombo_ != nullptr
        ? languageCombo_->currentData().toString()
        : QStringLiteral("system");
    result.therionExecutablePath = therionExecutableEdit_ != nullptr
        ? therionExecutableEdit_->text().trimmed()
        : QString();
    result.defaultTextEditorMode = defaultTextEditorModeCombo_ != nullptr
        ? defaultTextEditorModeCombo_->currentData().toString()
        : QStringLiteral("raw");
    result.automaticProjectValidationEnabled = automaticProjectValidationCheckBox_ != nullptr
        && automaticProjectValidationCheckBox_->isChecked();
    result.troubleshootingLogsEnabled = troubleshootingLogsCheckBox_ != nullptr
        && troubleshootingLogsCheckBox_->isChecked();
    return result;
}

void MainWindowSettingsDialog::autoDetectTherionExecutable()
{
    const QString detected = TherionExecutableDetector::detect();
    if (detected.isEmpty()) {
        QMessageBox::information(this,
                                 tr("Auto-detect Therion Executable"),
                                 tr("Therion executable could not be found automatically.\n"
                                    "Please use Browse to locate it manually."));
        return;
    }

    if (therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setText(detected);
        therionExecutableEdit_->setCursorPosition(therionExecutableEdit_->text().size());
    }
}

void MainWindowSettingsDialog::browseTherionExecutable()
{
    const QString initialPath = TherionExecutableSelectionController::initialBrowsePath(
        therionExecutableEdit_ != nullptr ? therionExecutableEdit_->text() : QString());
    const QString selectedExecutablePath = QFileDialog::getOpenFileName(this,
                                                                         tr("Select Therion Executable"),
                                                                         initialPath);
    const TherionExecutableSelectionController::SelectionResult selectionResult =
        TherionExecutableSelectionController::evaluateSelection(selectedExecutablePath);
    if (selectionResult.showWarningDialog) {
        QMessageBox::warning(this,
                             selectionResult.warningDialogTitle,
                             selectionResult.warningDialogMessage);
        return;
    }

    if (!selectionResult.isAccepted
        || !selectionResult.shouldUpdateExecutableText
        || therionExecutableEdit_ == nullptr) {
        return;
    }

    therionExecutableEdit_->setText(selectionResult.updatedExecutableText);
    therionExecutableEdit_->setCursorPosition(therionExecutableEdit_->text().size());
}

void MainWindowSettingsDialog::openDiagnosticLogFolder()
{
    const QString directoryPath = diagnosticLogDirectoryPath();
    if (directoryPath.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             tr("Open Log Folder"),
                             tr("The diagnostic log folder is not available on this system."));
        return;
    }

    QDir().mkpath(directoryPath);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directoryPath))) {
        QMessageBox::warning(this,
                             tr("Open Log Folder"),
                             tr("Could not open the diagnostic log folder."));
    }
}

void MainWindowSettingsDialog::clearDiagnosticLogs()
{
    QString errorMessage;
    if (!TherionStudio::clearDiagnosticLogs(&errorMessage)) {
        QMessageBox::warning(this,
                             tr("Clear Logs"),
                             errorMessage.isEmpty() ? tr("Could not clear diagnostic logs.") : errorMessage);
        return;
    }

    QMessageBox::information(this,
                             tr("Clear Logs"),
                             tr("Diagnostic logs were cleared."));
}

void MainWindowSettingsDialog::selectComboData(QComboBox *combo,
                                               const QString &data,
                                               const QString &fallbackData)
{
    if (combo == nullptr) {
        return;
    }

    int index = combo->findData(data.trimmed().toLower());
    if (index < 0) {
        index = combo->findData(fallbackData);
    }
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}
}
