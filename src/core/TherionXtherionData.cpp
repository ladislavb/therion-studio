#include "TherionXtherionData.h"

#include "TherionStringUtils.h"

#include <QStringList>

namespace TherionStudio
{
namespace
{
const QString kSurveyTreeCommand = QStringLiteral("xth_cp_data_tree_insert");

// id parent level name fullname title stitle stat (xtherion cp_procs.tcl)
constexpr int kSurveyTreeWordCount = 8;

QChar tclEscapedCharacter(QChar character)
{
    if (character == QLatin1Char('n')) {
        return QLatin1Char('\n');
    }
    if (character == QLatin1Char('t')) {
        return QLatin1Char('\t');
    }
    return character;
}

// Tcl words of one command line: "quoted" with backslash escapes, {braced} kept literally, or bare.
QStringList tclWords(const QString &line)
{
    QStringList words;
    int index = 0;
    const int length = line.size();
    while (index < length) {
        while (index < length && line.at(index).isSpace()) {
            ++index;
        }
        if (index >= length) {
            break;
        }

        QString word;
        if (line.at(index) == QLatin1Char('{')) {
            int depth = 1;
            ++index;
            while (index < length) {
                const QChar character = line.at(index);
                if (character == QLatin1Char('\\') && index + 1 < length) {
                    word += character;
                    word += line.at(index + 1);
                    index += 2;
                    continue;
                }
                if (character == QLatin1Char('{')) {
                    ++depth;
                } else if (character == QLatin1Char('}') && --depth == 0) {
                    ++index;
                    break;
                }
                word += character;
                ++index;
            }
        } else {
            const bool quoted = line.at(index) == QLatin1Char('"');
            if (quoted) {
                ++index;
            }
            while (index < length) {
                const QChar character = line.at(index);
                if (quoted ? character == QLatin1Char('"') : character.isSpace()) {
                    if (quoted) {
                        ++index;
                    }
                    break;
                }
                if (character == QLatin1Char('\\') && index + 1 < length) {
                    word += tclEscapedCharacter(line.at(index + 1));
                    index += 2;
                    continue;
                }
                word += character;
                ++index;
            }
        }
        words.append(word);
    }
    return words;
}
}

QVector<TherionXtherionSurvey> parseTherionXtherionSurveys(const QString &text)
{
    QVector<TherionXtherionSurvey> surveys;
    for (const QString &line : splitLinesNormalizingLineEndings(text)) {
        if (!line.startsWith(kSurveyTreeCommand + QLatin1Char(' '))) {
            continue;
        }
        const QStringList words = tclWords(line.mid(kSurveyTreeCommand.size()));
        if (words.size() != kSurveyTreeWordCount || words.at(3).isEmpty()) {
            continue;
        }
        TherionXtherionSurvey survey;
        survey.name = words.at(3);
        survey.fullName = words.at(4);
        survey.title = words.at(5);
        survey.statistics = words.at(7);
        surveys.append(survey);
    }
    return surveys;
}
}
