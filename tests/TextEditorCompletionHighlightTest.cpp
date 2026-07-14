#include "../src/app/text_editor/ContextHelpController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextLayout>
#include <QToolTip>

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

void triggerManualCompletion(QPlainTextEdit *editor)
{
    sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier, QStringLiteral(" "));
    sendKey(editor, QEvent::KeyRelease, Qt::Key_Space, Qt::ControlModifier);
    pumpEvents();
}

bool modelContainsToken(QCompleter *completer, const QString &token)
{
    if (completer == nullptr || completer->completionModel() == nullptr) {
        return false;
    }

    for (int row = 0; row < completer->completionModel()->rowCount(); ++row) {
        const QString candidate = completer->completionModel()->index(row, 0).data(Qt::DisplayRole).toString();
        if (candidate.compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool validationContainsDiagnosticCode(const TherionSourceValidationResult &result, const QString &code)
{
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

QStringList completionTokens(QCompleter *completer)
{
    QStringList tokens;
    if (completer == nullptr || completer->completionModel() == nullptr) {
        return tokens;
    }

    for (int row = 0; row < completer->completionModel()->rowCount(); ++row) {
        tokens.append(completer->completionModel()->index(row, 0).data(Qt::DisplayRole).toString());
    }
    return tokens;
}

bool tokenHasWaveUnderline(const QTextBlock &block, const QString &token)
{
    if (!block.isValid() || block.layout() == nullptr) {
        return false;
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return false;
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (range.format.underlineStyle() == QTextCharFormat::WaveUnderline) {
                return true;
            }
        }
    }
    return false;
}

bool tokenHasBoldFormat(const QTextBlock &block, const QString &token)
{
    if (!block.isValid() || block.layout() == nullptr) {
        return false;
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return false;
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (range.format.fontWeight() >= QFont::Bold
                && range.format.underlineStyle() != QTextCharFormat::WaveUnderline) {
                return true;
            }
        }
    }
    return false;
}

QColor tokenForegroundColor(const QTextBlock &block, const QString &token, bool *found)
{
    if (found != nullptr) {
        *found = false;
    }
    if (!block.isValid() || block.layout() == nullptr) {
        return {};
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return {};
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())) {
            if (found != nullptr) {
                *found = true;
            }
            return range.format.foreground().color();
        }
    }
    return {};
}

QColor expectedIdentifierColor()
{
    const QColor baseColor = QGuiApplication::palette().color(QPalette::Base);
    const bool darkPalette = baseColor.isValid() && baseColor.lightnessF() < 0.5;
    return darkPalette ? QColor(QStringLiteral("#DCDCAA")) : QColor(QStringLiteral("#8250df"));
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtFileSystem fileSystem;

    {
        const QString helpHtml = ContextHelpController::renderHelpHtml(QStringLiteral("survey"),
                                                                       QStringLiteral("Survey summary"),
                                                                       QStringLiteral("survey <id> [OPTIONS]"),
                                                                       {QStringLiteral("<id> = survey identifier")},
                                                                       {},
                                                                       {QStringLiteral("-title <text> = survey title")},
                                                                       true);
        if (!expect(helpHtml.contains(QStringLiteral("<code"))
                        && helpHtml.contains(QStringLiteral("<li"))
                        && helpHtml.contains(QStringLiteral("<b><tt"))
                        && helpHtml.contains(QStringLiteral("&lt;id&gt;"))
                        && helpHtml.contains(QStringLiteral("-title &lt;text&gt;")),
                    "Contextual help should render argument and option signatures as compact bold monospace bullet items.")) {
            return 1;
        }
    }

    {
        const QString helpHtml = ContextHelpController::renderHelpHtml(QStringLiteral("sample"),
                                                                       QStringLiteral("Sample summary"),
                                                                       QStringLiteral("sample [OPTIONS]"),
                                                                       {QStringLiteral("<argument-second> = positional second"),
                                                                        QStringLiteral("<argument-first> = positional first")},
                                                                       {},
                                                                       {QStringLiteral("-zeta <value> = zeta option"),
                                                                        QStringLiteral("-alpha <value> = alpha option"),
                                                                        QStringLiteral("-middle = middle option")},
                                                                       true);
        const int secondArgumentIndex = helpHtml.indexOf(QStringLiteral("&lt;argument-second&gt;"));
        const int firstArgumentIndex = helpHtml.indexOf(QStringLiteral("&lt;argument-first&gt;"));
        const int alphaOptionIndex = helpHtml.indexOf(QStringLiteral("-alpha"));
        const int middleOptionIndex = helpHtml.indexOf(QStringLiteral("-middle"));
        const int zetaOptionIndex = helpHtml.indexOf(QStringLiteral("-zeta"));
        if (!expect(secondArgumentIndex >= 0
                        && firstArgumentIndex > secondArgumentIndex
                        && alphaOptionIndex >= 0
                        && middleOptionIndex > alphaOptionIndex
                        && zetaOptionIndex > middleOptionIndex,
                    "Contextual help should preserve positional argument order and sort options alphabetically.")) {
            return 1;
        }
    }

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    {
        QDir rootDir(tempDir.path());
        if (!expect(rootDir.mkpath(QStringLiteral("subdir")), "Failed to create subdir fixture.")) {
            return 1;
        }
        QFile nestedFixture(tempDir.path() + QStringLiteral("/subdir/child.th2"));
        if (!expect(nestedFixture.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create nested fixture file.")) {
            return 1;
        }
        nestedFixture.write("scrap s1 -projection plan\nendscrap\n");
        nestedFixture.close();

        QFile nestedCurrentFixture(tempDir.path() + QStringLiteral("/subdir/current.th"));
        if (!expect(nestedCurrentFixture.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create nested current fixture file.")) {
            return 1;
        }
        nestedCurrentFixture.write("input \n");
        nestedCurrentFixture.close();
    }

    const QString filePath = tempDir.path() + QStringLiteral("/completion-highlight-test.th");
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create test file.")) {
        return 1;
    }
    file.write("survey demo\nendsurvey\n");
    file.close();

    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    QString errorMessage;
    if (!expect(tab.loadFile(filePath, &errorMessage), "Failed to load test file in TextEditorTab.")) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }
    tab.setProjectRootPath(tempDir.path());

    tab.resize(960, 640);
    tab.show();
    pumpEvents();

    auto *editor = tab.findChild<QPlainTextEdit *>();
    if (!expect(editor != nullptr, "Failed to find raw text editor widget.")) {
        return 1;
    }
    auto *completer = tab.findChild<QCompleter *>();
    if (!expect(completer != nullptr, "Failed to find completion widget.")) {
        return 1;
    }
    auto *helpBrowser = tab.findChild<QTextBrowser *>(QStringLiteral("rawContextHelpBrowser"));
    if (!expect(helpBrowser != nullptr, "Failed to find contextual help widget.")) {
        return 1;
    }
    if (!expect(completer->popup() != nullptr, "Failed to find completion popup.")) {
        return 1;
    }

    editor->setPlainText(QStringLiteral("point 0 0 station -name bad-name\n"));
    TherionSourceDiagnostic externalDiagnostic;
    externalDiagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    externalDiagnostic.code = QStringLiteral("unknown-station-reference");
    externalDiagnostic.lineNumber = 1;
    externalDiagnostic.columnNumber = 25;
    externalDiagnostic.columnLength = QStringLiteral("bad-name").size();
    externalDiagnostic.currentText = QStringLiteral("point 0 0 station -name bad-name");
    tab.setProjectValidationDiagnostics({externalDiagnostic});
    pumpEvents();
    const QTextBlock externalDiagnosticLine = editor->document()->findBlockByLineNumber(0);
    if (!expect(tokenHasWaveUnderline(externalDiagnosticLine, QStringLiteral("bad-name")),
                "Project validation station-reference diagnostics should underline the invalid station name.")) {
        return 1;
    }
    tab.setProjectValidationDiagnostics({});

    {
        editor->setPlainText(QStringLiteral("\n"));
        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    ".th command-position completion should appear on a blank top-level line.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("survey")),
                    ".th command-position completion should include source commands.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("source"))
                    && !modelContainsToken(completer, QStringLiteral("export"))
                    && !modelContainsToken(completer, QStringLiteral("select")),
                    ".th command-position completion should not include thconfig-only commands.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("scrap"))
                    && !modelContainsToken(completer, QStringLiteral("line"))
                    && !modelContainsToken(completer, QStringLiteral("point")),
                    ".th command-position completion should not include TH2 map-object commands.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("source index.th\n"));
        pumpEvents();

        const QTextBlock sourceLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(sourceLine, QStringLiteral("source")),
                    ".th inline validation should mark thconfig-only commands as invalid for the current document type.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("survey cave\n"
                                            "  centerline\n"
                                            "    cs long-lat\n"
                                            "    data normal from to compass clino tape\n"
                                            "    1 2 0 0 1\n"
                                            "  endcenterline\n"
                                            "  map cave.m\n"
                                            "    scrap1\n"
                                            "    break\n"
                                            "    scrap2\n"
                                            "  endmap\n"
                                            "  join scrap1 scrap2\n"
                                            "endsurvey\n"));
        pumpEvents();

        const TherionSourceValidationResult sourceValidation = tab.validateDocument();
        if (!expect(!validationContainsDiagnosticCode(sourceValidation, QStringLiteral("invalid-command-context")),
                    ".th validation should accept real source contexts such as centerline cs, map break, and survey join.")) {
            return 1;
        }
        if (!expect(!validationContainsDiagnosticCode(sourceValidation, QStringLiteral("invalid-document-type")),
                    ".th validation should accept source commands from the generated document-type catalog.")) {
            return 1;
        }
    }

    {
        const QString configPath = tempDir.path() + QStringLiteral("/thconfig");
        QFile configFile(configPath);
        if (!expect(configFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create thconfig completion fixture.")) {
            return 1;
        }
        configFile.write("\n");
        configFile.close();

        TextEditorTab configTab{fileSystem, CommandCatalogStore()};
        QString configErrorMessage;
        if (!expect(configTab.loadFile(configPath, &configErrorMessage),
                    "Failed to load thconfig completion fixture.")) {
            std::cerr << configErrorMessage.toStdString() << '\n';
            return 1;
        }
        configTab.setProjectRootPath(tempDir.path());
        configTab.resize(640, 480);
        configTab.show();
        pumpEvents();

        auto *configEditor = configTab.findChild<QPlainTextEdit *>();
        auto *configCompleter = configTab.findChild<QCompleter *>();
        if (!expect(configEditor != nullptr && configCompleter != nullptr,
                    "Failed to find thconfig editor completion widgets.")) {
            return 1;
        }

        QTextBlock firstLine = configEditor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        configEditor->setTextCursor(cursor);
        configEditor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(configEditor);

        if (!expect(configCompleter->popup()->isVisible(),
                    "thconfig command-position completion should appear on a blank top-level line.")) {
            return 1;
        }
        if (!expect(modelContainsToken(configCompleter, QStringLiteral("source"))
                    && modelContainsToken(configCompleter, QStringLiteral("export"))
                    && modelContainsToken(configCompleter, QStringLiteral("select")),
                    "thconfig command-position completion should include processing commands.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(configCompleter, QStringLiteral("survey"))
                    && !modelContainsToken(configCompleter, QStringLiteral("scrap"))
                    && !modelContainsToken(configCompleter, QStringLiteral("line")),
                    "thconfig command-position completion should not include .th or TH2-only commands.")) {
            return 1;
        }

        configEditor->setPlainText(QStringLiteral("survey demo\nendsurvey\n"));
        pumpEvents();

        const QTextBlock surveyLine = configEditor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(surveyLine, QStringLiteral("survey")),
                    "thconfig inline validation should mark .th-only commands as invalid for the current document type.")) {
            return 1;
        }

        configEditor->setPlainText(QStringLiteral("source cave.th\n"
                                                  "input ../layouts\n"
                                                  "cs iJTSK\n"
                                                  "select cave.m@cave\n"
                                                  "layout l_plan\n"
                                                  "  copy l_default\n"
                                                  "  cs iJTSK\n"
                                                  "  debug on\n"
                                                  "  map-header 70 100 nw\n"
                                                  "  legend-columns 2\n"
                                                  "  legend-width 25 cm\n"
                                                  "  doc-author \"ZO CSS 6-28 Babicka speleologicka skupina\"\n"
                                                  "  doc-keywords \"Skalky, Babicka plosina, Moravsky kras\"\n"
                                                  "  doc-subject \"Skalky\"\n"
                                                  "  doc-title \"Skalky\"\n"
                                                  "  symbol-hide point label\n"
                                                  "endlayout\n"
                                                  "export map -output out.pdf -layout l_plan\n"));
        pumpEvents();

        const TherionSourceValidationResult configValidation = configTab.validateDocument();
        if (!expect(!validationContainsDiagnosticCode(configValidation, QStringLiteral("unknown-command")),
                    "thconfig validation should accept generated layout setting commands.")) {
            return 1;
        }
        if (!expect(!validationContainsDiagnosticCode(configValidation, QStringLiteral("invalid-command-context")),
                    "thconfig validation should accept real processing-file and layout-setting contexts.")) {
            return 1;
        }
        if (!expect(!validationContainsDiagnosticCode(configValidation, QStringLiteral("invalid-document-type")),
                    "thconfig validation should accept processing commands from the generated document-type catalog.")) {
            return 1;
        }

        const QTextBlock copyLine = configEditor->document()->findBlockByLineNumber(5);
        const QTextBlock debugLine = configEditor->document()->findBlockByLineNumber(7);
        const QTextBlock mapHeaderLine = configEditor->document()->findBlockByLineNumber(8);
        const QTextBlock symbolHideLine = configEditor->document()->findBlockByLineNumber(15);
        if (!expect(!tokenHasWaveUnderline(copyLine, QStringLiteral("copy"))
                        && !tokenHasWaveUnderline(debugLine, QStringLiteral("debug"))
                        && !tokenHasWaveUnderline(mapHeaderLine, QStringLiteral("map-header"))
                        && !tokenHasWaveUnderline(mapHeaderLine, QStringLiteral("70"))
                        && !tokenHasWaveUnderline(symbolHideLine, QStringLiteral("symbol-hide")),
                    "Layout setting commands and free-form layout arguments should not receive inline validation underlines.")) {
            return 1;
        }

        configEditor->setPlainText(QStringLiteral("layout l_plan\n"
                                                  "  map-header 70 100 sideways\n"
                                                  "endlayout\n"));
        pumpEvents();

        const TherionSourceValidationResult invalidLayoutValidation = configTab.validateDocument();
        if (!expect(validationContainsDiagnosticCode(invalidLayoutValidation, QStringLiteral("unknown-argument-value")),
                    "Invalid positional layout enum values should be reported by validation.")) {
            return 1;
        }

        const QTextBlock invalidMapHeaderLine = configEditor->document()->findBlockByLineNumber(1);
        if (!expect(!tokenHasWaveUnderline(invalidMapHeaderLine, QStringLiteral("70"))
                        && tokenHasWaveUnderline(invalidMapHeaderLine, QStringLiteral("sideways")),
                    "Layout setting positional enums should underline the invalid enum token, not earlier numeric arguments.")) {
            return 1;
        }

        configEditor->setPlainText(QStringLiteral("layout l_plan\n"
                                                  "  map-header 70 100 nw bbb\n"
                                                  "endlayout\n"));
        pumpEvents();

        const TherionSourceValidationResult extraLayoutValidation = configTab.validateDocument();
        if (!expect(validationContainsDiagnosticCode(extraLayoutValidation, QStringLiteral("extra-argument")),
                    "Extra positional layout arguments should be reported by validation.")) {
            return 1;
        }

        const QTextBlock extraMapHeaderLine = configEditor->document()->findBlockByLineNumber(1);
        if (!expect(!tokenHasWaveUnderline(extraMapHeaderLine, QStringLiteral("nw"))
                        && tokenHasWaveUnderline(extraMapHeaderLine, QStringLiteral("bbb")),
                    "Extra positional layout arguments should underline only the extra token.")) {
            return 1;
        }
    }

    {
        const QString mapPath = tempDir.path() + QStringLiteral("/completion-highlight-map.th2");
        QFile mapFile(mapPath);
        if (!expect(mapFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create TH2 validation fixture.")) {
            return 1;
        }
        mapFile.write("scrap s1 -projection plan\n"
                      "point 0 0 station -name 1@survey\n"
                      "line wall\n"
                      "  0 0\n"
                      "  1 1\n"
                      "endline\n"
                      "area water\n"
                      "  border1\n"
                      "endarea\n"
                      "endscrap\n"
                      "join o12p1 o12p2\n");
        mapFile.close();

        TextEditorTab mapTab{fileSystem, CommandCatalogStore()};
        QString mapErrorMessage;
        if (!expect(mapTab.loadFile(mapPath, &mapErrorMessage),
                    "Failed to load TH2 validation fixture.")) {
            std::cerr << mapErrorMessage.toStdString() << '\n';
            return 1;
        }
        mapTab.setProjectRootPath(tempDir.path());
        pumpEvents();

        const TherionSourceValidationResult mapValidation = mapTab.validateDocument();
        if (!expect(!validationContainsDiagnosticCode(mapValidation, QStringLiteral("invalid-command-context")),
                    ".th2 validation should accept real map-object contexts such as scrap, point, line, area, and join.")) {
            return 1;
        }
        if (!expect(!validationContainsDiagnosticCode(mapValidation, QStringLiteral("invalid-document-type")),
                    ".th2 validation should accept map-object commands and cross-document join from the generated document-type catalog.")) {
            return 1;
        }
    }

    {
        const QString mapPath = tempDir.path() + QStringLiteral("/highlight-validation-ranges.th2");
        QFile mapFile(mapPath);
        if (!expect(mapFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create TH2 highlight validation fixture.")) {
            return 1;
        }
        mapFile.write("scrap test\n"
                      "line wall -id line-2\n"
                      "  0 0\n"
                      "endline\n"
                      "area water\n"
                      "  line-1\n"
                      "  line-2\n"
                      "endarea\n"
                      "endscrapx\n");
        mapFile.close();

        TextEditorTab mapTab{fileSystem, CommandCatalogStore()};
        QString mapErrorMessage;
        if (!expect(mapTab.loadFile(mapPath, &mapErrorMessage),
                    "Failed to load TH2 highlight validation fixture.")) {
            std::cerr << mapErrorMessage.toStdString() << '\n';
            return 1;
        }
        mapTab.setProjectRootPath(tempDir.path());
        mapTab.show();
        pumpEvents();

        auto *mapEditor = mapTab.findChild<QPlainTextEdit *>();
        if (!expect(mapEditor != nullptr, "Failed to find TH2 highlight validation editor.")) {
            return 1;
        }
        mapEditor->document()->markContentsDirty(0, mapEditor->document()->characterCount());
        pumpEvents();

        const QTextBlock scrapLine = mapEditor->document()->findBlockByLineNumber(0);
        const QTextBlock missingAreaLine = mapEditor->document()->findBlockByLineNumber(5);
        const QTextBlock unknownLine = mapEditor->document()->findBlockByLineNumber(8);
        if (!expect(tokenHasWaveUnderline(scrapLine, QStringLiteral("scrap")),
                    "Unclosed block diagnostics should highlight the opening block directive.")) {
            return 1;
        }
        if (!expect(tokenHasWaveUnderline(missingAreaLine, QStringLiteral("line-1")),
                    "Unknown area line references should highlight the missing line id token.")) {
            return 1;
        }
        if (!expect(tokenHasWaveUnderline(unknownLine, QStringLiteral("endscrapx")),
                    "Unknown command diagnostics should highlight the unknown command token.")) {
            return 1;
        }
    }

    editor->setPlainText(QStringLiteral("survey \nendsurvey\n"));
    pumpEvents();

    {
        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasBoldFormat(firstLine, QStringLiteral("survey")),
                    "Opening directive survey should be highlighted as structural keyword.")) {
            return 1;
        }
    }

    {
        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (completer->popup()->isVisible()) {
            std::cerr << "Unexpected completion list: "
                      << completionTokens(completer).join(QStringLiteral(", ")).toStdString()
                      << '\n';
        }
        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden while required survey id argument is missing.")) {
            return 1;
        }
    }

    const QColor expectedIdColor = expectedIdentifierColor();
    {
        editor->setPlainText(QStringLiteral("survey demo bogus\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool idFound = false;
        bool plainFound = false;
        const QColor idColor = tokenForegroundColor(firstLine, QStringLiteral("demo"), &idFound);
        const QColor plainColor = tokenForegroundColor(firstLine, QStringLiteral("bogus"), &plainFound);
        if (!expect(idFound && plainFound,
                    "Failed to resolve survey id/plain token formatting for syntax highlighting validation.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Survey positional id token should use identifier highlighting color.")) {
            return 1;
        }
        if (!expect(idColor != plainColor,
                    "Survey positional id token should be distinct from plain base-text token color.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -id line-8 -close on\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool idFound = false;
        const QColor idColor = tokenForegroundColor(firstLine, QStringLiteral("line-8"), &idFound);
        if (!expect(idFound, "Failed to resolve -id option value formatting.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Value of -id option should use identifier highlighting color.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\nendsurvey demo\n"));
        pumpEvents();

        const QTextBlock closingLine = editor->document()->findBlockByLineNumber(1);
        bool idFound = false;
        const QColor idColor = tokenForegroundColor(closingLine, QStringLiteral("demo"), &idFound);
        if (!expect(idFound, "Failed to resolve closing directive id formatting.")) {
            return 1;
        }
        if (!expect(idColor == expectedIdColor,
                    "Optional closing directive id should use identifier highlighting color.")) {
            return 1;
        }
    }

    {
        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_Space);
        sendKey(editor, QEvent::KeyPress, Qt::Key_D, Qt::NoModifier, QStringLiteral("d"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_D);
        sendKey(editor, QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, QStringLiteral("e"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_E);
        sendKey(editor, QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, QStringLiteral("m"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_M);
        sendKey(editor, QEvent::KeyPress, Qt::Key_O, Qt::NoModifier, QStringLiteral("o"));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_O);
        sendKey(editor, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        sendKey(editor, QEvent::KeyRelease, Qt::Key_Space);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after required survey id argument is provided.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("-title")),
                    "Survey options should include -title once required id is present.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo -titlle \"x\"\nendsurvey\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("-titlle")),
                    "Unknown option token should be marked with invalid highlighting (wave underline).")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo -title \"x\" \\\n"
                                            "  -bogus value\n"
                                            "endsurvey\n"));
        pumpEvents();

        const QTextBlock continuationLine = editor->document()->findBlockByLineNumber(1);
        if (!expect(tokenHasWaveUnderline(continuationLine, QStringLiteral("-bogus")),
                    "Unknown options on continuation rows should be marked with invalid highlighting on the physical continuation line.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line rock-border -close on -clip off \"-clip off\" \"-clip off\"\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("\"-clip off\"")),
                    "Quoted option/value tokens such as \"-clip off\" should be marked with invalid highlighting.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line label -text \"-clip off\"\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("\"-clip off\"")),
                    "Quoted text values that start with '-' should remain valid option values.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -close maybe\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("maybe")),
                    "Invalid enum value for -close should be marked with invalid highlighting.")) {
            return 1;
        }

        const int maybePos = firstLine.text().indexOf(QStringLiteral("maybe"));
        if (!expect(maybePos >= 0, "Failed to locate invalid enum token for help validation.")) {
            return 1;
        }
        QTextCursor cursor(firstLine);
        cursor.setPosition(firstLine.position() + maybePos + 1);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        const QString tooltipText = QToolTip::text();
        if (!expect(!helpText.contains(QStringLiteral("Unknown option value"), Qt::CaseInsensitive),
                    "Context Help should remain command documentation instead of duplicating validation findings.")) {
            return 1;
        }
        if (!expect(tooltipText.contains(QStringLiteral("Unknown option value"), Qt::CaseInsensitive),
                    "Validation guidance should surface the validator finding in the inline tooltip.")) {
            return 1;
        }
        const bool allowedValuesInTooltip = tooltipText.contains(QStringLiteral("on"), Qt::CaseInsensitive)
            && tooltipText.contains(QStringLiteral("off"), Qt::CaseInsensitive)
            && tooltipText.contains(QStringLiteral("auto"), Qt::CaseInsensitive);
        if (!expect(allowedValuesInTooltip,
                    "Known enum values from the validator should surface in the inline tooltip for invalid option value.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -bogus x -close maybe\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        const int maybePos = firstLine.text().indexOf(QStringLiteral("maybe"));
        if (!expect(maybePos >= 0, "Failed to locate invalid enum token on mixed-problem line.")) {
            return 1;
        }
        QTextCursor cursor(firstLine);
        cursor.setPosition(firstLine.position() + maybePos + 1);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        const QString tooltipText = QToolTip::text();
        if (!expect(!helpText.contains(QStringLiteral("Unknown option value"), Qt::CaseInsensitive),
                    "Context Help should not be replaced by token validation details on mixed-problem lines.")) {
            return 1;
        }
        if (!expect(tooltipText.contains(QStringLiteral("Unknown option value"), Qt::CaseInsensitive),
                    "Validation tooltip should select the diagnostic for the token under the cursor when a line has multiple findings.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("input missing/path.th222\n"));
        TherionSourceDiagnostic diagnostic;
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.code = QStringLiteral("missing-source-reference");
        diagnostic.title = QStringLiteral("Missing referenced source file");
        diagnostic.message = QStringLiteral("Command `input` references `missing/path.th222`, but no matching project file was found.");
        diagnostic.lineNumber = 1;
        diagnostic.columnNumber = 7;
        diagnostic.columnLength = QStringLiteral("missing/path.th222").size();
        diagnostic.currentText = QStringLiteral("input missing/path.th222");
        tab.setProjectValidationDiagnostics({diagnostic});
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.setPosition(firstLine.position() + diagnostic.columnNumber);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString tooltipText = QToolTip::text();
        if (!expect(tooltipText.contains(QStringLiteral("Missing referenced source file"), Qt::CaseInsensitive),
                    "Project validation diagnostics should surface in the inline tooltip.")) {
            return 1;
        }

        tab.setProjectValidationDiagnostics({});
    }

    {
        editor->setPlainText(QStringLiteral("export map -o map.pdf -layout moj\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("-o")),
                    "Known option aliases such as export -o should not be marked with invalid highlighting.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -close on\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(!tokenHasWaveUnderline(firstLine, QStringLiteral("on")),
                    "Valid enum value for -close should not be marked invalid.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -subtype cave\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        if (!expect(tokenHasWaveUnderline(firstLine, QStringLiteral("cave")),
                    "Subtype value incompatible with current line type should be marked invalid.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("line wall -context point \n"));
        pumpEvents();

        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for multi-value -context option after first value.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("line")),
                    "Multi-value -context completion should suggest remaining enum values.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("-close")),
                    "Multi-value value-context completion should not fall back to unrelated option tokens.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("point")),
                    "Multi-value completion should suppress already used values in the same option group.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock commandLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(commandLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear on blank command slot inside centerline block.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("data")),
                    "Centerline command-position completion should include inline command 'data'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("date")),
                    "Centerline command-position completion should include inline command 'date'.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("what"))
                    && !modelContainsToken(completer, QStringLiteral("x"))
                    && !modelContainsToken(completer, QStringLiteral("y"))
                    && !modelContainsToken(completer, QStringLiteral("z"))
                    && !modelContainsToken(completer, QStringLiteral("zero")),
                    "Centerline command-position completion should not include argument-placeholder noise tokens.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  data \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock dataLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(dataLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after 'data ' inside centerline.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("normal")),
                    "Data value completion should include style token 'normal'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("from"))
                    && modelContainsToken(completer, QStringLiteral("to")),
                    "Data value completion should include reading tokens 'from' and 'to'.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("length"))
                    && modelContainsToken(completer, QStringLiteral("compass"))
                    && modelContainsToken(completer, QStringLiteral("clino")),
                    "Data value completion should include reading tokens like length/compass/clino.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("-title")),
                    "Data value completion should not fall back to unrelated option tokens.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  data normal f\nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock dataLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(dataLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for partial data reading token prefix.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("from")),
                    "Data value completion should keep matching reading token 'from' for prefix 'f'.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncenterline\n  infer \nendcenterline\nendsurvey\n"));
        pumpEvents();

        QTextBlock inferLine = editor->document()->findBlockByLineNumber(2);
        QTextCursor cursor(inferLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear after 'infer ' inside centerline.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("on"))
                    && modelContainsToken(completer, QStringLiteral("off")),
                    "Infer value completion should include on/off tokens from centerline metadata.")) {
            return 1;
        }

        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        if (!expect(!helpText.trimmed().isEmpty(),
                    "Contextual help should remain non-empty when cursor is on infer line.")) {
            return 1;
        }
        if (!expect(!helpText.contains(QStringLiteral("Related Keywords"), Qt::CaseInsensitive),
                    "Contextual help should not render Related Keywords section by default.")) {
            return 1;
        }

        const QTextBlock exploDateLine = editor->document()->findBlockByLineNumber(2);
        if (!expect(tokenHasBoldFormat(exploDateLine, QStringLiteral("infer")),
                    "Centerline inline command token should be highlighted as keyword.")) {
            return 1;
        }

        const QTextBlock endCenterlineLine = editor->document()->findBlockByLineNumber(3);
        if (!expect(tokenHasBoldFormat(endCenterlineLine, QStringLiteral("endcenterline")),
                    "Closing directive endcenterline should be highlighted as a structural keyword.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\ncentreline\nendcentreline\nendsurvey\n"));
        pumpEvents();

        const QTextBlock endCentrelineLine = editor->document()->findBlockByLineNumber(2);
        if (!expect(tokenHasBoldFormat(endCentrelineLine, QStringLiteral("endcentreline")),
                    "Alias closing directive endcentreline should be highlighted as structural keyword.")) {
            return 1;
        }

        QTextCursor cursor(endCentrelineLine);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
        editor->setTextCursor(cursor);
        pumpEvents();

        const QString helpText = helpBrowser->toPlainText();
        if (!expect(!helpText.trimmed().isEmpty(),
                    "Contextual help should remain non-empty when cursor is on endcentreline.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("survey demo\nscrap s1 -projection plan\nline wall\nendline\narea water\nendarea\nendscrap\nmap m1\nendmap\nendsurvey\n"));
        pumpEvents();

        const QTextBlock endLineLine = editor->document()->findBlockByLineNumber(3);
        if (!expect(tokenHasBoldFormat(endLineLine, QStringLiteral("endline")),
                    "Closing directive endline should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endAreaLine = editor->document()->findBlockByLineNumber(5);
        if (!expect(tokenHasBoldFormat(endAreaLine, QStringLiteral("endarea")),
                    "Closing directive endarea should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endScrapLine = editor->document()->findBlockByLineNumber(6);
        if (!expect(tokenHasBoldFormat(endScrapLine, QStringLiteral("endscrap")),
                    "Closing directive endscrap should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endMapLine = editor->document()->findBlockByLineNumber(8);
        if (!expect(tokenHasBoldFormat(endMapLine, QStringLiteral("endmap")),
                    "Closing directive endmap should be highlighted as structural keyword.")) {
            return 1;
        }

        const QTextBlock endSurveyLine = editor->document()->findBlockByLineNumber(9);
        if (!expect(tokenHasBoldFormat(endSurveyLine, QStringLiteral("endsurvey")),
                    "Closing directive endsurvey should be highlighted as structural keyword.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("input \n"));
        pumpEvents();

        QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        QTextCursor cursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden for input until ./ or ../ prefix is typed.")) {
            return 1;
        }

        const QString nestedCurrentPath = tempDir.path() + QStringLiteral("/subdir/current.th");
        if (!expect(tab.loadFile(nestedCurrentPath, &errorMessage), "Failed to load nested current file for relative-path completion test.")) {
            std::cerr << errorMessage.toStdString() << '\n';
            return 1;
        }
        tab.setProjectRootPath(tempDir.path());
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(!completer->popup()->isVisible(),
                    "Completion popup should stay hidden in nested file until ./ or ../ prefix is typed.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("input ./\n"));
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for input ./ prefix in nested file.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("child.th2")),
                    "Input completion with ./ prefix should include same-directory targets.")) {
            return 1;
        }
        if (!expect(!modelContainsToken(completer, QStringLiteral("../completion-highlight-test.th")),
                    "Input completion with ./ prefix should not include parent-directory candidates.")) {
            return 1;
        }

        editor->setPlainText(QStringLiteral("input ../\n"));
        pumpEvents();

        firstLine = editor->document()->findBlockByLineNumber(0);
        cursor = QTextCursor(firstLine);
        cursor.movePosition(QTextCursor::EndOfBlock);
        editor->setTextCursor(cursor);
        editor->setFocus(Qt::OtherFocusReason);
        pumpEvents();

        triggerManualCompletion(editor);

        if (!expect(completer->popup()->isVisible(),
                    "Completion popup should appear for input ../ prefix in nested file.")) {
            return 1;
        }
        if (!expect(modelContainsToken(completer, QStringLiteral("../completion-highlight-test.th")),
                    "Input completion with ../ prefix should include parent-directory candidates.")) {
            return 1;
        }
    }

    {
        editor->setPlainText(QStringLiteral("scrap clopy01 -projection plan -author 1985.04.29 \"Hugo Havel\" -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]\n"));
        pumpEvents();

        const QTextBlock firstLine = editor->document()->findBlockByLineNumber(0);
        bool firstFound = false;
        bool referenceFound = false;
        const QColor firstColor = tokenForegroundColor(firstLine, QStringLiteral("-128"), &firstFound);
        const QColor referenceColor = tokenForegroundColor(firstLine, QStringLiteral("851"), &referenceFound);
        if (!expect(firstFound && referenceFound,
                    "Failed to resolve numeric token formatting in -scale array.")) {
            return 1;
        }
        if (!expect(firstColor == referenceColor,
                    "Bracket-attached numeric token in -scale array should be highlighted as number.")) {
            return 1;
        }
    }

    return 0;
}
