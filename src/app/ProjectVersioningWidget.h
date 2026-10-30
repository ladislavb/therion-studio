#pragma once

#include "ProjectVersioningSettings.h"
#include "ProjectVersioningStore.h"

#include <QWidget>

#include <memory>

class QCheckBox;
class QLabel;
class QPushButton;
class QSettings;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
template<typename T> class QFutureWatcher;

namespace TherionStudio
{
class ProjectVersioningWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectVersioningWidget(QString projectRootPath, QWidget *parent = nullptr);
    ProjectVersioningWidget(QString projectRootPath, QSettings &settings,
                            QString applicationDataDirectory, QWidget *parent = nullptr);
    ~ProjectVersioningWidget() override;
    void showGitHubPage();

signals:
    void preferencesChanged();

private:
    struct OperationResult
    {
        ProjectVersioningStore::HistoryResult history;
        QString error;
        bool historyRemoved = false;
    };

    void initialize();
    void changeEnabled(bool enabled);
    void loadHistory(bool createCheckpoint);
    void updateControls();
    void showError(const QString &error);
    void reloadIgnore();
    void saveIgnore();
    void saveAutomaticSettings();
    void removeHistory();

    QString projectRootPath_;
    QString applicationDataDirectory_;
    std::unique_ptr<QSettings> ownedSettings_;
    QSettings *settings_ = nullptr;
    ProjectVersioningSettings::Settings preferences_;
    bool settingsAvailable_ = false;
    QCheckBox *enabled_ = nullptr;
    QLabel *status_ = nullptr;
    QLabel *location_ = nullptr;
    QPushButton *checkpoint_ = nullptr;
    QPushButton *refresh_ = nullptr;
    QPushButton *openFolder_ = nullptr;
    QTableWidget *history_ = nullptr;
    QPlainTextEdit *ignoreEditor_ = nullptr;
    QPushButton *saveIgnore_ = nullptr;
    QPushButton *reloadIgnore_ = nullptr;
    QByteArray ignoreContents_;
    bool ignoreExisted_ = false;
    bool ignoreLoaded_ = false;
    QCheckBox *automatic_ = nullptr;
    QSpinBox *interval_ = nullptr;
    QPushButton *remove_ = nullptr;
    QFutureWatcher<OperationResult> *watcher_ = nullptr;
};
} // namespace TherionStudio
