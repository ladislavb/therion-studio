#include "ProjectSearchScanner.h"

#include "ProjectFileDiscovery.h"
#include "../core/DocumentFile.h"
#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSet>
#include <QTimer>
#include <QtConcurrent>

namespace TherionStudio
{
namespace
{
constexpr int kMaximumProjectSearchMatches = 1000;
constexpr qsizetype kMaximumSearchableFileBytes = 4 * 1024 * 1024;

bool isSearchableTherionTextFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    if (isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

QString readSearchableFileText(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.size() > kMaximumSearchableFileBytes) {
        return QString();
    }

    QString contents;
    if (!DocumentFile::readTextFile(filePath, &contents, nullptr, nullptr, nullptr)) {
        return QString();
    }
    return contents;
}

bool isWordCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

bool isWholeWordMatch(const QString &line, int matchIndex, int matchLength)
{
    const bool hasWordBefore = matchIndex > 0 && isWordCharacter(line.at(matchIndex - 1));
    const int afterIndex = matchIndex + matchLength;
    const bool hasWordAfter = afterIndex < line.size() && isWordCharacter(line.at(afterIndex));
    return !hasWordBefore && !hasWordAfter;
}

void appendMatchesForText(ProjectSearchScanner::Result *result,
                          const QString &filePath,
                          const QString &text)
{
    if (result == nullptr || result->query.isEmpty()) {
        return;
    }

    const Qt::CaseSensitivity sensitivity = result->matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString &line = lines.at(lineIndex);
        int from = 0;
        while (from <= line.size()) {
            const int matchIndex = line.indexOf(result->query, from, sensitivity);
            if (matchIndex < 0) {
                break;
            }

            if (result->wholeWord && !isWholeWordMatch(line, matchIndex, result->query.size())) {
                from = matchIndex + qMax(1, result->query.size());
                continue;
            }

            ProjectSearchScanner::Match match;
            match.filePath = filePath;
            match.lineNumber = lineIndex + 1;
            match.columnNumber = matchIndex + 1;
            match.lineText = line.trimmed();
            result->matches.append(match);
            if (result->matches.size() >= kMaximumProjectSearchMatches) {
                result->limitReached = true;
                return;
            }

            from = matchIndex + qMax(1, result->query.size());
        }
    }
}

ProjectSearchScanner::Result performProjectSearch(const QString &projectRootPath,
                                                  const QString &query,
                                                  bool wholeWord,
                                                  bool matchCase,
                                                  const QHash<QString, QString> &inMemoryProjectContentsByPath,
                                                  quint64 generation)
{
    ProjectSearchScanner::Result result;
    result.generation = generation;
    result.projectRootPath = projectRootPath;
    result.query = query.trimmed();
    result.wholeWord = wholeWord;
    result.matchCase = matchCase;

    if (result.projectRootPath.trimmed().isEmpty() || !QDir(result.projectRootPath).exists()) {
        result.errorMessage = QObject::tr("Open a project before searching.");
        return result;
    }
    if (result.query.isEmpty()) {
        result.errorMessage = QObject::tr("Enter text to search for.");
        return result;
    }

    QSet<QString> searchedPaths;
    const QVector<ProjectDiscoveredFile> filePaths =
        ProjectFileDiscovery::collectFiles(result.projectRootPath, [](const QFileInfo &info) {
            return isSearchableTherionTextFile(info.absoluteFilePath());
        });
    for (const ProjectDiscoveredFile &candidate : filePaths) {
        const QString filePath = candidate.filePath;
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;

        const auto memoryIt = inMemoryProjectContentsByPath.constFind(filePath);
        const QString text = memoryIt != inMemoryProjectContentsByPath.constEnd()
            ? *memoryIt
            : readSearchableFileText(filePath);
        appendMatchesForText(&result, filePath, text);
        if (result.limitReached) {
            return result;
        }
    }

    for (auto it = inMemoryProjectContentsByPath.constBegin();
         it != inMemoryProjectContentsByPath.constEnd();
         ++it) {
        const QString filePath = ProjectFileDiscovery::canonicalOrAbsolutePath(it.key());
        if (searchedPaths.contains(filePath) || !isSearchableTherionTextFile(filePath)) {
            continue;
        }
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;
        appendMatchesForText(&result, filePath, *it);
        if (result.limitReached) {
            return result;
        }
    }

    return result;
}
}

ProjectSearchScanner::ProjectSearchScanner(QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , searchWatcher_(new QFutureWatcher<Result>(this))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(120);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectSearchScanner::startSearch);
    connect(searchWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectSearchScanner::handleSearchFinished);
}

void ProjectSearchScanner::requestSearch(const QString &projectRootPath,
                                         const QString &query,
                                         bool wholeWord,
                                         bool matchCase,
                                         const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    pendingRequest_.projectRootPath = projectRootPath;
    pendingRequest_.query = query;
    pendingRequest_.wholeWord = wholeWord;
    pendingRequest_.matchCase = matchCase;
    pendingRequest_.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectSearchScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

void ProjectSearchScanner::startSearch()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (searchWatcher_->isRunning()) {
        queuedSearch_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const quint64 generation = ++generation_;

    auto future = QtConcurrent::run([request, generation]() {
        return performProjectSearch(request.projectRootPath,
                                    request.query,
                                    request.wholeWord,
                                    request.matchCase,
                                    request.inMemoryProjectContentsByPath,
                                    generation);
    });
    searchWatcher_->setFuture(future);
}

void ProjectSearchScanner::handleSearchFinished()
{
    const Result result = searchWatcher_->result();
    emit searchFinished(result);

    if (queuedSearch_) {
        queuedSearch_ = false;
        startSearch();
    }
}
}
