#include "QTestSuiteDispatcher.h"

#include <QCoreApplication>
#include <QtTest/QtTest>

int runTherionExecutableSelectionControllerTest(int argc, char **argv);
int runTherionRunnerConfigDisplayControllerTest(int argc, char **argv);
int runTherionRunnerConfigResolverTest(int argc, char **argv);
int runTherionRunnerOutputLinkerTest(int argc, char **argv);
int runTherionRunnerPresenterTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    return runSelectedQTestSuites(argc, argv, {
        {"TherionExecutableSelectionControllerTest", runTherionExecutableSelectionControllerTest},
        {"TherionRunnerConfigDisplayControllerTest", runTherionRunnerConfigDisplayControllerTest},
        {"TherionRunnerConfigResolverTest", runTherionRunnerConfigResolverTest},
        {"TherionRunnerOutputLinkerTest", runTherionRunnerOutputLinkerTest},
        {"TherionRunnerPresenterTest", runTherionRunnerPresenterTest}});
}
