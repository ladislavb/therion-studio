#include "MainWindowSettingsDialog.h"

#include "../platform/DiagnosticLogging.h"
#include "TherionExecutableDetector.h"
#include "TherionExecutableSelectionController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace TherionStudio
{
MainWindowSettingsDialog::MainWindowSettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent)
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

    rootLayout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
    resize(520, sizeHint().height());
}

MainWindowSettingsDialog::Settings MainWindowSettingsDialog::settings() const
{
    Settings result;
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
