#include "ProjectVersioningStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QMap>
#include <QRegularExpression>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimeZone>

#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kGitTimeoutMs = 30000;
const QString kDefaultExcludeRules = QStringLiteral(
    ".git/\n"
    "**/.git/\n"
    "output/\n"
    "outputs/\n"
    "*.3d\n"
    "*.lox\n"
    "*.plt\n");

struct GitCommandResult
{
    bool succeeded = false;
    int exitCode = -1;
    QByteArray standardOutput;
    QString error;
};

QString nullDevicePath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("NUL");
#else
    return QStringLiteral("/dev/null");
#endif
}

QProcessEnvironment gitEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString &key : environment.keys()) {
        if (key.startsWith(QStringLiteral("GIT_"), Qt::CaseInsensitive)) {
            environment.remove(key);
        }
    }
    environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), nullDevicePath());
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_ATTR_NOSYSTEM"), QStringLiteral("1"));
    return environment;
}

GitCommandResult runGit(const QString &gitExecutable,
                        const QStringList &arguments,
                        const QString &workingDirectory = QString(),
                        const QString &indexPath = QString())
{
    QProcess process;
    QProcessEnvironment environment = gitEnvironment();
    if (!indexPath.isEmpty()) {
        environment.insert(QStringLiteral("GIT_INDEX_FILE"), indexPath);
    }
    process.setProcessEnvironment(environment);
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }
    process.start(gitExecutable, arguments);
    if (!process.waitForStarted(5000)) {
        return {false, -1, {}, QStringLiteral("Could not start Git: %1").arg(process.errorString())};
    }
    if (!process.waitForFinished(kGitTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return {false, -1, {}, QStringLiteral("Git operation timed out.")};
    }

    GitCommandResult result;
    result.exitCode = process.exitCode();
    result.standardOutput = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || result.exitCode != 0) {
        const QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        result.error = detail.isEmpty()
            ? QStringLiteral("Git exited with code %1.").arg(result.exitCode)
            : detail;
        return result;
    }
    result.succeeded = true;
    return result;
}

bool isWithinDirectory(const QString &parentPath, const QString &candidatePath)
{
    const QString relativePath = QDir(parentPath).relativeFilePath(candidatePath);
    return relativePath == QStringLiteral(".")
        || (!relativePath.startsWith(QStringLiteral("../"))
            && relativePath != QStringLiteral("..")
            && !QDir::isAbsolutePath(relativePath));
}

QString canonicalizedPath(const QString &path)
{
    QString currentPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QStringList missingPathParts;
    while (!QFileInfo(currentPath).exists() && !QFileInfo(currentPath).isSymLink()) {
        const QFileInfo currentInfo(currentPath);
        const QString parentPath = currentInfo.absolutePath();
        if (parentPath == currentPath) {
            return {};
        }
        missingPathParts.prepend(currentInfo.fileName());
        currentPath = parentPath;
    }

    QString resolvedPath = QFileInfo(currentPath).canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        return {};
    }
    for (const QString &part : missingPathParts) {
        resolvedPath = QDir(resolvedPath).filePath(part);
    }
    return QDir::cleanPath(resolvedPath);
}

bool resolveProjectAndHistoryPaths(const QString &projectRootPath,
                                   const QString &historyDirectory,
                                   QString *projectRoot,
                                   QString *historyPath,
                                   QString *error)
{
    const QFileInfo projectInfo(projectRootPath);
    if (!projectInfo.exists() || !projectInfo.isDir()) {
        *error = QStringLiteral("The selected project root does not exist or is not a directory.");
        return false;
    }

    *projectRoot = projectInfo.canonicalFilePath();
    *historyPath = canonicalizedPath(historyDirectory);
    if (projectRoot->isEmpty() || historyPath->isEmpty()
        || isWithinDirectory(*projectRoot, *historyPath)
        || isWithinDirectory(*historyPath, *projectRoot)) {
        *error = QStringLiteral("The local history store must be outside and separate from the project root.");
        return false;
    }
    return true;
}

QStringList repositoryArguments(const QString &gitDirectory, const QString &projectRoot)
{
    return {
        QStringLiteral("--git-dir=%1").arg(gitDirectory),
        QStringLiteral("--work-tree=%1").arg(projectRoot),
        QStringLiteral("-c"),
        QStringLiteral("core.excludesFile=%1").arg(nullDevicePath()),
        QStringLiteral("-c"),
        QStringLiteral("core.hooksPath=%1").arg(QDir(gitDirectory).filePath(QStringLiteral("hooks-disabled"))),
        QStringLiteral("-c"),
        QStringLiteral("core.autocrlf=false"),
        QStringLiteral("-c"),
        QStringLiteral("core.attributesFile=%1").arg(nullDevicePath()),
        QStringLiteral("-c"),
        QStringLiteral("commit.gpgsign=false"),
        QStringLiteral("-c"),
        QStringLiteral("user.name=Therion Studio"),
        QStringLiteral("-c"),
        QStringLiteral("user.email=therion-studio@users.noreply.github.com")
    };
}

bool writeDefaultExcludeRules(const QString &gitDirectory, QString *error)
{
    const QString infoDirectory = QDir(gitDirectory).filePath(QStringLiteral("info"));
    if (!QDir().mkpath(infoDirectory)) {
        *error = QStringLiteral("Could not create the local history metadata directory.");
        return false;
    }
    QFile excludeFile(QDir(infoDirectory).filePath(QStringLiteral("exclude")));
    const QByteArray marker = QByteArrayLiteral("# Therion Studio generated-output defaults");
    if (excludeFile.open(QIODevice::ReadOnly)) {
        const QByteArray currentRules = excludeFile.readAll();
        excludeFile.close();
        if (currentRules.contains(marker)) {
            return true;
        }
    }
    if (!excludeFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        *error = QStringLiteral("Could not write local history ignore rules: %1").arg(excludeFile.errorString());
        return false;
    }
    const QByteArray separator = excludeFile.size() > 0 ? QByteArrayLiteral("\n") : QByteArray();
    if (excludeFile.write(separator + marker + '\n' + kDefaultExcludeRules.toUtf8()) < 0 || !excludeFile.flush()) {
        *error = QStringLiteral("Could not finish writing local history ignore rules: %1").arg(excludeFile.errorString());
        return false;
    }
    return true;
}

bool writeSnapshotAttributes(const QString &gitDirectory, QString *error)
{
    QSaveFile attributes(QDir(gitDirectory).filePath(QStringLiteral("info/attributes")));
    const QByteArray rules = QByteArrayLiteral("* -text -ident -filter -working-tree-encoding\n");
    if (!attributes.open(QIODevice::WriteOnly) || attributes.write(rules) != rules.size()
        || !attributes.commit()) {
        *error = QStringLiteral("Could not configure byte-preserving project snapshots: %1")
                     .arg(attributes.errorString());
        return false;
    }
    return true;
}

bool validateManagedRepository(const QString &gitExecutable,
                               const QString &gitDirectory,
                               const QString &projectRoot,
                               QString *error)
{
    const GitCommandResult marker = runGit(
        gitExecutable,
        {QStringLiteral("--git-dir=%1").arg(gitDirectory),
         QStringLiteral("config"),
         QStringLiteral("--local"),
         QStringLiteral("--get"),
         QStringLiteral("therionstudio.versioningStore")});
    if (!marker.succeeded
        || QString::fromLatin1(marker.standardOutput).trimmed() != QStringLiteral("true")) {
        *error = QStringLiteral("The selected history directory is not managed by Therion Studio.");
        return false;
    }
    const GitCommandResult root = runGit(
        gitExecutable,
        {QStringLiteral("--git-dir=%1").arg(gitDirectory),
         QStringLiteral("config"), QStringLiteral("--local"),
         QStringLiteral("--get"), QStringLiteral("therionstudio.projectRoot")});
    if (!root.succeeded || QString::fromUtf8(root.standardOutput).trimmed() != projectRoot) {
        *error = QStringLiteral("The local history store is not bound to the selected project root.");
        return false;
    }
    return true;
}

bool markRepositoryManaged(const QString &gitExecutable,
                           const QString &gitDirectory,
                           const QString &projectRoot,
                           QString *error)
{
    const GitCommandResult root = runGit(
        gitExecutable,
        {QStringLiteral("--git-dir=%1").arg(gitDirectory),
         QStringLiteral("config"), QStringLiteral("--local"),
         QStringLiteral("therionstudio.projectRoot"), projectRoot});
    if (!root.succeeded) {
        *error = QStringLiteral("Could not bind local history to the project root: %1").arg(root.error);
        return false;
    }
    const GitCommandResult marker = runGit(
        gitExecutable,
        {QStringLiteral("--git-dir=%1").arg(gitDirectory),
         QStringLiteral("config"),
         QStringLiteral("--local"),
         QStringLiteral("therionstudio.versioningStore"),
         QStringLiteral("true")});
    if (!marker.succeeded) {
        *error = QStringLiteral("Could not mark the local history repository as app-managed: %1")
                     .arg(marker.error);
        return false;
    }
    return true;
}

bool ensureRepository(const QString &gitExecutable,
                      const QString &projectRootPath,
                      const QString &historyDirectory,
                      QString *error)
{
    QString projectRoot;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath, historyDirectory, &projectRoot, &historyPath, error)) {
        return false;
    }

    const QFileInfo historyInfo(historyPath);
    if (historyInfo.exists()) {
        const QStringList entries = QDir(historyPath).entryList(
            QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
        if (!entries.isEmpty()) {
            const GitCommandResult repositoryCheck = runGit(
                gitExecutable,
                {QStringLiteral("--git-dir=%1").arg(historyPath),
                 QStringLiteral("rev-parse"),
                 QStringLiteral("--is-bare-repository")});
            if (!repositoryCheck.succeeded
                || QString::fromLatin1(repositoryCheck.standardOutput).trimmed() != QStringLiteral("true")) {
                *error = QStringLiteral("The local history directory is not an empty app-managed Git repository.");
                return false;
            }
            if (!validateManagedRepository(gitExecutable, historyPath, projectRoot, error)) {
                return false;
            }
            return writeDefaultExcludeRules(historyPath, error)
                && writeSnapshotAttributes(historyPath, error);
        }
    } else if (!QDir().mkpath(QFileInfo(historyPath).absolutePath())) {
        *error = QStringLiteral("Could not create the parent directory for local history.");
        return false;
    }

    const GitCommandResult initialized = runGit(
        gitExecutable,
        {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral("--quiet"), historyPath});
    if (!initialized.succeeded) {
        *error = QStringLiteral("Could not initialize local project history: %1").arg(initialized.error);
        return false;
    }
    return markRepositoryManaged(gitExecutable, historyPath, projectRoot, error)
        && writeDefaultExcludeRules(historyPath, error)
        && writeSnapshotAttributes(historyPath, error);
}

QStringList repositoryCommand(const QString &gitDirectory,
                              const QString &projectRoot,
                              const QStringList &command)
{
    QStringList arguments = repositoryArguments(gitDirectory, projectRoot);
    arguments.append(command);
    return arguments;
}
}

ProjectVersioningStore::ProjectVersioningStore(QString projectRootPath,
                                               QString historyDirectory,
                                               QString gitExecutable)
    : projectRootPath_(QDir::cleanPath(QFileInfo(std::move(projectRootPath)).absoluteFilePath()))
    , historyDirectory_(QDir::cleanPath(QFileInfo(std::move(historyDirectory)).absoluteFilePath()))
    , gitExecutable_(std::move(gitExecutable))
{
}

ProjectVersioningStore::CheckpointResult ProjectVersioningStore::createCheckpoint(const QString &label)
{
    CheckpointResult result;
    const QString checkpointLabel = label.simplified();
    if (checkpointLabel.isEmpty()) {
        result.error = QStringLiteral("A checkpoint label is required.");
        return result;
    }
    if (gitExecutable_.trimmed().isEmpty()) {
        result.error = QStringLiteral("Git executable is not configured.");
        return result;
    }
    QString projectRoot;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath_, historyDirectory_,
                                       &projectRoot, &historyPath, &result.error)) {
        return result;
    }
    if (!QDir().mkpath(QFileInfo(historyPath).absolutePath())) {
        result.error = QStringLiteral("Could not create the parent directory for local history.");
        return result;
    }
    QLockFile lock(historyPath + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        result.error = QStringLiteral("The local history store is busy or could not be locked.");
        return result;
    }
    if (QFileInfo::exists(QDir(historyPath).filePath(QStringLiteral("restore-pending")))) {
        result.error = QStringLiteral("A project restore was interrupted. Restore the safety checkpoint "
                                      "from the backup toolbar before creating more checkpoints.");
        return result;
    }
    return createCheckpointLocked(checkpointLabel);
}

ProjectVersioningStore::CheckpointResult ProjectVersioningStore::createCheckpointLocked(const QString &checkpointLabel)
{
    CheckpointResult result;
    QString projectRoot;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath_, historyDirectory_,
                                      &projectRoot, &historyPath, &result.error)) {
        return result;
    }
    if (!ensureRepository(gitExecutable_, projectRoot, historyPath, &result.error)) {
        return result;
    }
    QTemporaryDir candidate(QDir(historyPath).filePath(QStringLiteral("candidate-XXXXXX")));
    if (!candidate.isValid()) {
        result.error = QStringLiteral("Could not create temporary storage for the project snapshot.");
        return result;
    }
    const auto command = [&](const QStringList &arguments) {
        return runGit(gitExecutable_, repositoryCommand(historyPath, projectRoot, arguments),
                      projectRoot, QDir(candidate.path()).filePath(QStringLiteral("index")));
    };
    const GitCommandResult previous = command({QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                               QStringLiteral("--quiet"), QStringLiteral("HEAD")});
    if (!previous.succeeded && previous.exitCode != 1) {
        result.error = QStringLiteral("Could not read the previous checkpoint: %1").arg(previous.error);
        return result;
    }
    const auto captureTree = [&]() -> GitCommandResult {
        for (const QStringList &arguments : {
                 QStringList{QStringLiteral("read-tree"), QStringLiteral("--empty")},
                 QStringList{QStringLiteral("add"), QStringLiteral("--all"), QStringLiteral("--")}}) {
            const GitCommandResult step = command(arguments);
            if (!step.succeeded) {
                return step;
            }
        }
        const GitCommandResult entries =
            command({QStringLiteral("ls-files"), QStringLiteral("--stage"), QStringLiteral("-z")});
        if (!entries.succeeded) {
            return entries;
        }
        for (const QByteArray &entry : entries.standardOutput.split('\0')) {
            if (entry.startsWith("160000 ")) {
                return {false, -1, {},
                        QStringLiteral("Nested Git repositories cannot yet be backed up. "
                                       "Exclude their folders in the project's .gitignore or select them as separate projects.")};
            }
        }
        return command({QStringLiteral("write-tree")});
    };
    const GitCommandResult captured = captureTree();
    const GitCommandResult verified = captured.succeeded ? captureTree() : captured;
    if (!captured.succeeded || !verified.succeeded) {
        result.error = QStringLiteral("Could not capture the project snapshot: %1").arg(verified.error);
        return result;
    }
    if (captured.standardOutput != verified.standardOutput) {
        result.error = QStringLiteral("Project files changed during capture. Retry the checkpoint after saving finishes.");
        return result;
    }
    const QString treeId = QString::fromLatin1(captured.standardOutput).trimmed();
    const QString previousId = QString::fromLatin1(previous.standardOutput).trimmed();
    if (previous.succeeded) {
        const GitCommandResult previousTree =
            command({QStringLiteral("rev-parse"), previousId + QStringLiteral("^{tree}")});
        if (!previousTree.succeeded) {
            result.error = QStringLiteral("Could not compare the previous checkpoint: %1").arg(previousTree.error);
            return result;
        }
        if (QString::fromLatin1(previousTree.standardOutput).trimmed() == treeId) {
            result.succeeded = true;
            result.checkpointId = previousId;
            return result;
        }
    }
    QStringList commitArguments{QStringLiteral("commit-tree"), treeId,
                                QStringLiteral("-m"), checkpointLabel};
    if (previous.succeeded) {
        commitArguments.append({QStringLiteral("-p"), previousId});
    }
    const GitCommandResult committed = command(commitArguments);
    if (!committed.succeeded) {
        result.error = QStringLiteral("Could not save the project checkpoint: %1").arg(committed.error);
        return result;
    }
    const QString checkpointId = QString::fromLatin1(committed.standardOutput).trimmed();
    const GitCommandResult published =
        command({QStringLiteral("update-ref"), QStringLiteral("HEAD"), checkpointId,
                 previous.succeeded ? previousId : QString(checkpointId.size(), QLatin1Char('0'))});
    if (!published.succeeded) {
        result.error = QStringLiteral("Could not publish the project checkpoint: %1").arg(published.error);
        return result;
    }
    result.succeeded = true;
    result.checkpointId = checkpointId;
    return result;
}

ProjectVersioningStore::HistoryResult ProjectVersioningStore::history() const
{
    HistoryResult result;
    if (!QFileInfo::exists(historyDirectory_)) {
        result.succeeded = true;
        return result;
    }

    QString projectRoot;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath_,
                                       historyDirectory_,
                                       &projectRoot,
                                       &historyPath,
                                       &result.error)) {
        return result;
    }
    const GitCommandResult repositoryCheck = runGit(
        gitExecutable_,
        {QStringLiteral("--git-dir=%1").arg(historyPath),
         QStringLiteral("rev-parse"),
         QStringLiteral("--is-bare-repository")});
    if (!repositoryCheck.succeeded
        || QString::fromLatin1(repositoryCheck.standardOutput).trimmed() != QStringLiteral("true")) {
        result.error = repositoryCheck.error.isEmpty()
            ? QStringLiteral("The local history directory is not a Git repository.")
            : repositoryCheck.error;
        return result;
    }
    if (!validateManagedRepository(gitExecutable_, historyPath, projectRoot, &result.error)) {
        return result;
    }

    const GitCommandResult log = runGit(
        gitExecutable_,
        repositoryCommand(historyPath,
                          projectRoot,
                          {QStringLiteral("log"),
                           QStringLiteral("--format=%H%x1f%ct%x1f%s")}));
    if (!log.succeeded) {
        result.error = QStringLiteral("Could not read checkpoint history: %1").arg(log.error);
        return result;
    }

    const QList<QByteArray> lines = log.standardOutput.split('\n');
    for (const QByteArray &line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        const QList<QByteArray> fields = line.split('\x1f');
        if (fields.size() != 3) {
            result.error = QStringLiteral("The local history contains an invalid checkpoint record.");
            result.checkpoints.clear();
            return result;
        }
        bool timestampOk = false;
        const qint64 timestamp = fields.at(1).toLongLong(&timestampOk);
        if (!timestampOk) {
            result.error = QStringLiteral("The local history contains an invalid checkpoint timestamp.");
            result.checkpoints.clear();
            return result;
        }
        result.checkpoints.append({QString::fromLatin1(fields.at(0)),
                                   QString::fromUtf8(fields.at(2)),
                                   QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::utc())});
    }
    if (!result.checkpoints.isEmpty()) {
        const auto &latest = result.checkpoints.first();
        result.currentCheckpointId = latest.id;
        const QString prefix = QStringLiteral("Restored checkpoint ");
        if (latest.label.startsWith(prefix)) {
            const QString restoredId = latest.label.mid(prefix.size());
            for (const auto &checkpoint : result.checkpoints) {
                if (checkpoint.id == restoredId) {
                    result.currentCheckpointId = restoredId;
                    break;
                }
            }
        }
    }
    result.succeeded = true;
    return result;
}

ProjectVersioningStore::CheckpointResult ProjectVersioningStore::removeHistory()
{
    CheckpointResult result;
    QString root;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath_, historyDirectory_, &root, &historyPath, &result.error)) {
        return result;
    }
    // Do not traverse a redirected store when executing a destructive operation.
    if (historyPath != historyDirectory_) {
        result.error = QStringLiteral("Refusing to delete a redirected local history directory.");
        return result;
    }
    QLockFile lock(historyPath + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        result.error = QStringLiteral("Local history is busy. Wait for the current checkpoint and retry.");
        return result;
    }
    if (!QFileInfo::exists(historyPath)) {
        result.succeeded = true;
        return result;
    }
    if (!validateManagedRepository(gitExecutable_, historyPath, root, &result.error)) {
        return result;
    }
    if (!QDir(historyPath).removeRecursively()) {
        result.error = QStringLiteral("Could not completely remove local history. Check folder permissions and retry.");
        return result;
    }
    result.succeeded = true;
    return result;
}

ProjectVersioningStore::RestoreResult ProjectVersioningStore::restoreCheckpoint(const QString &checkpointId)
{
    RestoreResult result;
    result.checkpointId = checkpointId;
    static const QRegularExpression objectId(QStringLiteral("^(?:[0-9a-f]{40}|[0-9a-f]{64})$"));
    if (!objectId.match(checkpointId).hasMatch()) {
        result.error = QStringLiteral("Select a valid checkpoint identifier.");
        return result;
    }
    QString root;
    QString historyPath;
    if (!resolveProjectAndHistoryPaths(projectRootPath_, historyDirectory_, &root, &historyPath, &result.error)) {
        return result;
    }
    QLockFile lock(historyPath + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        result.error = QStringLiteral("Local history is busy. Wait for the current checkpoint and retry.");
        return result;
    }
    if (!validateManagedRepository(gitExecutable_, historyPath, root, &result.error)) {
        return result;
    }
    const auto command = [&](const QStringList &arguments) {
        return runGit(gitExecutable_, repositoryCommand(historyPath, root, arguments), root);
    };
    const auto reachable = command({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"),
                                    checkpointId, QStringLiteral("HEAD")});
    if (!reachable.succeeded) {
        result.error = QStringLiteral("The selected checkpoint is not in this project's history: %1").arg(reachable.error);
        return result;
    }
    const QString journalPath = QDir(historyPath).filePath(QStringLiteral("restore-pending"));
    const bool recovering = QFileInfo::exists(journalPath);
    if (recovering) {
        QFile journal(journalPath);
        if (!journal.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("Could not read the interrupted restore record: %1").arg(journal.errorString());
            return result;
        }
        const QString recoveryId = QString::fromLatin1(journal.readAll()).trimmed();
        if (checkpointId != recoveryId) {
            result.error = QStringLiteral("A restore was interrupted. First restore safety checkpoint %1.").arg(recoveryId);
            return result;
        }
    }
    const auto safety = createCheckpointLocked(QStringLiteral("Safety checkpoint before restore"));
    result.safetyCheckpointId = safety.checkpointId;
    if (!safety.succeeded) {
        result.error = QStringLiteral("Restore cancelled: the current project could not be backed up. %1").arg(safety.error);
        return result;
    }

    struct Entry
    {
        QString blob;
        bool executable = false;
    };
    const auto readTree = [&](const QString &id, QMap<QString, Entry> *tree) {
        const auto listed = command({QStringLiteral("ls-tree"), QStringLiteral("-rz"), id});
        if (!listed.succeeded) {
            result.error = listed.error;
            return false;
        }
        for (const QByteArray &record : listed.standardOutput.split('\0')) {
            if (record.isEmpty()) {
                continue;
            }
            const int tab = record.indexOf('\t');
            const auto fields = record.left(tab).split(' ');
            const QByteArray rawPath = record.mid(tab + 1);
            const QString path = QString::fromUtf8(rawPath);
            bool safePath = !path.isEmpty() && path.toUtf8() == rawPath
                && !QDir::isAbsolutePath(path) && !path.contains(QLatin1Char('\\')) && !path.contains(QLatin1Char(':'));
            for (const QString &part : path.split(QLatin1Char('/'))) {
                safePath = safePath && !part.isEmpty() && part != QStringLiteral(".")
                    && part != QStringLiteral("..") && part.compare(QStringLiteral(".git"), Qt::CaseInsensitive) != 0
                    && !part.endsWith(QLatin1Char('.')) && !part.endsWith(QLatin1Char(' '));
            }
            if (tab < 0 || fields.size() != 3 || fields.at(1) != "blob"
                || (fields.at(0) != "100644" && fields.at(0) != "100755") || !safePath
                || !objectId.match(QString::fromLatin1(fields.at(2))).hasMatch()) {
                result.error = QStringLiteral("Restore requires regular files with safe portable paths; "
                                              "links and nested repositories are not supported: %1").arg(path);
                return false;
            }
            tree->insert(path, {QString::fromLatin1(fields.at(2)), fields.at(0) == "100755"});
        }
        return true;
    };
    QMap<QString, Entry> before;
    QMap<QString, Entry> after;
    if (!readTree(safety.checkpointId, &before) || !readTree(checkpointId, &after)) {
        return result;
    }
    QStringList paths = before.keys();
    paths.append(after.keys());
    paths.removeDuplicates();
    paths.sort();
    struct Change
    {
        QString relativePath;
        QString oldFile;
        QString newFile;
        QFileDevice::Permissions oldPermissions;
        QFileDevice::Permissions newPermissions;
    };
    QTemporaryDir staging(QDir(historyPath).filePath(QStringLiteral("restore-XXXXXX")));
    if (!staging.isValid()) {
        result.error = QStringLiteral("Could not stage the restore.");
        return result;
    }
    const auto safeDestination = [&](const QString &relativePath) {
        const QFileInfo rootInfo(root);
        if (rootInfo.isSymLink() || rootInfo.canonicalFilePath() != root) {
            result.error = QStringLiteral("The project root changed during restoration.");
            return false;
        }
        QString current = root;
        const QStringList components = relativePath.split(QLatin1Char('/'));
        for (int i = 0; i < components.size(); ++i) {
            current = QDir(current).filePath(components.at(i));
            const QFileInfo info(current);
            if (info.isSymLink() || (info.exists() && (i == components.size() - 1 ? !info.isFile() : !info.isDir()))) {
                result.error = QStringLiteral("Restore blocked by a link or file/directory conflict: %1").arg(relativePath);
                return false;
            }
        }
        return true;
    };
    const auto matchesBlob = [&](const QString &path, const QString &blob) {
        const QFileInfo info(path);
        if (blob.isEmpty()) {
            return !info.exists() && !info.isSymLink();
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        QCryptographicHash hash(blob.size() == 64 ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1);
        hash.addData(QByteArray("blob ") + QByteArray::number(file.size()) + '\0');
        return hash.addData(&file) && file.error() == QFileDevice::NoError
            && QString::fromLatin1(hash.result().toHex()) == blob;
    };
    const auto stageBlob = [&](const QString &blob, const QString &path) {
        const auto contents = command({QStringLiteral("cat-file"), QStringLiteral("blob"), blob});
        QFile file(path);
        if (!contents.succeeded || !file.open(QIODevice::WriteOnly)
            || file.write(contents.standardOutput) != contents.standardOutput.size() || !file.flush()) {
            result.error = QStringLiteral("Could not stage a checkpoint file: %1").arg(
                contents.succeeded ? file.errorString() : contents.error);
            return false;
        }
        file.close();
        if (!matchesBlob(path, blob)) {
            result.error = QStringLiteral("A staged checkpoint file failed integrity verification.");
            return false;
        }
        return true;
    };
    QVector<Change> changes;
    for (const QString &path : paths) {
        const bool existed = before.contains(path);
        const bool existsAfter = after.contains(path);
        if (existed && existsAfter && before[path].blob == after[path].blob
            && before[path].executable == after[path].executable) {
            continue;
        }
        const QString absolute = QDir(root).filePath(path);
        if (!safeDestination(path) || !matchesBlob(absolute, existed ? before[path].blob : QString())) {
            if (result.error.isEmpty()) {
                result.error = QStringLiteral("Restore would overwrite an ignored file or a concurrent change: %1").arg(path);
            }
            return result;
        }
        const QString prefix = QDir(staging.path()).filePath(QString::number(changes.size()));
        Change change{path, existed ? prefix + QStringLiteral("-old") : QString(),
                      existsAfter ? prefix + QStringLiteral("-new") : QString(),
                      QFileInfo(absolute).permissions(), {}};
        change.newPermissions = existed ? change.oldPermissions
            : QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther;
        const auto execute = QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther;
        change.newPermissions &= ~execute;
        if (existsAfter && after[path].executable) {
            change.newPermissions |= QFileDevice::ExeOwner | QFileDevice::ExeUser;
            if (change.newPermissions.testFlag(QFileDevice::ReadGroup)) {
                change.newPermissions |= QFileDevice::ExeGroup;
            }
            if (change.newPermissions.testFlag(QFileDevice::ReadOther)) {
                change.newPermissions |= QFileDevice::ExeOther;
            }
        }
        if ((existed && !stageBlob(before[path].blob, change.oldFile))
            || (existsAfter && !stageBlob(after[path].blob, change.newFile))) {
            return result;
        }
        changes.append(change);
        if (!existed) {
            result.addedFiles.append(path);
        } else if (!existsAfter) {
            result.removedFiles.append(path);
        } else {
            result.changedFiles.append(path);
        }
    }
    // Keep a durable recovery pointer if the application exits mid-transaction.
    if (!QFileInfo::exists(journalPath)) {
        QSaveFile journal(journalPath);
        const QByteArray contents = safety.checkpointId.toLatin1() + '\n';
        if (!journal.open(QIODevice::WriteOnly) || journal.write(contents) != contents.size() || !journal.commit()) {
            result.error = QStringLiteral("Could not write the restore recovery record: %1").arg(journal.errorString());
            return result;
        }
    }
    const auto apply = [&](const Change &change, bool rollback) {
        if (!safeDestination(change.relativePath)) {
            return false;
        }
        const QString absolute = QDir(root).filePath(change.relativePath);
        const QString source = rollback ? change.oldFile : change.newFile;
        if (source.isEmpty()) {
            return !QFileInfo::exists(absolute) || QFile::remove(absolute);
        }
        if (!QDir().mkpath(QFileInfo(absolute).absolutePath())) {
            return false;
        }
        QFile input(source);
        QSaveFile output(absolute);
        if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
            return false;
        }
        while (!input.atEnd()) {
            const QByteArray bytes = input.read(1024 * 1024);
            if (input.error() != QFileDevice::NoError || output.write(bytes) != bytes.size()) {
                return false;
            }
        }
        return output.setPermissions(rollback ? change.oldPermissions : change.newPermissions) && output.commit();
    };
    int applied = 0;
    for (const Change &change : changes) {
        const QString expected = before.contains(change.relativePath) ? before[change.relativePath].blob : QString();
        if (!safeDestination(change.relativePath)
            || !matchesBlob(QDir(root).filePath(change.relativePath), expected) || !apply(change, false)) {
            result.error = QStringLiteral("Could not restore %1; rolling back.").arg(change.relativePath);
            break;
        }
        ++applied;
    }
    if (result.error.isEmpty()) {
        const auto published = createCheckpointLocked(QStringLiteral("Restored checkpoint %1").arg(checkpointId));
        if (!published.succeeded) {
            result.error = QStringLiteral("Could not record the restored state: %1").arg(published.error);
        }
    }
    if (!result.error.isEmpty()) {
        QStringList failedRollbacks;
        while (applied > 0) {
            const Change &change = changes.at(--applied);
            const QString expected = after.contains(change.relativePath) ? after[change.relativePath].blob : QString();
            if (!safeDestination(change.relativePath)
                || !matchesBlob(QDir(root).filePath(change.relativePath), expected) || !apply(change, true)) {
                failedRollbacks.append(change.relativePath);
            }
        }
        if (!failedRollbacks.isEmpty()) {
            result.error += QStringLiteral("\nRollback incomplete: %1. Restore safety checkpoint %2.")
                                .arg(failedRollbacks.join(QStringLiteral(", ")), safety.checkpointId);
            return result;
        }
        if (recovering) {
            return result;
        }
    }
    if (!QFile::remove(journalPath)) {
        result.error += QStringLiteral("\nCould not clear the recovery record. Safety checkpoint: %1.").arg(safety.checkpointId);
        return result;
    }
    result.succeeded = result.error.isEmpty();
    return result;
}
} // namespace TherionStudio
