#include "TherionSqlReportTab.h"

#include "../ExportFileName.h"
#include "../text_editor/InspectorPanel.h"

#include <QAbstractTableModel>
#include <QDateTime>
#include <QAbstractItemView>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
constexpr int kReportQueryRole = Qt::UserRole + 910;
constexpr int kInspectorMinWidth = 320;
constexpr int kInspectorMaxWidth = 460;
constexpr int kQueryRowLimit = 1000;

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

QString csvEscaped(QString value)
{
    if (value.contains(QLatin1Char('"'))) {
        value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    }
    if (value.contains(QLatin1Char(','))
        || value.contains(QLatin1Char('\n'))
        || value.contains(QLatin1Char('\r'))
        || value.contains(QLatin1Char('"'))) {
        return QStringLiteral("\"%1\"").arg(value);
    }
    return value;
}
}

class SqlReportTableModel final : public QAbstractTableModel
{
public:
    explicit SqlReportTableModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : table_.rows.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : table_.columns.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return {};
        }
        if (index.row() < 0 || index.row() >= table_.rows.size()) {
            return {};
        }
        const QStringList &row = table_.rows.at(index.row());
        if (index.column() < 0 || index.column() >= row.size()) {
            return {};
        }
        return row.at(index.column());
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole) {
            return {};
        }
        if (orientation == Qt::Vertical) {
            return section + 1;
        }
        if (section < 0 || section >= table_.columns.size()) {
            return {};
        }
        return table_.columns.at(section);
    }

    void setTable(TherionSqlReportTable table)
    {
        beginResetModel();
        table_ = std::move(table);
        endResetModel();
    }

private:
    TherionSqlReportTable table_;
};

TherionSqlReportTab::TherionSqlReportTab(QWidget *parent)
    : QWidget(parent)
{
    reports_ = TherionSqlReportDatabase::predefinedReports();
    buildUi();
    populateReports();
}

bool TherionSqlReportTab::loadFile(const QString &path, QString *errorMessage)
{
    const TherionSqlReportImportResult result = database_.importFile(path);
    if (!result.success) {
        if (errorMessage != nullptr) {
            *errorMessage = result.errorMessage;
        }
        return false;
    }

    statusLabel_->setText(tr("Imported %1 SQL statements from %2.")
                              .arg(result.importedStatementCount)
                              .arg(QDir::toNativeSeparators(database_.filePath())));
    refreshSchemaView();
    if (reportList_ != nullptr && reportList_->count() > 0) {
        reportList_->setCurrentRow(0);
        applySelectedPreset();
    }
    emit titleChanged();
    emit statusChanged();
    return true;
}

bool TherionSqlReportTab::reloadFile(QString *errorMessage)
{
    if (database_.filePath().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No SQL export is open.");
        }
        return false;
    }
    return loadFile(database_.filePath(), errorMessage);
}

bool TherionSqlReportTab::save(QString *errorMessage)
{
    if (errorMessage != nullptr) {
        *errorMessage = tr("SQL report tabs are read-only.");
    }
    return false;
}

bool TherionSqlReportTab::isDirty() const
{
    return false;
}

QString TherionSqlReportTab::filePath() const
{
    return database_.filePath();
}

QString TherionSqlReportTab::displayName() const
{
    return database_.displayName();
}

int TherionSqlReportTab::currentLineNumber() const
{
    return 0;
}

void TherionSqlReportTab::goToLine(int)
{
}

void TherionSqlReportTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
}

void TherionSqlReportTab::showFindBar(bool)
{
}

void TherionSqlReportTab::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    applyThinSplitterStyle(splitter, QStringLiteral("sqlReportInspectorSplitter"));
    rootLayout->addWidget(splitter, 1);

    auto *workArea = new QFrame(splitter);
    workArea->setFrameShape(QFrame::NoFrame);
    workArea->setObjectName(QStringLiteral("sqlReportCanvas"));
    workArea->setStyleSheet(QStringLiteral(
        "QFrame#sqlReportCanvas {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    auto *workLayout = new QVBoxLayout(workArea);
    workLayout->setContentsMargins(8, 8, 8, 8);
    workLayout->setSpacing(8);

    customSqlEdit_ = new QPlainTextEdit(workArea);
    customSqlEdit_->setPlaceholderText(tr("Enter one read-only SELECT query."));
    customSqlEdit_->setPlainText(QStringLiteral("select * from SURVEY limit 100"));
    customSqlEdit_->setMaximumHeight(120);
    workLayout->addWidget(customSqlEdit_, 0);

    auto *queryButtonLayout = new QHBoxLayout;
    queryButtonLayout->setContentsMargins(0, 0, 0, 0);
    queryButtonLayout->addStretch(1);
    runCustomSqlButton_ = new QPushButton(tr("Run SELECT"), workArea);
    queryButtonLayout->addWidget(runCustomSqlButton_);
    workLayout->addLayout(queryButtonLayout);

    resultModel_ = new SqlReportTableModel(this);
    resultTable_ = new QTableView(workArea);
    resultTable_->setModel(resultModel_);
    resultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resultTable_->setAlternatingRowColors(true);
    resultTable_->horizontalHeader()->setStretchLastSection(false);
    workLayout->addWidget(resultTable_, 1);
    statusLabel_ = new QLabel(tr("Open a Therion SQL export to view reports."), workArea);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    workLayout->addWidget(statusLabel_);

    splitter->addWidget(workArea);

    sidebarPanel_ = new InspectorPanel(splitter);
    sidebarPanel_->setMinimumWidth(kInspectorMinWidth);
    sidebarPanel_->setMaximumWidth(kInspectorMaxWidth);
    auto *presetsPage = sidebarPanel_->addPlainTab(tr("Presets"));
    auto *presetsLayout = qobject_cast<QVBoxLayout *>(presetsPage->layout());
    reportList_ = new QListWidget(presetsPage);
    if (presetsLayout != nullptr) {
        presetsLayout->addWidget(reportList_, 1);
    }

    auto *schemaPage = sidebarPanel_->addPlainTab(tr("Schema"));
    auto *schemaLayout = qobject_cast<QVBoxLayout *>(schemaPage->layout());
    schemaText_ = new QPlainTextEdit(schemaPage);
    schemaText_->setReadOnly(true);
    if (schemaLayout != nullptr) {
        schemaLayout->addWidget(schemaText_, 1);
    }

    splitter->addWidget(sidebarPanel_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setCollapsible(1, true);
    splitter->setSizes({980, 380});

    connect(reportList_, &QListWidget::currentRowChanged, this, [this](int) {
        applySelectedPreset();
    });
    connect(runCustomSqlButton_, &QPushButton::clicked, this, [this]() {
        runCustomQuery();
    });
}

void TherionSqlReportTab::populateReports()
{
    reportList_->clear();
    for (const TherionSqlReportDefinition &report : reports_) {
        auto *item = new QListWidgetItem(report.title, reportList_);
        item->setData(kReportQueryRole, report.query);
    }
}

void TherionSqlReportTab::refreshSchemaView()
{
    QStringList lines;
    lines.append(tr("Tables"));
    for (const QString &table : database_.tableNames()) {
        lines.append(QStringLiteral("- %1").arg(table));
        const QStringList columns = database_.tableColumns(table);
        for (const QString &column : columns) {
            lines.append(QStringLiteral("  - %1").arg(column));
        }
    }
    schemaText_->setPlainText(lines.join(QLatin1Char('\n')));
}

QString TherionSqlReportTab::currentPresetQuery() const
{
    QListWidgetItem *item = reportList_ != nullptr ? reportList_->currentItem() : nullptr;
    return item != nullptr ? item->data(kReportQueryRole).toString() : QString();
}

void TherionSqlReportTab::applySelectedPreset()
{
    if (!database_.isOpen()) {
        return;
    }
    const QString query = currentPresetQuery();
    if (query.isEmpty()) {
        return;
    }

    customSqlEdit_->setPlainText(query);
    runCustomQuery();
}

void TherionSqlReportTab::runCustomQuery()
{
    if (!database_.isOpen()) {
        showError(tr("No Therion SQL export is open."));
        return;
    }

    QString errorMessage;
    QElapsedTimer timer;
    timer.start();
    const TherionSqlReportTable table = database_.executeCustomQuery(customSqlEdit_->toPlainText(),
                                                                     &errorMessage,
                                                                     kQueryRowLimit);
    const qint64 elapsedMs = timer.elapsed();
    if (!errorMessage.isEmpty()) {
        showError(errorMessage);
        return;
    }
    showTable(table);
    statusLabel_->setText(table.truncated
                              ? tr("Query returned more than %1 rows in %2 ms; showing the first %1.")
                                    .arg(kQueryRowLimit)
                                    .arg(elapsedMs)
                              : tr("Query returned %1 rows and %2 columns in %3 ms.")
                                    .arg(table.rows.size())
                                    .arg(table.columns.size())
                                    .arg(elapsedMs));
}

void TherionSqlReportTab::showTable(const TherionSqlReportTable &table)
{
    currentTable_ = table;
    resultModel_->setTable(table);
    resultTable_->resizeColumnsToContents();
    emit statusChanged();
}

void TherionSqlReportTab::showError(const QString &message)
{
    statusLabel_->setText(message);
    emit statusChanged();
}

bool TherionSqlReportTab::canExportCsv() const
{
    return !currentTable_.columns.isEmpty();
}

void TherionSqlReportTab::exportCurrentTableCsv()
{
    if (currentTable_.columns.isEmpty()) {
        return;
    }

    const QString initialDirectory = !database_.filePath().isEmpty()
        ? QFileInfo(database_.filePath()).absolutePath()
        : projectRootPath_;
    const QString defaultName = defaultExportFileName(QStringLiteral("report"),
                                                      projectRootPath_,
                                                      database_.filePath(),
                                                      QStringLiteral("csv"),
                                                      QDateTime::currentDateTime());
    const QString outputPath = QFileDialog::getSaveFileName(this,
                                                            tr("Export CSV"),
                                                            QDir(initialDirectory).filePath(defaultName),
                                                            tr("CSV files (*.csv);;All files (*)"));
    if (outputPath.isEmpty()) {
        return;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export CSV"), tr("Could not write %1.").arg(QDir::toNativeSeparators(outputPath)));
        return;
    }

    QTextStream stream(&file);
    QStringList escapedHeader;
    for (const QString &column : currentTable_.columns) {
        escapedHeader.append(csvEscaped(column));
    }
    stream << escapedHeader.join(QLatin1Char(',')) << '\n';
    for (const QStringList &row : std::as_const(currentTable_.rows)) {
        QStringList escapedRow;
        for (const QString &value : row) {
            escapedRow.append(csvEscaped(value));
        }
        stream << escapedRow.join(QLatin1Char(',')) << '\n';
    }
    statusLabel_->setText(tr("Exported CSV to %1.").arg(QDir::toNativeSeparators(outputPath)));
}

} // namespace TherionStudio
