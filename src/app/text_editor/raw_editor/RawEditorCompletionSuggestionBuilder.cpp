#include "RawEditorCompletionSuggestionBuilder.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCompletionTokenContext.h"
#include "../TextEditorCommandMetadata.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionFileTypes.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../TextEditorSourceSnapshotContext.h"

#include <algorithm>
#include <utility>

namespace
{
void appendUnique(QStringList &target, const QString &value)
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

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}

bool isActivatedInputPathPrefix(const QString &rawPrefix)
{
    const QString normalized = QDir::fromNativeSeparators(rawPrefix).trimmed();
    return normalized.startsWith(QStringLiteral("./")) || normalized.startsWith(QStringLiteral("../"));
}

QString normalizeInputSuggestionPath(QString path)
{
    path = QDir::fromNativeSeparators(path).trimmed();
    if (path.isEmpty() || path == QStringLiteral(".")) {
        return QString();
    }
    while (path.startsWith(QStringLiteral("./"))) {
        path.remove(0, 2);
        path = path.trimmed();
    }
    return path;
}

QString catalogDocumentTypeTokenForFilePath(const QString &filePath)
{
    return TherionStudio::therionSourceDocumentTypeCatalogToken(
        TherionStudio::therionSourceDocumentTypeForFilePath(filePath));
}

TherionStudio::RawEditorCompletionTokenContext cursorTokenContextForEditor(
    const QPlainTextEdit *editor,
    TherionStudio::TherionSourceSnapshotCache *sourceSnapshotCache)
{
    TherionStudio::RawEditorCompletionTokenContext context;
    if (editor == nullptr) {
        return context;
    }

    const QTextCursor cursor = editor->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return context;
    }

    const int cursorOffset = cursor.position();
    const TherionStudio::TextEditorSourceSnapshotContext snapshotContext =
        TherionStudio::TextEditorSourceSnapshotContext::fromEditor(editor);
    return snapshotContext.withLogicalDocument(sourceSnapshotCache,
                                               [cursorOffset](const TherionStudio::TherionSourceLogicalDocument &logicalDocument) {
                                                   return TherionStudio::rawEditorCompletionTokenContextAtOffset(logicalDocument,
                                                                                                                cursorOffset);
                                               });
}

bool commandAllowedForDocumentType(const TherionStudio::TextEditorCommandMetadata &metadata,
                                   const QString &commandToken,
                                   const QString &documentTypeToken)
{
    const QString normalizedCommand = commandToken.trimmed().toLower();
    if (normalizedCommand.isEmpty() || documentTypeToken.isEmpty()) {
        return true;
    }

    const QStringList documentTypes = metadata.commandDocumentTypeTokens.value(normalizedCommand);
    return documentTypes.isEmpty()
        || documentTypes.contains(QStringLiteral("all"), Qt::CaseInsensitive)
        || documentTypes.contains(documentTypeToken, Qt::CaseInsensitive);
}

QStringList filterCommandsForDocumentType(const QStringList &candidates,
                                          const TherionStudio::TextEditorCommandMetadata &metadata,
                                          const QString &documentTypeToken)
{
    if (documentTypeToken.isEmpty()) {
        return candidates;
    }

    QStringList filtered;
    for (const QString &candidate : candidates) {
        if (commandAllowedForDocumentType(metadata, candidate, documentTypeToken)) {
            appendUnique(filtered, candidate);
        }
    }
    return filtered;
}
}

namespace TherionStudio
{
RawEditorCompletionSuggestionBuilder::RawEditorCompletionSuggestionBuilder(RawEditorCompletionSuggestionContext context)
    : context_(std::move(context))
{
}

const TextEditorCommandMetadata &RawEditorCompletionSuggestionBuilder::metadata() const
{
    return *context_.metadata;
}

RawEditorCompletionContext RawEditorCompletionSuggestionBuilder::completionContext() const
{
    RawEditorCompletionContext context;
    context.editor = context_.editor;
    context.metadata = context_.metadata;
    context.sourceSnapshotCache = context_.sourceSnapshotCache;
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    return context;
}

QString RawEditorCompletionSuggestionBuilder::normalizeInputCompletionPrefix(QString prefix) const
{
    prefix = QDir::fromNativeSeparators(prefix).trimmed();
    if (prefix.isEmpty()) {
        return prefix;
    }

    while (prefix.startsWith(QStringLiteral("./"))) {
        prefix.remove(0, 2);
    }
    return prefix;
}

QStringList RawEditorCompletionSuggestionBuilder::projectInputFileCompletionCandidates() const
{
    if (context_.filePath.trimmed().isEmpty() && context_.projectRootPath.trimmed().isEmpty()) {
        return {};
    }

    QString rootPath = context_.projectRootPath.trimmed();
    if (rootPath.isEmpty()) {
        if (context_.filePath.isEmpty()) {
            return {};
        }
        rootPath = QFileInfo(context_.filePath).absolutePath();
    }

    QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        return {};
    }

    QString rootBasePath = QFileInfo(rootDir.absolutePath()).canonicalFilePath();
    if (rootBasePath.isEmpty()) {
        rootBasePath = rootDir.absolutePath();
    }
    QDir rootBaseDir(rootBasePath);

    QString baseDirPath = rootBasePath;
    if (!context_.filePath.isEmpty()) {
        const QFileInfo currentFileInfo(context_.filePath);
        QString candidateBasePath = currentFileInfo.absolutePath();
        const QString canonicalCandidateBasePath = QFileInfo(candidateBasePath).canonicalFilePath();
        if (!canonicalCandidateBasePath.isEmpty()) {
            candidateBasePath = canonicalCandidateBasePath;
        }
        if (QDir(candidateBasePath).exists()) {
            baseDirPath = candidateBasePath;
        }
    }

    QDir baseDir(baseDirPath);

    QStringList candidates;
    QDirIterator iterator(rootBaseDir.absolutePath(),
                          QDir::Files | QDir::Readable | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString absolutePath = iterator.next();
        const QFileInfo fileInfo(absolutePath);
        const QString suffix = fileInfo.suffix().trimmed().toLower();
        if (suffix != QStringLiteral("th") && suffix != QStringLiteral("th2")) {
            continue;
        }

        QString candidatePath = fileInfo.absoluteFilePath();
        const QString canonicalCandidatePath = fileInfo.canonicalFilePath();
        if (!canonicalCandidatePath.isEmpty()) {
            candidatePath = canonicalCandidatePath;
        }

        const QString projectRelativePath = QDir::fromNativeSeparators(rootBaseDir.relativeFilePath(candidatePath)).trimmed();
        if (projectRelativePath.isEmpty() || projectRelativePath.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const QString relativePath = normalizeInputSuggestionPath(baseDir.relativeFilePath(candidatePath));
        if (relativePath.isEmpty()) {
            continue;
        }
        appendUnique(candidates, relativePath);
    }

    return candidates;
}

QStringList RawEditorCompletionSuggestionBuilder::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (context_.editor == nullptr || context_.metadata == nullptr) {
        return QStringList();
    }

    const QTextCursor cursor = context_.editor->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QStringList();
    }

    const RawEditorCompletionTokenContext cursorTokenContext =
        cursorTokenContextForEditor(context_.editor, context_.sourceSnapshotCache);
    const TherionParsedLine parsedLine = cursorTokenContext.parsedLine;
    const int column = cursor.positionInBlock();
    const int tokenIndexAtCursor = cursorTokenContext.tokenIndexAtCursor;
    const bool cursorInsideToken = cursorTokenContext.cursorInsideToken;

    const QString currentToken = cursorInsideToken && tokenIndexAtCursor < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor)
        : QString();
    const QString previousToken = tokenIndexAtCursor > 0 && tokenIndexAtCursor - 1 < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor - 1)
        : QString();
    QString effectivePrefix = !prefix.isEmpty()
        ? prefix
        : (cursorInsideToken ? currentToken.trimmed() : QString());

    const RawEditorCompletionContextAnalyzer contextAnalyzer(completionContext());
    const QString command = contextAnalyzer.currentCompletionCommand();
    int tokenStart = column;
    int tokenEnd = column;
    const QString blockText = block.text();
    while (tokenStart > 0 && !blockText.at(tokenStart - 1).isSpace()) {
        --tokenStart;
    }
    while (tokenEnd < blockText.length() && !blockText.at(tokenEnd).isSpace()) {
        ++tokenEnd;
    }
    const QString rawTokenAtCursor = blockText.mid(tokenStart, tokenEnd - tokenStart).trimmed();

    if (command == QStringLiteral("input") && cursorInsideToken) {
        const QString tokenPrefix = currentToken.trimmed();
        if (!tokenPrefix.isEmpty()) {
            effectivePrefix = tokenPrefix;
        }
    }
    QStringList candidates;
    bool allowGlobalFallback = true;
    bool inputFileContext = false;
    const QString documentTypeToken = catalogDocumentTypeTokenForFilePath(context_.filePath);
    struct ActiveValueContext
    {
        bool active = false;
        QString optionToken;
        QString arity;
        int optionIndex = -1;
        int nextOptionIndex = -1;
    };
    ActiveValueContext activeValueContext;

    if (tokenIndexAtCursor <= 0 && !effectivePrefix.startsWith(QLatin1Char('-'))) {
        const QStringList scopeStack = contextAnalyzer.activeCompletionScopeStack();
        const QString activeScope = scopeStack.isEmpty() ? QStringLiteral("none") : scopeStack.constLast();
        const bool strictScopedCommandContext = !scopeStack.isEmpty();

        if (scopeStack.isEmpty()) {
            appendUniqueList(candidates, metadata().contextCommandTokens.value(QStringLiteral("none")));
            appendUniqueList(candidates, metadata().contextCommandTokens.value(QStringLiteral("all")));
        } else {
            appendUniqueList(candidates, metadata().contextCommandTokens.value(activeScope));
            appendUniqueList(candidates, metadata().contextCommandTokens.value(QStringLiteral("all")));
        }

        if (candidates.isEmpty() && !strictScopedCommandContext) {
            candidates = metadata().commandCompletionTokens;
        }
        candidates = filterCommandsForDocumentType(candidates, metadata(), documentTypeToken);
    } else if (!command.isEmpty()) {
        const bool optionContext = currentToken.startsWith(QLatin1Char('-'))
            || effectivePrefix.startsWith(QLatin1Char('-'))
            || (previousToken.startsWith(QLatin1Char('-')) && effectivePrefix.startsWith(QLatin1Char('-')));
        if (!optionContext) {
            const QString normalizedPreviousToken = previousToken.trimmed().toLower();
            if (normalizedPreviousToken.startsWith(QLatin1Char('-'))) {
                activeValueContext.active = true;
                activeValueContext.optionToken = normalizedPreviousToken;
                activeValueContext.arity = metadata().commandOptionValueArityTokens
                    .value(commandOptionValueKey(command, activeValueContext.optionToken))
                    .trimmed()
                    .toUpper();
            } else {
                const int scanTokenIndex = cursorInsideToken ? tokenIndexAtCursor : tokenIndexAtCursor - 1;
                if (scanTokenIndex >= 1 && scanTokenIndex < parsedLine.tokens.size()) {
                    int optionIndex = -1;
                    for (int index = scanTokenIndex; index >= 1; --index) {
                        const QString token = parsedLine.tokens.at(index).trimmed().toLower();
                        if (token.startsWith(QLatin1Char('-'))) {
                            optionIndex = index;
                            break;
                        }
                    }

                    if (optionIndex >= 1) {
                        int nextOptionIndex = parsedLine.tokens.size();
                        for (int index = optionIndex + 1; index < parsedLine.tokens.size(); ++index) {
                            const QString token = parsedLine.tokens.at(index).trimmed();
                            if (token.startsWith(QLatin1Char('-'))) {
                                nextOptionIndex = index;
                                break;
                            }
                        }

                        const QString optionToken = parsedLine.tokens.at(optionIndex).trimmed().toLower();
                        const QString arity = metadata().commandOptionValueArityTokens
                            .value(commandOptionValueKey(command, optionToken))
                            .trimmed()
                            .toUpper();

                        const bool cursorWithinValueRange = tokenIndexAtCursor > optionIndex
                            && tokenIndexAtCursor <= nextOptionIndex
                            && !(cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-')));
                        if (cursorWithinValueRange) {
                            const int valueOrdinal = tokenIndexAtCursor - optionIndex;
                            const QString normalizedArity = canonicalOptionArityToken(arity);
                            const bool multiValueOption = normalizedArity == QStringLiteral("ONE_OR_MORE")
                                || normalizedArity == QStringLiteral("ZERO_OR_MORE");
                            const bool singleValueOption = normalizedArity == QStringLiteral("EXACTLY_ONE");
                            if (multiValueOption || (singleValueOption && valueOrdinal == 1)) {
                                activeValueContext.active = true;
                                activeValueContext.optionToken = optionToken;
                                activeValueContext.arity = arity;
                                activeValueContext.optionIndex = optionIndex;
                                activeValueContext.nextOptionIndex = nextOptionIndex;
                            }
                        }
                    }
                }
            }
        }
        const bool valueContext = activeValueContext.active;
        inputFileContext = command == QStringLiteral("input") && !optionContext;

        if (inputFileContext) {
            allowGlobalFallback = false;
            if (isActivatedInputPathPrefix(rawTokenAtCursor)) {
                candidates = projectInputFileCompletionCandidates();
            }
        } else if (optionContext) {
            allowGlobalFallback = false;
            candidates = metadata().commandOptionTokens.value(command);
            QSet<QString> usedOptionTokens;
            for (const QString &lineToken : parsedLine.tokens) {
                const QString normalizedLineToken = lineToken.trimmed().toLower();
                if (normalizedLineToken.startsWith(QLatin1Char('-'))) {
                    usedOptionTokens.insert(normalizedLineToken);
                }
            }

            const QString activeOptionToken = cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-'))
                ? currentToken.trimmed().toLower()
                : QString();
            QStringList optionCandidates;
            for (const QString &candidate : candidates) {
                if (!candidate.startsWith(QLatin1Char('-'))) {
                    continue;
                }

                const QString normalizedCandidate = candidate.trimmed().toLower();
                if (usedOptionTokens.contains(normalizedCandidate) && normalizedCandidate != activeOptionToken) {
                    continue;
                }

                if (candidate.startsWith(QLatin1Char('-'))) {
                    appendUnique(optionCandidates, candidate);
                }
            }
            candidates = optionCandidates;
        } else if (valueContext) {
            allowGlobalFallback = false;
            const QString valueOptionToken = activeValueContext.optionToken;
            if (valueOptionToken == QStringLiteral("-subtype")) {
                const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
                const QHash<QString, QStringList> subtypeByType = metadata().commandSubtypeByTypeTokens.value(command);

                if (!symbolTypeToken.isEmpty()) {
                    appendConcreteSubtypeValues(candidates, subtypeByType.value(symbolTypeToken));
                }

                if (candidates.isEmpty()) {
                    for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
                        appendConcreteSubtypeValues(candidates, subtypeIterator.value());
                    }
                }
            }

            if (candidates.isEmpty()) {
                candidates = metadata().commandOptionValueTokens.value(commandOptionValueKey(command, valueOptionToken));
            }

            if (canonicalOptionArityToken(activeValueContext.arity) == QStringLiteral("ONE_OR_MORE")
                && activeValueContext.optionIndex >= 0
                && !candidates.isEmpty()) {
                QSet<QString> usedValues;
                for (int index = activeValueContext.optionIndex + 1; index < activeValueContext.nextOptionIndex; ++index) {
                    if (cursorInsideToken && index == tokenIndexAtCursor) {
                        continue;
                    }
                    usedValues.insert(parsedLine.tokens.at(index).trimmed().toLower());
                }

                QStringList filteredCandidates;
                for (const QString &candidate : candidates) {
                    if (!usedValues.contains(candidate.trimmed().toLower())) {
                        appendUnique(filteredCandidates, candidate);
                    }
                }
                candidates = filteredCandidates;
            }
        } else {
            const int requiredPositionalCount = qMax(0, metadata().commandRequiredPositionalCount.value(command));
            const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
            if (requiredPositionalCount > 0 && positionalCountBeforeCursor < requiredPositionalCount) {
                allowGlobalFallback = false;
                candidates = metadata().commandValueTokens.value(command);
            } else {
                candidates = metadata().commandValueTokens.value(command);
                appendUniqueList(candidates, metadata().commandOptionTokens.value(command));
            }
        }
    }

    if (candidates.isEmpty() && allowGlobalFallback) {
        candidates = metadata().completionTokens;
        if (tokenIndexAtCursor <= 0 && !effectivePrefix.startsWith(QLatin1Char('-'))) {
            candidates = filterCommandsForDocumentType(candidates, metadata(), documentTypeToken);
        }
    }

    QStringList filtered;
    QString inputPathPrefix;
    QString normalizedInputPathPrefix;
    if (inputFileContext) {
        const QString rawInputPrefix = rawTokenAtCursor;
        inputPathPrefix = QDir::fromNativeSeparators(rawInputPrefix);
        normalizedInputPathPrefix = normalizeInputCompletionPrefix(rawInputPrefix);
    }
    const bool sameDirectoryHint = inputFileContext && inputPathPrefix.startsWith(QStringLiteral("./"));
    const bool treatAsEmptyInputPrefix = inputFileContext
        && !inputPathPrefix.isEmpty()
        && normalizedInputPathPrefix.isEmpty();
    for (const QString &candidate : candidates) {
        if (inputFileContext && sameDirectoryHint && candidate.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const bool matchesDefaultPrefix = treatAsEmptyInputPrefix
            || effectivePrefix.isEmpty()
            || candidate.startsWith(effectivePrefix, Qt::CaseInsensitive);
        const bool matchesNormalizedInputPrefix = inputFileContext
            && !normalizedInputPathPrefix.isEmpty()
            && candidate.startsWith(normalizedInputPathPrefix, Qt::CaseInsensitive);
        if (matchesDefaultPrefix || matchesNormalizedInputPrefix) {
            appendUnique(filtered, candidate);
        }
    }

    std::sort(filtered.begin(),
              filtered.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });

    return filtered;
}
}
