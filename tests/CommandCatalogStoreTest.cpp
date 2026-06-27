#include "../src/core/CommandCatalogStore.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class CommandCatalogStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsInjectedCatalogObject();
    void parsesJsonBytes();
    void loadsCatalogFiles();
};
}

void CommandCatalogStoreTest::acceptsInjectedCatalogObject()
{
    QJsonObject root;
    root.insert(QStringLiteral("commands"), QJsonObject{{QStringLiteral("survey"), QJsonObject{}}});

    const CommandCatalogStore store(root);
    QVERIFY(store.isCatalogAvailable());
    QVERIFY(store.catalogObject().contains(QStringLiteral("commands")));

    const CommandCatalogStore emptyStore{QJsonObject()};
    QVERIFY(!emptyStore.isCatalogAvailable());
}

void CommandCatalogStoreTest::parsesJsonBytes()
{
    const CommandCatalogStore validStore = CommandCatalogStore::fromJsonBytes(
        QByteArrayLiteral("{\"commands\":{\"centerline\":{}}}"));
    QVERIFY(validStore.isCatalogAvailable());
    QVERIFY(validStore.catalogObject().value(QStringLiteral("commands")).isObject());

    const CommandCatalogStore arrayStore = CommandCatalogStore::fromJsonBytes(QByteArrayLiteral("[]"));
    QVERIFY(!arrayStore.isCatalogAvailable());

    const CommandCatalogStore invalidStore = CommandCatalogStore::fromJsonBytes(QByteArrayLiteral("{"));
    QVERIFY(!invalidStore.isCatalogAvailable());
}

void CommandCatalogStoreTest::loadsCatalogFiles()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString catalogPath = temporaryDir.filePath(QStringLiteral("catalog.json"));
    QFile catalogFile(catalogPath);
    QVERIFY(catalogFile.open(QIODevice::WriteOnly));
    catalogFile.write(QByteArrayLiteral("{\"commands\":{\"map\":{}}}"));
    catalogFile.close();

    const CommandCatalogStore fileStore = CommandCatalogStore::fromFile(catalogPath);
    QVERIFY(fileStore.isCatalogAvailable());
    QVERIFY(fileStore.catalogObject().value(QStringLiteral("commands")).toObject().contains(QStringLiteral("map")));

    const CommandCatalogStore missingFileStore = CommandCatalogStore::fromFile(
        temporaryDir.filePath(QStringLiteral("missing.json")));
    QVERIFY(!missingFileStore.isCatalogAvailable());
}

int runCommandCatalogStoreTest(int argc, char **argv)
{
    CommandCatalogStoreTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "CommandCatalogStoreTest.moc"
