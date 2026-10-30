#include "../src/app/ProjectGitHubBackup.h"
#include "../src/app/ProjectVersioningStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>

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

bool writeFile(const QString &path, const QByteArray &data)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct Fixture
{
    QTemporaryDir directory{QDir::current().filePath(QStringLiteral("github-backup-fixture-XXXXXX"))};
    QString root = directory.path() + QStringLiteral("/project");
    QString state = directory.path() + QStringLiteral("/state");
    QString remote = directory.path() + QStringLiteral("/remote.git");
    QString gh = directory.path() + QStringLiteral("/gh.py");
    QString git = directory.path() + QStringLiteral("/git.py");
    QString realGit = QStandardPaths::findExecutable(QStringLiteral("git"));
    QJsonObject policy{{QStringLiteral("id"), 12345}, {QStringLiteral("auth"), true},
                       {QStringLiteral("private"), true}, {QStringLiteral("visibility"), QStringLiteral("private")},
                       {QStringLiteral("full_name"), QStringLiteral("owner/backup")},
                       {QStringLiteral("push"), true}};

    QByteArray command(const QStringList &arguments, bool *ok = nullptr) const
    {
        QProcess process;
        auto env = QProcessEnvironment::systemEnvironment();
        for (const QString &key : env.keys()) {
            if (key.startsWith(QStringLiteral("GIT_"))) {
                env.remove(key);
            }
        }
        env.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("/dev/null"));
        env.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
        env.insert(QStringLiteral("GIT_AUTHOR_NAME"), QStringLiteral("Foreign User"));
        env.insert(QStringLiteral("GIT_AUTHOR_EMAIL"), QStringLiteral("foreign@example.invalid"));
        env.insert(QStringLiteral("GIT_COMMITTER_NAME"), QStringLiteral("Foreign User"));
        env.insert(QStringLiteral("GIT_COMMITTER_EMAIL"), QStringLiteral("foreign@example.invalid"));
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(directory.path());
        process.start(realGit, arguments);
        const bool success = process.waitForFinished(10000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (ok) {
            *ok = success;
        }
        if (!success) {
            std::cerr << process.readAllStandardError().toStdString();
        }
        return process.readAllStandardOutput().trimmed();
    }

    QByteArray remoteCommand(const QStringList &args) const
    {
        QStringList arguments{QStringLiteral("--git-dir=%1").arg(remote)};
        arguments.append(args);
        return command(arguments);
    }

    bool savePolicy()
    {
        policy.insert(QStringLiteral("git"), realGit);
        return writeFile(directory.path() + QStringLiteral("/policy.json"), QJsonDocument(policy).toJson());
    }

    ProjectGitHubBackup backup() const { return ProjectGitHubBackup(root, state, gh, git); }

    bool initialize()
    {
        if (!directory.isValid() || realGit.isEmpty() || !QDir().mkpath(root) || !savePolicy()) {
            return false;
        }
        bool ok = false;
        command({QStringLiteral("init"), QStringLiteral("--quiet"), QStringLiteral("--bare"), remote}, &ok);
        const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        if (!ok || python.isEmpty()) {
            return false;
        }
        const QByteArray ghScript = QByteArrayLiteral(R"PY(import json, os, pathlib, sys
base = pathlib.Path(__file__).resolve().parent
policy = json.loads((base / 'policy.json').read_text())
args = sys.argv[1:]
assert not os.environ.get('GH_TOKEN') and not os.environ.get('GITHUB_TOKEN')
with (base / 'gh.log').open('a') as stream:
    stream.write(str(args) + '\n')
if args == ['auth', 'status', '--hostname', 'github.com']:
    if not policy['auth']:
        sys.stderr.write('fixture-token-not-real: deliberately sensitive failure')
        sys.exit(1)
elif args == ['auth', 'token', '--hostname', 'github.com']:
    print('fixture-token-not-real')
elif args == ['api', '--hostname', 'github.com', 'repos/owner/backup']:
    print(json.dumps(dict(policy, permissions={'push': policy['push']})))
else:
    sys.exit(91)
)PY");
        const QByteArray gitScript = QByteArrayLiteral(R"PY(import base64, json, os, pathlib, subprocess, sys
base = pathlib.Path(__file__).resolve().parent
policy = json.loads((base / 'policy.json').read_text())
args = sys.argv[1:]
assert all('fixture-token-not-real' not in arg for arg in args)
url = 'https://github.com/owner/backup.git'
if any('https://' in arg for arg in args):
    assert url in args
    assert os.environ.get('GIT_CONFIG_COUNT') == '1'
    assert os.environ.get('GIT_CONFIG_KEY_0') == 'http.https://github.com/.extraheader'
    expected = 'Authorization: Basic ' + base64.b64encode(b'x-access-token:fixture-token-not-real').decode()
    assert os.environ.get('GIT_CONFIG_VALUE_0') == expected
    assert '--force' not in args and '--force-with-lease' not in args
    operation = 'push' if 'push' in args else 'ls-remote'
    with (base / 'network.log').open('a') as stream:
        stream.write(operation + '\n')
    if operation == 'ls-remote' and (base / 'fail-reads').exists():
        sys.stderr.write('fixture-token-not-real: simulated transport failure')
        sys.exit(23)
    if operation == 'push' and (base / 'fail-before-push').exists():
        sys.exit(24)
    args = [str(base / 'remote.git') if arg == url else arg for arg in args]
    os.environ['GIT_ALLOW_PROTOCOL'] = 'file'
    result = subprocess.call([policy['git']] + args)
    if operation == 'push' and (base / 'ambiguous').exists():
        (base / 'fail-reads').touch()
        sys.exit(25)
    sys.exit(result)
os.execv(policy['git'], [policy['git']] + args)
)PY");
        const auto executable = [&](const QString &path, const QByteArray &script) {
            return writeFile(path, "#!" + python.toUtf8() + '\n' + script)
                && QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        };
        return executable(gh, ghScript) && executable(git, gitScript)
            && writeFile(root + QStringLiteral("/survey.th"), "survey cave\nendsurvey\n");
    }
};

int snapshotAndHistoryTest()
{
    Fixture f;
    if (!expect(f.initialize(), "Could not initialize backup fixtures.")) {
        return 1;
    }
    // A local capture contains an old secret that is ignored before the first upload.
    if (!writeFile(f.root + QStringLiteral("/old.secret"), "LOCAL-HISTORY-ONLY")
        || !f.backup().check(QStringLiteral("owner/backup")).succeeded
        || !writeFile(f.root + QStringLiteral("/.gitignore"), "old.secret\nignored/\nexports/\n!outputs/\n!outputs/forced.txt\n")
        || !writeFile(f.root + QStringLiteral("/ignored/private.txt"), "ignored")
        || !writeFile(f.root + QStringLiteral("/outputs/forced.txt"), "never upload")
        || !writeFile(f.root + QStringLiteral("/output/model.txt"), "never upload")
        || !writeFile(f.root + QStringLiteral("/exports/map.pdf"), "never upload")
        || !writeFile(f.root + QStringLiteral("/cave.3d"), "never upload")
        || !writeFile(f.root + QStringLiteral("/cave.lox"), "never upload")
        || !writeFile(f.root + QStringLiteral("/cave.plt"), "never upload")
        || !writeFile(f.root + QStringLiteral("/input.xvi"), "preserve input")
        || !writeFile(f.root + QStringLiteral("/input.svg"), "preserve svg input")
        || !writeFile(f.root + QStringLiteral("/input.pdf"), "preserve pdf input")
        || !writeFile(f.root + QStringLiteral("/.git/untouched"), "do not change")
        || !writeFile(f.root + QStringLiteral("/odd\nname.th"), "unusual filename")) {
        return 1;
    }
    auto backup = f.backup();
    const auto check = backup.check(QStringLiteral("owner/backup"));
    if (!expect(check.succeeded && check.changed, "Initial included tree must be changed.")
        || !expect(!QFileInfo::exists(f.directory.path() + QStringLiteral("/gh.log"))
                   && !QFileInfo::exists(f.directory.path() + QStringLiteral("/network.log")),
                   "check() must never authenticate or contact the network.")) {
        return 1;
    }
    qputenv("GH_TOKEN", "poison-inherited-token");
    qputenv("GITHUB_TOKEN", "poison-inherited-token");
    const auto uploaded = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("First saved backup"));
    qunsetenv("GH_TOKEN");
    qunsetenv("GITHUB_TOKEN");
    if (!expect(uploaded.succeeded && uploaded.changed && !uploaded.commitId.isEmpty(),
                "First private upload should succeed.")) {
        std::cerr << uploaded.error.toStdString() << '\n';
        return 1;
    }
    const QByteArray files = f.remoteCommand({QStringLiteral("ls-tree"), QStringLiteral("-rz"), QStringLiteral("main")});
    if (!expect(files.contains("survey.th") && files.contains(".gitignore") && files.contains("odd\nname.th")
                && files.contains("input.xvi") && files.contains("input.svg") && files.contains("input.pdf"),
                "The exact included source tree, including non-ASCII-safe path encoding and inputs, must be uploaded.")
        || !expect(!files.contains("old.secret") && !files.contains("private.txt") && !files.contains("outputs/")
                   && !files.contains("output/") && !files.contains("exports/") && !files.contains(".git/")
                   && !files.contains("cave.3d") && !files.contains("cave.lox") && !files.contains("cave.plt"),
                   "Ignored, generated, and Git metadata files must never be uploaded.")
        || !expect(f.remoteCommand({QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("main")}) == "1",
                   "The first upload must be a root commit, not local snapshot history.")
        || !expect(!f.remoteCommand({QStringLiteral("rev-list"), QStringLiteral("--objects"), QStringLiteral("--all")}).contains("old.secret"),
                   "Earlier local secrets must not be reachable from remote history.")
        || !expect(f.remoteCommand({QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%an <%ae>"), QStringLiteral("main")})
                   == "Therion Studio Backup <backup@therion-studio.invalid>", "Commit identity must not expose user PII.")
        || !expect(readFile(f.root + QStringLiteral("/.git/untouched")) == "do not change", "Project .git must remain untouched.")) {
        return 1;
    }
    const QByteArray oldBlob = f.command({QStringLiteral("hash-object"), f.root + QStringLiteral("/old.secret")});
    bool oldBlobUploaded = false;
    f.command({QStringLiteral("--git-dir=%1").arg(f.remote), QStringLiteral("cat-file"),
               QStringLiteral("-e"), QString::fromLatin1(oldBlob)}, &oldBlobUploaded);
    if (!expect(!oldBlobUploaded, "Objects from excluded local history must not even exist in the remote object database.")) {
        return 1;
    }
    QDirIterator stateFiles(f.state, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (stateFiles.hasNext()) {
        if (!expect(!readFile(stateFiles.next()).contains("fixture-token-not-real"),
                    "GitHub credentials must never be persisted to state or Git configuration.")) {
            return 1;
        }
    }
    const QByteArray networkBefore = readFile(f.directory.path() + QStringLiteral("/network.log"));
    const auto unchanged = backup.check(QStringLiteral("owner/backup"));
    if (!expect(unchanged.succeeded && !unchanged.changed && unchanged.commitId == uploaded.commitId,
                "check() should compare against the confirmed upload.")
        || !expect(readFile(f.directory.path() + QStringLiteral("/network.log")) == networkBefore,
                   "Even an established check() must stay offline.")) {
        return 1;
    }
    const auto noOp = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("No changes"));
    if (!expect(noOp.succeeded && !noOp.changed && noOp.commitId == uploaded.commitId,
                "Uploading the same tree must be a no-op.")
        || !expect(readFile(f.directory.path() + QStringLiteral("/network.log")).count("push\n") == 1,
                   "No-op upload must not push.")) {
        return 1;
    }
    writeFile(f.root + QStringLiteral("/survey.th"), "changed source");
    const auto second = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Second backup"));
    if (!expect(second.succeeded && second.changed && second.commitId != uploaded.commitId,
                "A second included tree must append one backup commit.")
        || !expect(f.remoteCommand({QStringLiteral("rev-parse"), QStringLiteral("main^")}) == uploaded.commitId.toLatin1(),
                   "Synthetic history must parent only the prior uploaded commit.")) {
        return 1;
    }
    f.policy.insert(QStringLiteral("id"), 54321);
    f.savePolicy();
    if (!expect(!backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Identity changed")).succeeded,
                "A recreated/renamed repository must not silently replace the bound identity.")) {
        return 1;
    }
    return 0;
}

int rejectionTest()
{
    Fixture f;
    if (!f.initialize()) {
        return 1;
    }
    auto backup = f.backup();
    for (const QString &name : {QStringLiteral("https://github.com/owner/backup"), QStringLiteral("owner/../backup"),
                                QStringLiteral("--evil/backup"), QStringLiteral("owner/backup.git"), QStringLiteral("owner/repo\n")}) {
        if (!expect(!backup.check(name).succeeded, "Invalid repository names must be rejected before any external operation.")) {
            return 1;
        }
    }
    if (!expect(!backup.upload(QStringLiteral("owner/backup"), QString()).succeeded
                && !backup.upload(QStringLiteral("owner/backup"), QString(QLatin1Char('\0'))).succeeded,
                "Empty and control-character commit messages must be rejected.")) {
        return 1;
    }
    f.policy.insert(QStringLiteral("auth"), false);
    f.savePolicy();
    const auto noAuth = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Missing auth"));
    if (!expect(!noAuth.succeeded && !noAuth.error.contains(QStringLiteral("fixture-token")),
                "Authentication failures must be sanitized.")) {
        return 1;
    }
    f.policy.insert(QStringLiteral("auth"), true);
    for (const auto &setting : {QPair<QString, QJsonValue>{QStringLiteral("private"), false},
                                QPair<QString, QJsonValue>{QStringLiteral("visibility"), QStringLiteral("public")},
                                QPair<QString, QJsonValue>{QStringLiteral("push"), false},
                                QPair<QString, QJsonValue>{QStringLiteral("full_name"), QStringLiteral("other/backup")}}) {
        const auto previous = f.policy.value(setting.first);
        f.policy.insert(setting.first, setting.second);
        f.savePolicy();
        if (!expect(!backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Unsafe target")).succeeded,
                    "Public, non-writable, or mismatched repositories must be rejected.")) {
            return 1;
        }
        f.policy.insert(setting.first, previous);
    }
    f.savePolicy();
    if (!expect(!QFileInfo::exists(f.directory.path() + QStringLiteral("/network.log")),
                "Rejected metadata must prevent Git remote operations.")) {
        return 1;
    }
    ProjectGitHubBackup inside(f.root, f.root + QStringLiteral("/state"), f.gh, f.git);
    if (!expect(!inside.check(QStringLiteral("owner/backup")).succeeded, "State inside the project must be rejected.")) {
        return 1;
    }
    if (!backup.check(QStringLiteral("owner/backup")).succeeded) {
        return 1;
    }
    QLockFile lock(f.state + QStringLiteral("/backup.lock"));
    if (!expect(lock.tryLock(0) && !backup.check(QStringLiteral("owner/backup")).succeeded,
                "Concurrent backend operations must be locked.")) {
        return 1;
    }
    lock.unlock();
    const QString target = f.directory.path() + QStringLiteral("/must-not-read");
    writeFile(target, "foreign state");
    if (!expect(QFile::link(target, f.state + QStringLiteral("/unsafe-link"))
                && !backup.check(QStringLiteral("owner/backup")).succeeded,
                "Symlinks anywhere in backup state must be rejected.")) {
        return 1;
    }
    return 0;
}

int foreignChangesTest()
{
    Fixture f;
    if (!f.initialize()) {
        return 1;
    }
    auto backup = f.backup();
    const auto first = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Initial"));
    if (!first.succeeded) {
        return 1;
    }
    const QString tree = QString::fromLatin1(f.remoteCommand({QStringLiteral("rev-parse"), QStringLiteral("main^{tree}")}));
    const QByteArray foreign = f.remoteCommand({QStringLiteral("commit-tree"), tree, QStringLiteral("-p"),
                                               first.commitId, QStringLiteral("-m"), QStringLiteral("Foreign edit")});
    f.remoteCommand({QStringLiteral("update-ref"), QStringLiteral("refs/heads/main"), QString::fromLatin1(foreign)});
    writeFile(f.root + QStringLiteral("/survey.th"), "new source");
    if (!expect(!backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Must not overwrite")).succeeded,
                "Foreign remote commits must cause a conflict.")
        || !expect(f.remoteCommand({QStringLiteral("rev-parse"), QStringLiteral("main")}) == foreign,
                   "A conflict must leave the foreign remote tip unchanged.")) {
        return 1;
    }
    ProjectGitHubBackup fresh(f.root, f.directory.path() + QStringLiteral("/fresh-state"), f.gh, f.git);
    if (!expect(!fresh.upload(QStringLiteral("owner/backup"), QStringLiteral("Must start empty")).succeeded,
                "The first upload must reject any nonempty repository, including app source repositories.")) {
        return 1;
    }
    return 0;
}

int pendingRetryTest(bool delivered)
{
    Fixture f;
    if (!f.initialize()) {
        return 1;
    }
    auto backup = f.backup();
    const QString marker = f.directory.path() + (delivered ? QStringLiteral("/ambiguous") : QStringLiteral("/fail-before-push"));
    writeFile(marker, "1");
    const auto ambiguous = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("First attempt"));
    if (!expect(!ambiguous.succeeded && !ambiguous.error.contains(QStringLiteral("fixture-token")),
                "Unconfirmed pushes must fail with sanitized errors while retaining pending state.")) {
        return 1;
    }
    const auto beforeRetry = backup.check(QStringLiteral("owner/backup"));
    if (!expect(beforeRetry.succeeded && beforeRetry.changed && beforeRetry.commitId.isEmpty(),
                "Offline checks must not promote an unconfirmed pending commit.")) {
        return 1;
    }
    QFile::remove(marker);
    QFile::remove(f.directory.path() + QStringLiteral("/fail-reads"));
    bool collected = false;
    f.command({QStringLiteral("--git-dir=%1/snapshot.git").arg(f.state), QStringLiteral("gc"),
               QStringLiteral("--prune=now")}, &collected);
    if (!expect(collected, "Pending backup objects must survive garbage collection.")) {
        return 1;
    }
    const auto retried = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Retry attempt"));
    if (!expect(retried.succeeded && retried.changed, "Retry must reconcile both delivered and undelivered pending pushes.")
        || !expect(f.remoteCommand({QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("main")}) == "1",
                   "Retry must reuse the exact pending commit rather than duplicate it.")
        || !expect(!backup.check(QStringLiteral("owner/backup")).changed, "Confirmed retry must update local success state.")) {
        return 1;
    }
    // Pending retries also include source edits made since the failed attempt.
    writeFile(marker, "1");
    writeFile(f.root + QStringLiteral("/survey.th"), "pending second source");
    if (backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Second attempt")).succeeded) {
        return 1;
    }
    QFile::remove(marker);
    QFile::remove(f.directory.path() + QStringLiteral("/fail-reads"));
    writeFile(f.root + QStringLiteral("/survey.th"), "latest saved source");
    const auto latest = backup.upload(QStringLiteral("owner/backup"), QStringLiteral("Latest saved backup"));
    if (!expect(latest.succeeded && f.remoteCommand({QStringLiteral("show"), QStringLiteral("main:survey.th")}) == "latest saved source",
                "A retry must reconcile pending state, then upload the current saved snapshot.")
        || !expect(!backup.check(QStringLiteral("owner/backup")).changed,
                   "A retry returning success must represent the latest included snapshot.")) {
        return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
#ifdef Q_OS_UNIX
    return snapshotAndHistoryTest() || rejectionTest() || foreignChangesTest()
        || pendingRetryTest(true) || pendingRetryTest(false);
#else
    std::cout << "GitHub backup executable fixtures require Unix and Python 3; skipped.\n";
    return 0;
#endif
}
