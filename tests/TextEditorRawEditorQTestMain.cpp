#include <QApplication>
#include <QtTest/QtTest>

int runRawEditorCompletionInsertionControllerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    int status = 0;
    status |= runRawEditorCompletionInsertionControllerTest(argc, argv);
    return status;
}
