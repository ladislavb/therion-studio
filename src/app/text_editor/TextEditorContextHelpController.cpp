#include "TextEditorContextHelpController.h"

#include <QCoreApplication>

#include "ContextHelpController.h"
#include "ContextHelpInspector.h"
#include "TextEditorSourceSnapshotContext.h"
#include "InspectorPanel.h"
#include "TextEditorCommandMetadata.h"
#include "block_editor/BlockEditorDirectiveRules.h"

#include <utility>
#include <QJsonObject>
#include <QPoint>
#include <QRect>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolTip>
#include <QVBoxLayout>

namespace
{
void appendUniqueCaseInsensitive(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

}

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

TextEditorContextHelpController::TextEditorContextHelpController(TextEditorContextHelpContext context,
                                                                 CommandCatalogStore catalogStore)
    : context_(std::move(context))
    , catalogStore_(std::move(catalogStore))
{
}

QString TextEditorContextHelpController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::TextEditorContextHelpController", text);
}

QPlainTextEdit *TextEditorContextHelpController::editor() const
{
    return context_.editor != nullptr ? *context_.editor : nullptr;
}

QSplitter *TextEditorContextHelpController::editorHelpSplitter() const
{
    return context_.editorHelpSplitter != nullptr ? *context_.editorHelpSplitter : nullptr;
}

QWidget *TextEditorContextHelpController::helpPanel() const
{
    return context_.helpPanel != nullptr ? *context_.helpPanel : nullptr;
}

QTextBrowser *TextEditorContextHelpController::helpBrowser() const
{
    return context_.helpBrowser != nullptr ? *context_.helpBrowser : nullptr;
}

void TextEditorContextHelpController::setHelpPanel(QWidget *helpPanel)
{
    if (context_.helpPanel != nullptr) {
        *context_.helpPanel = helpPanel;
    }
}

void TextEditorContextHelpController::setHelpBrowser(QTextBrowser *helpBrowser)
{
    if (context_.helpBrowser != nullptr) {
        *context_.helpBrowser = helpBrowser;
    }
}

void TextEditorContextHelpController::setHelpTitle(const QString &title)
{
    if (helpInspector_ != nullptr) {
        helpInspector_->setTitle(title);
    }
}

void TextEditorContextHelpController::buildHelpPanel()
{
    if (context_.rootWidget == nullptr) {
        return;
    }

    auto *inspectorPanel = context_.createInspectorPanel
        ? context_.createInspectorPanel(context_.rootWidget)
        : new InspectorPanel(context_.rootWidget);
    if (inspectorPanel == nullptr) {
        return;
    }
    setHelpPanel(inspectorPanel);
    helpPanel()->setObjectName(QStringLiteral("textContextHelpPanel"));

    auto *contextTab = inspectorPanel->addScrollTab(tr("Help"));
    auto *contextLayout = qobject_cast<QVBoxLayout *>(contextTab->layout());

    helpInspector_ = new ContextHelpInspector(contextTab, tr("Help"));
    setHelpBrowser(helpInspector_->browser());
    helpBrowser()->setObjectName(QStringLiteral("rawContextHelpBrowser"));
    helpBrowser()->setHtml(
        tr("<p>Select a Therion command or item to see contextual help.</p>"));

    contextLayout->addWidget(helpInspector_, 1);

    if (context_.configureInspectorPanel) {
        context_.configureInspectorPanel(inspectorPanel);
    }
}

void TextEditorContextHelpController::loadHelpMetadata()
{
    if (context_.metadata == nullptr) {
        return;
    }

    (*context_.metadata).clear();
    loadHelpMetadataFromCommandCatalog();
    if (context_.rebuildCompletionModel) {
        context_.rebuildCompletionModel();
    }
    if (context_.populateBlockToolboxScopeCombo) {
        context_.populateBlockToolboxScopeCombo();
    }
    cachedValidationRevision_ = -1;
    cachedValidationResult_ = {};
}

void TextEditorContextHelpController::loadHelpMetadataFromCommandCatalog()
{
    resetCatalogBlockDirectiveMetadataToDefaults();
    cachedValidationRevision_ = -1;
    cachedValidationResult_ = {};

    const QJsonObject catalogObject = catalogStore_.catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }
    applyCatalogBlockDirectiveMetadata(catalogObject);
    if (context_.applyCatalogCommandsMetadata) {
        context_.applyCatalogCommandsMetadata(catalogObject);
    }
}

void TextEditorContextHelpController::setHelpCollapsed(bool collapsed)
{
    if (context_.helpCollapsed == nullptr || context_.helpPanelExtent == nullptr) {
        return;
    }

    (*context_.helpCollapsed) = collapsed;
    if (helpBrowser() != nullptr) {
        helpBrowser()->setVisible(!collapsed);
    }
    if (editorHelpSplitter() != nullptr) {
        if (!collapsed && (*context_.helpPanelExtent) > 0) {
            const QList<int> sizes = editorHelpSplitter()->sizes();
            editorHelpSplitter()->setSizes(QList<int>{sizes.value(0, 1), (*context_.helpPanelExtent)});
        } else if (collapsed) {
            const QList<int> sizes = editorHelpSplitter()->sizes();
            if (sizes.size() >= 2) {
                const int minimumExtent = helpPanel() != nullptr
                    ? qMax(helpPanel()->minimumSizeHint().width(), helpPanel()->minimumWidth())
                    : 1;
                (*context_.helpPanelExtent) = qMax(sizes.at(1), minimumExtent);
            }
            editorHelpSplitter()->setSizes(QList<int>{1, 0});
        }
    }
}

void TextEditorContextHelpController::updateContextHelp()
{
    if (helpBrowser() == nullptr || context_.metadata == nullptr) {
        return;
    }

    const QStringList candidates = helpCandidateTokens();
    for (const QString &candidate : candidates) {
        const auto entryIt = (*context_.metadata).helpEntries.constFind(candidate.toLower());
        if (entryIt == (*context_.metadata).helpEntries.constEnd()) {
            continue;
        }

        const TherionHelpEntry &entry = entryIt.value();
        setHelpTitle(candidate);
        helpBrowser()->setHtml(ContextHelpController::renderHelpHtml(candidate,
                                                                             entry.summary,
                                                                             entry.syntax,
                                                                             entry.arguments,
                                                                             entry.acceptedValues,
                                                                             entry.options,
                                                                             true,
                                                                             false));
        return;
    }

    setHelpTitle(tr("Help"));
    helpBrowser()->setHtml(tr("<p>No contextual help is available for the current token.</p>"));
}

void TextEditorContextHelpController::updateValidationTooltipForCursor()
{
    if (editor() == nullptr || context_.lastValidationTooltipKey == nullptr) {
        return;
    }

    QString tooltipText;
    QString tooltipKey;
    const QString validationHtml = validationHelpHtmlForCursor(&tooltipText, &tooltipKey);
    if (validationHtml.isEmpty() || tooltipText.trimmed().isEmpty()) {
        editor()->setToolTip(QString());
        (*context_.lastValidationTooltipKey).clear();
        return;
    }

    if (tooltipKey == (*context_.lastValidationTooltipKey)) {
        return;
    }

    (*context_.lastValidationTooltipKey) = tooltipKey;
    QToolTip::showText(editor()->mapToGlobal(editor()->cursorRect().bottomLeft()),
                       tooltipText,
                       editor(),
                       QRect(),
                       2200);
}

void TextEditorContextHelpController::invalidateValidationCache()
{
    cachedValidationRevision_ = -1;
    cachedValidationResult_ = {};
}

bool TextEditorContextHelpController::showValidationTooltipForPosition(const QPoint &viewportPosition,
                                                                       const QPoint &globalPosition)
{
    if (editor() == nullptr) {
        return false;
    }

    QString tooltipText;
    const QTextCursor hoverCursor = editor()->cursorForPosition(viewportPosition);
    const QString validationHtml = validationHelpHtmlForTextCursor(hoverCursor, &tooltipText);
    if (validationHtml.isEmpty() || tooltipText.trimmed().isEmpty()) {
        QToolTip::hideText();
        return false;
    }

    QToolTip::showText(globalPosition, tooltipText, editor()->viewport(), QRect(), 3200);
    return true;
}

QStringList TextEditorContextHelpController::helpCandidateTokens() const
{
    if (editor() == nullptr) {
        return {};
    }

    QStringList candidates;
    const QString directToken = currentHelpTokenForCursor();
    if (!directToken.isEmpty()) {
        appendUniqueCaseInsensitive(candidates, directToken);
    }

    const QTextCursor cursor = editor()->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return candidates;
    }

    const TherionSourceLogicalDocument &logicalDocument = logicalDocumentForEditor();
    const int cursorOffset = cursor.position();
    const TherionSourceLogicalCommand *command = logicalDocument.commandAtOffset(cursorOffset);
    if (command == nullptr && cursorOffset > 0) {
        const TherionSourceLogicalCommand *previousCommand = logicalDocument.commandAtOffset(cursorOffset - 1);
        if (previousCommand != nullptr && previousCommand->endOffset == cursorOffset) {
            command = previousCommand;
        }
    }
    if (command != nullptr && !command->parsed.directive.isEmpty() && command->parsed.directive != directToken.toLower()) {
        appendUniqueCaseInsensitive(candidates, command->parsed.directive);
        if (context_.normalizedDirectiveToken && context_.openingDirectiveForClosingToken) {
            const QString normalizedDirective = context_.normalizedDirectiveToken(command->parsed.directive);
            const QString openingDirective = context_.openingDirectiveForClosingToken(normalizedDirective);
            if (!openingDirective.isEmpty()) {
                appendUniqueCaseInsensitive(candidates, openingDirective);
            }
        }
    }

    if (context_.currentCompletionCommand) {
        const QString resolvedCommand = context_.currentCompletionCommand();
        if (!resolvedCommand.isEmpty()) {
            appendUniqueCaseInsensitive(candidates, resolvedCommand);
        }
    }

    return candidates;
}

QString TextEditorContextHelpController::currentHelpTokenForCursor() const
{
    if (editor() == nullptr) {
        return QString();
    }

    const QTextCursor cursor = editor()->textCursor();
    if (!cursor.block().isValid()) {
        return QString();
    }

    const TherionStudio::TextEditorSourceSnapshotContext snapshotContext =
        TherionStudio::TextEditorSourceSnapshotContext::fromEditor(editor());
    const TherionSourceLogicalDocument &logicalDocument = snapshotContext.logicalDocument(sourceSnapshotCache_);
    const int cursorOffset = cursor.position();
    const TherionSourceLogicalTokenRange *tokenRange = logicalDocument.tokenAtOffset(cursorOffset);
    if (tokenRange == nullptr && cursorOffset > 0) {
        const TherionSourceLogicalTokenRange *previousTokenRange = logicalDocument.tokenAtOffset(cursorOffset - 1);
        if (previousTokenRange != nullptr
            && previousTokenRange->physicalRange.startOffset + previousTokenRange->physicalRange.length == cursorOffset) {
            tokenRange = previousTokenRange;
        }
    }
    if (tokenRange != nullptr) {
        return tokenRange->text.toLower();
    }

    return QString();
}

QString TextEditorContextHelpController::validationHelpHtmlForCursor(QString *tooltipText, QString *tooltipKey) const
{
    if (editor() == nullptr) {
        return QString();
    }
    return validationHelpHtmlForTextCursor(editor()->textCursor(), tooltipText, tooltipKey);
}

QString TextEditorContextHelpController::validationHelpHtmlForTextCursor(const QTextCursor &cursor,
                                                                         QString *tooltipText,
                                                                         QString *tooltipKey) const
{
    const TherionSourceDiagnostic *diagnostic = validationDiagnosticForTextCursor(cursor);
    if (diagnostic == nullptr) {
        return QString();
    }

    if (tooltipText != nullptr) {
        *tooltipText = validationDiagnosticTooltip(*diagnostic);
    }
    if (tooltipKey != nullptr) {
        *tooltipKey = QStringLiteral("%1|%2|%3|%4|%5")
            .arg(QString::number(editor() != nullptr && editor()->document() != nullptr
                                     ? editor()->document()->revision()
                                     : -1),
                 QString::number(diagnostic->lineNumber),
                 QString::number(diagnostic->columnNumber),
                 diagnostic->code,
                 diagnostic->title);
    }

    return validationDiagnosticHtml(*diagnostic);
}

const TherionSourceDiagnostic *TextEditorContextHelpController::validationDiagnosticForTextCursor(const QTextCursor &cursor) const
{
    if (!cursor.block().isValid()) {
        return nullptr;
    }

    const int lineNumber = cursor.blockNumber() + 1;
    const int columnNumber = cursor.positionInBlock() + 1;
    const TherionSourceValidationResult &validation = cachedValidationResult();
    const TherionSourceDiagnostic *lineFallback = nullptr;
    for (const TherionSourceDiagnostic &diagnostic : validation.diagnostics) {
        if (diagnostic.lineNumber != lineNumber) {
            continue;
        }

        if (diagnostic.columnLength <= 0) {
            if (lineFallback == nullptr) {
                lineFallback = &diagnostic;
            }
            continue;
        }

        const int startColumn = qMax(1, diagnostic.columnNumber);
        const int endColumn = startColumn + diagnostic.columnLength;
        if (columnNumber >= startColumn && columnNumber <= endColumn) {
            return &diagnostic;
        }
    }
    return lineFallback;
}

const TherionSourceValidationResult &TextEditorContextHelpController::cachedValidationResult() const
{
    if (editor() == nullptr || editor()->document() == nullptr || !context_.validateDocument) {
        static const TherionSourceValidationResult emptyResult;
        return emptyResult;
    }

    const int revision = editor()->document()->revision();
    if (revision != cachedValidationRevision_) {
        cachedValidationResult_ = context_.validateDocument();
        cachedValidationRevision_ = revision;
    }
    return cachedValidationResult_;
}

QString TextEditorContextHelpController::validationDiagnosticHtml(const TherionSourceDiagnostic &diagnostic) const
{
    const QString severity = diagnostic.severity == TherionSourceDiagnosticSeverity::Error
        ? tr("Error")
        : tr("Warning");
    QString html;
    html += QStringLiteral("<p><strong>%1:</strong> %2</p>")
                .arg(severity.toHtmlEscaped(), diagnostic.title.toHtmlEscaped());
    if (!diagnostic.message.isEmpty()) {
        html += QStringLiteral("<p>%1</p>").arg(diagnostic.message.toHtmlEscaped());
    }
    if (!diagnostic.currentText.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4><pre>%2</pre>")
                    .arg(tr("Current Source").toHtmlEscaped(), diagnostic.currentText.toHtmlEscaped());
    }
    if (!diagnostic.hasFix) {
        return html;
    }

    html += QStringLiteral("<h4>%1</h4><pre>%2</pre>")
                .arg(tr("Suggested Source").toHtmlEscaped(), diagnostic.suggestedText.toHtmlEscaped());
    return html;
}

QString TextEditorContextHelpController::validationDiagnosticTooltip(const TherionSourceDiagnostic &diagnostic) const
{
    const QString severity = diagnostic.severity == TherionSourceDiagnosticSeverity::Error
        ? tr("Error")
        : tr("Warning");
    QString tooltip = tr("Line %1: %2: %3")
                          .arg(diagnostic.lineNumber)
                          .arg(severity, diagnostic.title);
    if (!diagnostic.message.isEmpty()) {
        tooltip += QStringLiteral("\n") + diagnostic.message;
    }
    return tooltip;
}

const TherionSourceLogicalDocument &TextEditorContextHelpController::logicalDocumentForEditor() const
{
    static const QString emptyDocument;

    if (editor() == nullptr || editor()->document() == nullptr) {
        return sourceSnapshotCache_.logicalDocument(emptyDocument);
    }

    const TherionStudio::TextEditorSourceSnapshotContext snapshotContext =
        TherionStudio::TextEditorSourceSnapshotContext::fromEditor(editor());
    return snapshotContext.logicalDocument(sourceSnapshotCache_);
}
}
