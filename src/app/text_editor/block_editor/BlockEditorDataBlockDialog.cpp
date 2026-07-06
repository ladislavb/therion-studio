#include "BlockEditorDataBlockDialog.h"

#include <QCoreApplication>

#include "BlockEditorDirectiveRules.h"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceDocument.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

constexpr int kRowTypeRole = Qt::UserRole + 1;
const QString kDataRowType = QStringLiteral("data");
const QString kDirectiveRowType = QStringLiteral("directive");
const QString kCommentRowType = QStringLiteral("comment");

struct RowTypeChoice
{
    QString id;
    QString label;
};

void appendUnique(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!target.contains(trimmed, Qt::CaseInsensitive)) {
        target.append(trimmed);
    }
}

QString titleCaseWords(QString label)
{
    label = label.trimmed();
    if (label.isEmpty()) {
        return label;
    }

    label = label.toLower();
    QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words) {
        if (word.isEmpty()) {
            continue;
        }
        word[0] = word.at(0).toUpper();
    }
    return words.join(QLatin1Char(' '));
}

QString dataFieldDisplayLabel(const QString &fieldToken)
{
    QString label = fieldToken.trimmed();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }

    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    label = label.simplified();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }
    return titleCaseWords(label);
}

void installDataRowCompleter(QLineEdit *lineEdit, const QStringList &values)
{
    if (lineEdit == nullptr) {
        return;
    }

    QStringList suggestions;
    for (const QString &value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !suggestions.contains(trimmed, Qt::CaseInsensitive)) {
            suggestions.append(trimmed);
        }
    }
    if (suggestions.isEmpty()) {
        return;
    }
    suggestions.sort(Qt::CaseInsensitive);

    auto *completer = new QCompleter(suggestions, lineEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    lineEdit->setCompleter(completer);
}

QStringList parseDataFields(const QString &columnsText, const QStringList &dataStyleValues)
{
    QStringList tokens = TherionStudio::TherionDocumentParser::tokenizeLine(columnsText.trimmed());
    if (tokens.isEmpty()) {
        return {};
    }
    if (tokens.size() > 1 && dataStyleValues.contains(tokens.first(), Qt::CaseInsensitive)) {
        tokens.removeFirst();
    }
    return tokens;
}

struct MixedRowEntry
{
    bool directive = false;
    bool commentOnly = false;
    QString directiveText;
    QStringList dataValues;
    QString commentText;
    QChar commentMarker = QLatin1Char('#');
};

class DataRowsTableWidget final : public QTableWidget
{
public:
    explicit DataRowsTableWidget(QWidget *parent = nullptr)
        : QTableWidget(parent)
    {
    }

    std::function<void()> onViewportResized;

    void setAdvanceColumns(const QSet<int> &columns)
    {
        advanceColumns_ = columns;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTableWidget::resizeEvent(event);
        if (onViewportResized) {
            onViewportResized();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event != nullptr
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0
            && columnCount() > 0
            && (advanceColumns_.isEmpty() ? (currentColumn() == columnCount() - 1)
                                          : advanceColumns_.contains(currentColumn()))) {
            const int currentRowIndex = qMax(0, currentRow());
            if (currentRowIndex >= rowCount() - 1) {
                insertRow(rowCount());
            }

            const int nextRow = currentRowIndex + 1;
            const int targetColumn = qBound(0, 1, qMax(0, columnCount() - 1));
            if (item(nextRow, targetColumn) == nullptr) {
                setItem(nextRow, targetColumn, new QTableWidgetItem);
            }
            setCurrentCell(nextRow, targetColumn);
            editItem(item(nextRow, targetColumn));
            event->accept();
            return;
        }

        QTableWidget::keyPressEvent(event);
    }

private:
    QSet<int> advanceColumns_;
};

class DataRowsTableDelegate final : public QStyledItemDelegate
{
public:
    using SuggestionsProvider = std::function<QStringList(const QModelIndex &)>;
    using EditableProvider = std::function<bool(const QModelIndex &)>;

    explicit DataRowsTableDelegate(SuggestionsProvider suggestionsProvider,
                                   EditableProvider editableProvider,
                                   QVector<RowTypeChoice> rowTypeChoices,
                                   QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , suggestionsProvider_(std::move(suggestionsProvider))
        , editableProvider_(std::move(editableProvider))
        , rowTypeChoices_(std::move(rowTypeChoices))
    {
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        if (index.isValid() && index.column() == 0) {
            auto *combo = new QComboBox(parent);
            for (const RowTypeChoice &choice : rowTypeChoices_) {
                combo->addItem(choice.label, choice.id);
            }
            return combo;
        }

        if (editableProvider_ && !editableProvider_(index)) {
            return nullptr;
        }

        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit == nullptr || !suggestionsProvider_) {
            return editor;
        }

        const QStringList suggestions = suggestionsProvider_(index);
        if (!suggestions.isEmpty()) {
            installDataRowCompleter(lineEdit, suggestions);
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor); combo != nullptr) {
            const QString value = index.data(kRowTypeRole).toString();
            int found = combo->findData(value);
            if (found < 0) {
                found = combo->findText(index.data(Qt::EditRole).toString(),
                                        Qt::MatchFixedString | Qt::MatchCaseSensitive);
            }
            combo->setCurrentIndex(found >= 0 ? found : 0);
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor); combo != nullptr) {
            model->setData(index, combo->currentText(), Qt::EditRole);
            model->setData(index, combo->currentData().toString(), kRowTypeRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

private:
    SuggestionsProvider suggestionsProvider_;
    EditableProvider editableProvider_;
    QVector<RowTypeChoice> rowTypeChoices_;
};
}

namespace TherionStudio
{
BlockEditorDataBlockDialog::BlockEditorDataBlockDialog(BlockEditorDataBlockDialogContext context)
    : context_(std::move(context))
{
}

QString BlockEditorDataBlockDialog::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorDataBlockDialog", text);
}

std::optional<BlockEditorDataBlockDialogResult> BlockEditorDataBlockDialog::configureRows(
    const QStringList &lines,
    int lineNumber)
{
    if (context_.commandMetadata == nullptr
        || context_.resolveScopeForCommandAtLine == nullptr
        || context_.isCommandDirectiveInScope == nullptr
        || lineNumber <= 0
        || lineNumber > lines.size()) {
        return std::nullopt;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(lines.join(QLatin1Char('\n')));
    const auto parsedLineForLine = [&sourceDocument](int currentLine) -> TherionParsedLine {
        if (const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(currentLine);
            sourceLine != nullptr) {
            return sourceLine->sourceLine.parsed;
        }
        return TherionParsedLine{};
    };

    const TherionParsedLine parsedLine = parsedLineForLine(lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        return std::nullopt;
    }

    const QString dataScope = context_.resolveScopeForCommandAtLine(QStringLiteral("data"), lines, lineNumber);
    const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
    if (dataScopeClosing.isEmpty()) {
        QMessageBox::warning(context_.dialogParent, tr("Configure Data"), tr("Unable to resolve parent data scope."));
        return std::nullopt;
    }

    int dataScopeStartLine = -1;
    int dataScopeDepth = 0;
    for (int currentLine = lineNumber; currentLine >= 1; --currentLine) {
        const TherionParsedLine currentParsedLine = parsedLineForLine(currentLine);
        const QString directive = normalizeDirective(currentParsedLine.directive);
        if (directive == dataScopeClosing) {
            ++dataScopeDepth;
            continue;
        }
        if (directive != dataScope) {
            continue;
        }
        if (dataScopeDepth == 0) {
            dataScopeStartLine = currentLine;
            break;
        }
        --dataScopeDepth;
    }

    if (dataScopeStartLine <= 0) {
        QMessageBox::warning(context_.dialogParent, tr("Configure Data"), tr("Unable to resolve parent data scope block."));
        return std::nullopt;
    }

    const int dataScopeEndLine = findMatchingBlockEndLine(lines,
                                                          dataScopeStartLine,
                                                          dataScope,
                                                          dataScopeClosing);
    if (dataScopeEndLine <= lineNumber) {
        QMessageBox::warning(context_.dialogParent, tr("Configure Data"), tr("Unable to resolve end of parent data scope block."));
        return std::nullopt;
    }

    int dataBodyLastLine = dataScopeEndLine - 1;
    for (int currentLine = lineNumber + 1; currentLine <= dataScopeEndLine - 1; ++currentLine) {
        const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
        const QString directive = normalizeDirective(currentParsedLine.directive);
        if (directive.isEmpty() || directive == QStringLiteral("extend")) {
            continue;
        }
        if (directive == QStringLiteral("data")
            || directive == dataScopeClosing
            || (!dataScope.isEmpty() && context_.isCommandDirectiveInScope(directive, dataScope))) {
            dataBodyLastLine = currentLine - 1;
            break;
        }
    }
    if (dataBodyLastLine < lineNumber) {
        dataBodyLastLine = lineNumber;
    }

    const QString currentColumns = parsedLine.tokens.size() > 1
        ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
        : QString();
    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto dataIndentMatch = indentPattern.match(lines.at(lineNumber - 1));
    const QString dataIndent = dataIndentMatch.hasMatch() ? dataIndentMatch.captured(0) : QString();
    const QString rowIndent = dataIndent + QStringLiteral("  ");

    const QStringList dataStyleValues = context_.commandMetadata->commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("data"), 0));
    const QStringList currentFieldNames = parseDataFields(currentColumns, dataStyleValues);
    const int currentFieldCount = currentFieldNames.size();

    QStringList directiveSuggestions = context_.commandMetadata->contextCommandTokens.value(dataScope);
    directiveSuggestions.removeAll(QStringLiteral("data"));
    if (!dataScopeClosing.isEmpty()) {
        directiveSuggestions.removeAll(dataScopeClosing);
    }
    const QStringList extendValues = context_.commandMetadata->commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("extend"), 0));
    if (extendValues.isEmpty()) {
        appendUnique(directiveSuggestions, QStringLiteral("extend right"));
        appendUnique(directiveSuggestions, QStringLiteral("extend left"));
        appendUnique(directiveSuggestions, QStringLiteral("extend vertical"));
    } else {
        for (const QString &extendValue : extendValues) {
            appendUnique(directiveSuggestions, QStringLiteral("extend %1").arg(extendValue.trimmed()));
        }
    }
    std::sort(directiveSuggestions.begin(), directiveSuggestions.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    QVector<MixedRowEntry> initialRows;
    bool schemaMismatchDetected = false;
    for (int currentLine = lineNumber + 1; currentLine <= dataBodyLastLine; ++currentLine) {
        const QString rowLine = lines.at(currentLine - 1);
        const TherionParsedLine parsedRow = parsedLineForLine(currentLine);
        QString rowText = parsedRow.commentStart >= 0 ? rowLine.left(parsedRow.commentStart) : rowLine;
        if (rowText.startsWith(rowIndent)) {
            rowText = rowText.mid(rowIndent.size());
        } else {
            rowText = rowText.trimmed();
        }
        rowText = rowText.trimmed();

        const QStringList rowTokens = parsedRow.tokens;
        const QString commentText = parsedRow.commentStart >= 0 ? parsedRow.commentText.trimmed() : QString();
        const QChar commentMarker = (parsedRow.commentStart >= 0 && parsedRow.commentStart < rowLine.size())
            ? rowLine.at(parsedRow.commentStart)
            : QLatin1Char('#');
        const bool looksLikeMeasurementRow = currentFieldCount > 0
            && rowTokens.size() == currentFieldCount
            && (rowTokens.isEmpty() || rowTokens.first().toLower() != QStringLiteral("extend"));
        if (looksLikeMeasurementRow) {
            initialRows.append(MixedRowEntry{false, false, QString(), rowTokens, commentText, commentMarker});
            continue;
        }

        if (rowText.trimmed().isEmpty() && commentText.isEmpty()) {
            continue;
        }

        if (rowText.trimmed().isEmpty() && !commentText.isEmpty()) {
            initialRows.append(MixedRowEntry{false, true, QString(), {}, commentText, commentMarker});
            continue;
        }

        const QString firstToken = rowTokens.isEmpty() ? QString() : rowTokens.first().trimmed().toLower();
        const bool recognizedDirective = firstToken == QStringLiteral("extend")
            || context_.isCommandDirectiveInScope(firstToken, dataScope);
        if (recognizedDirective) {
            initialRows.append(MixedRowEntry{true, false, rowText, {}, commentText, commentMarker});
            continue;
        }

        if (!rowTokens.isEmpty() && currentFieldCount > 0) {
            schemaMismatchDetected = true;
            const QStringList clipped = rowTokens.mid(0, qMin(currentFieldCount, rowTokens.size()));
            initialRows.append(MixedRowEntry{false, false, QString(), clipped, commentText, commentMarker});
            continue;
        }

        initialRows.append(MixedRowEntry{true, false, rowText, {}, commentText, commentMarker});
    }

    if (initialRows.isEmpty()) {
        initialRows.append(MixedRowEntry{false, false, QString(), {}, QString(), QLatin1Char('#')});
    }

    QDialog dialog(context_.dialogParent);
    dialog.setWindowTitle(tr("Configure Data Block"));
    dialog.setModal(true);
    dialog.resize(900, 520);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *rowsLabel = new QLabel(tr("Rows (mix measurement rows, directives, and comments)"), &dialog);
    layout->addWidget(rowsLabel);

    auto *buttonRow = new QHBoxLayout;
    auto *addDataRowButton = new QPushButton(tr("Add Data Row"), &dialog);
    auto *addDirectiveRowButton = new QPushButton(tr("Add Directive Row"), &dialog);
    auto *addCommentRowButton = new QPushButton(tr("Add Comment Row"), &dialog);
    auto *removeRowButton = new QPushButton(tr("Remove Row"), &dialog);
    buttonRow->addWidget(addDataRowButton);
    buttonRow->addWidget(addDirectiveRowButton);
    buttonRow->addWidget(addCommentRowButton);
    buttonRow->addWidget(removeRowButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    auto *rowsTable = new DataRowsTableWidget(&dialog);
    rowsTable->setEditTriggers(QAbstractItemView::DoubleClicked
                               | QAbstractItemView::SelectedClicked
                               | QAbstractItemView::EditKeyPressed);
    rowsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    rowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    rowsTable->horizontalHeader()->setStretchLastSection(true);
    rowsTable->verticalHeader()->setVisible(false);
    rowsTable->setAlternatingRowColors(false);
    rowsTable->setRowCount(initialRows.size());

    QStringList headerLabels;
    headerLabels << tr("Type");
    for (const QString &fieldName : currentFieldNames) {
        headerLabels << dataFieldDisplayLabel(fieldName);
    }
    headerLabels << tr("Directive") << tr("Comment");
    rowsTable->setColumnCount(headerLabels.size());
    rowsTable->setHorizontalHeaderLabels(headerLabels);

    const int directiveColumn = headerLabels.size() - 2;
    const int commentColumn = headerLabels.size() - 1;

    const QVector<RowTypeChoice> rowTypeChoices{
        {kDataRowType, tr("Data")},
        {kDirectiveRowType, tr("Directive")},
        {kCommentRowType, tr("Comment")}};
    auto rowTypeLabel = [rowTypeChoices](const QString &typeId) {
        for (const RowTypeChoice &choice : rowTypeChoices) {
            if (choice.id == typeId) {
                return choice.label;
            }
        }
        return rowTypeChoices.first().label;
    };
    auto normalizedRowTypeId = [rowTypeChoices](const QTableWidgetItem *typeItem) {
        const QString stored = typeItem != nullptr ? typeItem->data(kRowTypeRole).toString().trimmed().toLower() : QString();
        if (stored == kDirectiveRowType || stored == kCommentRowType) {
            return stored;
        }
        if (stored == kDataRowType) {
            return stored;
        }

        const QString visible = typeItem != nullptr ? typeItem->text().trimmed() : QString();
        for (const RowTypeChoice &choice : rowTypeChoices) {
            if (visible.compare(choice.label, Qt::CaseInsensitive) == 0
                || visible.compare(choice.id, Qt::CaseInsensitive) == 0) {
                return choice.id;
            }
        }
        return kDataRowType;
    };
    auto makeRowTypeItem = [rowTypeLabel](const QString &typeId) {
        auto *item = new QTableWidgetItem(rowTypeLabel(typeId));
        item->setData(kRowTypeRole, typeId);
        return item;
    };

    auto applyRowType = [rowsTable, directiveColumn, commentColumn, rowTypeLabel, normalizedRowTypeId](int row) {
        if (row < 0 || row >= rowsTable->rowCount()) {
            return;
        }

        const QSignalBlocker blocker(rowsTable);
        QTableWidgetItem *typeItem = rowsTable->item(row, 0);
        const QString rowTypeId = normalizedRowTypeId(typeItem);
        const bool directiveRow = rowTypeId == kDirectiveRowType;
        const bool commentRow = rowTypeId == kCommentRowType;
        const bool dataRow = !directiveRow && !commentRow;
        const int dataStartColumn = 1;

        if (typeItem == nullptr) {
            typeItem = new QTableWidgetItem;
            rowsTable->setItem(row, 0, typeItem);
        }
        typeItem->setData(kRowTypeRole, rowTypeId);
        const QString label = rowTypeLabel(rowTypeId);
        if (typeItem->text() != label) {
            typeItem->setText(label);
        }

        for (int column = dataStartColumn; column < directiveColumn; ++column) {
            QTableWidgetItem *item = rowsTable->item(row, column);
            if (item == nullptr) {
                item = new QTableWidgetItem;
                rowsTable->setItem(row, column, item);
            }
            Qt::ItemFlags flags = item->flags();
            if (dataRow) {
                flags |= Qt::ItemIsEditable;
                flags |= Qt::ItemIsEnabled;
            } else {
                flags &= ~Qt::ItemIsEditable;
                flags &= ~Qt::ItemIsEnabled;
                item->setText(QString());
            }
            item->setFlags(flags);
        }

        {
            QTableWidgetItem *item = rowsTable->item(row, directiveColumn);
            if (item == nullptr) {
                item = new QTableWidgetItem;
                rowsTable->setItem(row, directiveColumn, item);
            }
            Qt::ItemFlags flags = item->flags();
            if (directiveRow) {
                flags |= Qt::ItemIsEditable;
                flags |= Qt::ItemIsEnabled;
            } else {
                flags &= ~Qt::ItemIsEditable;
                flags &= ~Qt::ItemIsEnabled;
                item->setText(QString());
            }
            item->setFlags(flags);
        }

        {
            QTableWidgetItem *item = rowsTable->item(row, commentColumn);
            if (item == nullptr) {
                item = new QTableWidgetItem;
                rowsTable->setItem(row, commentColumn, item);
            }
            Qt::ItemFlags flags = item->flags();
            if (directiveRow || commentRow || dataRow) {
                flags |= Qt::ItemIsEditable;
                flags |= Qt::ItemIsEnabled;
            }
            item->setFlags(flags);
        }
    };

    for (int row = 0; row < initialRows.size(); ++row) {
        const MixedRowEntry &entry = initialRows.at(row);
        const QString typeId = entry.commentOnly ? kCommentRowType
            : (entry.directive ? kDirectiveRowType : kDataRowType);
        rowsTable->setItem(row, 0, makeRowTypeItem(typeId));

        for (int column = 1; column < directiveColumn; ++column) {
            const int valueIndex = column - 1;
            const QString value = valueIndex < entry.dataValues.size()
                ? entry.dataValues.at(valueIndex)
                : QString();
            rowsTable->setItem(row, column, new QTableWidgetItem(value));
        }

        rowsTable->setItem(row, directiveColumn, new QTableWidgetItem(entry.directiveText));
        rowsTable->setItem(row, commentColumn, new QTableWidgetItem(entry.commentText));
        applyRowType(row);
    }

    if (rowsTable->rowCount() > 0) {
        rowsTable->setCurrentCell(0, 0);
    }

    rowsTable->setAdvanceColumns({directiveColumn, commentColumn});
    rowsTable->onViewportResized = [rowsTable]() {
        if (rowsTable == nullptr || rowsTable->columnCount() <= 0) {
            return;
        }
        const int fixedTypeColumnWidth = 110;
        const int fixedDirectiveColumnWidth = 130;
        const int fixedCommentColumnWidth = 130;
        rowsTable->setColumnWidth(0, fixedTypeColumnWidth);
        rowsTable->setColumnWidth(rowsTable->columnCount() - 2, fixedDirectiveColumnWidth);
        rowsTable->setColumnWidth(rowsTable->columnCount() - 1, fixedCommentColumnWidth);
        const int viewportWidth = rowsTable->viewport()->width();
        const int dataColumns = qMax(0, rowsTable->columnCount() - 3);
        if (dataColumns <= 0) {
            return;
        }
        const int remainingWidth =
            qMax(0, viewportWidth - fixedTypeColumnWidth - fixedDirectiveColumnWidth - fixedCommentColumnWidth);
        const int perColumnWidth = qMax(90, remainingWidth / dataColumns);
        for (int column = 1; column <= dataColumns; ++column) {
            rowsTable->setColumnWidth(column, perColumnWidth);
        }
    };

    auto suggestionsProvider = [directiveColumn, commentColumn, currentFieldNames, directiveSuggestions](const QModelIndex &index) {
        if (!index.isValid()) {
            return QStringList{};
        }
        if (index.column() == directiveColumn) {
            return directiveSuggestions;
        }
        if (index.column() == commentColumn) {
            return QStringList{};
        }
        const int fieldIndex = index.column() - 1;
        if (fieldIndex >= 0 && fieldIndex < currentFieldNames.size()) {
            return QStringList{};
        }
        return QStringList{};
    };
    auto editableProvider = [rowsTable, directiveColumn](const QModelIndex &index) {
        if (!index.isValid()) {
            return false;
        }
        if (index.column() == 0) {
            return true;
        }
        QTableWidgetItem *typeItem = rowsTable->item(index.row(), 0);
        const QString rowType = typeItem != nullptr
            ? typeItem->data(kRowTypeRole).toString()
            : kDataRowType;
        if (rowType == kDirectiveRowType) {
            return index.column() == directiveColumn || index.column() == rowsTable->columnCount() - 1;
        }
        if (rowType == kCommentRowType) {
            return index.column() == rowsTable->columnCount() - 1;
        }
        return index.column() >= 1 && index.column() < rowsTable->columnCount();
    };

    rowsTable->setItemDelegate(new DataRowsTableDelegate(suggestionsProvider, editableProvider, rowTypeChoices, rowsTable));
    rowsTable->onViewportResized();
    layout->addWidget(rowsTable, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    QObject::connect(addDataRowButton, &QPushButton::clicked, &dialog, [rowsTable, makeRowTypeItem, applyRowType]() {
        const int row = rowsTable->rowCount();
        rowsTable->insertRow(row);
        rowsTable->setItem(row, 0, makeRowTypeItem(kDataRowType));
        applyRowType(row);
        rowsTable->setCurrentCell(row, 0);
    });

    QObject::connect(addDirectiveRowButton, &QPushButton::clicked, &dialog, [rowsTable, makeRowTypeItem, applyRowType]() {
        const int row = rowsTable->rowCount();
        rowsTable->insertRow(row);
        rowsTable->setItem(row, 0, makeRowTypeItem(kDirectiveRowType));
        applyRowType(row);
        rowsTable->setCurrentCell(row, 0);
    });

    QObject::connect(addCommentRowButton, &QPushButton::clicked, &dialog, [rowsTable, makeRowTypeItem, applyRowType]() {
        const int row = rowsTable->rowCount();
        rowsTable->insertRow(row);
        rowsTable->setItem(row, 0, makeRowTypeItem(kCommentRowType));
        applyRowType(row);
        rowsTable->setCurrentCell(row, rowsTable->columnCount() - 1);
    });

    QObject::connect(removeRowButton, &QPushButton::clicked, &dialog, [rowsTable, makeRowTypeItem, applyRowType]() {
        const int currentRow = rowsTable->currentRow();
        if (currentRow >= 0 && currentRow < rowsTable->rowCount()) {
            rowsTable->removeRow(currentRow);
        }
        if (rowsTable->rowCount() == 0) {
            rowsTable->insertRow(0);
            rowsTable->setItem(0, 0, makeRowTypeItem(kDataRowType));
            applyRowType(0);
            rowsTable->setCurrentCell(0, 0);
        }
    });

    QObject::connect(rowsTable,
                     &QTableWidget::itemChanged,
                     &dialog,
                     [applyRowType](QTableWidgetItem *item) {
                         if (item == nullptr || item->column() != 0) {
                             return;
                         }
                         applyRowType(item->row());
                     });

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QStringList rowLines;
    bool mismatchDetected = schemaMismatchDetected;

    for (int row = 0; row < rowsTable->rowCount(); ++row) {
        const QString rowType = normalizedRowTypeId(rowsTable->item(row, 0));
        const QString commentText = (rowsTable->item(row, rowsTable->columnCount() - 1) != nullptr
                                         ? rowsTable->item(row, rowsTable->columnCount() - 1)->text().trimmed()
                                         : QString());

        if (rowType == kDirectiveRowType) {
            const QString directiveText = (rowsTable->item(row, rowsTable->columnCount() - 2) != nullptr
                                              ? rowsTable->item(row, rowsTable->columnCount() - 2)->text().trimmed()
                                              : QString());
            if (directiveText.isEmpty() && commentText.isEmpty()) {
                continue;
            }
            QString line = rowIndent + directiveText;
            if (!commentText.isEmpty()) {
                if (!directiveText.isEmpty()) {
                    line += QStringLiteral(" ");
                }
                line += QStringLiteral("# ") + commentText;
            }
            rowLines.append(line.trimmed().isEmpty() ? QString() : line);
            continue;
        }

        if (rowType == kCommentRowType) {
            if (commentText.isEmpty()) {
                continue;
            }
            rowLines.append(rowIndent + QStringLiteral("# ") + commentText);
            continue;
        }

        QStringList values;
        for (int column = 1; column < rowsTable->columnCount() - 2; ++column) {
            values.append(rowsTable->item(row, column) != nullptr
                              ? rowsTable->item(row, column)->text().trimmed()
                              : QString());
        }

        const bool rowCompletelyEmpty = std::all_of(values.begin(), values.end(), [](const QString &value) {
            return value.trimmed().isEmpty();
        });
        if (rowCompletelyEmpty && commentText.isEmpty()) {
            continue;
        }

        const int nonEmptyCount = std::count_if(values.begin(), values.end(), [](const QString &value) {
            return !value.trimmed().isEmpty();
        });
        if (nonEmptyCount > 0 && nonEmptyCount < currentFieldCount) {
            mismatchDetected = true;
        }

        QStringList serializedValues;
        for (const QString &value : values) {
            serializedValues.append(value.trimmed());
        }

        QString line = rowIndent + serializedValues.join(QLatin1Char(' ')).trimmed();
        if (!commentText.isEmpty()) {
            if (!line.trimmed().isEmpty()) {
                line += QStringLiteral(" ");
            }
            line += QStringLiteral("# ") + commentText;
        }
        rowLines.append(line.trimmed().isEmpty() ? QString() : line);
    }

    return BlockEditorDataBlockDialogResult{rowLines, dataBodyLastLine, mismatchDetected};
}
}
