#pragma once

#include "TherionSqlReportDatabase.h"
#include "TherionSqlReportPresetStore.h"

#include <QSettings>
#include <QWidget>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableView;

namespace TherionStudio
{

class InspectorPanel;
class SqlReportTableModel;

class TherionSqlReportTab final : public QWidget
{
    Q_OBJECT

public:
    explicit TherionSqlReportTab(QWidget *parent = nullptr);

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

    TherionSqlReportDatabase database_;
    QVector<TherionSqlReportDefinition> reports_;
    QVector<TherionSqlReportDefinition> customReports_;
    QSettings customPresetSettings_;
    TherionSqlReportPresetStore customPresetStore_;
    QString projectRootPath_;
    TherionSqlReportTable currentTable_;

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
