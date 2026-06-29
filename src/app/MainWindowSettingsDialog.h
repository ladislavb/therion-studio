#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace TherionStudio
{
class MainWindowSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Settings
    {
        QString applicationLanguage = QStringLiteral("system");
        QString therionExecutablePath;
        QString defaultTextEditorMode = QStringLiteral("raw");
        bool automaticProjectValidationEnabled = false;
        bool troubleshootingLogsEnabled = false;
    };

    explicit MainWindowSettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void browseTherionExecutable();
    void autoDetectTherionExecutable();
    void openDiagnosticLogFolder();
    void clearDiagnosticLogs();
    void selectComboData(QComboBox *combo, const QString &data, const QString &fallbackData);

    QComboBox *languageCombo_ = nullptr;
    QLineEdit *therionExecutableEdit_ = nullptr;
    QPushButton *therionExecutableBrowseButton_ = nullptr;
    QPushButton *therionExecutableAutoDetectButton_ = nullptr;
    QComboBox *defaultTextEditorModeCombo_ = nullptr;
    QCheckBox *automaticProjectValidationCheckBox_ = nullptr;
    QCheckBox *troubleshootingLogsCheckBox_ = nullptr;
    QPushButton *openLogFolderButton_ = nullptr;
    QPushButton *clearLogsButton_ = nullptr;
};
}
