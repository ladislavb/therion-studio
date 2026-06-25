#include <QApplication>
#include <QtTest/QtTest>

int runRawEditorCompletionInsertionControllerTest(int argc, char **argv);
int runRawEditorFindShortcutTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    int status = 0;
    status |= runRawEditorCompletionInsertionControllerTest(argc, argv);
    status |= runRawEditorFindShortcutTest(argc, argv);
    return status;
}
