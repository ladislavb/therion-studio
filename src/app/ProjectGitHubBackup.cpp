#include "ProjectGitHubBackup.h"
#include "ProjectVersioningStore.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>

#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
struct CommandResult
{
    bool ok = false;
    QByteArray output;
};

QString nullDevice()
{
#ifdef Q_OS_WIN
    return QStringLiteral("NUL");
#else
    return QStringLiteral("/dev/null");
#endif
}

QProcessEnvironment environment(bool git)
{
    auto env = QProcessEnvironment::systemEnvironment();
    for (const QString &key : env.keys()) {
        if (key.startsWith(QStringLiteral("GIT_"), Qt::CaseInsensitive)
            || key.startsWith(QStringLiteral("GH_"), Qt::CaseInsensitive)
            || key.startsWith(QStringLiteral("GITHUB_"), Qt::CaseInsensitive)
            || key == QStringLiteral("SSH_ASKPASS")) {
            env.remove(key);
        }
    }
    env.insert(QStringLiteral("GH_HOST"), QStringLiteral("github.com"));
    env.insert(QStringLiteral("GH_PROMPT_DISABLED"), QStringLiteral("1"));
    env.insert(QStringLiteral("GH_NO_UPDATE_NOTIFIER"), QStringLiteral("1"));
    env.insert(QStringLiteral("GH_NO_EXTENSION_UPDATE_NOTIFIER"), QStringLiteral("1"));
    if (git) {
        env.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
        env.insert(QStringLiteral("GIT_CONFIG_SYSTEM"), nullDevice());
        env.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), nullDevice());
        env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
        env.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
        env.insert(QStringLiteral("GIT_ATTR_NOSYSTEM"), QStringLiteral("1"));
        env.insert(QStringLiteral("GIT_NO_REPLACE_OBJECTS"), QStringLiteral("1"));
        env.insert(QStringLiteral("GIT_ALLOW_PROTOCOL"), QStringLiteral("https"));
        for (const QString &role : {QStringLiteral("AUTHOR"), QStringLiteral("COMMITTER")}) {
            env.insert(QStringLiteral("GIT_%1_NAME").arg(role), QStringLiteral("Therion Studio Backup"));
            env.insert(QStringLiteral("GIT_%1_EMAIL").arg(role), QStringLiteral("backup@therion-studio.invalid"));
        }
    }
    return env;
}

CommandResult run(const QString &executable, const QStringList &arguments,
                  const QProcessEnvironment &env, const QString &workingDirectory,
                  const QByteArray &input = {})
{
    QProcess process;
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(workingDirectory);
    process.start(executable, arguments);
    if (!process.waitForStarted(5000)) {
        return {};
    }
    if (!input.isEmpty()) {
        process.write(input);
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    // Child output is never included in diagnostics: it can contain credentials.
    return {process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
            process.readAllStandardOutput()};
}

ProjectGitHubBackup::Result failure(const char *stage, const char *message)
{
    qWarning().noquote() << "github_backup stage=" + QString::fromLatin1(stage)
                         << "status=failed" << QString::fromLatin1(message);
    ProjectGitHubBackup::Result result;
    result.error = QString::fromLatin1(message);
    return result;
}

bool validName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral(
        "\\A[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?/[A-Za-z0-9_.-]{1,100}\\z"));
    return pattern.match(name).hasMatch() && !name.endsWith(QStringLiteral("/."))
        && !name.endsWith(QStringLiteral("/..")) && !name.endsWith(QStringLiteral(".git"), Qt::CaseInsensitive);
}

bool validOid(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9a-f]{40}\\z"));
    return pattern.match(value).hasMatch();
}

bool validMessage(const QString &message)
{
    if (message.trimmed().isEmpty() || message.toUtf8().size() > 4096) {
        return false;
    }
    for (const QChar character : message) {
        if ((character.unicode() < 32 && character != QLatin1Char('\n')
             && character != QLatin1Char('\t')) || character.unicode() == 127) {
            return false;
        }
    }
    return true;
}

bool within(const QString &parent, const QString &child)
{
    const QString relative = QDir(parent).relativeFilePath(child);
    return relative == QStringLiteral(".")
        || (relative != QStringLiteral("..") && !relative.startsWith(QStringLiteral("../"))
            && !QDir::isAbsolutePath(relative));
}

bool noSymlinks(const QString &path)
{
    QString current = QFileInfo(path).absoluteFilePath();
    for (;;) {
        const QFileInfo info(current);
        if (info.isSymLink() || (info.exists() && !info.isDir())) {
            return false;
        }
        const QString parent = info.absolutePath();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    QDirIterator iterator(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (info.isSymLink() || (!info.isDir() && !info.isFile())) {
            return false;
        }
    }
    return true;
}

bool save(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly)
        && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        && file.write(data) == data.size() && file.commit();
}

bool excluded(const QByteArray &path)
{
    const auto parts = path.toLower().split('/');
    for (const auto &part : parts) {
        if (part == ".git" || part == "output" || part == "outputs") {
            return true;
        }
    }
    const QByteArray lower = path.toLower();
    return lower.endsWith(".3d") || lower.endsWith(".lox") || lower.endsWith(".plt");
}
} // namespace

ProjectGitHubBackup::ProjectGitHubBackup(QString root, QString stateDirectory,
                                       QString ghExecutable, QString gitExecutable)
    : root_(QDir::cleanPath(QFileInfo(std::move(root)).absoluteFilePath()))
    , stateDirectory_(QDir::cleanPath(QFileInfo(std::move(stateDirectory)).absoluteFilePath()))
    , ghExecutable_(std::move(ghExecutable))
    , gitExecutable_(std::move(gitExecutable))
{
}

ProjectGitHubBackup::Result ProjectGitHubBackup::check(const QString &repositoryFullName) const
{
    return perform(repositoryFullName, nullptr);
}

ProjectGitHubBackup::Result ProjectGitHubBackup::upload(const QString &repositoryFullName,
                                                      const QString &commitMessage) const
{
    return perform(repositoryFullName, &commitMessage);
}

ProjectGitHubBackup::Result ProjectGitHubBackup::perform(const QString &repositoryFullName,
                                                       const QString *commitMessage) const
{
    if (!validName(repositoryFullName) || (commitMessage && !validMessage(*commitMessage))) {
        return failure("validation", "Enter a valid owner/repository and a nonempty commit message (at most 4096 bytes, no control characters).");
    }
    const QString root = QFileInfo(root_).canonicalFilePath();
    if (root.isEmpty() || !QFileInfo(root).isDir() || gitExecutable_.trimmed().isEmpty()
        || within(root, stateDirectory_) || within(stateDirectory_, root) || !noSymlinks(stateDirectory_)) {
        return failure("storage", "Backup state must be a nonsymlink directory outside and separate from the existing project.");
    }
    if (!QDir().mkpath(stateDirectory_)) {
        return failure("storage", "Could not create the backup state directory.");
    }
    QLockFile lock(QDir(stateDirectory_).filePath(QStringLiteral("backup.lock")));
    if (!lock.tryLock(0)) {
        return failure("lock", "The GitHub backup is busy or could not be locked.");
    }
    if (!noSymlinks(stateDirectory_)) {
        return failure("storage", "Backup state contains a symlink or unsupported file.");
    }
    // Each destination has independent identity and pending-push state, sharing only local snapshots.
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        repositoryFullName.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString statePath = QDir(stateDirectory_).filePath(key + QStringLiteral(".json"));
    QJsonObject state;
    if (QFileInfo::exists(statePath)) {
        QFile file(statePath);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16384) {
            return failure("state", "Could not read backup state.");
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
            return failure("state", "Backup state is invalid; refusing to upload.");
        }
        state = document.object();
        if (state.value(QStringLiteral("schema")).toInt() != 1
            || state.value(QStringLiteral("root")).toString() != root
            || state.value(QStringLiteral("repository")).toString() != repositoryFullName
            || !QRegularExpression(QStringLiteral("\\A[1-9][0-9]*\\z")).match(
                state.value(QStringLiteral("repositoryId")).toString()).hasMatch()) {
            return failure("state", "Backup state is not bound to this project and repository.");
        }
        for (const QString &field : {QStringLiteral("lastCommit"), QStringLiteral("lastTree"),
                                      QStringLiteral("pendingCommit"), QStringLiteral("pendingTree")}) {
            if (!state.value(field).isString()
                || (!state.value(field).toString().isEmpty() && !validOid(state.value(field).toString()))) {
                return failure("state", "Backup state contains an invalid object identifier.");
            }
        }
        if (state.value(QStringLiteral("lastCommit")).toString().isEmpty()
                != state.value(QStringLiteral("lastTree")).toString().isEmpty()
            || state.value(QStringLiteral("pendingCommit")).toString().isEmpty()
                != state.value(QStringLiteral("pendingTree")).toString().isEmpty()) {
            return failure("state", "Backup state contains an incomplete commit record.");
        }
    }
    const QString snapshotPath = QDir(stateDirectory_).filePath(QStringLiteral("snapshot.git"));
    const auto git = [&](const QStringList &command, const QByteArray &input = QByteArray(),
                         const QByteArray &token = QByteArray()) {
        auto env = environment(true);
        if (!token.isEmpty()) {
            env.insert(QStringLiteral("GIT_CONFIG_COUNT"), QStringLiteral("1"));
            env.insert(QStringLiteral("GIT_CONFIG_KEY_0"), QStringLiteral("http.https://github.com/.extraheader"));
            env.insert(QStringLiteral("GIT_CONFIG_VALUE_0"), QStringLiteral("Authorization: Basic ")
                       + QString::fromLatin1((QByteArrayLiteral("x-access-token:") + token).toBase64()));
        }
        QStringList args{QStringLiteral("--no-replace-objects"),
                         QStringLiteral("--git-dir=%1").arg(snapshotPath),
                         QStringLiteral("-c"), QStringLiteral("core.hooksPath=%1").arg(nullDevice()),
                         QStringLiteral("-c"), QStringLiteral("credential.helper="),
                         QStringLiteral("-c"), QStringLiteral("http.followRedirects=false"),
                         QStringLiteral("-c"), QStringLiteral("http.sslVerify=true"),
                         QStringLiteral("-c"), QStringLiteral("push.followTags=false"),
                         QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false")};
        args.append(command);
        return run(gitExecutable_, args, env, stateDirectory_, input);
    };
    const auto gh = [&](const QStringList &args) {
        return run(ghExecutable_, args, environment(false), stateDirectory_);
    };

    QByteArray token;
    QString remoteUrl;
    if (commitMessage) {
        if (ghExecutable_.trimmed().isEmpty()
            || !gh({QStringLiteral("auth"), QStringLiteral("status"), QStringLiteral("--hostname"),
                    QStringLiteral("github.com")}).ok) {
            return failure("authentication", "GitHub CLI is missing or not signed in to github.com. Sign in using gh auth login.");
        }
        const auto authentication = gh({QStringLiteral("auth"), QStringLiteral("token"),
                                        QStringLiteral("--hostname"), QStringLiteral("github.com")});
        token = authentication.output.trimmed();
        if (!authentication.ok || token.isEmpty() || token.size() > 4096
            || token.contains('\r') || token.contains('\n') || token.contains('\0')
            || token.contains(' ') || token.contains('\t')) {
            return failure("authentication", "Could not obtain GitHub credentials from GitHub CLI.");
        }
        const auto metadata = gh({QStringLiteral("api"), QStringLiteral("--hostname"),
                                  QStringLiteral("github.com"), QStringLiteral("repos/") + repositoryFullName});
        const QJsonDocument document = QJsonDocument::fromJson(metadata.output);
        const QJsonObject repository = document.object();
        const double id = repository.value(QStringLiteral("id")).toDouble();
        if (!metadata.ok || !document.isObject() || !repository.value(QStringLiteral("private")).isBool()
            || !repository.value(QStringLiteral("private")).toBool()
            || repository.value(QStringLiteral("visibility")).toString() != QStringLiteral("private")
            || repository.value(QStringLiteral("full_name")).toString() != repositoryFullName
            || !repository.value(QStringLiteral("permissions")).toObject().value(QStringLiteral("push")).toBool()
            || !std::isfinite(id) || id < 1 || id > 9007199254740991.0 || std::floor(id) != id) {
            return failure("repository", "The destination must be the exact, verified private github.com repository with push permission.");
        }
        const QString repositoryId = QString::number(id, 'f', 0);
        if (!state.isEmpty() && state.value(QStringLiteral("repositoryId")).toString() != repositoryId) {
            return failure("identity", "The remote repository identity changed; refusing to upload.");
        }
        if (state.isEmpty()) {
            state = {{QStringLiteral("schema"), 1}, {QStringLiteral("root"), root},
                     {QStringLiteral("repository"), repositoryFullName},
                     {QStringLiteral("repositoryId"), repositoryId},
                     {QStringLiteral("lastCommit"), QString()}, {QStringLiteral("lastTree"), QString()},
                     {QStringLiteral("pendingCommit"), QString()}, {QStringLiteral("pendingTree"), QString()}};
        }
        remoteUrl = QStringLiteral("https://github.com/%1.git").arg(repositoryFullName);
    }

    // Refuse injected includes, URL rewrites, helpers, or object alternates in persisted state.
    if (QFileInfo::exists(snapshotPath)) {
        if (QFileInfo::exists(QDir(snapshotPath).filePath(QStringLiteral("objects/info/alternates")))) {
            return failure("storage", "Backup snapshot object alternates are not permitted.");
        }
        const auto config = git({QStringLiteral("config"), QStringLiteral("--local"),
                                 QStringLiteral("--no-includes"), QStringLiteral("--null"),
                                 QStringLiteral("--list")});
        if (!config.ok) {
            return failure("storage", "Could not validate backup snapshot configuration.");
        }
        for (const QByteArray &entry : config.output.split('\0')) {
            if (entry.isEmpty()) {
                continue;
            }
            const qsizetype newline = entry.indexOf('\n');
            const QByteArray name = entry.left(newline);
            const QByteArray value = entry.mid(newline + 1);
            const bool safe = (name == "core.repositoryformatversion" && value == "0")
                || (name == "core.bare" && value == "true")
                || ((name == "core.filemode" || name == "core.ignorecase" || name == "core.symlinks"
                     || name == "core.precomposeunicode") && (value == "true" || value == "false"))
                || (name == "therionstudio.versioningstore" && value == "true")
                || (name == "therionstudio.projectroot" && value == root.toUtf8());
            if (!safe) {
                return failure("storage", "Backup snapshot configuration contains unsupported settings.");
            }
        }
    }
    ProjectVersioningStore snapshot(root, snapshotPath, gitExecutable_);
    const auto captured = snapshot.createCheckpoint(QStringLiteral("GitHub backup capture"));
    if (!captured.succeeded) {
        return failure("snapshot", "Could not capture included project files. Save files, check Git and ignore rules, then retry.");
    }
    // Rebuild a tree, not a commit: no local safety-history ancestors can become reachable in a push.
    const auto entries = git({QStringLiteral("ls-tree"), QStringLiteral("-rz"), captured.checkpointId});
    if (!entries.ok) {
        return failure("snapshot", "Could not inspect the included project snapshot.");
    }
    const QString indexPath = QDir(stateDirectory_).filePath(QStringLiteral("upload.index"));
    QFile::remove(indexPath);
    QByteArray indexEntries;
    for (const QByteArray &entry : entries.output.split('\0')) {
        if (entry.isEmpty()) {
            continue;
        }
        const qsizetype tab = entry.indexOf('\t');
        const qsizetype space = entry.indexOf(' ');
        if (tab < 0 || space < 0 || entry.mid(space + 1, 4) != "blob") {
            return failure("snapshot", "The snapshot contains an unsupported entry.");
        }
        const QByteArray path = entry.mid(tab + 1);
        if (!excluded(path)) {
            indexEntries += entry.left(space) + ' ' + entry.mid(tab - 40, 40) + '\t' + path + '\0';
        }
    }
    // A private index never stages into the project's real Git index.
    auto indexEnv = environment(true);
    indexEnv.insert(QStringLiteral("GIT_INDEX_FILE"), indexPath);
    const QStringList indexArgs{QStringLiteral("--git-dir=%1").arg(snapshotPath)};
    auto step = [&](const QStringList &command, const QByteArray &input = QByteArray()) {
        QStringList args = indexArgs;
        args.append(command);
        return run(gitExecutable_, args, indexEnv, stateDirectory_, input);
    };
    if (!step({QStringLiteral("read-tree"), QStringLiteral("--empty")}).ok
        || !step({QStringLiteral("update-index"), QStringLiteral("-z"), QStringLiteral("--index-info")}, indexEntries).ok) {
        QFile::remove(indexPath);
        return failure("snapshot", "Could not filter the upload snapshot.");
    }
    const auto tree = step({QStringLiteral("write-tree")});
    QFile::remove(indexPath);
    const QString treeId = QString::fromLatin1(tree.output).trimmed();
    if (!tree.ok || !validOid(treeId)) {
        return failure("snapshot", "Could not prepare the upload tree.");
    }
    Result result;
    result.changed = treeId != state.value(QStringLiteral("lastTree")).toString();
    result.commitId = state.value(QStringLiteral("lastCommit")).toString();
    if (!commitMessage) {
        result.succeeded = true;
        return result;
    }
    const auto remoteTip = [&]() -> CommandResult {
        const auto refs = git({QStringLiteral("ls-remote"), QStringLiteral("--refs"), remoteUrl}, {}, token);
        if (!refs.ok) {
            return {};
        }
        QByteArray tip;
        for (const auto &line : refs.output.split('\n')) {
            if (line.isEmpty()) {
                continue;
            }
            const auto columns = line.split('\t');
            if (columns.size() != 2 || columns.at(1) != "refs/heads/main"
                || !validOid(QString::fromLatin1(columns.at(0))) || !tip.isEmpty()) {
                return {};
            }
            tip = columns.at(0);
        }
        return {true, tip};
    };
    const auto remote = remoteTip();
    if (!remote.ok) {
        return failure("remote", "Could not verify remote refs, or the destination contains foreign branches or tags. Nothing was overwritten.");
    }
    const QString backupRef = QStringLiteral("refs/therionstudio/github/%1/").arg(key);
    const auto promotePending = [&]() {
        if (!git({QStringLiteral("update-ref"), backupRef + QStringLiteral("uploaded"),
                  state.value(QStringLiteral("pendingCommit")).toString()}).ok) {
            return false;
        }
        state.insert(QStringLiteral("lastCommit"), state.value(QStringLiteral("pendingCommit")));
        state.insert(QStringLiteral("lastTree"), state.value(QStringLiteral("pendingTree")));
        state.insert(QStringLiteral("pendingCommit"), QString());
        state.insert(QStringLiteral("pendingTree"), QString());
        if (!save(statePath, state)) {
            return false;
        }
        git({QStringLiteral("update-ref"), QStringLiteral("-d"), backupRef + QStringLiteral("pending")});
        return true;
    };
    QString expected = state.value(QStringLiteral("lastCommit")).toString();
    QString pending = state.value(QStringLiteral("pendingCommit")).toString();
    const QString observed = QString::fromLatin1(remote.output);
    bool reconciled = false;
    if (!pending.isEmpty()) {
        const auto record = git({QStringLiteral("show"), QStringLiteral("-s"),
                                  QStringLiteral("--format=%T%n%P"), pending});
        const QByteArray expectedRecord = state.value(QStringLiteral("pendingTree")).toString().toLatin1()
            + '\n' + expected.toLatin1() + '\n';
        if (!record.ok || record.output != expectedRecord) {
            return failure("state", "The pending backup commit does not match its saved state.");
        }
        if (observed != pending && observed != expected) {
            return failure("conflict", "The remote changed outside this backup. Nothing was overwritten.");
        }
        if (observed != pending) {
            git({QStringLiteral("push"), QStringLiteral("--porcelain"), QStringLiteral("--no-verify"),
                 remoteUrl, pending + QStringLiteral(":refs/heads/main")}, {}, token);
            const auto confirmed = remoteTip();
            if (!confirmed.ok || QString::fromLatin1(confirmed.output) != pending) {
                return failure("push", "The pending upload could not be confirmed. Retry safely to reconcile.");
            }
        }
        if (!promotePending()) {
            return failure("state", "The remote backup was confirmed, but local state could not be saved. Retry to reconcile.");
        }
        expected = pending;
        pending.clear();
        reconciled = true;
    } else if (observed != expected) {
        return failure("conflict", "First upload requires an empty repository; later uploads require the exact last backup tip. Nothing was overwritten.");
    }
    if (pending.isEmpty() && treeId == state.value(QStringLiteral("lastTree")).toString()) {
        result.succeeded = true;
        result.changed = reconciled;
        result.commitId = expected;
        return result;
    }
    if (pending.isEmpty()) {
        QStringList args{QStringLiteral("commit-tree"), treeId};
        if (!expected.isEmpty()) {
            args.append({QStringLiteral("-p"), expected});
        }
        const auto commit = git(args, commitMessage->toUtf8() + '\n');
        pending = QString::fromLatin1(commit.output).trimmed();
        if (!commit.ok || !validOid(pending)) {
            return failure("commit", "Could not create the isolated backup commit.");
        }
        // Keep unconfirmed commits alive across retries and Git garbage collection.
        if (!git({QStringLiteral("update-ref"), backupRef + QStringLiteral("pending"), pending}).ok) {
            return failure("state", "Could not retain the pending backup commit. Nothing was pushed.");
        }
        state.insert(QStringLiteral("pendingCommit"), pending);
        state.insert(QStringLiteral("pendingTree"), treeId);
        if (!save(statePath, state)) {
            return failure("state", "Could not save pending backup state. Nothing was pushed.");
        }
    }
    // No force flags, local history refs, tags, or configured remotes are ever pushed.
    git({QStringLiteral("push"), QStringLiteral("--porcelain"), QStringLiteral("--no-verify"),
         remoteUrl, pending + QStringLiteral(":refs/heads/main")}, {}, token);
    const auto confirmation = remoteTip();
    if (!confirmation.ok || QString::fromLatin1(confirmation.output) != pending) {
        return failure("push", "The upload could not be confirmed. Pending state was retained; retry safely to reconcile. Foreign changes will not be overwritten.");
    }
    if (!promotePending()) {
        return failure("state", "The remote backup was confirmed, but local state could not be saved. Retry to reconcile.");
    }
    result.succeeded = true;
    result.changed = true;
    result.commitId = pending;
    return result;
}
} // namespace TherionStudio
