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

    int status = 0;
    status |= runTherionExecutableSelectionControllerTest(argc, argv);
    status |= runTherionRunnerConfigDisplayControllerTest(argc, argv);
    status |= runTherionRunnerConfigResolverTest(argc, argv);
    status |= runTherionRunnerOutputLinkerTest(argc, argv);
    status |= runTherionRunnerPresenterTest(argc, argv);
    return status;
}
