#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
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
    };

    explicit MainWindowSettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void browseTherionExecutable();
    void autoDetectTherionExecutable();
    void selectComboData(QComboBox *combo, const QString &data, const QString &fallbackData);

    QComboBox *languageCombo_ = nullptr;
    QLineEdit *therionExecutableEdit_ = nullptr;
    QPushButton *therionExecutableBrowseButton_ = nullptr;
    QPushButton *therionExecutableAutoDetectButton_ = nullptr;
    QComboBox *defaultTextEditorModeCombo_ = nullptr;
};
}
