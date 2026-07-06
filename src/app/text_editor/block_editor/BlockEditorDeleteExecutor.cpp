#include "BlockEditorDeleteExecutor.h"

#include <QCoreApplication>

#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"

#include <QMessageBox>
#include <QPlainTextEdit>

#include <utility>

#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceDocument.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;
}

namespace TherionStudio
{
BlockEditorDeleteExecutor::BlockEditorDeleteExecutor(BlockEditorDeleteContext context)
    : context_(std::move(context))
{
}

QString BlockEditorDeleteExecutor::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorDeleteExecutor", text);
}

bool BlockEditorDeleteExecutor::deleteCommandAtLine(int lineNumber)
{
    if (context_.sourceContext == nullptr || lineNumber <= 0) {
        return false;
    }

    const BlockEditorSourceController source(context_.sourceContext());
    if (!source.hasEditor()) {
        return false;
    }

    QStringList lines = source.normalizedLines();
    if (lineNumber > lines.size()) {
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(lines.join(QLatin1Char('\n')));
    const auto parsedLineForLine = [&sourceDocument](int currentLine) -> TherionParsedLine {
        if (const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(currentLine);
            sourceLine != nullptr) {
            return sourceLine->sourceLine.parsed;
        }
        return TherionParsedLine{};
    };

    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, lineNumber, &logicalLine)) {
        return false;
    }

    const TherionParsedLine parsedLine = parsedLineForLine(logicalLine.startLine);
    const QString directive = normalizeDirective(parsedLine.directive);
    if (directive.isEmpty() && isFullLineComment(parsedLine)) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            context_.dialogParent,
            tr("Delete Block"),
            tr("Delete comment line?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return source.removeLineRange(logicalLine.startLine, logicalLine.endLine);
    }
    if (directive.isEmpty()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            context_.dialogParent,
            tr("Delete Block"),
            tr("Delete unrecognized line?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return source.removeLineRange(logicalLine.startLine, logicalLine.endLine);
    }
    if (directive == QStringLiteral("encoding")) {
        QMessageBox::information(context_.dialogParent,
                                 tr("Delete Block"),
                                 tr("`encoding` is fixed as the document root in Blocks mode and cannot be deleted."));
        return false;
    }

    int removeStartLine = logicalLine.startLine;
    int removeEndLine = logicalLine.endLine;

    if (isContainerDirectiveInstance(directive, parsedLine)) {
        const QString closingDirective = closingDirectiveFor(directive);
        const int endLine = findMatchingBlockEndLine(lines, logicalLine.startLine, directive, closingDirective);
        if (endLine <= logicalLine.startLine) {
            QMessageBox::warning(context_.dialogParent,
                                 tr("Delete Block"),
                                 tr("Unable to resolve closing directive for `%1`.").arg(directive));
            return false;
        }
        removeEndLine = endLine;
    } else if (directive == QStringLiteral("data")) {
        const QString dataScope = context_.resolveScopeForCommandAtLine != nullptr
            ? context_.resolveScopeForCommandAtLine(QStringLiteral("data"), lines, logicalLine.startLine)
            : QString();
        const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
        if (dataScopeClosing.isEmpty()) {
            QMessageBox::warning(context_.dialogParent, tr("Delete Block"), tr("Unable to resolve parent data scope."));
            return false;
        }

        int dataScopeStartLine = -1;
        int dataScopeDepth = 0;
        for (int currentLine = logicalLine.startLine; currentLine >= 1; --currentLine) {
            const TherionParsedLine currentParsedLine = parsedLineForLine(currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective == dataScopeClosing) {
                ++dataScopeDepth;
                continue;
            }
            if (currentDirective != dataScope) {
                continue;
            }
            if (dataScopeDepth == 0) {
                dataScopeStartLine = currentLine;
                break;
            }
            --dataScopeDepth;
        }

        if (dataScopeStartLine <= 0) {
            QMessageBox::warning(context_.dialogParent, tr("Delete Block"), tr("Unable to resolve parent data scope block."));
            return false;
        }

        const int dataScopeEndLine = findMatchingBlockEndLine(lines,
                                                              dataScopeStartLine,
                                                              dataScope,
                                                              dataScopeClosing);
        if (dataScopeEndLine <= logicalLine.startLine) {
            QMessageBox::warning(context_.dialogParent, tr("Delete Block"), tr("Unable to resolve end of parent data scope block."));
            return false;
        }

        int dataBodyLastLine = dataScopeEndLine - 1;
        for (int currentLine = logicalLine.endLine + 1; currentLine <= dataScopeEndLine - 1; ++currentLine) {
            const TherionParsedLine currentParsedLine = parsedLineForLine(currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective.isEmpty() || currentDirective == QStringLiteral("extend")) {
                continue;
            }
            if (currentDirective == QStringLiteral("data")
                || currentDirective == dataScopeClosing
                || (!dataScope.isEmpty()
                    && context_.isCommandDirectiveInScope != nullptr
                    && context_.isCommandDirectiveInScope(currentDirective, dataScope))) {
                dataBodyLastLine = currentLine - 1;
                break;
            }
        }
        removeEndLine = qMax(logicalLine.endLine, dataBodyLastLine);
    }

    const QString label = directive == QStringLiteral("data")
        ? tr("data block")
        : tr("`%1` block").arg(directive);
    const int lineCount = qMax(1, removeEndLine - removeStartLine + 1);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        context_.dialogParent,
        tr("Delete Block"),
        tr("Delete %1 (%2 line(s))?").arg(label).arg(lineCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return false;
    }

    return source.removeLineRange(removeStartLine, removeEndLine);
}
}
