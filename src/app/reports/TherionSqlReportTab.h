#pragma once

#include "TherionSqlReportPresetStore.h"
#include "TherionSqlReportSession.h"

#include <QWidget>

#include <memory>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableView;

namespace TherionStudio
{

class InspectorPanel;
class SqlReportTableModel;
class TherionSqlReportCsvExporter;

class TherionSqlReportTab final : public QWidget
{
    Q_OBJECT

public:
    TherionSqlReportTab(TherionSqlReportSession *session,
                        std::unique_ptr<TherionSqlReportPresetStore> customPresetStore,
                        std::unique_ptr<TherionSqlReportCsvExporter> csvExporter,
                        QWidget *parent = nullptr);
    ~TherionSqlReportTab() override;

    bool loadFile(const QString &filePath, QString *errorMessage);
    bool reloadFile(QString *errorMessage);
    bool save(QString *errorMessage);
    bool isDirty() const;
    QString filePath() const;
    QString displayName() const;
    int currentLineNumber() const;
    void goToLine(int lineNumber);
    void setProjectRootPath(const QString &projectRootPath);
    void showFindBar(bool replaceMode);
    bool canExportCsv() const;
    void exportCurrentTableCsv();

signals:
    void titleChanged();
    void statusChanged();

private:
    void buildUi();
    void populateReports();
    void populateBuiltInReports();
    void populateCustomReports(const QString &selectedId = QString());
    void refreshSchemaView();
    void handleImportFinished(const TherionSqlReportImportWorkerResult &result);
    void handleQueryFinished(const TherionSqlReportQueryWorkerResult &result);
    void setImportBusy(bool busy);
    quint64 nextRequestId();
    void applySelectedPreset();
    void runCustomQuery();
    void saveCurrentQueryAsPreset();
    void renameSelectedCustomPreset();
    void deleteSelectedCustomPreset();
    void updateCustomPresetButtons();
    void showTable(const TherionSqlReportTable &table);
    void showError(const QString &message);
    QString currentPresetQuery() const;
    int customPresetIndexById(const QString &id) const;
    int customPresetIndexByTitle(const QString &title, const QString &ignoredId = QString()) const;
    TherionSqlReportDefinition *selectedCustomPreset();

    TherionSqlReportSession *session_ = nullptr;
    QVector<TherionSqlReportDefinition> reports_;
    QVector<TherionSqlReportDefinition> customReports_;
    std::unique_ptr<TherionSqlReportPresetStore> customPresetStore_;
    std::unique_ptr<TherionSqlReportCsvExporter> csvExporter_;
    QString projectRootPath_;
    QString filePath_;
    QString sourceIdentity_;
    QVector<TherionSqlReportSchemaTable> schema_;
    TherionSqlReportTable currentTable_;
    quint64 requestSequence_ = 0;
    quint64 latestRequestId_ = 0;
    quint64 sourceGeneration_ = 0;
    qint64 queryStartedAtMs_ = 0;
    bool databaseReady_ = false;
    bool importBusy_ = false;

    QLabel *statusLabel_ = nullptr;
    QListWidget *builtInReportList_ = nullptr;
    QListWidget *customReportList_ = nullptr;
    QTableView *resultTable_ = nullptr;
    SqlReportTableModel *resultModel_ = nullptr;
    QPlainTextEdit *schemaText_ = nullptr;
    QPlainTextEdit *customSqlEdit_ = nullptr;
    QPushButton *runCustomSqlButton_ = nullptr;
    QPushButton *savePresetButton_ = nullptr;
    QPushButton *renamePresetButton_ = nullptr;
    QPushButton *deletePresetButton_ = nullptr;
    InspectorPanel *sidebarPanel_ = nullptr;
};

} // namespace TherionStudio
