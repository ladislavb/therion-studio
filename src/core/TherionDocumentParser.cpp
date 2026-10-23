#include "TherionDocumentParser.h"

#include "TherionTokenRules.h"

namespace TherionStudio
{
bool TherionParsedSourceLine::hasTokens() const
{
    return !parsed.tokens.isEmpty();
}

bool TherionParsedSourceLine::isBlank() const
{
    return text.trimmed().isEmpty();
}

bool TherionParsedSourceLine::isCommentOnly() const
{
    return parsed.tokens.isEmpty() && parsed.commentStart >= 0;
}

int TherionParsedSourceLine::absoluteTokenStart(const TherionParsedToken &token) const
{
    return startOffset + token.start;
}

int TherionParsedSourceLine::absoluteTokenEnd(const TherionParsedToken &token) const
{
    return absoluteTokenStart(token) + token.length;
}

QString TherionParsedSourceDocument::toText() const
{
    QString contents;
    for (const TherionParsedSourceLine &line : lines) {
        contents += line.text;
        contents += line.lineEnding;
    }
    return contents;
}

QVector<TherionParsedLine> TherionParsedSourceDocument::tokenLines() const
{
    QVector<TherionParsedLine> parsedLines;
    for (const TherionParsedSourceLine &line : lines) {
        if (line.parsed.tokens.isEmpty()) {
            continue;
        }
        parsedLines.append(line.parsed);
    }
    return parsedLines;
}

int TherionDocumentParser::continuationMarkerIndex(const QString &line)
{
    // Therion joins a line ending in a backslash to the next one (thinput.cxx).
    int end = line.size();
    while (end > 0 && line.at(end - 1).isSpace()) {
        --end;
    }
    return end > 0 && line.at(end - 1) == QLatin1Char('\\') ? end - 1 : -1;
}

TherionParsedLine TherionDocumentParser::parseLine(const QString &line, int lineNumber)
{
    TherionParsedLine parsedLine;
    parsedLine.lineNumber = lineNumber;
    parsedLine.rawText = line;

    // Splits values as Therion's thsplit_args (thparse.cxx): plain words, "quoted" values with "" for a quote,
    // and [bracketed] values that nest and keep their brackets. A comment starts only with # at a word start.
    enum class State
    {
        BetweenValues,
        Word,
        Quoted,
        Bracketed
    };
    State state = State::BetweenValues;
    int valueStart = -1;
    QString quotedText;
    int bracketDepth = 0;
    const int continuationIndex = continuationMarkerIndex(line);

    auto appendToken = [&parsedLine](int start, int length, const QString &text, TherionTokenType type) {
        parsedLine.tokens.append(text);
        TherionParsedToken tokenSpan;
        tokenSpan.start = start;
        tokenSpan.length = length;
        tokenSpan.text = text;
        tokenSpan.type = type;
        parsedLine.tokenSpans.append(tokenSpan);
    };

    // An unclosed quoted or bracketed value runs to the end of the line, without trailing spaces.
    auto unclosedEnd = [&line](int start, int end) {
        while (end > start && line.at(end - 1).isSpace()) {
            --end;
        }
        return end;
    };

    auto finishValue = [&](int endIndex) {
        if (state == State::Word) {
            appendToken(valueStart, endIndex - valueStart, line.mid(valueStart, endIndex - valueStart),
                        TherionTokenType::Other);
        } else if (state == State::Quoted) {
            const int end = unclosedEnd(valueStart, endIndex);
            QString text = quotedText;
            text.truncate(qMax(0, text.size() - (endIndex - end)));
            appendToken(valueStart, end - valueStart, text, TherionTokenType::QuotedString);
        } else if (state == State::Bracketed) {
            const int end = unclosedEnd(valueStart, endIndex);
            appendToken(valueStart, end - valueStart, line.mid(valueStart, end - valueStart),
                        TherionTokenType::Other);
        }
        state = State::BetweenValues;
    };

    for (int index = 0; index < line.length(); ++index) {
        const QChar character = line.at(index);

        // A quoted or bracketed value ends before a line-continuation marker; the marker stays a word.
        if ((state == State::Quoted || state == State::Bracketed) && index == continuationIndex) {
            finishValue(index);
        }

        switch (state) {
        case State::Word:
            if (character.isSpace()) {
                finishValue(index);
            }
            continue;
        case State::Quoted:
            if (character != QLatin1Char('"')) {
                quotedText.append(character);
            } else if (index + 1 < line.length() && line.at(index + 1) == QLatin1Char('"')) {
                quotedText.append(character);
                ++index;
            } else {
                appendToken(valueStart, index + 1 - valueStart, quotedText, TherionTokenType::QuotedString);
                state = State::BetweenValues;
            }
            continue;
        case State::Bracketed:
            if (character == QLatin1Char('[')) {
                ++bracketDepth;
            } else if (character == QLatin1Char(']') && --bracketDepth == 0) {
                appendToken(valueStart, index + 1 - valueStart, line.mid(valueStart, index + 1 - valueStart),
                            TherionTokenType::Other);
                state = State::BetweenValues;
            }
            continue;
        case State::BetweenValues:
            break;
        }

        if (character.isSpace()) {
            continue;
        }

        if (character == QLatin1Char('#')) {
            parsedLine.commentStart = index;
            parsedLine.commentText = line.mid(index + 1);
            TherionParsedToken commentSpan;
            commentSpan.start = index;
            commentSpan.length = line.length() - index;
            commentSpan.text = line.mid(index);
            commentSpan.type = TherionTokenType::Comment;
            parsedLine.tokenSpans.append(commentSpan);
            break;
        }

        valueStart = index;
        if (character == QLatin1Char('"') && index != continuationIndex) {
            state = State::Quoted;
            quotedText.clear();
        } else if (character == QLatin1Char('[') && index != continuationIndex) {
            state = State::Bracketed;
            bracketDepth = 1;
        } else {
            state = State::Word;
        }
    }

    finishValue(line.length());

    if (!parsedLine.tokens.isEmpty()) {
        parsedLine.directive = parsedLine.tokens.first().toLower();
    }

    if (!parsedLine.tokenSpans.isEmpty()) {
        for (int index = 0; index < parsedLine.tokenSpans.size(); ++index) {
            TherionParsedToken &tokenSpan = parsedLine.tokenSpans[index];
            if (tokenSpan.type == TherionTokenType::QuotedString || tokenSpan.type == TherionTokenType::Comment) {
                continue;
            }

            if (index == 0) {
                tokenSpan.type = TherionTokenType::Other;
                continue;
            }

            if (TherionTokenRules::isNumericToken(tokenSpan.text)) {
                tokenSpan.type = TherionTokenType::Other;
            }
        }
    }

    return parsedLine;
}

QVector<TherionParsedLine> TherionDocumentParser::parseText(const QString &text)
{
    QVector<TherionParsedLine> parsedLines;
    const QStringList lines = text.split(QLatin1Char('\n'));

    int lineNumber = 0;
    for (QString line : lines) {
        ++lineNumber;
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const TherionParsedLine parsedLine = parseLine(line, lineNumber);
        if (parsedLine.tokens.isEmpty()) {
            continue;
        }

        parsedLines.append(parsedLine);
    }

    return parsedLines;
}

QVector<TherionParsedLine> TherionDocumentParser::parseTokenLines(const QString &text)
{
    return parseSourceDocument(text).tokenLines();
}

TherionParsedSourceDocument TherionDocumentParser::parseSourceDocument(const QString &text)
{
    return parseSourceDocument(TherionSourceText::fromText(text));
}

TherionParsedSourceDocument TherionDocumentParser::parseSourceDocument(const TherionSourceText &sourceText)
{
    TherionParsedSourceDocument document;
    const QVector<TherionSourceLine> &physicalLines = sourceText.physicalLines();
    const QVector<TherionSourceLineSpan> lineSpans = sourceText.lineSpans();
    document.lines.reserve(physicalLines.size());

    for (int index = 0; index < physicalLines.size(); ++index) {
        const TherionSourceLine &sourceLine = physicalLines.at(index);
        const TherionSourceLineSpan &lineSpan = lineSpans.at(index);
        document.lines.append(TherionParsedSourceLine{
            lineSpan.lineNumber,
            lineSpan.startOffset,
            lineSpan.textLength,
            lineSpan.lineEndingLength,
            lineSpan.endOffset,
            sourceLine.text,
            sourceLine.lineEnding,
            parseLine(sourceLine.text, lineSpan.lineNumber)
        });
    }

    return document;
}

QStringList TherionDocumentParser::tokenizeLine(const QString &line)
{
    return parseLine(line).tokens;
}

QVector<TherionParsedToken> TherionDocumentParser::bracketedValueSpans(const TherionParsedToken &token)
{
    if (!token.text.startsWith(QLatin1Char('['))) {
        return {token};
    }

    const int contentEnd = token.text.endsWith(QLatin1Char(']')) ? token.text.size() - 1 : token.text.size();
    QVector<TherionParsedToken> spans;
    int wordStart = -1;
    for (int index = 1; index <= contentEnd; ++index) {
        const bool wordCharacter = index < contentEnd && !token.text.at(index).isSpace();
        if (wordCharacter && wordStart < 0) {
            wordStart = index;
        } else if (!wordCharacter && wordStart >= 0) {
            TherionParsedToken span;
            span.start = token.start + wordStart;
            span.length = index - wordStart;
            span.text = token.text.mid(wordStart, span.length);
            spans.append(span);
            wordStart = -1;
        }
    }
    return spans;
}

QStringList TherionDocumentParser::bracketedValueParts(const QString &token)
{
    TherionParsedToken span;
    span.text = token.trimmed();
    QStringList parts;
    for (const TherionParsedToken &part : bracketedValueSpans(span)) {
        parts.append(part.text);
    }
    return parts;
}
}
