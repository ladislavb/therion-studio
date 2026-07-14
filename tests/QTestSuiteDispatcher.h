#pragma once

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVector>

#include <initializer_list>

struct QTestSuiteEntry
{
    const char *name;
    int (*run)(int, char **);
};

inline int runSelectedQTestSuites(int argc,
                                  char **argv,
                                  std::initializer_list<QTestSuiteEntry> suites)
{
    QString selectedSuite;
    QVector<QByteArray> qtestArguments;
    qtestArguments.reserve(argc);
    if (argc > 0) {
        qtestArguments.append(argv[0]);
    }

    for (int index = 1; index < argc; ++index) {
        const QByteArray argument(argv[index]);
        if (argument == QByteArrayLiteral("--suite")) {
            if (index + 1 >= argc) {
                qWarning() << "Missing suite name after --suite.";
                return 2;
            }

            selectedSuite = QString::fromLocal8Bit(argv[++index]);
            continue;
        }

        qtestArguments.append(argument);
    }

    QVector<char *> filteredArgv;
    filteredArgv.reserve(qtestArguments.size());
    for (QByteArray &argument : qtestArguments) {
        filteredArgv.append(argument.data());
    }

    const int filteredArgc = filteredArgv.size();
    if (!selectedSuite.isEmpty()) {
        for (const QTestSuiteEntry &suite : suites) {
            if (selectedSuite == QLatin1StringView(suite.name)) {
                return suite.run(filteredArgc, filteredArgv.data());
            }
        }

        qWarning().noquote() << "Unknown QTest suite:" << selectedSuite;
        return 2;
    }

    int status = 0;
    for (const QTestSuiteEntry &suite : suites) {
        status |= suite.run(filteredArgc, filteredArgv.data());
    }
    return status;
}
