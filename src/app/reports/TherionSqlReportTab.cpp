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
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
constexpr int kReportQueryRole = Qt::UserRole + 910;
constexpr int kReportIdRole = Qt::UserRole + 911;
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
    , customPresetStore_(customPresetSettings_)
{
    reports_ = TherionSqlReportDatabase::predefinedReports();
    customReports_ = customPresetStore_.loadCustomPresets();
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
    if (builtInReportList_ != nullptr && builtInReportList_->count() > 0) {
        builtInReportList_->setCurrentRow(0);
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
    if (presetsLayout != nullptr) {
        QVBoxLayout *builtInLayout = nullptr;
        QFrame *builtInSection = InspectorPanel::createSection(presetsPage, tr("Built-in"), &builtInLayout);
        builtInReportList_ = new QListWidget(builtInSection);
        if (builtInLayout != nullptr) {
            builtInLayout->addWidget(builtInReportList_, 1);
        }
        presetsLayout->addWidget(builtInSection, 3);

        QVBoxLayout *customLayout = nullptr;
        QFrame *customSection = InspectorPanel::createSection(presetsPage, tr("Custom"), &customLayout);
        customReportList_ = new QListWidget(customSection);
        if (customLayout != nullptr) {
            customLayout->addWidget(customReportList_, 1);

            auto *customButtonLayout = new QHBoxLayout;
            customButtonLayout->setContentsMargins(0, 0, 0, 0);
            savePresetButton_ = new QPushButton(tr("Save Preset"), customSection);
            renamePresetButton_ = new QPushButton(tr("Rename"), customSection);
            deletePresetButton_ = new QPushButton(tr("Delete"), customSection);
            customButtonLayout->addWidget(savePresetButton_);
            customButtonLayout->addWidget(renamePresetButton_);
            customButtonLayout->addWidget(deletePresetButton_);
            customLayout->addLayout(customButtonLayout);
        }
        presetsLayout->addWidget(customSection, 2);
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

    connect(builtInReportList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (current != nullptr && customReportList_ != nullptr) {
            QSignalBlocker blocker(customReportList_);
            Q_UNUSED(blocker);
            customReportList_->clearSelection();
            customReportList_->setCurrentItem(nullptr);
        }
        applySelectedPreset();
        updateCustomPresetButtons();
    });
    connect(customReportList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (current != nullptr && builtInReportList_ != nullptr) {
            QSignalBlocker blocker(builtInReportList_);
            Q_UNUSED(blocker);
            builtInReportList_->clearSelection();
            builtInReportList_->setCurrentItem(nullptr);
        }
        applySelectedPreset();
        updateCustomPresetButtons();
    });
    connect(runCustomSqlButton_, &QPushButton::clicked, this, [this]() {
        runCustomQuery();
    });
    connect(customSqlEdit_, &QPlainTextEdit::textChanged, this, &TherionSqlReportTab::updateCustomPresetButtons);
    connect(savePresetButton_, &QPushButton::clicked, this, &TherionSqlReportTab::saveCurrentQueryAsPreset);
    connect(renamePresetButton_, &QPushButton::clicked, this, &TherionSqlReportTab::renameSelectedCustomPreset);
    connect(deletePresetButton_, &QPushButton::clicked, this, &TherionSqlReportTab::deleteSelectedCustomPreset);
    updateCustomPresetButtons();
}

void TherionSqlReportTab::populateReports()
{
    populateBuiltInReports();
    populateCustomReports();
}

void TherionSqlReportTab::populateBuiltInReports()
{
    builtInReportList_->clear();
    for (const TherionSqlReportDefinition &report : reports_) {
        auto *item = new QListWidgetItem(report.title, builtInReportList_);
        item->setData(kReportIdRole, report.id);
        item->setData(kReportQueryRole, report.query);
    }
}

void TherionSqlReportTab::populateCustomReports(const QString &selectedId)
{
    if (customReportList_ == nullptr) {
        return;
    }

    customReportList_->clear();
    int selectedRow = -1;
    for (const TherionSqlReportDefinition &report : customReports_) {
        auto *item = new QListWidgetItem(report.title, customReportList_);
        item->setData(kReportIdRole, report.id);
        item->setData(kReportQueryRole, report.query);
        if (!selectedId.isEmpty() && report.id == selectedId) {
            selectedRow = customReportList_->count() - 1;
        }
    }
    if (selectedRow >= 0) {
        customReportList_->setCurrentRow(selectedRow);
    }
    updateCustomPresetButtons();
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
    QListWidgetItem *item = customReportList_ != nullptr ? customReportList_->currentItem() : nullptr;
    if (item == nullptr) {
        item = builtInReportList_ != nullptr ? builtInReportList_->currentItem() : nullptr;
    }
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

void TherionSqlReportTab::saveCurrentQueryAsPreset()
{
    const QString query = customSqlEdit_ != nullptr ? customSqlEdit_->toPlainText().trimmed() : QString();
    if (query.isEmpty()) {
        QMessageBox::information(this, tr("Save SQL Preset"), tr("Enter a SELECT query before saving a preset."));
        return;
    }

    TherionSqlReportDefinition *selectedPreset = selectedCustomPreset();
    const QString selectedId = selectedPreset != nullptr ? selectedPreset->id : QString();
    const QString initialName = selectedPreset != nullptr ? selectedPreset->title : tr("Custom preset");
    bool accepted = false;
    const QString title = QInputDialog::getText(this,
                                                tr("Save SQL Preset"),
                                                tr("Preset name:"),
                                                QLineEdit::Normal,
                                                initialName,
                                                &accepted)
                              .trimmed();
    if (!accepted) {
        return;
    }
    if (title.isEmpty()) {
        QMessageBox::warning(this, tr("Save SQL Preset"), tr("Preset name cannot be empty."));
        return;
    }

    const int duplicateIndex = customPresetIndexByTitle(title, selectedId);
    if (duplicateIndex >= 0) {
        const int answer = QMessageBox::question(this,
                                                 tr("Save SQL Preset"),
                                                 tr("Replace custom preset \"%1\"?").arg(title));
        if (answer != QMessageBox::Yes) {
            return;
        }
        customReports_[duplicateIndex].query = query;
        customPresetStore_.saveCustomPresets(customReports_);
        populateCustomReports(customReports_.at(duplicateIndex).id);
        statusLabel_->setText(tr("Saved custom SQL preset \"%1\".").arg(title));
        return;
    }

    if (selectedPreset != nullptr) {
        selectedPreset->title = title;
        selectedPreset->query = query;
        customPresetStore_.saveCustomPresets(customReports_);
        populateCustomReports(selectedId);
        statusLabel_->setText(tr("Saved custom SQL preset \"%1\".").arg(title));
        return;
    }

    TherionSqlReportDefinition preset;
    preset.id = TherionSqlReportPresetStore::createPresetId();
    preset.title = title;
    preset.query = query;
    customReports_.append(preset);
    customPresetStore_.saveCustomPresets(customReports_);
    populateCustomReports(preset.id);
    statusLabel_->setText(tr("Saved custom SQL preset \"%1\".").arg(title));
}

void TherionSqlReportTab::renameSelectedCustomPreset()
{
    TherionSqlReportDefinition *preset = selectedCustomPreset();
    if (preset == nullptr) {
        return;
    }

    bool accepted = false;
    const QString title = QInputDialog::getText(this,
                                                tr("Rename SQL Preset"),
                                                tr("Preset name:"),
                                                QLineEdit::Normal,
                                                preset->title,
                                                &accepted)
                              .trimmed();
    if (!accepted) {
        return;
    }
    if (title.isEmpty()) {
        QMessageBox::warning(this, tr("Rename SQL Preset"), tr("Preset name cannot be empty."));
        return;
    }
    if (customPresetIndexByTitle(title, preset->id) >= 0) {
        QMessageBox::warning(this,
                             tr("Rename SQL Preset"),
                             tr("A custom preset named \"%1\" already exists.").arg(title));
        return;
    }

    const QString presetId = preset->id;
    preset->title = title;
    customPresetStore_.saveCustomPresets(customReports_);
    populateCustomReports(presetId);
    statusLabel_->setText(tr("Renamed custom SQL preset to \"%1\".").arg(title));
}

void TherionSqlReportTab::deleteSelectedCustomPreset()
{
    const TherionSqlReportDefinition *preset = selectedCustomPreset();
    if (preset == nullptr) {
        return;
    }

    const QString title = preset->title;
    const int answer = QMessageBox::question(this,
                                             tr("Delete SQL Preset"),
                                             tr("Delete custom preset \"%1\"?").arg(title));
    if (answer != QMessageBox::Yes) {
        return;
    }

    const int index = customPresetIndexById(preset->id);
    if (index < 0) {
        return;
    }
    customReports_.removeAt(index);
    customPresetStore_.saveCustomPresets(customReports_);
    populateCustomReports();
    statusLabel_->setText(tr("Deleted custom SQL preset \"%1\".").arg(title));
}

void TherionSqlReportTab::updateCustomPresetButtons()
{
    const bool hasCustomSelection = selectedCustomPreset() != nullptr;
    if (savePresetButton_ != nullptr) {
        savePresetButton_->setEnabled(customSqlEdit_ != nullptr && !customSqlEdit_->toPlainText().trimmed().isEmpty());
    }
    if (renamePresetButton_ != nullptr) {
        renamePresetButton_->setEnabled(hasCustomSelection);
    }
    if (deletePresetButton_ != nullptr) {
        deletePresetButton_->setEnabled(hasCustomSelection);
    }
}

int TherionSqlReportTab::customPresetIndexById(const QString &id) const
{
    for (int i = 0; i < customReports_.size(); ++i) {
        if (customReports_.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

int TherionSqlReportTab::customPresetIndexByTitle(const QString &title, const QString &ignoredId) const
{
    for (int i = 0; i < customReports_.size(); ++i) {
        const TherionSqlReportDefinition &preset = customReports_.at(i);
        if (preset.id != ignoredId && preset.title.compare(title, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

TherionSqlReportDefinition *TherionSqlReportTab::selectedCustomPreset()
{
    if (customReportList_ == nullptr || customReportList_->currentItem() == nullptr) {
        return nullptr;
    }
    const QString id = customReportList_->currentItem()->data(kReportIdRole).toString();
    const int index = customPresetIndexById(id);
    return index >= 0 ? &customReports_[index] : nullptr;
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
