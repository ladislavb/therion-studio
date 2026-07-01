#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/app/text_editor/block_editor/BlockEditorCanvasItem.h"
#include "../src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void pumpEventsFast()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
}

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &localPos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    if (widget == nullptr) {
        return;
    }

    const QPoint globalPos = widget->mapToGlobal(localPos);
    QMouseEvent event(type,
                      QPointF(localPos),
                      QPointF(globalPos),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

void sendKey(QWidget *widget,
             QEvent::Type type,
             int key,
             Qt::KeyboardModifiers modifiers = Qt::NoModifier,
             const QString &text = QString())
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers, text);
    QCoreApplication::sendEvent(widget, &event);
}

QPushButton *findButtonByText(QWidget *root, const QString &text)
{
    if (root == nullptr) {
        return nullptr;
    }
    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button != nullptr && button->text().trimmed() == text) {
            return button;
        }
    }
    return nullptr;
}

bool selectBlockAtScenePoint(QGraphicsView *view, const QPointF &scenePoint)
{
    if (view == nullptr || view->scene() == nullptr) {
        return false;
    }

    const QPoint viewportPoint = view->mapFromScene(scenePoint);
    QWidget *viewport = view->viewport();
    if (viewport == nullptr) {
        return false;
    }

    sendMouse(viewport, QEvent::MouseButtonPress, viewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(viewport, QEvent::MouseButtonRelease, viewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEventsFast();
    return !view->scene()->selectedItems().isEmpty();
}

bool selectBlockByKind(QGraphicsView *view, QLabel *statusLabel, const QString &kindToken)
{
    Q_UNUSED(statusLabel);

    if (view == nullptr || view->scene() == nullptr) {
        return false;
    }

    const QString normalizedKind = kindToken.trimmed().toLower();
    const QList<QGraphicsItem *> sceneItems = view->scene()->items(Qt::AscendingOrder);
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr || !(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            continue;
        }

        const QRectF sceneRect = item->sceneBoundingRect();
        if (!sceneRect.isValid() || sceneRect.width() < 16.0 || sceneRect.height() < 8.0) {
            continue;
        }

        if (!selectBlockAtScenePoint(view, sceneRect.center())) {
            continue;
        }

        for (QGraphicsItem *selectedItem : view->scene()->selectedItems()) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(selectedItem);
                blockItem != nullptr && blockItem->kind().trimmed().toLower() == normalizedKind) {
                return true;
            }
        }
    }
    return false;
}

QListWidgetItem *findToolboxCommandItem(QListWidget *toolboxList, const QString &commandToken)
{
    if (toolboxList == nullptr) {
        return nullptr;
    }
    const QString normalizedToken = commandToken.trimmed().toLower();
    for (int row = 0; row < toolboxList->count(); ++row) {
        QListWidgetItem *item = toolboxList->item(row);
        if (item == nullptr) {
            continue;
        }
        if (item->data(Qt::UserRole).toString().trimmed().toLower() == normalizedToken) {
            return item;
        }
    }
    return nullptr;
}

QStringList readingsOrderTagTokens(QWidget *root)
{
    if (root == nullptr) {
        return {};
    }
    auto *tagEditor = dynamic_cast<BlockEditorTokenTagEditor *>(
        root->findChild<QWidget *>(QStringLiteral("blockDetailsReadingsTagEditor")));
    if (tagEditor == nullptr) {
        return {};
    }
    return tagEditor->tokens();
}

void clearReadingsOrderTagTokens(QWidget *root)
{
    if (root == nullptr) {
        return;
    }
    QWidget *tagEditor = root->findChild<QWidget *>(QStringLiteral("blockDetailsReadingsTagEditor"));
    if (tagEditor == nullptr) {
        return;
    }

    int safetyCounter = 0;
    while (safetyCounter < 64) {
        ++safetyCounter;
        const QList<QToolButton *> chips = tagEditor->findChildren<QToolButton *>();
        if (chips.isEmpty()) {
            break;
        }
        QToolButton *chip = chips.first();
        if (chip == nullptr) {
            break;
        }
        chip->click();
        pumpEvents();
    }
}

void commitBlockDetailsEdit(QLineEdit *editor, QWidget *focusTarget)
{
    if (editor != nullptr) {
        editor->setFocus(Qt::OtherFocusReason);
        sendKey(editor, QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_Return);
    }
    if (QWidget *focusedWidget = QApplication::focusWidget(); focusedWidget != nullptr) {
        focusedWidget->clearFocus();
    }
    if (focusTarget != nullptr) {
        focusTarget->setFocus(Qt::OtherFocusReason);
    }
    pumpEvents();
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtFileSystem fileSystem;

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = tempDir.path() + QStringLiteral("/caret-test.th");
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create test file.")) {
        return 1;
    }
    file.write("survey demo -title old # survey comment\ncenterline\n# caret-target line for typing test\nmystery-command foo bar\nteam oldteam\nexplo-team old-discovery-team\ndata normal from to tape compass clino\n  1 2 3 4 5 6\nendcenterline\nendsurvey\n");
    file.close();

    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    QString errorMessage;
    if (!expect(tab.loadFile(filePath, &errorMessage), "Failed to load test file in TextEditorTab.")) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }

    tab.resize(960, 640);
    tab.show();
    pumpEvents();

    auto *editor = tab.findChild<QPlainTextEdit *>();
    if (!expect(editor != nullptr, "Failed to find raw text editor widget.")) {
        return 1;
    }

    QTextBlock thirdLine = editor->document()->findBlockByLineNumber(2);
    if (!expect(thirdLine.isValid(), "Failed to resolve third line in editor document.")) {
        return 1;
    }

    QTextCursor targetCursor(thirdLine);
    targetCursor.movePosition(QTextCursor::StartOfBlock);
    editor->setTextCursor(targetCursor);
    pumpEvents();

    const QPoint clickPos = editor->cursorRect().center() + QPoint(45, 0);

    QTextCursor firstLineCursor(editor->document()->findBlockByLineNumber(0));
    firstLineCursor.movePosition(QTextCursor::StartOfBlock);
    editor->setTextCursor(firstLineCursor);
    editor->setFocus(Qt::OtherFocusReason);
    pumpEvents();

    QWidget *viewport = editor->viewport();
    sendMouse(viewport, QEvent::MouseButtonPress, clickPos, Qt::LeftButton, Qt::LeftButton);
    sendMouse(viewport, QEvent::MouseButtonRelease, clickPos, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QTextCursor clickedCursor = editor->textCursor();
    if (!expect(clickedCursor.blockNumber() == 2, "Caret click did not move cursor to the expected line.")) {
        return 1;
    }
    if (!expect(clickedCursor.positionInBlock() > 0, "Caret click did not place cursor inside the line content.")) {
        return 1;
    }

    const int beforeLength = editor->toPlainText().size();
    sendKey(editor, QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_X);
    pumpEvents();

    if (!expect(editor->toPlainText().size() == beforeLength + 1, "Typing did not insert text at caret.")) {
        return 1;
    }
    if (!expect(editor->textCursor().blockNumber() == 2, "Typing after click moved cursor away from target line.")) {
        return 1;
    }

    const QString beforeTabText = editor->toPlainText();
    const int beforeTabPos = editor->textCursor().position();
    sendKey(editor, QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_Tab);
    pumpEvents();

    const QString afterTabText = editor->toPlainText();
    if (!expect(afterTabText.size() == beforeTabText.size() + 4,
                "Tab should insert exactly four spaces.")) {
        return 1;
    }
    if (!expect(afterTabText.mid(beforeTabPos, 4) == QStringLiteral("    "),
                "Tab insertion should add four space characters at the caret.")) {
        return 1;
    }
    if (!expect(!afterTabText.contains(QLatin1Char('\t')),
                "Document should not contain tab characters after Tab insertion.")) {
        return 1;
    }

    QPushButton *blocksModeButton = findButtonByText(&tab, QStringLiteral("Blocks"));
    if (!expect(blocksModeButton != nullptr, "Failed to find Blocks mode button.")) {
        return 1;
    }
    blocksModeButton->click();
    pumpEvents();

    auto *blockView = tab.findChild<QGraphicsView *>();
    if (!expect(blockView != nullptr, "Failed to find block canvas view.")) {
        return 1;
    }
    auto *detailsStatus = tab.findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
    if (!expect(detailsStatus != nullptr, "Failed to find block details status label.")) {
        return 1;
    }
    auto *detailsHelp = tab.findChild<QTextEdit *>(QStringLiteral("blockDetailsHelpBrowser"));
    if (!expect(detailsHelp != nullptr, "Failed to find block details help browser.")) {
        return 1;
    }
    auto *toolboxList = tab.findChild<QListWidget *>();
    if (!expect(toolboxList != nullptr, "Failed to find block toolbox list.")) {
        return 1;
    }
    QComboBox *scopeCombo = nullptr;
    const QList<QComboBox *> combos = tab.findChildren<QComboBox *>();
    for (QComboBox *combo : combos) {
        if (combo == nullptr) {
            continue;
        }
        bool hasAutoScope = false;
        for (int row = 0; row < combo->count(); ++row) {
            if (combo->itemData(row).toString() == QStringLiteral("__auto__")) {
                hasAutoScope = true;
                break;
            }
        }
        if (hasAutoScope) {
            scopeCombo = combo;
            break;
        }
    }
    if (!expect(scopeCombo != nullptr, "Failed to find block toolbox scope combo.")) {
        return 1;
    }
    auto *primaryEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
    auto *secondaryEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsSecondaryEdit"));
    auto *readingsTagInput = tab.findChild<QLineEdit *>(QStringLiteral("tokenTagEditorInput"));
    auto *commentEdit = tab.findChild<QLineEdit *>(QStringLiteral("blockDetailsCommentEdit"));
    auto *primaryLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsPrimaryLabel"));
    auto *secondaryLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsSecondaryLabel"));
    auto *optionsLabel = tab.findChild<QLabel *>(QStringLiteral("blockDetailsOptionsLabel"));
    auto *optionsTable = tab.findChild<QTableWidget *>(QStringLiteral("blockDetailsOptionsTable"));
    auto *addOptionButton = tab.findChild<QPushButton *>(QStringLiteral("blockDetailsAddOptionButton"));
    if (!expect(primaryEdit != nullptr, "Failed to find blockDetailsPrimaryEdit.")) {
        return 1;
    }
    if (!expect(secondaryEdit != nullptr, "Failed to find blockDetailsSecondaryEdit.")) {
        return 1;
    }
    if (!expect(commentEdit != nullptr, "Failed to find blockDetailsCommentEdit.")) {
        return 1;
    }
    if (!expect(readingsTagInput != nullptr, "Failed to find tokenTagEditorInput.")) {
        return 1;
    }
    if (!expect(primaryLabel != nullptr, "Failed to find blockDetailsPrimaryLabel.")) {
        return 1;
    }
    if (!expect(secondaryLabel != nullptr, "Failed to find blockDetailsSecondaryLabel.")) {
        return 1;
    }
    if (!expect(optionsLabel != nullptr, "Failed to find blockDetailsOptionsLabel.")) {
        return 1;
    }
    if (!expect(optionsTable != nullptr, "Failed to find blockDetailsOptionsTable.")) {
        return 1;
    }
    if (!expect(addOptionButton != nullptr, "Failed to find blockDetails add option button.")) {
        return 1;
    }
    const QString blocksText = tab.text();
    if (!expect(!blocksText.startsWith(QStringLiteral("encoding utf-8")),
                "Opening Blocks mode should not inject `encoding utf-8` into source text.")) {
        return 1;
    }
    int encodingDirectiveCount = 0;
    const QStringList blocksLines = blocksText.split(QLatin1Char('\n'));
    for (int index = 0; index < blocksLines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(blocksLines.at(index), index + 1);
        if (parsedLine.directive.trimmed().compare(QStringLiteral("encoding"), Qt::CaseInsensitive) == 0) {
            ++encodingDirectiveCount;
        }
    }
    if (!expect(encodingDirectiveCount == 0,
                "Blocks mode should not synthesize an `encoding` directive in source text.")) {
        return 1;
    }

    if (!expect(findToolboxCommandItem(toolboxList, QStringLiteral("encoding")) == nullptr,
                "Toolbox should not expose `encoding` command templates.")) {
        return 1;
    }

    const int allScopeIndex = scopeCombo->findData(QStringLiteral("all"));
    if (!expect(allScopeIndex >= 0, "Failed to find `All` scope in toolbox scope selector.")) {
        return 1;
    }
    scopeCombo->setCurrentIndex(allScopeIndex);
    pumpEvents();

    if (!expect(findToolboxCommandItem(toolboxList, QStringLiteral("source")) == nullptr,
                ".th toolbox should not expose thconfig `source` command templates.")) {
        return 1;
    }
    if (!expect(findToolboxCommandItem(toolboxList, QStringLiteral("select")) == nullptr,
                ".th toolbox should not expose thconfig `select` command templates.")) {
        return 1;
    }
    if (!expect(findToolboxCommandItem(toolboxList, QStringLiteral("export")) == nullptr,
                ".th toolbox should not expose thconfig `export` command templates.")) {
        return 1;
    }
    for (const QString &mapObjectCommand : {QStringLiteral("scrap"),
                                            QStringLiteral("point"),
                                            QStringLiteral("line"),
                                            QStringLiteral("area")}) {
        if (!expect(findToolboxCommandItem(toolboxList, mapObjectCommand) == nullptr,
                    ".th toolbox should not expose .th2 map object command templates.")) {
            return 1;
        }
    }

    QListWidgetItem *toolboxDataItem = findToolboxCommandItem(toolboxList, QStringLiteral("data"));
    if (!expect(toolboxDataItem != nullptr, "Failed to find `data` command in block toolbox list.")) {
        return 1;
    }
    toolboxList->setCurrentItem(toolboxDataItem);
    pumpEvents();
    if (!expect(!primaryEdit->isVisible(),
                "Selecting toolbox command should hide editable Block Details section.")) {
        return 1;
    }
    const QString toolboxHelpText = detailsHelp->toPlainText().toLower();
    const QString toolboxHelpHtml = detailsHelp->toHtml().toLower();
    if (!expect(toolboxHelpText.trimmed().size() > 12,
                "Selecting toolbox command should show non-empty compact descriptive help.")) {
        return 1;
    }
    if (!expect(toolboxHelpText.contains(QStringLiteral("summary:")),
                "Selecting toolbox command should keep 'Summary:' label for consistency.")) {
        return 1;
    }
    if (!expect(!toolboxHelpText.contains(QStringLiteral("inspect help before drag/drop insertion")),
                "Toolbox command preview should not include introductory instruction sentence.")) {
        return 1;
    }
    if (!expect(toolboxHelpHtml.contains(QStringLiteral("arguments")),
                "Toolbox command preview should show full argument sections.")) {
        return 1;
    }
    if (!expect(toolboxHelpText.contains(QStringLiteral("syntax:")),
                "Toolbox command preview should show complete command syntax.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("survey")),
                "Failed to select survey block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryEdit->isVisible(),
                "Selecting a canvas block should show editable Block Details section.")) {
        return 1;
    }
    if (!expect(!secondaryEdit->isVisible(),
                "Extra arguments field should stay hidden when no extra positional tokens exist.")) {
        return 1;
    }
    if (!expect(optionsLabel->isVisible(),
                "Options section should be visible for commands that support options.")) {
        return 1;
    }
    if (!expect(commentEdit->isVisible()
                    && commentEdit->text().trimmed().compare(QStringLiteral("survey comment"), Qt::CaseInsensitive) == 0,
                "Selected block should expose editable inline comment field with parsed comment text.")) {
        return 1;
    }
    const int surveyOptionRowCount = optionsTable->rowCount();
    addOptionButton->click();
    pumpEvents();
    if (!expect(optionsTable->rowCount() == surveyOptionRowCount + 1,
                "Add-option header button should append a new option row.")) {
        return 1;
    }
    QTableWidgetItem *newOptionItem = optionsTable->item(optionsTable->rowCount() - 1, 0);
    if (!expect(newOptionItem != nullptr && newOptionItem->text().trimmed().isEmpty(),
                "Add-option header button should keep new option key empty (no prepopulated placeholder).")) {
        return 1;
    }
    optionsTable->removeRow(optionsTable->rowCount() - 1);
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Selecting a block should show command/parameter help by default, not sticky option-row help.")) {
        return 1;
    }
    optionsTable->setFocus();
    optionsTable->setCurrentCell(0, 0);
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Focusing an option row should keep parent command/parameter help visible.")) {
        return 1;
    }
    blockView->setFocus();
    pumpEvents();
    if (!expect(!detailsHelp->toPlainText().toLower().contains(QStringLiteral("option: |title")),
                "Leaving option-table focus should restore command/parameter help.")) {
        return 1;
    }
    if (!expect(detailsStatus->text().toLower().contains(QStringLiteral("source line")),
                "Block Details status should show source line context.")) {
        return 1;
    }
    if (!expect(detailsHelp->toPlainText().toLower().contains(QStringLiteral("syntax:")),
                "Block details contextual help should show complete Syntax section in Blocks mode.")) {
        return 1;
    }

    primaryEdit->setText(QStringLiteral("demo2"));
    commitBlockDetailsEdit(primaryEdit, blockView);
    if (!expect(editor->toPlainText().contains(QStringLiteral("survey demo2 -title old")),
                "Applying details for survey block should update survey id.")) {
        return 1;
    }
    if (!expect(editor->toPlainText().contains(QStringLiteral("# survey comment")),
                "Applying block details should preserve existing inline comment.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("comment")),
                "Failed to select a full-line comment block in blocks view.")) {
        return 1;
    }
    if (!expect(!primaryEdit->text().trimmed().isEmpty(),
                "Comment block should populate primary field with non-empty line comment text.")) {
        return 1;
    }
    if (!expect(!detailsStatus->text().toLower().contains(QStringLiteral("selected line is empty")),
                "Comment block must not report 'Selected line is empty' in Block Details status.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("__unrecognized")),
                "Failed to select unrecognized fallback block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryLabel->text().trimmed().compare(QStringLiteral("Raw line"), Qt::CaseInsensitive) == 0,
                "Unrecognized block should expose editable raw-line field.")) {
        return 1;
    }
    if (!expect(primaryEdit->text().contains(QStringLiteral("mystery-command foo bar")),
                "Unrecognized block should show original raw source line.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("mystery-command fixed-value"));
    commitBlockDetailsEdit(primaryEdit, blockView);
    if (!expect(editor->toPlainText().contains(QStringLiteral("mystery-command fixed-value")),
                "Applying unrecognized block details should rewrite the full raw source line.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("team")),
                "Failed to select team block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryEdit->isVisible() && secondaryEdit->isVisible(),
                "Team block should expose separate Person and Roles fields.")) {
        return 1;
    }
    if (!expect(primaryLabel->text().trimmed().compare(QStringLiteral("Person"), Qt::CaseInsensitive) == 0,
                "Team block primary field should be labeled Person.")) {
        return 1;
    }
    if (!expect(secondaryLabel->text().trimmed().compare(QStringLiteral("Roles"), Qt::CaseInsensitive) == 0,
                "Team block secondary field should be labeled Roles.")) {
        return 1;
    }
    if (!expect(!optionsLabel->isVisible(),
                "Options section should stay hidden for commands without options support.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("ZO /CSS 6-28"));
    commitBlockDetailsEdit(primaryEdit, blockView);
    if (!expect(editor->toPlainText().contains(QStringLiteral("team \"ZO /CSS 6-28\"")),
                "Applying details for team block should quote spaced team value.")) {
        return 1;
    }
    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("team")),
                "Failed to reselect team block after apply.")) {
        return 1;
    }
    if (!expect(secondaryEdit->text().trimmed().isEmpty(),
                QStringLiteral("Team block without roles should parse empty Roles field (actual: `%1`).")
                    .arg(secondaryEdit->text())
                    .toUtf8()
                    .constData())) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("explo-team")),
                "Failed to select explo-team block in blocks view.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("Babicka Speleo Group"));
    commitBlockDetailsEdit(primaryEdit, blockView);
    if (!expect(editor->toPlainText().contains(QStringLiteral("explo-team \"Babicka Speleo Group\"")),
                "Applying details for explo-team block should quote spaced person value.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("data")),
                "Failed to select data block in blocks view.")) {
        return 1;
    }
    if (!expect(primaryLabel->text().trimmed().compare(QStringLiteral("Style"), Qt::CaseInsensitive) == 0,
                "Data block primary field should be labeled Style.")) {
        return 1;
    }
    if (!expect(secondaryLabel->isVisible()
                    && secondaryLabel->text().trimmed().compare(QStringLiteral("Readings Order"), Qt::CaseInsensitive) == 0,
                "Data block secondary field should be visible and labeled Readings Order.")) {
        return 1;
    }
    if (!expect(readingsOrderTagTokens(&tab)
                    == QStringList({QStringLiteral("from"),
                                    QStringLiteral("to"),
                                    QStringLiteral("tape"),
                                    QStringLiteral("compass"),
                                    QStringLiteral("clino")}),
                "Data block should parse existing readings order into tag chips.")) {
        return 1;
    }
    primaryEdit->setText(QStringLiteral("normal"));
    clearReadingsOrderTagTokens(&tab);
    readingsTagInput->setFocus();
    readingsTagInput->setText(QStringLiteral("from to length compass clino"));
    sendKey(readingsTagInput, QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
    sendKey(readingsTagInput, QEvent::KeyRelease, Qt::Key_Return);
    commitBlockDetailsEdit(primaryEdit, blockView);
    if (!expect(editor->toPlainText().contains(QStringLiteral("data normal from to length compass clino")),
                "Applying details for data block should update style + readings order.")) {
        return 1;
    }
    if (!expect(editor->toPlainText().contains(QStringLiteral("  1 2 3 4 5 6")),
                "Applying data header edit should preserve existing data rows.")) {
        return 1;
    }

    if (!expect(selectBlockByKind(blockView, detailsStatus, QStringLiteral("centerline")),
                "Failed to select centerline block in blocks view.")) {
        return 1;
    }
    const int centerlineOptionRows = optionsTable->rowCount();
    addOptionButton->click();
    pumpEvents();
    if (!expect(optionsTable->rowCount() == centerlineOptionRows + 1,
                "Centerline Add Option should append a new option row.")) {
        return 1;
    }
    const int newRow = optionsTable->rowCount() - 1;
    optionsTable->setItem(newRow, 0, new QTableWidgetItem(QStringLiteral("-walls")));
    optionsTable->setItem(newRow, 1, new QTableWidgetItem(QStringLiteral("invalid-value")));
    pumpEvents();
    optionsTable->removeRow(newRow);
    pumpEvents();

    {
        auto *crashGuardTab = new TextEditorTab(fileSystem, CommandCatalogStore());
        if (!expect(crashGuardTab->loadFile(filePath, &errorMessage),
                    "Failed to load crash-guard tab instance.")) {
            delete crashGuardTab;
            return 1;
        }
        const QString cleanLoadedText = crashGuardTab->text();
        crashGuardTab->resize(960, 640);
        crashGuardTab->show();
        pumpEvents();

        QPushButton *guardBlocksButton = findButtonByText(crashGuardTab, QStringLiteral("Blocks"));
        if (guardBlocksButton != nullptr) {
            guardBlocksButton->click();
            pumpEvents();
            if (!expect(!crashGuardTab->isDirty(),
                        "Switching to Blocks mode must not mark a clean document as modified.")) {
                delete crashGuardTab;
                return 1;
            }
            if (!expect(crashGuardTab->text() == cleanLoadedText,
                        "Switching to Blocks mode must not rewrite clean source text.")) {
                delete crashGuardTab;
                return 1;
            }
        }
        auto *guardView = crashGuardTab->findChild<QGraphicsView *>();
        auto *guardStatus = crashGuardTab->findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
        if (guardView != nullptr) {
            selectBlockByKind(guardView, guardStatus, QStringLiteral("survey"));
        }
        auto *guardPrimaryEdit = crashGuardTab->findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
        if (guardPrimaryEdit != nullptr) {
            guardPrimaryEdit->setText(QStringLiteral("discard-change"));
        }
        delete crashGuardTab;
        pumpEvents();
    }

    {
        auto *defaultBlocksTab = new TextEditorTab(fileSystem, CommandCatalogStore());
        defaultBlocksTab->setInitialEditorMode(TextEditorTab::EditorMode::Blocks);
        if (!expect(defaultBlocksTab->loadFile(filePath, &errorMessage),
                    "Failed to load default-blocks tab instance.")) {
            delete defaultBlocksTab;
            return 1;
        }
        pumpEvents();
        if (!expect(!defaultBlocksTab->isDirty(),
                    "Opening a clean file directly in Blocks mode must not mark it dirty.")) {
            delete defaultBlocksTab;
            return 1;
        }
        if (!expect(!defaultBlocksTab->text().startsWith(QStringLiteral("encoding utf-8")),
                    "Opening directly in Blocks mode must not inject `encoding utf-8` into source text.")) {
            delete defaultBlocksTab;
            return 1;
        }
        delete defaultBlocksTab;
        pumpEvents();
    }

    const QString configPath = tempDir.path() + QStringLiteral("/thconfig");
    QFile configFile(configPath);
    if (!expect(configFile.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create thconfig test file.")) {
        return 1;
    }
    configFile.write("encoding utf-8\nsource index.th\nselect 1302.m@1302\nexport map -output _output/1302_plan.pdf -projection plan\n");
    configFile.close();

    {
        auto *configTab = new TextEditorTab(fileSystem, CommandCatalogStore());
        if (!expect(configTab->loadFile(configPath, &errorMessage),
                    "Failed to load thconfig test file in TextEditorTab.")) {
            std::cerr << errorMessage.toStdString() << '\n';
            delete configTab;
            return 1;
        }
        configTab->resize(960, 640);
        configTab->show();
        pumpEvents();

        QPushButton *configBlocksButton = findButtonByText(configTab, QStringLiteral("Blocks"));
        if (!expect(configBlocksButton != nullptr, "Failed to find Blocks mode button for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        configBlocksButton->click();
        pumpEvents();

        auto *configView = configTab->findChild<QGraphicsView *>();
        auto *configStatus = configTab->findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
        auto *configToolboxList = configTab->findChild<QListWidget *>();
        QComboBox *configScopeCombo = nullptr;
        const QList<QComboBox *> configCombos = configTab->findChildren<QComboBox *>();
        for (QComboBox *combo : configCombos) {
            if (combo == nullptr) {
                continue;
            }
            bool hasAutoScope = false;
            for (int row = 0; row < combo->count(); ++row) {
                if (combo->itemData(row).toString() == QStringLiteral("__auto__")) {
                    hasAutoScope = true;
                    break;
                }
            }
            if (hasAutoScope) {
                configScopeCombo = combo;
                break;
            }
        }
        if (!expect(configView != nullptr, "Failed to find block canvas view for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configStatus != nullptr, "Failed to find block details status for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configToolboxList != nullptr, "Failed to find block toolbox list for thconfig tab.")) {
            delete configTab;
            return 1;
        }
        if (!expect(configScopeCombo != nullptr, "Failed to find scope combo for thconfig tab.")) {
            delete configTab;
            return 1;
        }

        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("select")) != nullptr,
                    "Top-level toolbox commands for thconfig should include `select`.")) {
            delete configTab;
            return 1;
        }
        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("export")) != nullptr,
                    "Top-level toolbox commands for thconfig should include `export`.")) {
            delete configTab;
            return 1;
        }
        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("source")) != nullptr,
                    "Top-level toolbox commands for thconfig should include `source`.")) {
            delete configTab;
            return 1;
        }
        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("survey")) == nullptr,
                    "thconfig toolbox should not expose `.th` survey command templates.")) {
            delete configTab;
            return 1;
        }
        if (!expect(findToolboxCommandItem(configToolboxList, QStringLiteral("input")) == nullptr,
                    "thconfig toolbox should not expose `.th` input command templates.")) {
            delete configTab;
            return 1;
        }

        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("select")),
                    "Blocks view should render top-level `select` command cards for thconfig files.")) {
            delete configTab;
            return 1;
        }
        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("export")),
                    "Blocks view should render top-level `export` command cards for thconfig files.")) {
            delete configTab;
            return 1;
        }
        if (!expect(selectBlockByKind(configView, configStatus, QStringLiteral("source")),
                    "Blocks view should render `source` command card for thconfig files.")) {
            delete configTab;
            return 1;
        }
        configScopeCombo->setCurrentIndex(configScopeCombo->findData(QStringLiteral("__auto__")));
        pumpEvents();
        const QString autoScopeTooltip = configScopeCombo->toolTip().toLower();
        if (!expect(autoScopeTooltip.contains(QStringLiteral("top-level")),
                    "Inline `source file` should behave as leaf (Auto scope should resolve to top-level when selected).")) {
            delete configTab;
            return 1;
        }

        delete configTab;
        pumpEvents();
    }

    const QString continuationPath = tempDir.path() + QStringLiteral("/continuation-test.th");
    QFile continuationFile(continuationPath);
    if (!expect(continuationFile.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create continuation test file.")) {
        return 1;
    }
    continuationFile.write(
        "survey wrapped \\\n"
        "  -title \"Wrapped Survey\" \\\n"
        "  -person \"Survey Author\"\n"
        "centerline\n"
        "endcenterline\n"
        "endsurvey\n");
    continuationFile.close();

    {
        auto *continuationTab = new TextEditorTab(fileSystem, CommandCatalogStore());
        if (!expect(continuationTab->loadFile(continuationPath, &errorMessage),
                    "Failed to load continuation test file in TextEditorTab.")) {
            std::cerr << errorMessage.toStdString() << '\n';
            delete continuationTab;
            return 1;
        }
        continuationTab->resize(960, 640);
        continuationTab->show();
        pumpEvents();

        QPushButton *continuationBlocksButton = findButtonByText(continuationTab, QStringLiteral("Blocks"));
        if (!expect(continuationBlocksButton != nullptr, "Failed to find Blocks mode button for continuation tab.")) {
            delete continuationTab;
            return 1;
        }
        continuationBlocksButton->click();
        pumpEvents();

        auto *continuationView = continuationTab->findChild<QGraphicsView *>();
        auto *continuationStatus = continuationTab->findChild<QLabel *>(QStringLiteral("blockDetailsStatusLabel"));
        auto *continuationPrimaryEdit = continuationTab->findChild<QLineEdit *>(QStringLiteral("blockDetailsPrimaryEdit"));
        auto *continuationOptionsTable = continuationTab->findChild<QTableWidget *>(QStringLiteral("blockDetailsOptionsTable"));
        if (!expect(continuationView != nullptr, "Failed to find block canvas view for continuation tab.")) {
            delete continuationTab;
            return 1;
        }
        if (!expect(continuationStatus != nullptr, "Failed to find block details status for continuation tab.")) {
            delete continuationTab;
            return 1;
        }
        if (!expect(continuationPrimaryEdit != nullptr, "Failed to find primary edit for continuation tab.")) {
            delete continuationTab;
            return 1;
        }
        if (!expect(continuationOptionsTable != nullptr, "Failed to find options table for continuation tab.")) {
            delete continuationTab;
            return 1;
        }

        if (!expect(selectBlockByKind(continuationView, continuationStatus, QStringLiteral("survey")),
                    "Blocks view failed to select wrapped survey command.")) {
            delete continuationTab;
            return 1;
        }
        if (!expect(continuationPrimaryEdit->text() == QStringLiteral("wrapped"),
                    "Blocks details should read the selected wrapped survey id from the logical command.")) {
            delete continuationTab;
            return 1;
        }
        bool foundTitleOption = false;
        bool foundPersonOption = false;
        for (int row = 0; row < continuationOptionsTable->rowCount(); ++row) {
            const QTableWidgetItem *keyItem = continuationOptionsTable->item(row, 0);
            if (keyItem == nullptr) {
                continue;
            }
            const QString key = keyItem->text().trimmed();
            foundTitleOption = foundTitleOption || key == QStringLiteral("-title");
            foundPersonOption = foundPersonOption || key == QStringLiteral("-person");
        }
        if (!expect(foundTitleOption && foundPersonOption,
                    "Blocks details should read options from wrapped logical command lines.")) {
            delete continuationTab;
            return 1;
        }
        continuationPrimaryEdit->setText(QStringLiteral("wrapped-renamed"));
        commitBlockDetailsEdit(continuationPrimaryEdit, continuationView);

        const QString continuationUpdatedText = continuationTab->text();
        const bool preservesWrappedOptions =
            continuationUpdatedText.contains(QStringLiteral("survey wrapped-renamed"))
            && continuationUpdatedText.contains(QStringLiteral("-title"))
            && continuationUpdatedText.contains(QStringLiteral("-person"));
        if (!preservesWrappedOptions) {
            std::cerr << "Continuation output after apply:\n"
                      << continuationUpdatedText.toStdString()
                      << '\n';
            std::cerr << "Applying wrapped-command details should preserve wrapped option tokens in merged command output.\n";
            delete continuationTab;
            return 1;
        }
        if (!expect(!continuationUpdatedText.contains(QStringLiteral("\n  -title")),
                    "Applying wrapped-command details should not leave orphan continuation rows.")) {
            delete continuationTab;
            return 1;
        }

        delete continuationTab;
        pumpEvents();
    }

    return 0;
}
