#include "../src/app/three_d_viewer/ThreeDViewerImageExportDialog.h"
#include "../src/app/ExportFileName.h"

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QSpinBox>

using namespace TherionStudio;

class ThreeDViewerImageExportDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void keepsPresetAndViewportAspectWhenAspectIsLocked();
    void manualSizeEditsSwitchToCustomPreset();
    void derivesDefaultFileNameFromOpenedLoxArtifact();
};

void ThreeDViewerImageExportDialogTest::keepsPresetAndViewportAspectWhenAspectIsLocked()
{
    ThreeDViewerImageExportDialog dialog(QSize(1000, 500));

    auto *presetCombo = dialog.findChild<QComboBox *>();
    const auto spinBoxes = dialog.findChildren<QSpinBox *>();

    QVERIFY(presetCombo != nullptr);
    QCOMPARE(spinBoxes.size(), 2);

    presetCombo->setCurrentIndex(1);

    QCOMPARE(presetCombo->currentIndex(), 1);
    QCOMPARE(spinBoxes.at(0)->value(), 1920);
    QCOMPARE(spinBoxes.at(1)->value(), 960);
    QCOMPARE(dialog.exportSize(), QSize(1920, 960));
}

void ThreeDViewerImageExportDialogTest::manualSizeEditsSwitchToCustomPreset()
{
    ThreeDViewerImageExportDialog dialog(QSize(1000, 500));

    auto *presetCombo = dialog.findChild<QComboBox *>();
    const auto spinBoxes = dialog.findChildren<QSpinBox *>();

    QVERIFY(presetCombo != nullptr);
    QCOMPARE(spinBoxes.size(), 2);

    presetCombo->setCurrentIndex(1);
    spinBoxes.at(0)->setValue(2048);

    QCOMPARE(presetCombo->currentIndex(), presetCombo->count() - 1);
    QCOMPARE(spinBoxes.at(1)->value(), 1024);
    QCOMPARE(dialog.exportSize(), QSize(2048, 1024));
}

void ThreeDViewerImageExportDialogTest::derivesDefaultFileNameFromOpenedLoxArtifact()
{
    const QDateTime timestamp(QDate(2026, 7, 4), QTime(14, 48, 39));

    QCOMPARE(defaultArtifactExportFileName(QStringLiteral("/project/_output/1318.lox"),
                                           QStringLiteral("therion-studio-3d"),
                                           QStringLiteral("png"),
                                           timestamp),
             QStringLiteral("1318-20260704-144839.png"));
    QCOMPARE(defaultArtifactExportFileName(QString(),
                                           QStringLiteral("therion-studio-3d"),
                                           QStringLiteral(".png"),
                                           timestamp),
             QStringLiteral("therion-studio-3d-20260704-144839.png"));
}

int runThreeDViewerImageExportDialogTest(int argc, char **argv)
{
    ThreeDViewerImageExportDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerImageExportDialogTest.moc"
