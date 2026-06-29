#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneRefreshController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorSelectionController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QIcon>
#include <QLabel>
#include <QLineF>
#include <QMainWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QDirIterator>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QThread>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorSourceLineRoleForTest = Qt::UserRole + 700;
constexpr int kInspectorObjectNameColumnForTest = 0;
constexpr int kInspectorObjectDragColumnForTest = 1;
constexpr int kInspectorObjectVisibilityColumnForTest = 2;
constexpr int kInspectorObjectDeleteColumnForTest = 3;

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

void pumpEvents();

QString repositoryFilePath(const QString &relativePath)
{
    const QString fromCurrentDirectory = QDir::current().absoluteFilePath(relativePath);
    if (QFileInfo::exists(fromCurrentDirectory)) {
        return QFileInfo(fromCurrentDirectory).absoluteFilePath();
    }

    const QString fromBuildDirectory = QDir(QCoreApplication::applicationDirPath())
                                           .absoluteFilePath(QStringLiteral("../") + relativePath);
    return QFileInfo(fromBuildDirectory).absoluteFilePath();
}

bool copyDirectoryRecursively(const QString &sourceDirectoryPath, const QString &targetDirectoryPath)
{
    QDir sourceDirectory(sourceDirectoryPath);
    if (!sourceDirectory.exists()) {
        return false;
    }
    if (!QDir().mkpath(targetDirectoryPath)) {
        return false;
    }

    QDirIterator iterator(sourceDirectoryPath,
                          QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QFileInfo sourceInfo(sourcePath);
        const QString relativePath = sourceDirectory.relativeFilePath(sourcePath);
        const QString targetPath = QDir(targetDirectoryPath).filePath(relativePath);
        if (sourceInfo.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                return false;
            }
            continue;
        }
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            return false;
        }
        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath)) {
            return false;
        }
    }

    return true;
}

bool tokenIsNumber(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}

bool tokenHasWaveUnderline(const QTextBlock &block, const QString &token)
{
    if (!block.isValid() || block.layout() == nullptr) {
        return false;
    }

    const QString lineText = block.text();
    const int tokenStart = lineText.indexOf(token);
    if (tokenStart < 0) {
        return false;
    }

    const QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start <= tokenStart && (range.start + range.length) >= (tokenStart + token.length())
            && range.format.underlineStyle() == QTextCharFormat::WaveUnderline) {
            return true;
        }
    }
    return false;
}

int countGraphicsEllipseItems(const QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return 0;
    }

    int count = 0;
    for (QGraphicsItem *item : scene->items()) {
        if (dynamic_cast<QGraphicsEllipseItem *>(item) != nullptr) {
            ++count;
        }
    }
    return count;
}

bool runFreehandBezierRowLogicSmoke()
{
    QVector<MapEditorInteractiveLineDraftVertex> xtherionDraft;
    const auto identitySceneToSource = [](const QPointF &point) {
        return point;
    };
    QVector<MapEditorInteractiveLineDraftVertex> firstDraggedDraft;
    captureInteractiveLineAnchor(&firstDraggedDraft,
                                 QPointF(0.0, 0.0),
                                 QPointF(0.0, 0.0),
                                 QPointF(0.0, 5.0),
                                 true,
                                 true,
                                 true,
                                 QString(),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    captureInteractiveLineAnchor(&firstDraggedDraft,
                                 QPointF(10.0, 0.0),
                                 QPointF(10.0, 0.0),
                                 QPointF(10.0, 5.0),
                                 true,
                                 true,
                                 true,
                                 QString(),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    if (!expect(firstDraggedDraft.size() == 2
                    && firstDraggedDraft.first().incomingControlScene == QPointF(0.0, -5.0)
                    && firstDraggedDraft.first().outgoingControlScene == QPointF(0.0, 5.0),
                "Dragging the first XTherion-style draft point should keep mirrored controls on the first anchor.")) {
        return false;
    }
    if (!expect(interactiveLineControlAt(firstDraggedDraft, QPointF(0.0, 5.0), 0.5).has_value()
                    && interactiveLineControlAt(firstDraggedDraft, QPointF(0.0, -5.0), 0.5).has_value(),
                "First draft anchor control handles should be hittable for immediate editing.")) {
        return false;
    }
    const QStringList firstDraggedOpenRows = lineCoordinateRowsForInteractiveDraft(firstDraggedDraft);
    if (!expect(firstDraggedOpenRows.size() == 2
                    && firstDraggedOpenRows.at(1) == QStringLiteral("0.0 5.0 10.0 -5.0 10.0 0.0"),
                "Open draft rows should serialize the first anchor's outgoing control on the first segment.")) {
        return false;
    }
    const QStringList firstDraggedClosedRows = closedLineCoordinateRowsForInteractiveDraft(firstDraggedDraft);
    if (!expect(firstDraggedClosedRows.size() == 3
                    && firstDraggedClosedRows.at(2) == QStringLiteral("10.0 5.0 0.0 -5.0 0.0 0.0"),
                "Closed draft rows should serialize a smooth Bezier closing segment using the first anchor's incoming control.")) {
        return false;
    }

    captureInteractiveLineAnchor(&xtherionDraft,
                                 QPointF(0.0, 0.0),
                                 QPointF(0.0, 0.0),
                                 std::nullopt,
                                 true,
                                 true,
                                 true,
                                 QString(),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    captureInteractiveLineAnchor(&xtherionDraft,
                                 QPointF(10.0, 0.0),
                                 QPointF(10.0, 0.0),
                                 QPointF(10.0, 5.0),
                                 true,
                                 true,
                                 true,
                                 QString(),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    if (!expect(xtherionDraft.size() == 2
                    && !xtherionDraft.first().outgoingControlScene.has_value()
                    && xtherionDraft.at(1).incomingControlScene == QPointF(10.0, -5.0)
                    && xtherionDraft.at(1).outgoingControlScene == QPointF(10.0, 5.0),
                "XTherion-style draft insertion should attach mirrored controls to the inserted point only.")) {
        return false;
    }
    captureInteractiveLineAnchor(&xtherionDraft,
                                 QPointF(20.0, 0.0),
                                 QPointF(20.0, 0.0),
                                 QPointF(20.0, 6.0),
                                 true,
                                 true,
                                 true,
                                 QString(),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    if (!expect(xtherionDraft.at(1).outgoingControlScene == QPointF(10.0, 5.0)
                    && xtherionDraft.at(2).incomingControlScene == QPointF(20.0, -6.0)
                    && xtherionDraft.at(2).outgoingControlScene == QPointF(20.0, 6.0),
                "Inserting the next XTherion-style draft point should not rewrite the previous point's next control.")) {
        return false;
    }
    const QStringList xtherionRows = lineCoordinateRowsForInteractiveDraft(xtherionDraft);
    if (!expect(xtherionRows.size() == 3
                    && xtherionRows.at(1) == QStringLiteral("0.0 0.0 10.0 -5.0 10.0 0.0")
                    && xtherionRows.at(2) == QStringLiteral("10.0 5.0 20.0 -6.0 20.0 0.0"),
                "XTherion-style draft rows should serialize each point's next control in the following segment row.")) {
        return false;
    }
    QVector<MapEditorInteractiveLineDraftVertex> repeatedSubtypeDraft;
    captureInteractiveLineAnchor(&repeatedSubtypeDraft,
                                 QPointF(0.0, 0.0),
                                 QPointF(0.0, 0.0),
                                 std::nullopt,
                                 true,
                                 true,
                                 true,
                                 QStringLiteral("presumed"),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    captureInteractiveLineAnchor(&repeatedSubtypeDraft,
                                 QPointF(10.0, 0.0),
                                 QPointF(10.0, 0.0),
                                 std::nullopt,
                                 true,
                                 true,
                                 true,
                                 QStringLiteral("presumed"),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    captureInteractiveLineAnchor(&repeatedSubtypeDraft,
                                 QPointF(20.0, 0.0),
                                 QPointF(20.0, 0.0),
                                 std::nullopt,
                                 true,
                                 true,
                                 true,
                                 QStringLiteral("presumed"),
                                 std::nullopt,
                                 std::nullopt,
                                 identitySceneToSource);
    const QStringList repeatedSubtypeRows = lineCoordinateRowsForInteractiveDraft(repeatedSubtypeDraft);
    if (!expect(repeatedSubtypeRows.count(QStringLiteral("subtype presumed")) == 1,
                "Repeated pending line-point subtype should serialize only when the active segment subtype changes.")) {
        return false;
    }

    QVector<QPointF> straightStroke;
    straightStroke.reserve(64);
    for (int index = 0; index < 64; ++index) {
        const double t = static_cast<double>(index) / 63.0;
        straightStroke.append(QPointF(t * 180.0, t * 12.0));
    }

    const QStringList straightRows = bezierLineCoordinateRowsForFreehandStroke(straightStroke);
    if (straightRows.size() > 3) {
        std::cerr << "Simple freehand strokes should collapse to very few bezier anchors; got "
                  << straightRows.size() << " rows.\n";
        return false;
    }

    QVector<QPointF> sampledStroke;
    sampledStroke.reserve(64);
    for (int index = 0; index < 64; ++index) {
        const double t = static_cast<double>(index) / 63.0;
        sampledStroke.append(QPointF(t * 120.0, std::sin(t * 8.0) * 35.0 + t * 80.0));
    }
    const QStringList simplifiedRows = bezierLineCoordinateRowsForFreehandStroke(sampledStroke);
    if (simplifiedRows.size() <= straightRows.size()) {
        std::cerr << "Complex freehand strokes should retain more bezier anchors than simple strokes; got "
                  << simplifiedRows.size() << " complex rows and "
                  << straightRows.size() << " simple rows.\n";
        return false;
    }

    QVector<QPointF> longerSampledStroke;
    longerSampledStroke.reserve(128);
    for (int index = 0; index < 128; ++index) {
        const double t = static_cast<double>(index) / 127.0;
        longerSampledStroke.append(QPointF(t * 260.0, std::sin(t * 16.0) * 42.0 + t * 120.0));
    }
    const QStringList longerRows = bezierLineCoordinateRowsForFreehandStroke(longerSampledStroke);
    if (longerRows.size() <= simplifiedRows.size()) {
        std::cerr << "More complex freehand strokes should retain proportionally more bezier anchors; got "
                  << longerRows.size() << " longer rows and "
                  << simplifiedRows.size() << " medium rows.\n";
        return false;
    }

    const QStringList rows = bezierLineCoordinateRowsForFreehandStroke({
        QPointF(0.0, 0.0),
        QPointF(10.0, 5.0),
        QPointF(20.0, 0.0),
    });
    if (!expect(rows.size() >= 2,
                "Freehand bezier row conversion should keep at least start and end rows.")) {
        return false;
    }
    const QStringList tokens = rows.at(1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    return expect(tokens.size() >= 6,
                  "Freehand bezier row conversion should produce cubic coordinate rows.");
}

int runProjectValidationDiagnosticsStayOutOfMapEditorSmoke()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for map validation boundary smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.path() + QStringLiteral("/validation-boundary.th2");
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for map validation boundary smoke test.")) {
        return 1;
    }

    file.write("scrap s1\n"
               "point 0 0 station -name test\n"
               "line wall\n"
               "  0 0\n"
               "  10 10\n"
               "endline\n"
               "endscrap\n");
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.resize(800, 600);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for map validation boundary smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *textEditor = mapTab->findChild<TextEditorTab *>();
    if (!expect(textEditor != nullptr,
                "Embedded TextEditorTab was not found for map validation boundary smoke test.")) {
        return 1;
    }
    auto *sourceEditor = textEditor->findChild<QPlainTextEdit *>();
    if (!expect(sourceEditor != nullptr,
                "Embedded source editor was not found for map validation boundary smoke test.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    TherionSourceDiagnostic diagnostic;
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.code = QStringLiteral("unknown-station-reference");
    diagnostic.title = QStringLiteral("Unknown station reference");
    diagnostic.message = QStringLiteral("Station reference `test` has no matching station in the project index.");
    diagnostic.lineNumber = 2;
    diagnostic.columnNumber = QStringLiteral("point 0 0 station -name ").size() + 1;
    diagnostic.columnLength = QStringLiteral("test").size();
    diagnostic.currentText = QStringLiteral("point 0 0 station -name test");

    mapTab->setProjectValidationDiagnostics({diagnostic});
    pumpEvents();

    const QTextBlock stationLine = sourceEditor->document()->findBlockByLineNumber(1);
    if (!expect(!tokenHasWaveUnderline(stationLine, QStringLiteral("test")),
                "Project diagnostics should not be injected into visual map-editor highlighting.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Project diagnostics should not mutate map editor source text.")) {
        return 1;
    }

    mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Raw);
    pumpEvents();
    if (!expect(tokenHasWaveUnderline(stationLine, QStringLiteral("test")),
                "Project diagnostics should be projected into the map editor Raw text highlighter.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Raw project diagnostic projection should not mutate map editor source text.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int countDirectiveLines(const QString &text, const QString &directive)
{
    if (directive.isEmpty()) {
        return 0;
    }

    const QString trimmedDirective = directive.trimmed().toLower();
    int count = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }
        if (trimmedLine == trimmedDirective || trimmedLine.startsWith(trimmedDirective + QLatin1Char(' '))) {
            ++count;
        }
    }
    return count;
}

int lastDirectiveLineNumber(const QString &text, const QString &directive)
{
    if (directive.isEmpty()) {
        return 0;
    }

    const QString trimmedDirective = directive.trimmed().toLower();
    int lineNumber = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmedLine = lines.at(index).trimmed();
        if (trimmedLine == trimmedDirective || trimmedLine.startsWith(trimmedDirective + QLatin1Char(' '))) {
            lineNumber = index + 1;
        }
    }
    return lineNumber;
}

QString sourceLineAt(const QString &text, int lineNumber)
{
    if (lineNumber <= 0) {
        return QString();
    }
    const QStringList lines = text.split(QLatin1Char('\n'));
    if (lineNumber > lines.size()) {
        return QString();
    }
    return lines.at(lineNumber - 1).trimmed();
}

bool hasVisibleLabelText(QWidget *root, const QString &text)
{
    if (root == nullptr) {
        return false;
    }
    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label != nullptr && label->isVisible() && label->text() == text) {
            return true;
        }
    }
    return false;
}

int lastDraftLineCoordinateTokenCount(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0 || blockStart + 1 >= lines.size()) {
        return 0;
    }

    int numericTokenCount = 0;
    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
    }
    return numericTokenCount;
}

bool lastDraftLineHasBezierCoordinateRow(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return false;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int numericTokenCount = 0;
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
        if (numericTokenCount >= 6) {
            return true;
        }
    }
    return false;
}

std::optional<QVector<double>> lastDraftLineBezierCoordinateRow(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return std::nullopt;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (numericValues.size() >= 6) {
            return numericValues;
        }
    }

    return std::nullopt;
}

QVector<QVector<double>> lastDraftLineNumericRows(const QString &text)
{
    QVector<QVector<double>> rows;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("line wall")) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return rows;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (!numericValues.isEmpty()) {
            rows.append(numericValues);
        }
    }

    return rows;
}

bool lastDraftAreaReferencesLineId(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int areaStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("area water")
            || trimmed.startsWith(QStringLiteral("area water "))) {
            areaStart = index;
        }
    }
    if (areaStart < 0) {
        return false;
    }

    for (int index = areaStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.compare(QStringLiteral("endarea"), Qt::CaseInsensitive) == 0) {
            return false;
        }

        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tokens.size() != 1) {
            return false;
        }
        const QString token = tokens.first();
        if (token.startsWith(QLatin1Char('-')) || tokenIsNumber(token)) {
            return false;
        }
        return true;
    }

    return false;
}

std::optional<QString> lastDraftAreaReferencedLineId(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int areaStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("area water")
            || trimmed.startsWith(QStringLiteral("area water "))) {
            areaStart = index;
        }
    }
    if (areaStart < 0) {
        return std::nullopt;
    }

    for (int index = areaStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.compare(QStringLiteral("endarea"), Qt::CaseInsensitive) == 0) {
            return std::nullopt;
        }

        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tokens.size() != 1) {
            return std::nullopt;
        }
        const QString token = tokens.first();
        if (token.startsWith(QLatin1Char('-')) || tokenIsNumber(token)) {
            return std::nullopt;
        }
        return token;
    }

    return std::nullopt;
}

bool lineBlockByIdHasBezierCoordinateRow(const QString &text, const QString &lineId)
{
    if (lineId.trimmed().isEmpty()) {
        return false;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (!trimmed.startsWith(QStringLiteral("line "), Qt::CaseInsensitive)) {
            continue;
        }
        const QString lower = trimmed.toLower();
        const QString idNeedle = QStringLiteral("-id %1").arg(lineId.toLower());
        if (lower.contains(idNeedle)) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return false;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int numericTokenCount = 0;
        for (const QString &token : tokens) {
            if (tokenIsNumber(token)) {
                ++numericTokenCount;
            }
        }
        if (numericTokenCount >= 6) {
            return true;
        }
    }

    return false;
}

QVector<QVector<double>> lineBlockByIdNumericRows(const QString &text, const QString &lineId)
{
    QVector<QVector<double>> rows;
    if (lineId.trimmed().isEmpty()) {
        return rows;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    int blockStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (!trimmed.startsWith(QStringLiteral("line "), Qt::CaseInsensitive)) {
            continue;
        }
        const QString lower = trimmed.toLower();
        const QString idNeedle = QStringLiteral("-id %1").arg(lineId.toLower());
        if (lower.contains(idNeedle)) {
            blockStart = index;
        }
    }

    if (blockStart < 0) {
        return rows;
    }

    for (int index = blockStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed().toLower();
        if (trimmed == QStringLiteral("endline")) {
            break;
        }
        const QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        QVector<double> numericValues;
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (ok) {
                numericValues.append(value);
            }
        }
        if (!numericValues.isEmpty()) {
            rows.append(numericValues);
        }
    }

    return rows;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void waitForMs(int delayMs)
{
    if (delayMs <= 0) {
        return;
    }

    QEventLoop loop;
    QTimer::singleShot(delayMs, &loop, &QEventLoop::quit);
    loop.exec();
    pumpEvents();
}

MapEditableGeometryVertexItem *findCenteredLineAnchor(QGraphicsScene *scene, const QRectF &visibleSceneRect)
{
    if (scene == nullptr) {
        return nullptr;
    }

    MapEditableGeometryVertexItem *best = nullptr;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    const QPointF center = visibleSceneRect.center();
    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(rawItem);
        if (vertexItem == nullptr) {
            continue;
        }
        if (!vertexItem->isVisible()) {
            continue;
        }
        if (vertexItem->geometryKind() != QStringLiteral("line")) {
            continue;
        }
        const QPointF scenePoint = vertexItem->scenePos();
        if (!visibleSceneRect.contains(scenePoint)) {
            continue;
        }

        const qreal dx = scenePoint.x() - center.x();
        const qreal dy = scenePoint.y() - center.y();
        const qreal distance = std::sqrt((dx * dx) + (dy * dy));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = vertexItem;
        }
    }

    return best;
}

MapEditablePointItem *findPointItemForLine(QGraphicsScene *scene, int lineNumber)
{
    if (scene == nullptr || lineNumber <= 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *pointItem = dynamic_cast<MapEditablePointItem *>(rawItem);
        if (pointItem == nullptr || !pointItem->isVisible()) {
            continue;
        }
        if (pointItem->data(kMapSceneLineNumberRole).toInt() == lineNumber) {
            return pointItem;
        }
    }

    return nullptr;
}

MapEditableGeometryVertexItem *findSelectedLineVertex(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(rawItem);
        if (vertexItem == nullptr) {
            continue;
        }
        if (vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
            return vertexItem;
        }
    }

    return nullptr;
}

QVector<MapEditableGeometryVertexItem *> visibleLineControls(QGraphicsScene *scene, QGraphicsView *view, int lineNumber)
{
    QVector<MapEditableGeometryVertexItem *> result;
    if (scene == nullptr || view == nullptr || lineNumber <= 0) {
        return result;
    }

    QVector<MapEditableGeometryVertexItem *> anchorItems;
    QVector<MapEditableGeometryVertexItem *> controlItems;
    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(rawItem);
        if (vertexItem == nullptr || !vertexItem->isVisible()) {
            continue;
        }
        if (vertexItem->lineNumber() != lineNumber) {
            continue;
        }
        if (vertexItem->geometryKind() == QStringLiteral("line")) {
            anchorItems.append(vertexItem);
        } else if (vertexItem->geometryKind().startsWith(QStringLiteral("line control"))) {
            controlItems.append(vertexItem);
        }
    }

    struct RankedControl
    {
        MapEditableGeometryVertexItem *item = nullptr;
        qreal nearestAnchorDistance = 0.0;
    };
    QVector<RankedControl> rankedControls;
    for (MapEditableGeometryVertexItem *controlItem : controlItems) {
        const QPoint controlViewportPoint = view->mapFromScene(controlItem->scenePos());
        if (view->itemAt(controlViewportPoint) != controlItem) {
            continue;
        }

        qreal nearestAnchorDistance = std::numeric_limits<qreal>::max();
        for (MapEditableGeometryVertexItem *anchorItem : anchorItems) {
            const QPoint anchorViewportPoint = view->mapFromScene(anchorItem->scenePos());
            const QPoint viewportDelta = controlViewportPoint - anchorViewportPoint;
            nearestAnchorDistance = qMin(nearestAnchorDistance,
                                         std::hypot(viewportDelta.x(), viewportDelta.y()));
        }
        rankedControls.append(RankedControl{controlItem, nearestAnchorDistance});
    }
    std::sort(rankedControls.begin(), rankedControls.end(), [](const RankedControl &lhs, const RankedControl &rhs) {
        return lhs.nearestAnchorDistance > rhs.nearestAnchorDistance;
    });
    result.reserve(rankedControls.size());
    for (const RankedControl &control : rankedControls) {
        result.append(control.item);
    }
    return result;
}

MapEditableGeometryVertexItem *findVisibleLineControl(QGraphicsScene *scene, QGraphicsView *view, int lineNumber)
{
    const QVector<MapEditableGeometryVertexItem *> controls = visibleLineControls(scene, view, lineNumber);
    return controls.isEmpty() ? nullptr : controls.first();
}

QGraphicsPathItem *findPathItemForLine(QGraphicsScene *scene,
                                       int lineNumber,
                                       std::optional<int> selectionSubtype);
bool clickScenePoint(QGraphicsView *view, const QPointF &scenePoint);

MapEditableGeometryVertexItem *waitForVisibleLineControl(QGraphicsScene *scene,
                                                         QGraphicsView *view,
                                                         int lineNumber,
                                                         int timeoutMs = 1000)
{
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    MapEditableGeometryVertexItem *lastControl = nullptr;
    while (QDateTime::currentMSecsSinceEpoch() <= deadline) {
        pumpEvents();
        lastControl = findVisibleLineControl(scene, view, lineNumber);
        if (lastControl != nullptr) {
            return lastControl;
        }
        QThread::msleep(5);
    }
    return lastControl;
}

bool ensureVisibleLineControls(MapEditorTab *mapTab, QGraphicsView *view, int lineNumber)
{
    if (mapTab == nullptr || view == nullptr || view->scene() == nullptr || lineNumber <= 0) {
        return false;
    }
    if (findVisibleLineControl(view->scene(), view, lineNumber) != nullptr) {
        return true;
    }

    mapTab->goToLine(lineNumber);
    pumpEvents();
    if (auto *linePath = findPathItemForLine(view->scene(), lineNumber, std::nullopt); linePath != nullptr) {
        const QPointF lineClickPoint = linePath->mapToScene(linePath->path().pointAtPercent(0.5));
        clickScenePoint(view, lineClickPoint);
    }
    pumpEvents();
    mapTab->triggerSelectMode();
    return waitForVisibleLineControl(view->scene(), view, lineNumber, 2000) != nullptr;
}

struct SelectedLineVertexSnapshot
{
    int lineNumber = 0;
    int vertexIndex = -1;
};

std::optional<SelectedLineVertexSnapshot> waitForSelectedLineVertex(QGraphicsScene *scene,
                                                                    int expectedLineNumber,
                                                                    int expectedVertexIndex,
                                                                    int timeoutMs = 500)
{
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    std::optional<SelectedLineVertexSnapshot> lastSelected;
    while (QDateTime::currentMSecsSinceEpoch() <= deadline) {
        pumpEvents();
        if (MapEditableGeometryVertexItem *selected = findSelectedLineVertex(scene)) {
            lastSelected = SelectedLineVertexSnapshot{
                selected->lineNumber(),
                selected->vertexIndex(),
            };
        }
        if (lastSelected.has_value()
            && lastSelected->lineNumber == expectedLineNumber
            && lastSelected->vertexIndex == expectedVertexIndex) {
            return lastSelected;
        }
        QThread::msleep(5);
    }
    return lastSelected;
}

int selectedSourceLineNumber(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return 0;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        if (rawItem == nullptr || !rawItem->isVisible()) {
            continue;
        }
        const int lineNumber = rawItem->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            return lineNumber;
        }
    }

    return 0;
}

QSet<int> selectedSourceLineNumbers(QGraphicsScene *scene);

int waitForSelectedSourceLineNumber(QGraphicsScene *scene, int expectedLineNumber, int timeoutMs = 500)
{
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    int lastSelectedLineNumber = 0;
    while (QDateTime::currentMSecsSinceEpoch() <= deadline) {
        pumpEvents();
        const QSet<int> lineNumbers = selectedSourceLineNumbers(scene);
        if (lineNumbers.contains(expectedLineNumber)) {
            return expectedLineNumber;
        }
        if (!lineNumbers.isEmpty()) {
            lastSelectedLineNumber = *lineNumbers.constBegin();
        } else {
            lastSelectedLineNumber = 0;
        }
        QThread::msleep(5);
    }
    return lastSelectedLineNumber;
}

bool waitForTextEquals(MapEditorTab *mapTab, const QString &expectedText, int timeoutMs = 500)
{
    if (mapTab == nullptr) {
        return false;
    }

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() <= deadline) {
        pumpEvents();
        if (mapTab->text() == expectedText) {
            return true;
        }
        QThread::msleep(5);
    }
    return mapTab->text() == expectedText;
}

bool waitForTextDiffers(MapEditorTab *mapTab, const QString &originalText, int timeoutMs = 500)
{
    if (mapTab == nullptr) {
        return false;
    }

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() <= deadline) {
        pumpEvents();
        if (mapTab->text() != originalText) {
            return true;
        }
        QThread::msleep(5);
    }
    return mapTab->text() != originalText;
}

QSet<int> selectedSourceLineNumbers(QGraphicsScene *scene)
{
    QSet<int> lineNumbers;
    if (scene == nullptr) {
        return lineNumbers;
    }

    const QList<QGraphicsItem *> selectedItems = scene->selectedItems();
    for (QGraphicsItem *rawItem : selectedItems) {
        if (rawItem == nullptr) {
            continue;
        }
        const int lineNumber = rawItem->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            lineNumbers.insert(lineNumber);
        }
    }

    return lineNumbers;
}


void sendMouse(QWidget *widget,
               QEvent::Type type,
               const QPoint &localPos,
               Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    const QPoint globalPos = widget->mapToGlobal(localPos);
    QMouseEvent event(type,
                      QPointF(localPos),
                      QPointF(globalPos),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

void sendKey(QWidget *widget, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if (widget == nullptr) {
        return;
    }

    QKeyEvent event(type, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

bool dragItemBySceneDelta(QGraphicsView *view, QGraphicsItem *item, const QPointF &requestedSceneDelta)
{
    if (view == nullptr || item == nullptr || view->viewport() == nullptr) {
        return false;
    }

    view->centerOn(item);
    pumpEvents();

    const QRectF visibleSceneRect = view->mapToScene(view->viewport()->rect()).boundingRect();
    QPointF sceneDelta = requestedSceneDelta;
    QPointF sceneStart = item->scenePos();
    QPointF sceneEnd = sceneStart + sceneDelta;
    if (!visibleSceneRect.contains(sceneEnd)) {
        const QPointF flipped = sceneStart - sceneDelta;
        if (visibleSceneRect.contains(flipped)) {
            sceneEnd = flipped;
        } else {
            sceneEnd = visibleSceneRect.center();
        }
    }

    const QPoint start = view->mapFromScene(sceneStart);
    const QPoint end = view->mapFromScene(sceneEnd);
    if (start == end) {
        return false;
    }

    QWidget *viewport = view->viewport();
    sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();

    constexpr int steps = 6;
    for (int step = 1; step <= steps; ++step) {
        const qreal t = static_cast<qreal>(step) / static_cast<qreal>(steps);
        const QPoint intermediate(qRound(start.x() + ((end.x() - start.x()) * t)),
                                  qRound(start.y() + ((end.y() - start.y()) * t)));
        sendMouse(viewport, QEvent::MouseMove, intermediate, Qt::NoButton, Qt::LeftButton);
        pumpEvents();
    }

    sendMouse(viewport, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    return true;
}

bool dragAnyVisibleLineControlUntilTextChanges(MapEditorTab *mapTab,
                                               QGraphicsView *view,
                                               int lineNumber,
                                               const QString &originalText)
{
    if (mapTab == nullptr || view == nullptr || view->scene() == nullptr) {
        return false;
    }

    const QVector<QPointF> dragDeltas = {
        QPointF(28.0, -18.0),
        QPointF(-28.0, 18.0),
        QPointF(12.0, -8.0),
        QPointF(-12.0, 8.0),
        QPointF(8.0, 12.0),
        QPointF(-8.0, -12.0),
    };
    for (const QPointF &dragDelta : dragDeltas) {
        if (!ensureVisibleLineControls(mapTab, view, lineNumber)) {
            continue;
        }
        for (int controlIndex = 0; controlIndex < 6; ++controlIndex) {
            const QVector<MapEditableGeometryVertexItem *> controls =
                visibleLineControls(view->scene(), view, lineNumber);
            if (controlIndex >= controls.size() || controls.at(controlIndex) == nullptr) {
                break;
            }
            MapEditableGeometryVertexItem *controlItem = controls.at(controlIndex);
            if (!dragItemBySceneDelta(view, controlItem, dragDelta)) {
                continue;
            }
            if (waitForTextDiffers(mapTab, originalText, 2000)) {
                return true;
            }
            pumpEvents();
        }
    }
    return mapTab->text() != originalText;
}

bool clickItem(QGraphicsView *view, QGraphicsItem *item)
{
    if (view == nullptr || item == nullptr || view->viewport() == nullptr) {
        return false;
    }

    view->centerOn(item);
    pumpEvents();
    const QPoint clickPosition = view->mapFromScene(item->scenePos());
    sendMouse(view->viewport(), QEvent::MouseButtonPress, clickPosition, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();
    sendMouse(view->viewport(), QEvent::MouseButtonRelease, clickPosition, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    return true;
}

bool clickScenePoint(QGraphicsView *view, const QPointF &scenePoint)
{
    if (view == nullptr || view->viewport() == nullptr) {
        return false;
    }

    view->centerOn(scenePoint);
    pumpEvents();
    const QPoint clickPosition = view->mapFromScene(scenePoint);
    sendMouse(view->viewport(), QEvent::MouseButtonPress, clickPosition, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();
    sendMouse(view->viewport(), QEvent::MouseButtonRelease, clickPosition, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    return true;
}

bool runSceneRefreshSelectionSourceSmoke()
{
    QObject sceneParent;
    QGraphicsView view;
    QGraphicsScene *scene = new QGraphicsScene(&sceneParent);
    view.setScene(scene);

    QHash<int, QGraphicsItem *> itemsByLine;
    bool commandApplyInProgress = false;
    bool sceneRefreshPending = false;
    bool autoFitEnabled = false;
    bool fitBackgroundRequested = false;
    int sceneRefreshSelectionLineNumber = 4;
    int sceneRefreshSelectionVertexIndex = 7;
    QString sceneRefreshSelectionKind = QStringLiteral("line control");
    int cursorLineNumber = 5;
    int refreshedSelectionLineNumber = 0;
    int refreshedSelectionVertexIndex = -1;
    int restoredPointSelectionLineNumber = 0;
    bool refreshedSelectionCentered = false;
    bool recreateControlDuringRefresh = true;
    MapEditorOrientationApplicabilityByCommand orientationApplicability;

    MapEditorSceneRefreshContext context{
        .sceneParent = &sceneParent,
        .selectionConnectionContext = &sceneParent,
        .scene = &scene,
        .view = &view,
        .undoStack = nullptr,
        .itemsByLine = &itemsByLine,
        .commandApplyInProgress = &commandApplyInProgress,
        .sceneRefreshPending = &sceneRefreshPending,
        .autoFitEnabled = &autoFitEnabled,
        .fitBackgroundRequested = &fitBackgroundRequested,
        .orientationApplicabilityByCommand = &orientationApplicability,
        .documentText = []() {
            return QStringLiteral("encoding utf-8\n");
        },
        .parsedLinesForCurrentDocument = []() {
            return QVector<TherionParsedLine>{};
        },
        .currentLineNumber = [&cursorLineNumber]() {
            return cursorLineNumber;
        },
        .sceneRefreshSelectionLineNumber = [&sceneRefreshSelectionLineNumber]() {
            return sceneRefreshSelectionLineNumber;
        },
        .sceneRefreshSelectionVertexIndex = [&sceneRefreshSelectionVertexIndex]() {
            return sceneRefreshSelectionVertexIndex;
        },
        .sceneRefreshSelectionKind = [&sceneRefreshSelectionKind]() {
            return sceneRefreshSelectionKind;
        },
        .filePath = []() {
            return QString();
        },
        .handleSceneSelectionChanged = []() {},
        .updateCommandSurfaceState = []() {},
        .clearMapScene = [&scene, &itemsByLine]() {
            scene->clear();
            itemsByLine.clear();
        },
        .mapSourceBoundsForCurrentDocument = []() {
            return QRectF();
        },
        .mapBackgroundFitBounds = []() {
            return QRectF();
        },
        .recordCardMove = [](int, const QPointF &, const QPointF &) {},
        .recordCardVisibility = [](int, bool, bool) {},
        .recordPointGeometryMove = [](int, const QPointF &, const QPointF &) {},
        .recordLineAreaVertexMove = [](int, const QString &, int, const QPointF &, const QPointF &) {},
        .recordPointOrientationHandleChange = [](int, qreal) {},
        .recordLinePointLeftHandleChange = [](int, int, qreal, qreal) {},
        .restoreBackgroundImageItems = []() {},
        .reprojectMetadataBackgroundLayersForCurrentDocument = []() {},
        .restoreDraftGeometryItems = [&scene, &recreateControlDuringRefresh]() {
            if (!recreateControlDuringRefresh) {
                return;
            }
            auto *controlItem = new MapEditableGeometryVertexItem(4,
                                                                  QStringLiteral("line control"),
                                                                  7,
                                                                  QPointF(30.0, 20.0),
                                                                  QRectF(0.0, -100.0, 100.0, 100.0),
                                                                  QRectF(0.0, 0.0, 100.0, 100.0));
            controlItem->setData(kMapSceneLineNumberRole, 4);
            controlItem->setData(kMapSceneSelectionGatedRole, true);
            controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
            controlItem->setData(kMapSceneOwnerVertexRole, 1);
            controlItem->setVisible(false);
            scene->addItem(controlItem);
        },
        .restorePointSelection = [&restoredPointSelectionLineNumber](int lineNumber) {
            restoredPointSelectionLineNumber = lineNumber;
        },
        .restoreLineAnchorSelection = [&refreshedSelectionLineNumber, &refreshedSelectionVertexIndex](int lineNumber,
                                                                                                      int sourceVertexIndex) {
            refreshedSelectionLineNumber = lineNumber;
            refreshedSelectionVertexIndex = sourceVertexIndex;
        },
        .selectMapLine = [&refreshedSelectionLineNumber, &refreshedSelectionCentered](int lineNumber, bool centerOnSelection) {
            refreshedSelectionLineNumber = lineNumber;
            refreshedSelectionCentered = centerOnSelection;
        },
        .applyInspectorObjectVisibility = []() {},
        .updateGeometrySelectionPresentation = []() {},
        .fitMapToView = [](bool) {},
        .syncZoomFactorFromView = []() {},
        .updateInteractiveDrawPreview = []() {},
        .refreshStatus = []() {},
        .refreshObjectDetailsPanel = []() {},
        .updateHelpPanel = []() {},
    };

    MapEditorSceneRefreshController(context).refreshMapScenePreservingUndoStack();
    auto *restoredControl = findSelectedLineVertex(scene);
    if (!expect(restoredControl != nullptr
                    && restoredControl->lineNumber() == 4
                    && restoredControl->vertexIndex() == 7
                    && restoredControl->geometryKind() == QStringLiteral("line control")
                    && restoredControl->isVisible(),
                "Scene refresh should restore selected line control points instead of downgrading them to anchor or whole-line selection.")) {
        return false;
    }
    if (!expect(restoredPointSelectionLineNumber == 0,
                "Scene refresh should not use point restore for a selected line control point.")) {
        return false;
    }

    sceneRefreshSelectionLineNumber = 0;
    sceneRefreshSelectionVertexIndex = -1;
    sceneRefreshSelectionKind.clear();
    recreateControlDuringRefresh = false;
    cursorLineNumber = 5;
    refreshedSelectionLineNumber = 0;
    refreshedSelectionVertexIndex = -1;
    refreshedSelectionCentered = false;
    MapEditorSceneRefreshController(context).refreshMapScenePreservingUndoStack();
    if (!expect(refreshedSelectionLineNumber == 5 && refreshedSelectionCentered,
                "Scene refresh should fall back to the text cursor row when there is no selected map object.")) {
        return false;
    }

    return true;
}

bool runLineSegmentPresentationSmoke()
{
    QGraphicsScene scene;
    QGraphicsView view;
    view.setScene(&scene);

    QPainterPath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(100.0, 0.0);
    auto *selectedLinePath = new QGraphicsPathItem(path);
    selectedLinePath->setData(kMapSceneLineNumberRole, 4);
    selectedLinePath->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    selectedLinePath->setFlag(QGraphicsItem::ItemIsSelectable, true);
    scene.addItem(selectedLinePath);
    selectedLinePath->setSelected(true);

    auto *startControl = new MapEditableGeometryVertexItem(4,
                                                           QStringLiteral("line control"),
                                                           10,
                                                           QPointF(25.0, 10.0),
                                                           QRectF(0.0, -100.0, 100.0, 100.0),
                                                           QRectF(0.0, 0.0, 100.0, 100.0));
    startControl->setData(kMapSceneLineNumberRole, 4);
    startControl->setData(kMapSceneSelectionGatedRole, true);
    startControl->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
    startControl->setData(kMapSceneOwnerVertexRole, 1);
    startControl->setVisible(false);
    scene.addItem(startControl);

    auto *endControl = new MapEditableGeometryVertexItem(4,
                                                         QStringLiteral("line control"),
                                                         11,
                                                         QPointF(75.0, -10.0),
                                                         QRectF(0.0, -100.0, 100.0, 100.0),
                                                         QRectF(0.0, 0.0, 100.0, 100.0));
    endControl->setData(kMapSceneLineNumberRole, 4);
    endControl->setData(kMapSceneSelectionGatedRole, true);
    endControl->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
    endControl->setData(kMapSceneOwnerVertexRole, 2);
    endControl->setVisible(false);
    scene.addItem(endControl);

    QHash<int, QGraphicsItem *> itemsByLine;
    QSet<int> hiddenObjectLines;
    QSet<int> suppressedAutoReselectLineNumbers;
    bool updatingSelection = false;
    bool autoFitEnabled = false;
    bool textNavigationInProgress = false;
    int lastCursorSyncedLine = 0;
    int lastCursorSyncedColumn = 0;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    int sceneRefreshSelectionLineNumber = 0;
    int sceneRefreshSelectionVertexIndex = -1;
    QString sceneRefreshSelectionGeometryKind;
    int selectedObjectLineNumber = 6;
    int selectedObjectVertexIndex = 2;
    int selectedLineSegmentStartVertexIndex = 1;
    int selectedLineSegmentEndVertexIndex = 2;
    QString selectedObjectKind = QStringLiteral("line");
    std::optional<QPointF> selectedObjectCoordinate;

    MapEditorSelectionContext context{
        .textEditor = nullptr,
        .scene = &scene,
        .view = &view,
        .itemsByLine = &itemsByLine,
        .hiddenObjectLines = &hiddenObjectLines,
        .suppressedAutoReselectLineNumbers = &suppressedAutoReselectLineNumbers,
        .updatingSelection = &updatingSelection,
        .autoFitEnabled = &autoFitEnabled,
        .textNavigationInProgress = &textNavigationInProgress,
        .lastCursorSyncedLine = &lastCursorSyncedLine,
        .lastCursorSyncedColumn = &lastCursorSyncedColumn,
        .pendingClickSelection = &pendingClickSelection,
        .pendingClickScenePosition = &pendingClickScenePosition,
        .pendingClickElapsed = &pendingClickElapsed,
        .pendingClickLineNumber = &pendingClickLineNumber,
        .pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex,
        .pendingClickGeometryKind = &pendingClickGeometryKind,
        .sceneRefreshSelectionLineNumber = &sceneRefreshSelectionLineNumber,
        .sceneRefreshSelectionVertexIndex = &sceneRefreshSelectionVertexIndex,
        .sceneRefreshSelectionGeometryKind = &sceneRefreshSelectionGeometryKind,
        .selectedObjectLineNumber = &selectedObjectLineNumber,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex,
        .selectedLineSegmentStartVertexIndex = &selectedLineSegmentStartVertexIndex,
        .selectedLineSegmentEndVertexIndex = &selectedLineSegmentEndVertexIndex,
        .selectedObjectKind = &selectedObjectKind,
        .selectedObjectCoordinate = &selectedObjectCoordinate,
        .currentLineNumber = []() {
            return 6;
        },
        .parsedLinesForCurrentDocument = []() {
            return QVector<TherionParsedLine>{};
        },
        .sourcePointFromScenePosition = [](const QPointF &point) {
            return point;
        },
        .updateCommandSurfaceState = []() {},
        .updateHelpPanel = []() {},
        .refreshObjectDetailsPanel = []() {},
        .clearInspectorObjectSelection = []() {},
        .syncInspectorObjectSelectionToLineExpanding = [](int) {},
    };

    MapEditorSelectionController(context).updateGeometrySelectionPresentation();
    return expect(startControl->isVisible() && endControl->isVisible(),
                  "Selected line segment controls should remain visible when text selection points at a vertex source row.");
}

QGraphicsPathItem *findPathItemForLine(QGraphicsScene *scene, int lineNumber, std::optional<int> selectionSubtype = std::nullopt)
{
    if (scene == nullptr || lineNumber <= 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *rawItem : items) {
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(rawItem);
        if (pathItem == nullptr || !pathItem->isVisible()) {
            continue;
        }
        if (pathItem->data(kMapSceneLineNumberRole).toInt() != lineNumber) {
            continue;
        }
        if (selectionSubtype.has_value()
            && pathItem->data(kMapSceneSelectionSubtypeRole).toInt() != selectionSubtype.value()) {
            continue;
        }
        if (!selectionSubtype.has_value()
            && pathItem->data(kMapSceneSelectionSubtypeRole).toInt() != kMapSceneSelectionSubtypeGeneric) {
            continue;
        }
        return pathItem;
    }

    return nullptr;
}

QModelIndex findObjectTreeIndexForLine(QAbstractItemModel *model, int lineNumber, const QModelIndex &parent = QModelIndex())
{
    if (model == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }

    const int rowCount = model->rowCount(parent);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, kInspectorObjectNameColumnForTest, parent);
        if (!childIndex.isValid()) {
            continue;
        }
        if (childIndex.data(kInspectorSourceLineRoleForTest).toInt() == lineNumber) {
            return childIndex;
        }
        if (const QModelIndex nestedIndex = findObjectTreeIndexForLine(model, lineNumber, childIndex);
            nestedIndex.isValid()) {
            return nestedIndex;
        }
    }

    return QModelIndex();
}

void revealAncestorTabs(QWidget *widget)
{
    for (QWidget *ancestor = widget != nullptr ? widget->parentWidget() : nullptr;
         ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
        auto *tabs = qobject_cast<QTabWidget *>(ancestor);
        if (tabs == nullptr) {
            continue;
        }
        for (int index = 0; index < tabs->count(); ++index) {
            QWidget *page = tabs->widget(index);
            if (page == widget || (page != nullptr && page->isAncestorOf(widget))) {
                tabs->setCurrentIndex(index);
                break;
            }
        }
    }
}

enum class InspectorObjectDropPosition
{
    BeforeTarget,
    AfterTarget
};

bool dragObjectTreeRow(QTreeView *objectsTree,
                       int sourceLineNumber,
                       int targetLineNumber,
                       InspectorObjectDropPosition position,
                       bool exerciseCancelBeforeDrop = false,
                       bool exerciseInvalidBeforeDrop = false,
                       bool exerciseCurrentLocationBeforeDrop = false,
                       int currentLocationTargetLineNumber = 0,
                       InspectorObjectDropPosition currentLocationPosition = InspectorObjectDropPosition::BeforeTarget)
{
    if (objectsTree == nullptr || objectsTree->model() == nullptr || objectsTree->viewport() == nullptr) {
        std::cerr << "Objects tree, model, or viewport is unavailable for drag simulation.\n";
        return false;
    }

    revealAncestorTabs(objectsTree);
    objectsTree->expandAll();
    pumpEvents();

    const QModelIndex sourceIndex = findObjectTreeIndexForLine(objectsTree->model(), sourceLineNumber);
    const QModelIndex targetIndex = findObjectTreeIndexForLine(objectsTree->model(), targetLineNumber);
    if (!sourceIndex.isValid() || !targetIndex.isValid()) {
        std::cerr << "Objects tree drag source/target line was not found: source valid="
                  << sourceIndex.isValid() << ", target valid=" << targetIndex.isValid() << '\n';
        return false;
    }

    const QModelIndex sourceDragIndex = sourceIndex.sibling(sourceIndex.row(), kInspectorObjectDragColumnForTest);
    if (!sourceDragIndex.isValid()
        || sourceDragIndex.data(Qt::DecorationRole).value<QIcon>().isNull()
        || sourceDragIndex.data(Qt::ToolTipRole).toString().isEmpty()) {
        std::cerr << "Objects tree movable rows should expose a visible drag handle icon and tooltip.\n";
        return false;
    }

    objectsTree->scrollTo(sourceIndex, QAbstractItemView::EnsureVisible);
    objectsTree->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    pumpEvents();

    const QRect sourceRect = objectsTree->visualRect(sourceDragIndex);
    const QRect targetRect = objectsTree->visualRect(targetIndex);
    if (!sourceRect.isValid() || !targetRect.isValid()) {
        std::cerr << "Objects tree drag source/target visual rect was invalid: source="
                  << sourceRect.isValid() << ", target=" << targetRect.isValid()
                  << ", tree visible=" << objectsTree->isVisible()
                  << ", viewport visible=" << objectsTree->viewport()->isVisible() << '\n';
        return false;
    }

    const QPoint sourcePoint = sourceRect.center();
    sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
    pumpEvents();
    if (!objectsTree->hasMouseTracking()
        || !objectsTree->viewport()->hasMouseTracking()
        || objectsTree->viewport()->cursor().shape() != Qt::OpenHandCursor) {
        std::cerr << "Objects tree movable row hover should enable mouse tracking and use an open-hand cursor.\n";
        return false;
    }
    for (int actionColumn : {kInspectorObjectVisibilityColumnForTest, kInspectorObjectDeleteColumnForTest}) {
        const QModelIndex actionIndex = sourceIndex.sibling(sourceIndex.row(), actionColumn);
        const QRect actionRect = objectsTree->visualRect(actionIndex);
        if (!actionRect.isValid()) {
            std::cerr << "Objects tree action column visual rect should be valid.\n";
            return false;
        }
        sendMouse(objectsTree->viewport(), QEvent::MouseMove, actionRect.center(), Qt::NoButton, Qt::NoButton);
        pumpEvents();
        if (objectsTree->viewport()->cursor().shape() != Qt::PointingHandCursor) {
            std::cerr << "Objects tree visibility/delete action hover should use a pointing-hand cursor.\n";
            return false;
        }
    }
    sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
    pumpEvents();

    const QPoint targetPoint(targetRect.center().x(),
                             position == InspectorObjectDropPosition::AfterTarget
                                 ? targetRect.bottom() - 1
                                 : targetRect.top() + 1);
    if (exerciseInvalidBeforeDrop) {
        const QPoint invalidPoint = sourcePoint;
        sendMouse(objectsTree->viewport(), QEvent::MouseButtonPress, sourcePoint, Qt::LeftButton, Qt::LeftButton);
        pumpEvents();
        sendMouse(objectsTree->viewport(), QEvent::MouseMove, invalidPoint, Qt::NoButton, Qt::LeftButton);
        pumpEvents();
        auto *dropIndicator = objectsTree->viewport()->findChild<QWidget *>(QStringLiteral("mapObjectsTreeDropIndicator"));
        if ((dropIndicator != nullptr && dropIndicator->isVisible())
            || objectsTree->viewport()->cursor().shape() != Qt::ForbiddenCursor) {
            std::cerr << "Objects tree invalid drop target should hide indicator and use forbidden cursor.\n";
            return false;
        }
        sendMouse(objectsTree->viewport(), QEvent::MouseButtonRelease, invalidPoint, Qt::LeftButton, Qt::NoButton);
        pumpEvents();

        sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
        pumpEvents();
    }
    if (exerciseCurrentLocationBeforeDrop) {
        const int noOpTargetLineNumber = currentLocationTargetLineNumber > 0
            ? currentLocationTargetLineNumber
            : targetLineNumber;
        const QModelIndex currentLocationTargetIndex = findObjectTreeIndexForLine(objectsTree->model(), noOpTargetLineNumber);
        if (!currentLocationTargetIndex.isValid()) {
            std::cerr << "Objects tree current-location target line was not found: "
                      << noOpTargetLineNumber << '\n';
            return false;
        }
        objectsTree->scrollTo(currentLocationTargetIndex, QAbstractItemView::EnsureVisible);
        pumpEvents();
        const QRect currentLocationTargetRect = objectsTree->visualRect(currentLocationTargetIndex);
        if (!currentLocationTargetRect.isValid()) {
            std::cerr << "Objects tree current-location target visual rect was invalid.\n";
            return false;
        }
        const QPoint currentLocationPoint(
            currentLocationTargetRect.center().x(),
            currentLocationPosition == InspectorObjectDropPosition::AfterTarget
                ? currentLocationTargetRect.bottom() - 1
                : currentLocationTargetRect.top() + 1);
        sendMouse(objectsTree->viewport(), QEvent::MouseButtonPress, sourcePoint, Qt::LeftButton, Qt::LeftButton);
        pumpEvents();
        sendMouse(objectsTree->viewport(), QEvent::MouseMove, currentLocationPoint, Qt::NoButton, Qt::LeftButton);
        pumpEvents();
        auto *dropIndicator = objectsTree->viewport()->findChild<QWidget *>(QStringLiteral("mapObjectsTreeDropIndicator"));
        if ((dropIndicator != nullptr && dropIndicator->isVisible())
            || objectsTree->viewport()->cursor().shape() != Qt::ForbiddenCursor) {
            std::cerr << "Objects tree current-location drop should hide indicator and use forbidden cursor.\n";
            return false;
        }
        sendMouse(objectsTree->viewport(), QEvent::MouseButtonRelease, currentLocationPoint, Qt::LeftButton, Qt::NoButton);
        pumpEvents();

        sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
        pumpEvents();
    }

    sendMouse(objectsTree->viewport(), QEvent::MouseButtonPress, sourcePoint, Qt::LeftButton, Qt::LeftButton);
    pumpEvents();
    sendMouse(objectsTree->viewport(), QEvent::MouseMove, targetPoint, Qt::NoButton, Qt::LeftButton);
    pumpEvents();
    auto *dropIndicator = objectsTree->viewport()->findChild<QWidget *>(QStringLiteral("mapObjectsTreeDropIndicator"));
    if (dropIndicator == nullptr || !dropIndicator->isVisible()) {
        std::cerr << "Objects tree drag should show a visible drop indicator before release.\n";
        return false;
    }
    const QRect indicatorRect = dropIndicator->geometry();
    const int expectedDropY = position == InspectorObjectDropPosition::AfterTarget
        ? targetRect.bottom()
        : targetRect.top();
    if (objectsTree->viewport()->cursor().shape() != Qt::ClosedHandCursor) {
        std::cerr << "Objects tree drag should use a closed-hand cursor while moving a row.\n";
        return false;
    }
    if (indicatorRect.height() != 2
        || indicatorRect.width() < targetRect.width()
        || qAbs(indicatorRect.center().y() - expectedDropY) > 3) {
        std::cerr << "Objects tree drop indicator should be a slim line at the target edge; geometry="
                  << indicatorRect.x() << ',' << indicatorRect.y() << ' '
                  << indicatorRect.width() << 'x' << indicatorRect.height()
                  << ", expected target y=" << expectedDropY << '\n';
        return false;
    }
    if (exerciseCancelBeforeDrop) {
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(objectsTree->viewport(), &leaveEvent);
        pumpEvents();
        if (dropIndicator->isVisible()
            || objectsTree->viewport()->cursor().shape() == Qt::ClosedHandCursor
            || objectsTree->viewport()->cursor().shape() == Qt::OpenHandCursor) {
            std::cerr << "Objects tree drag leave should clear indicator and hand cursor state.\n";
            return false;
        }

        sendMouse(objectsTree->viewport(), QEvent::MouseMove, sourcePoint, Qt::NoButton, Qt::NoButton);
        pumpEvents();
        sendMouse(objectsTree->viewport(), QEvent::MouseButtonPress, sourcePoint, Qt::LeftButton, Qt::LeftButton);
        pumpEvents();
        sendMouse(objectsTree->viewport(), QEvent::MouseMove, targetPoint, Qt::NoButton, Qt::LeftButton);
        pumpEvents();
        dropIndicator = objectsTree->viewport()->findChild<QWidget *>(QStringLiteral("mapObjectsTreeDropIndicator"));
        if (dropIndicator == nullptr
            || !dropIndicator->isVisible()
            || objectsTree->viewport()->cursor().shape() != Qt::ClosedHandCursor) {
            std::cerr << "Objects tree drag should restart cleanly after leave cancellation.\n";
            return false;
        }
    }
    sendMouse(objectsTree->viewport(), QEvent::MouseButtonRelease, targetPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (dropIndicator->isVisible()) {
        std::cerr << "Objects tree drag drop indicator should be hidden after release.\n";
        return false;
    }
    return true;
}

int runInspectorObjectMoveScenario(const char *scenarioName,
                                   const QByteArray &th2Contents,
                                   int sourceLineNumber,
                                   int targetLineNumber,
                                   InspectorObjectDropPosition position,
                                   int expectedCurrentLineNumber,
                                   const QString &expectedMovedText,
                                   int currentLocationTargetLineNumber = 0,
                                   InspectorObjectDropPosition currentLocationPosition = InspectorObjectDropPosition::BeforeTarget)
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for object move smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("object_move_%1.th2").arg(QString::fromLatin1(scenarioName)));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for object move smoke test.")) {
        return 1;
    }

    file.write(th2Contents);
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(900, 700);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for object move smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->model() != nullptr,
                "Objects inspector tree was not initialized for object move smoke test.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    if (!expect(dragObjectTreeRow(objectsTree,
                                  sourceLineNumber,
                                  targetLineNumber,
                                  position,
                                  qstrcmp(scenarioName, "after_line") == 0,
                                  qstrcmp(scenarioName, "after_line") == 0,
                                  currentLocationTargetLineNumber > 0,
                                  currentLocationTargetLineNumber,
                                  currentLocationPosition),
                "Dragging an object row onto another object row should be routed through the inspector move handler.")) {
        return 1;
    }

    const QString movedText = mapTab->text();
    if (!expect(movedText == expectedMovedText,
                "Dragging an object row in Objects inspector should rewrite the TH2 source order.")) {
        std::cerr << "Scenario: " << scenarioName << '\n';
        std::cerr << "Actual moved source:\n" << movedText.toStdString() << '\n';
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == expectedCurrentLineNumber,
                "After object-row move, the text cursor should reveal the moved object at its new source line.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Object-row move should be undoable.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == originalText,
                "Undo after object-row move should restore the original source order.")) {
        return 1;
    }
    if (!expect(!mapTab->canUndo(),
                "Undo after object-row move should not leave a duplicate text-editor undo command.")) {
        return 1;
    }
    if (!expect(mapTab->canRedo(), "Object-row move should be redoable after undo.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == expectedMovedText,
                "Redo after object-row move should restore the moved source order.")) {
        return 1;
    }
    if (!expect(!mapTab->canRedo(),
                "Redo after object-row move should consume the map redo command.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int runInspectorObjectMoveSmoke()
{
    const QByteArray afterLineContents =
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "point 1 2 station -name P1\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 3 4 station -name P2\n"
        "endscrap\n";
    const QString afterLineExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("after_line",
                                                      afterLineContents,
                                                      3,
                                                      4,
                                                      InspectorObjectDropPosition::AfterTarget,
                                                      7,
                                                      afterLineExpected,
                                                      4,
                                                      InspectorObjectDropPosition::BeforeTarget);
        rc != 0) {
        return rc;
    }

    const QByteArray beforePointContents =
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "# keep hidden source between visible objects\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "endscrap\n";
    const QString beforePointExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap move -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "point 1 2 station -name P1\n"
        "point 3 4 station -name P2\n"
        "# keep hidden source between visible objects\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("before_point",
                                                      beforePointContents,
                                                      6,
                                                      3,
                                                      InspectorObjectDropPosition::BeforeTarget,
                                                      3,
                                                      beforePointExpected,
                                                      4,
                                                      InspectorObjectDropPosition::AfterTarget);
        rc != 0) {
        return rc;
    }

    const QByteArray betweenScrapContents =
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n";
    const QString betweenScrapExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "area water\n"
        "  0 0 1 0 1 1\n"
        "endarea\n"
        "point 5 6 label -text target\n"
        "endscrap\n");
    if (const int rc = runInspectorObjectMoveScenario("between_scraps",
                                                      betweenScrapContents,
                                                      3,
                                                      8,
                                                      InspectorObjectDropPosition::BeforeTarget,
                                                      5,
                                                      betweenScrapExpected);
        rc != 0) {
        return rc;
    }

    const QByteArray intoScrapContents =
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "point 1 2 station -name P1\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "endscrap\n";
    const QString intoScrapExpected = QStringLiteral(
        "encoding utf-8\n"
        "scrap a -projection plan\n"
        "endscrap\n"
        "scrap b -projection plan\n"
        "point 5 6 label -text target\n"
        "point 1 2 station -name P1\n"
        "endscrap\n");
    return runInspectorObjectMoveScenario("into_scrap",
                                          intoScrapContents,
                                          3,
                                          5,
                                          InspectorObjectDropPosition::AfterTarget,
                                          6,
                                          intoScrapExpected);
}

int runTemplateBezierEditScenario(const QString &fixtureName,
                                  const QString &th2RelativePath,
                                  int lineNumber,
                                  bool expectBackgroundLayer,
                                  bool loadWhileHidden = false,
                                  const QString &targetProjectName = QString(),
                                  bool openConfigSibling = false)
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for template Bezier smoke test.")) {
        return 1;
    }

    const QString fixtureRoot = repositoryFilePath(
        QStringLiteral("tests/fixtures/projects/map_editor_regression/%1").arg(fixtureName));
    const QString projectRoot = QDir(tempDir.path()).filePath(targetProjectName.isEmpty()
                                                                 ? fixtureName
                                                                 : targetProjectName);
    if (!expect(copyDirectoryRecursively(fixtureRoot, projectRoot),
                "Failed to copy map editor regression fixture project.")) {
        return 1;
    }
    const QString filePath = QDir(projectRoot).filePath(th2RelativePath);

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(900, 700);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QTabWidget *documentTabs = nullptr;
    if (openConfigSibling) {
        documentTabs = new QTabWidget(central);
        layout->addWidget(documentTabs);
    }

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), openConfigSibling ? nullptr : central);
    mapTab->setProjectRootPath(projectRoot);
    if (!openConfigSibling) {
        layout->addWidget(mapTab);
    }
    hostWindow.setCentralWidget(central);
    if (!loadWhileHidden) {
        hostWindow.show();
        pumpEvents();
    }

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load template-style Bezier TH2 file.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    TextEditorTab *configTab = nullptr;
    if (openConfigSibling) {
        documentTabs->addTab(mapTab, QStringLiteral("scrap1.th2"));
        configTab = new TextEditorTab(fileSystem, CommandCatalogStore(), documentTabs);
        configTab->setProjectRootPath(projectRoot);
        configTab->setModeSelectorVisible(false);
        QString configErrorMessage;
        if (!expect(configTab->loadFile(QDir(projectRoot).filePath(QStringLiteral("main.thconfig")),
                                        &configErrorMessage),
                    "Template-style config sibling tab should load next to the map editor.")) {
            if (!configErrorMessage.isEmpty()) {
                std::cerr << configErrorMessage.toStdString() << '\n';
            }
            return 1;
        }
        documentTabs->addTab(configTab, QStringLiteral("main.thconfig"));
        documentTabs->setCurrentWidget(configTab);
    }
    if (loadWhileHidden) {
        hostWindow.show();
    }
    pumpEvents();
    if (openConfigSibling) {
        documentTabs->setCurrentWidget(mapTab);
        pumpEvents();
    }

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr && mapView->scene() != nullptr,
                "Map view was not initialized for template-style Bezier smoke test.")) {
        return 1;
    }

    if (expectBackgroundLayer
        && !expect(mapTab->backgroundLayerCount() == 1,
                   "Background fixture should load one background layer from TH2 metadata.")) {
        return 1;
    }

    mapTab->goToLine(lineNumber);
    pumpEvents();
    auto *linePath = findPathItemForLine(mapView->scene(), lineNumber);
    if (!expect(linePath != nullptr, "Template-style Bezier line path was not found.")) {
        return 1;
    }
    const QPointF lineClickPoint = linePath->mapToScene(linePath->path().pointAtPercent(0.5));
    if (!expect(clickScenePoint(mapView, lineClickPoint), "Failed to click template-style Bezier line path.")) {
        return 1;
    }
    pumpEvents();

    const QString originalText = mapTab->text();
    mapTab->triggerSelectMode();
    pumpEvents();
    auto *controlItem = waitForVisibleLineControl(mapView->scene(), mapView, lineNumber, 2000);
    if (!expect(controlItem != nullptr,
                "Clicking a template-style Bezier line segment should reveal editable control handles.")) {
        return 1;
    }

    Q_UNUSED(controlItem);
    if (!expect(dragAnyVisibleLineControlUntilTextChanges(mapTab, mapView, lineNumber, originalText),
                "Dragging a template-style Bezier line control handle should update TH2 source text.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Template-style Bezier control drag should be undoable.")) {
        return 1;
    }

    mapTab->triggerUndo();
    if (!expect(waitForTextEquals(mapTab, originalText, 2000),
                "Undo after template-style Bezier control drag should restore original TH2 source text.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int runTemplateBezierEditSmoke()
{
    if (const int rc = runTemplateBezierEditScenario(QStringLiteral("default_template_like"),
                                                     QStringLiteral("scraps/scrap1.th2"),
                                                     6,
                                                     false);
        rc != 0) {
        return rc;
    }
    if (const int rc = runTemplateBezierEditScenario(QStringLiteral("default_template_like"),
                                                     QStringLiteral("scraps/scrap1.th2"),
                                                     6,
                                                     false,
                                                     true,
                                                     QStringLiteral("New Project"),
                                                     true);
        rc != 0) {
        return rc;
    }
    if (const int rc = runTemplateBezierEditScenario(QStringLiteral("xvi_background_like"),
                                                     QStringLiteral("scraps/scrap1.th2"),
                                                     9,
                                                     true);
        rc != 0) {
        return rc;
    }
    return runTemplateBezierEditScenario(QStringLiteral("real_cave_raster"),
                                         QStringLiteral("data/clopy01.th2"),
                                         30,
                                         true);
}

int runAreaBorderHitSelectionSmoke()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for area hit-selection smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("area_hit_selection_smoke.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for area hit-selection smoke test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap area-hit -projection plan\n"
        "line wall -id border\n"
        "  0 0\n"
        "  100 0\n"
        "  100 -100\n"
        "  0 -100\n"
        "  0 0\n"
        "endline\n"
        "area water\n"
        "  border\n"
        "endarea\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(900, 700);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load referenced-area TH2 file for hit-selection smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();
    mapTab->triggerSelectMode();
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr && mapView->scene() != nullptr,
                "Map view/scene was not initialized for area hit-selection smoke test.")) {
        return 1;
    }
    auto *objectsTree = mapTab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    if (!expect(objectsTree != nullptr && objectsTree->selectionModel() != nullptr,
                "Objects inspector tree was not initialized for area hit-selection smoke test.")) {
        return 1;
    }

    auto *linePathItem = findPathItemForLine(mapView->scene(), 4);
    auto *areaFillItem = findPathItemForLine(mapView->scene(), 11, kMapSceneSelectionSubtypeAreaFill);
    if (!expect(linePathItem != nullptr, "Referenced area border line path was not found in the map scene.")) {
        return 1;
    }
    if (!expect(areaFillItem != nullptr, "Referenced area fill path was not found in the map scene.")) {
        return 1;
    }

    mapView->centerOn(areaFillItem);
    pumpEvents();

    const QPointF borderScenePoint = linePathItem->mapToScene(linePathItem->path().pointAtPercent(0.25));
    const QPoint borderViewportPoint = mapView->mapFromScene(borderScenePoint);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, borderViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, borderViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4}),
                "Clicking a referenced area border should select the owning line object.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == 4,
                "Clicking a referenced area border should move the text cursor to the line directive.")) {
        return 1;
    }
    if (!expect(!objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Map-object selection should select the corresponding row in the Objects inspector.")) {
        return 1;
    }

    const QPointF fillCenterScene = areaFillItem->mapToScene(areaFillItem->path().boundingRect().center());
    const QPointF borderSegmentScenePoint = linePathItem->mapToScene(linePathItem->path().pointAtPercent(0.125));
    const QPointF borderToCenter = fillCenterScene - borderSegmentScenePoint;
    const qreal borderToCenterLength = std::hypot(borderToCenter.x(), borderToCenter.y());
    if (!expect(borderToCenterLength > 0.001,
                "Area fill center should be distinct from the referenced border hit point.")) {
        return 1;
    }
    const QPointF nearBorderFillScenePoint = borderSegmentScenePoint + (borderToCenter / borderToCenterLength) * 4.0;
    mapView->resetTransform();
    mapView->scale(6.0, 6.0);
    mapView->centerOn(nearBorderFillScenePoint);
    const QPoint nearBorderFillViewportPoint = mapView->mapFromScene(nearBorderFillScenePoint);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, nearBorderFillViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, nearBorderFillViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    const QSet<int> nearBorderSelection = selectedSourceLineNumbers(mapView->scene());
    if (!expect(nearBorderSelection == QSet<int>({11}),
                "Clicking inside an area near its border at high zoom should select the area, not the border line.")) {
        std::cerr << "Actual selected source lines:";
        for (const int lineNumber : nearBorderSelection) {
            std::cerr << ' ' << lineNumber;
        }
        std::cerr << '\n';
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == 11,
                "Clicking inside an area near its border at high zoom should move the text cursor to the area directive.")) {
        return 1;
    }

    mapTab->goToLine(4);
    pumpEvents();

    const QPointF fillScenePoint = areaFillItem->mapToScene(areaFillItem->path().boundingRect().center());
    const QPoint fillViewportPoint = mapView->mapFromScene(fillScenePoint);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fillViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fillViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({11}),
                "Clicking inside a referenced area fill should select the area object.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == 11,
                "Clicking inside a referenced area fill should move the text cursor to the area directive.")) {
        return 1;
    }
    if (!expect(!objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Area fill selection should select the corresponding row in the Objects inspector.")) {
        return 1;
    }

    const QPoint emptyViewportPoint = mapView->mapFromScene(QPointF(36.0, 36.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, emptyViewportPoint, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, emptyViewportPoint, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(mapView->scene()->selectedItems().isEmpty(),
                "Clicking empty map-canvas space should clear the graphical selection.")) {
        return 1;
    }
    if (!expect(objectsTree->selectionModel()->selectedRows().isEmpty(),
                "Clicking empty map-canvas space should clear the Objects inspector selection.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}

int runDragUndoRedoSmoke()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for smoke test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("drag_undo_redo_smoke.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for smoke test.")) {
        return 1;
    }

    const QByteArray th2Contents =
        "encoding utf-8\n"
        "\n"
        "scrap smoke -projection plan\n"
        "line wall\n"
        "  0 0\n"
        "  100 0\n"
        "  100 -100\n"
        "  smooth off\n"
        "  subtype presumed\n"
        "  altitude 12\n"
        "endline\n"
        "area water\n"
        "  0 0\n"
        "  100 0 100 -100\n"
        "endarea\n"
        "\n"
        "point 200 -50 station -name P1\n"
        "endscrap\n";
    file.write(th2Contents);
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(1280, 900);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load temporary TH2 file for smoke test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    if (!expect(mapView != nullptr, "Map view was not found in MapEditorTab.")) {
        return 1;
    }
    if (!expect(mapView->scene() != nullptr, "Map scene was not initialized.")) {
        return 1;
    }
    auto *textEditor = mapTab->findChild<TextEditorTab *>();
    if (!expect(textEditor != nullptr, "Text editor was not found in MapEditorTab.")) {
        return 1;
    }
    auto *sourceEditor = textEditor->findChild<QPlainTextEdit *>();
    if (!expect(sourceEditor != nullptr, "Underlying source editor was not found in TextEditorTab.")) {
        return 1;
    }

    const QRectF visibleSceneRect = mapView->mapToScene(mapView->viewport()->rect()).boundingRect();
    mapTab->goToLine(4);
    pumpEvents();
    if (!expect(mapTab->currentLineNumber() == 4, "Expected map tab current line to be the line object start.")) {
        return 1;
    }
    textEditor->goToLineColumn(3, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4, 12, 17}),
                "Moving text cursor to scrap should select all map objects inside the scrap block.")) {
        return 1;
    }
    textEditor->goToLineColumn(18, 3);
    pumpEvents();
    if (!expect(selectedSourceLineNumbers(mapView->scene()) == QSet<int>({4, 12, 17}),
                "Moving text cursor to endscrap should select all map objects inside the scrap block.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();

    auto *anchorItem = findCenteredLineAnchor(mapView->scene(), visibleSceneRect);
    if (!expect(anchorItem != nullptr, "No visible editable line anchor was found after selecting line geometry.")) {
        return 1;
    }
    anchorItem->setSelected(true);
    pumpEvents();
    const int anchorSelectedLine = mapTab->currentLineNumber();
    if (!expect(anchorSelectedLine >= 5 && anchorSelectedLine <= 7,
                "Selecting map vertex should move text editor cursor to that vertex coordinate row.")) {
        return 1;
    }
    textEditor->goToLineColumn(8, 3);
    const std::optional<SelectedLineVertexSnapshot> selectedVertexFromSmooth =
        waitForSelectedLineVertex(mapView->scene(), 4, 2);
    if (!expect(selectedVertexFromSmooth.has_value(),
                "Moving text cursor to a smooth-option row should select the corresponding map vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromSmooth->lineNumber == 4 && selectedVertexFromSmooth->vertexIndex == 2,
                "Text-to-map sync should map smooth-option row to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(9, 3);
    const std::optional<SelectedLineVertexSnapshot> selectedVertexFromSubtype =
        waitForSelectedLineVertex(mapView->scene(), 4, 2);
    if (!expect(selectedVertexFromSubtype.has_value(),
                "Moving text cursor to subtype-option row should select the current line vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromSubtype->lineNumber == 4 && selectedVertexFromSubtype->vertexIndex == 2,
                "Text-to-map sync should map subtype-option row to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(10, 3);
    const std::optional<SelectedLineVertexSnapshot> selectedVertexFromGenericOption =
        waitForSelectedLineVertex(mapView->scene(), 4, 2);
    if (!expect(selectedVertexFromGenericOption.has_value(),
                "Moving text cursor to an arbitrary line option row should select the current line vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromGenericOption->lineNumber == 4 && selectedVertexFromGenericOption->vertexIndex == 2,
                "Text-to-map sync should map arbitrary line option rows to the current line vertex.")) {
        return 1;
    }
    textEditor->goToLineColumn(11, 3);
    if (!expect(waitForSelectedSourceLineNumber(mapView->scene(), 4, 2000) == 4,
                "Moving text cursor to endline should keep selection on the owning line block start.")) {
        return 1;
    }
    textEditor->goToLineColumn(15, 3);
    const int selectedAfterEndArea = waitForSelectedSourceLineNumber(mapView->scene(), 12, 2000);
    if (!expect(selectedAfterEndArea == 12,
                "Moving text cursor to endarea should keep selection on the owning area block start.")) {
        return 1;
    }

    auto *pointItem = findPointItemForLine(mapView->scene(), 17);
    if (!expect(pointItem != nullptr, "Failed to find map point geometry item for source line 17.")) {
        return 1;
    }
    mapView->scene()->clearSelection();
    pointItem->setData(kMapSceneLineNumberRole, 16);
    pointItem->setSelected(true);
    pumpEvents();
    if (!expect(mapTab->currentLineNumber() == 17,
                "Selecting map point geometry should resolve the coordinate row, not the blank line above it.")) {
        return 1;
    }
    mapTab->goToLine(4);
    pumpEvents();
    textEditor->goToLineColumn(6, 3);
    pumpEvents();
    auto *selectedVertexFromText = findSelectedLineVertex(mapView->scene());
    if (!expect(selectedVertexFromText != nullptr,
                "Moving text cursor to a vertex row should select the corresponding map vertex.")) {
        return 1;
    }
    if (!expect(selectedVertexFromText->lineNumber() == 4 && selectedVertexFromText->vertexIndex() == 1,
                "Text-to-map vertex sync should select line vertex index 1 for source row line 6.")) {
        return 1;
    }

    const QString mixedUndoMarker = QStringLiteral("# mixed-undo-marker");
    const QString baselineBeforeMixedUndo = mapTab->text();
    textEditor->goToLineColumn(18, 9);
    pumpEvents();
    sourceEditor->setFocus(Qt::OtherFocusReason);
    sourceEditor->insertPlainText(QStringLiteral("\n") + mixedUndoMarker);
    if (!expect(mapTab->text() != baselineBeforeMixedUndo,
                "Failed to apply text-side undo entry before mixed map/text arbitration test.")) {
        return 1;
    }
    pumpEvents();

    const QString textAfterTextUndoEntry = mapTab->text();
    if (!expect(textAfterTextUndoEntry.contains(mixedUndoMarker),
                "Mixed undo arbitration test should include marker text after text-side edit.")) {
        return 1;
    }

    mapTab->goToLine(4);
    pumpEvents();

    auto *mixedUndoAnchorItem = findCenteredLineAnchor(mapView->scene(), visibleSceneRect);
    if (!expect(mixedUndoAnchorItem != nullptr,
                "No visible editable line anchor was found for mixed map/text undo arbitration test.")) {
        return 1;
    }
    mixedUndoAnchorItem->setSelected(true);
    pumpEvents();
    mapTab->triggerSelectMode();
    pumpEvents();

    if (!expect(dragItemBySceneDelta(mapView, mixedUndoAnchorItem, QPointF(28.0, 16.0)),
                "Failed to apply map-side undo entry for mixed map/text arbitration test.")) {
        return 1;
    }

    if (!expect(waitForTextDiffers(mapTab, textAfterTextUndoEntry, 2000),
                "Mixed map/text arbitration test should eventually apply the map edit on top of marker text.")) {
        return 1;
    }
    const QString textAfterMixedMapEdit = mapTab->text();
    if (!expect(textAfterMixedMapEdit.contains(mixedUndoMarker) && textAfterMixedMapEdit != textAfterTextUndoEntry,
                "Mixed map/text arbitration test should produce a map edit layered on top of marker text.")) {
        return 1;
    }
    if (!expect(mapTab->nextUndoOwner() == MapEditorUndoOwner::MapCommand,
                "Mixed map/text state should report map command as next undo owner when both map and text undo are available.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterTextUndoEntry,
                "First mixed undo should revert the map-side edit while keeping text-side marker edit.")) {
        return 1;
    }
    if (!expect(mapTab->nextRedoOwner() == MapEditorUndoOwner::MapCommand,
                "Mixed map/text state should report map command as next redo owner immediately after map undo.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterMixedMapEdit,
                "Mixed redo should restore the full combined map+text edit state.")) {
        return 1;
    }

    const QString mixedTimelineMarker = QStringLiteral("# mixed-timeline-marker");
    textEditor->goToLineColumn(18, 9);
    pumpEvents();
    sourceEditor->setFocus(Qt::OtherFocusReason);
    sourceEditor->insertPlainText(QStringLiteral("\n") + mixedTimelineMarker);
    pumpEvents();

    const QString textAfterTextTimelineEdit = mapTab->text();
    if (!expect(textAfterTextTimelineEdit.contains(mixedTimelineMarker)
                    && textAfterTextTimelineEdit != textAfterMixedMapEdit,
                "Mixed timeline arbitration test should add a text edit after the map edit.")) {
        return 1;
    }
    if (!expect(mapTab->nextUndoOwner() == MapEditorUndoOwner::TextEdit,
                "Mixed map/text state should report text edit as next undo owner after a newer text edit.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterMixedMapEdit,
                "Timeline undo should revert the newer text-side edit while keeping the older map edit.")) {
        return 1;
    }
    if (!expect(mapTab->nextRedoOwner() == MapEditorUndoOwner::TextEdit,
                "Mixed map/text state should report text edit as next redo owner after text undo.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterTextTimelineEdit,
                "Timeline redo should restore the newer text-side edit.")) {
        return 1;
    }

    QString mixedResetError;
    if (!expect(mapTab->loadFile(filePath, &mixedResetError),
                "Mixed map/text arbitration cleanup should reload baseline TH2 source.")) {
        if (!mixedResetError.isEmpty()) {
            std::cerr << mixedResetError.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();
    if (!expect(mapTab->text() == baselineBeforeMixedUndo,
                "Mixed map/text arbitration cleanup should restore baseline before continuing smoke test.")) {
        return 1;
    }

    const QString textBeforeVertexDelete = mapTab->text();
    if (!expect(!textBeforeVertexDelete.isEmpty(), "TH2 text should not be empty before vertex delete test.")) {
        return 1;
    }
    mapView->setFocus(Qt::OtherFocusReason);
    mapView->viewport()->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_Backspace);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_Backspace);
    pumpEvents();
    waitForMs(140);

    const QString textAfterVertexDelete = mapTab->text();
    if (!expect(textAfterVertexDelete != textBeforeVertexDelete,
                "Backspace on a selected line vertex should modify TH2 text by deleting the selected vertex.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Vertex delete should be undoable.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeVertexDelete,
                "Undo after vertex delete should restore original TH2 text.")) {
        return 1;
    }
    if (!expect(mapTab->canRedo(), "Vertex delete undo should enable redo.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterVertexDelete,
                "Redo after vertex delete undo should restore edited TH2 text.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeVertexDelete,
                "Final undo after vertex-delete redo should return the TH2 text to baseline for subsequent tests.")) {
        return 1;
    }

    mapTab->goToLine(4);
    pumpEvents();

    const QString originalText = mapTab->text();
    if (!expect(!originalText.isEmpty(), "Loaded TH2 text should not be empty.")) {
        return 1;
    }

    const QRectF dragVisibleSceneRect = mapView->mapToScene(mapView->viewport()->rect()).boundingRect();
    auto *dragAnchorItem = findCenteredLineAnchor(mapView->scene(), dragVisibleSceneRect);
    if (!expect(dragAnchorItem != nullptr, "No visible editable line anchor was found before drag edit.")) {
        return 1;
    }
    if (!expect(clickItem(mapView, dragAnchorItem), "Failed to click editable map anchor before drag edit.")) {
        return 1;
    }
    dragAnchorItem = findSelectedLineVertex(mapView->scene());
    if (!expect(dragAnchorItem != nullptr && dragAnchorItem->lineNumber() == 4,
                "Clicking editable map anchor should select a draggable line vertex.")) {
        return 1;
    }
    mapTab->triggerSelectMode();
    pumpEvents();

    if (!expect(dragItemBySceneDelta(mapView, dragAnchorItem, QPointF(36.0, -24.0)),
                "Failed to drag editable map anchor during smoke test.")) {
        return 1;
    }

    const QString changedText = mapTab->text();
    if (!expect(changedText != originalText, "Drag did not change TH2 text content.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(), "Undo should be enabled after a map drag edit.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();

    if (!expect(mapTab->text() == originalText, "Undo did not restore original TH2 text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Map tab should be clean after undo to baseline.")) {
        return 1;
    }
    if (!expect(mapTab->canRedo(), "Redo should be enabled after undoing map drag edit.")) {
        return 1;
    }

    mapTab->triggerRedo();
    pumpEvents();

    if (!expect(mapTab->text() == changedText, "Redo did not restore edited TH2 text.")) {
        return 1;
    }
    if (!expect(mapTab->isDirty(), "Map tab should be dirty after redo reapplies edit.")) {
        return 1;
    }

    const QString textBeforeInteractiveDrawing = mapTab->text();
    const int pointDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("point"));
    const int lineDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("line"));
    const int areaDirectivesBefore = countDirectiveLines(textBeforeInteractiveDrawing, QStringLiteral("area"));
    const QPoint viewportCenter = mapView->viewport()->rect().center();
    const QString textBeforePointInsert = mapTab->text();
    mapTab->triggerAddPoint();
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, viewportCenter, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, viewportCenter, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("point")) == pointDirectivesBefore + 1,
                "Point mode click-to-place should insert one new point directive.")) {
        return 1;
    }
    const QString textAfterPointInsert = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforePointInsert,
                "Undo after Point mode insertion should remove the newly inserted point directive.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterPointInsert,
                "Redo after undoing Point mode insertion should restore the point directive.")) {
        return 1;
    }
    const QPoint secondPointPosition(viewportCenter.x() + 18, viewportCenter.y() + 12);
    const QString textAfterFirstPointInsert = mapTab->text();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondPointPosition, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondPointPosition, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    const QString textAfterSecondPointInsert = mapTab->text();
    if (!expect(countDirectiveLines(textAfterSecondPointInsert, QStringLiteral("point")) == pointDirectivesBefore + 2,
                "A second Point mode click should insert a second point directive.")) {
        return 1;
    }
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterFirstPointInsert,
                "First Undo after two Point mode insertions should remove only the second point directive.")) {
        return 1;
    }
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforePointInsert,
                "Second Undo after two Point mode insertions should remove the first point directive too.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterFirstPointInsert,
                "First Redo after two point undos should restore the first point directive.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterSecondPointInsert,
                "Second Redo after two point undos should restore the second point directive.")) {
        return 1;
    }

    const int pointDirectivesBeforeEscCancel = countDirectiveLines(mapTab->text(), QStringLiteral("point"));
    mapTab->triggerAddPoint();
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, viewportCenter, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, viewportCenter, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("point")) == pointDirectivesBeforeEscCancel,
                "Esc in Point mode should cancel insert mode and return to Select mode.")) {
        return 1;
    }

    mapTab->triggerAddLine();
    pumpEvents();
    const QString textBeforeLineInsert = mapTab->text();
    const QPoint firstLineVertex(viewportCenter.x() - 20, viewportCenter.y() - 12);
    const QPoint secondLineVertex(viewportCenter.x() + 24, viewportCenter.y() - 8);
    const QPoint thirdLineVertex(viewportCenter.x() + 18, viewportCenter.y() + 18);
    const QPoint fourthLineVertex(viewportCenter.x() - 14, viewportCenter.y() + 22);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, thirdLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, thirdLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fourthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fourthLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapView->setFocus(Qt::OtherFocusReason);
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_Return);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_Return);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBefore + 1,
                "Line mode click-to-add + Enter should insert one new line directive.")) {
        return 1;
    }
    if (!expect(lastDraftLineCoordinateTokenCount(mapTab->text()) >= 8,
                "Committing a 4-vertex line draft should preserve all captured vertices in source text.")) {
        return 1;
    }
    if (!expect(!lastDraftLineHasBezierCoordinateRow(mapTab->text()),
                "Line mode click-to-add without drag should commit straight coordinate rows, not bezier rows.")) {
        return 1;
    }
    const QString textAfterLineInsert = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeLineInsert,
                "Undo after Line mode completion should remove the newly inserted line block.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterLineInsert,
                "Redo after undoing Line mode completion should restore the line block.")) {
        return 1;
    }

    const QString textBeforeDraftStepUndo = mapTab->text();
    const int lineDirectivesBeforeDraftStepUndo = countDirectiveLines(textBeforeDraftStepUndo, QStringLiteral("line"));
    const QPoint draftUndoFirstVertex(viewportCenter.x() - 34, viewportCenter.y() + 34);
    const QPoint draftUndoSecondVertex(viewportCenter.x() + 34, viewportCenter.y() + 38);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, draftUndoFirstVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, draftUndoFirstVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, draftUndoSecondVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, draftUndoSecondVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeDraftStepUndo,
                "Undo during an active line draft should remove the draft vertex without undoing the previous committed line.")) {
        return 1;
    }
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeDraftStepUndo,
                "Undo during an active line draft should keep committed line directives intact.")) {
        return 1;
    }
    mapTab->triggerAddLine();
    pumpEvents();

    const int lineDirectivesBeforeModePersistenceCheck = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    const QPoint fifthLineVertex(viewportCenter.x() - 14, viewportCenter.y() + 26);
    const QPoint sixthLineVertex(viewportCenter.x() + 28, viewportCenter.y() + 30);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, fifthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, fifthLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, sixthLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, sixthLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeModePersistenceCheck + 1,
                "After Enter commit, Line mode should stay active so a new line can be inserted without reselecting Line mode.")) {
        return 1;
    }

    mapTab->triggerAddLine();
    pumpEvents();
    const int lineDirectivesBeforeDoubleClickCommit = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    const QPoint doubleClickFirstVertex(viewportCenter.x() - 52, viewportCenter.y() + 4);
    const QPoint doubleClickSecondVertex(viewportCenter.x() + 6, viewportCenter.y() + 8);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, doubleClickFirstVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, doubleClickFirstVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonDblClick, doubleClickSecondVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, doubleClickSecondVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeDoubleClickCommit + 1,
                "Line mode double-click should insert the double-clicked vertex and commit the line draft.")) {
        return 1;
    }
    const QString textAfterDoubleClickCommit = mapTab->text();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, QPoint(viewportCenter.x() + 42, viewportCenter.y() + 12), Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, QPoint(viewportCenter.x() + 42, viewportCenter.y() + 12), Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(mapTab->text() == textAfterDoubleClickCommit,
                "Line mode double-click commit should leave insert mode so the next click does not start another line.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint bezierAnchor1(viewportCenter.x() - 60, viewportCenter.y() - 30);
    const QPoint bezierAnchor2(viewportCenter.x() + 60, viewportCenter.y() - 22);
    const QPoint bezierDrag(viewportCenter.x() + 60, viewportCenter.y() + 24);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, bezierAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, bezierAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, bezierAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, bezierDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, bezierDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierInsert + 1,
                "Line mode click+drag should insert a new line directive.")) {
        return 1;
    }
    if (!expect(lastDraftLineHasBezierCoordinateRow(mapTab->text()),
                "Line mode click+drag should create a bezier coordinate row with control points.")) {
        return 1;
    }
    const auto baselineBezierRow = lastDraftLineBezierCoordinateRow(mapTab->text());
    if (!expect(baselineBezierRow.has_value(),
                "Bezier insert should expose a parseable coordinate row with control-point values.")) {
        return 1;
    }
    const QVector<QVector<double>> baselineBezierRows = lastDraftLineNumericRows(mapTab->text());
    if (!expect(baselineBezierRows.size() >= 2 && baselineBezierRows.first().size() >= 2,
                "Bezier insert should expose the start anchor row for XTherion-style control checks.")) {
        return 1;
    }
    if (!expect(std::abs(baselineBezierRow->at(0) - baselineBezierRows.first().at(0)) < 0.001
                    && std::abs(baselineBezierRow->at(1) - baselineBezierRows.first().at(1)) < 0.001,
                "XTherion-style two-point draft should serialize the inserted point's next control only once a following segment exists.")) {
        return 1;
    }
    const QPointF baselineBezierAnchor2Source(baselineBezierRow->at(4), baselineBezierRow->at(5));
    const QPointF baselineBezierIncomingSource(baselineBezierRow->at(2), baselineBezierRow->at(3));
    if (!expect(QLineF(baselineBezierAnchor2Source, baselineBezierIncomingSource).length() > 0.01,
                "XTherion-style dragged vertex placement should seed the inserted point's previous control.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierAdjustInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint adjustedBezierAnchor1(viewportCenter.x() - 62, viewportCenter.y() + 42);
    const QPoint adjustedBezierAnchor2(viewportCenter.x() + 58, viewportCenter.y() + 50);
    const QPoint adjustedBezierDrag(viewportCenter.x() + 58, viewportCenter.y() + 98);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, adjustedBezierAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, adjustedBezierAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, adjustedBezierAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, adjustedBezierDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, adjustedBezierDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QPointF initialControlScene = mapView->mapToScene(adjustedBezierDrag);
    const QPoint controlPress = mapView->mapFromScene(initialControlScene);
    const QPoint controlDrag = mapView->mapFromScene(initialControlScene + QPointF(48.0, -30.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, controlPress, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, controlDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, controlDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierAdjustInsert + 1,
                "Dragging a bezier control handle in Line mode should still commit as a new line.")) {
        return 1;
    }
    const auto adjustedBezierRow = lastDraftLineBezierCoordinateRow(mapTab->text());
    if (!expect(adjustedBezierRow.has_value(),
                "Adjusted bezier insert should keep a parseable coordinate row with control-point values.")) {
        return 1;
    }
    if (!expect(std::abs(adjustedBezierRow->at(2) - baselineBezierRow->at(2)) > 0.001
                || std::abs(adjustedBezierRow->at(3) - baselineBezierRow->at(3)) > 0.001,
                "Dragging a bezier control handle should change serialized control-point coordinates.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierAnchorMirrorInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint anchorMirror1(viewportCenter.x() - 88, viewportCenter.y() - 28);
    const QPoint anchorMirror2(viewportCenter.x() - 18, viewportCenter.y() - 14);
    const QPoint anchorMirrorDrag2(viewportCenter.x() - 18, viewportCenter.y() + 34);
    const QPoint anchorMirror3(viewportCenter.x() + 74, viewportCenter.y() - 40);
    const QPoint anchorMirrorDrag3(viewportCenter.x() + 74, viewportCenter.y() - 90);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirror1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, anchorMirrorDrag2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirrorDrag2, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, anchorMirror3, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, anchorMirrorDrag3, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, anchorMirrorDrag3, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierAnchorMirrorInsert + 1,
                "Placing consecutive curved anchors should still commit as a new line.")) {
        return 1;
    }
    const QVector<QVector<double>> anchorMirrorRows = lastDraftLineNumericRows(mapTab->text());
    if (!expect(anchorMirrorRows.size() >= 3 && anchorMirrorRows.at(1).size() >= 6 && anchorMirrorRows.at(2).size() >= 6,
                "Consecutive curved-anchor draft should serialize two cubic coordinate rows.")) {
        return 1;
    }
    const QPointF anchorMirrorMiddleAnchorSource(anchorMirrorRows.at(1).at(4), anchorMirrorRows.at(1).at(5));
    const QPointF anchorMirrorMiddleIncomingSource(anchorMirrorRows.at(1).at(2), anchorMirrorRows.at(1).at(3));
    const QPointF anchorMirrorMiddleOutgoingSource(anchorMirrorRows.at(2).at(0), anchorMirrorRows.at(2).at(1));
    const QPointF anchorMirrorIncomingVector = anchorMirrorMiddleIncomingSource - anchorMirrorMiddleAnchorSource;
    const QPointF anchorMirrorOutgoingVector = anchorMirrorMiddleOutgoingSource - anchorMirrorMiddleAnchorSource;
    const qreal anchorMirrorIncomingLength = std::hypot(anchorMirrorIncomingVector.x(), anchorMirrorIncomingVector.y());
    const qreal anchorMirrorOutgoingLength = std::hypot(anchorMirrorOutgoingVector.x(), anchorMirrorOutgoingVector.y());
    if (!expect(anchorMirrorIncomingLength > 0.01 && anchorMirrorOutgoingLength > 0.01,
                "Consecutive XTherion-style dragged anchors should keep both controls on the middle vertex.")) {
        return 1;
    }
    const qreal anchorMirrorCross = anchorMirrorIncomingVector.x() * anchorMirrorOutgoingVector.y()
        - anchorMirrorIncomingVector.y() * anchorMirrorOutgoingVector.x();
    const qreal anchorMirrorDot = anchorMirrorIncomingVector.x() * anchorMirrorOutgoingVector.x()
        + anchorMirrorIncomingVector.y() * anchorMirrorOutgoingVector.y();
    if (!expect(std::abs(anchorMirrorCross / (anchorMirrorIncomingLength * anchorMirrorOutgoingLength)) < 0.08
                    && anchorMirrorDot / (anchorMirrorIncomingLength * anchorMirrorOutgoingLength) < -0.9,
                "Consecutive XTherion-style dragged anchors should keep middle controls collinear/opposed.")) {
        return 1;
    }

    const int lineDirectivesBeforeBezierMirrorInsert = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint mirroredAnchor1(viewportCenter.x() - 70, viewportCenter.y() + 58);
    const QPoint mirroredAnchor2(viewportCenter.x() - 8, viewportCenter.y() + 46);
    const QPoint mirroredDrag2(viewportCenter.x() - 8, viewportCenter.y() + 92);
    const QPoint mirroredAnchor3(viewportCenter.x() + 66, viewportCenter.y() + 54);
    const QPoint mirroredDrag3(viewportCenter.x() + 66, viewportCenter.y() + 8);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor1, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredAnchor1, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor2, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredDrag2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredDrag2, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredAnchor3, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredDrag3, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredDrag3, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    const QPointF middleOutgoingControlScene = mapView->mapToScene(mirroredDrag3);
    const QPoint mirroredControlPress = mapView->mapFromScene(middleOutgoingControlScene);
    const QPoint mirroredControlDrag = mapView->mapFromScene(middleOutgoingControlScene + QPointF(52.0, -24.0));
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, mirroredControlPress, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, mirroredControlDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, mirroredControlDrag, Qt::LeftButton, Qt::NoButton);
    pumpEvents();

    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeBezierMirrorInsert + 1,
                "Dragging one control on a smooth draft vertex should still commit as a new line.")) {
        return 1;
    }
    const QVector<QVector<double>> mirroredRows = lastDraftLineNumericRows(mapTab->text());
    if (!expect(mirroredRows.size() >= 3 && mirroredRows.at(1).size() >= 6 && mirroredRows.at(2).size() >= 6,
                "Three-anchor bezier draft should serialize two cubic coordinate rows.")) {
        return 1;
    }
    const QPointF middleAnchorSource(mirroredRows.at(1).at(4), mirroredRows.at(1).at(5));
    const QPointF middleIncomingSource(mirroredRows.at(1).at(2), mirroredRows.at(1).at(3));
    const QPointF middleOutgoingSource(mirroredRows.at(2).at(0), mirroredRows.at(2).at(1));
    const QPointF incomingVector = middleIncomingSource - middleAnchorSource;
    const QPointF outgoingVector = middleOutgoingSource - middleAnchorSource;
    const qreal incomingLength = std::hypot(incomingVector.x(), incomingVector.y());
    const qreal outgoingLength = std::hypot(outgoingVector.x(), outgoingVector.y());
    if (!expect(incomingLength > 0.01 && outgoingLength > 0.01,
                "Dragging a visible XTherion-style draft handle should keep both middle controls.")) {
        return 1;
    }
    const qreal cross = incomingVector.x() * outgoingVector.y() - incomingVector.y() * outgoingVector.x();
    const qreal dot = incomingVector.x() * outgoingVector.x() + incomingVector.y() * outgoingVector.y();
    if (!expect(std::abs(cross / (incomingLength * outgoingLength)) < 0.05
                    && dot / (incomingLength * outgoingLength) < -0.95,
                "Dragging a visible XTherion-style draft handle should mirror-adapt the opposite middle control.")) {
        return 1;
    }

    const int lineDirectivesBeforeEscCancel = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstLineVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondLineVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondLineVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeEscCancel + 1,
                "Esc in Line mode should commit captured vertices and return to Select mode.")) {
        return 1;
    }
    if (!expect(hasVisibleLabelText(mapTab, QStringLiteral("Line")),
                "Esc after committing a line draft should leave the inserted line selected.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == lastDirectiveLineNumber(mapTab->text(), QStringLiteral("line")),
                "Esc after committing a line draft should move the Raw cursor to the inserted line.")) {
        return 1;
    }
    const int selectedEscLineNumber = mapTab->currentLineNumber();
    mapView->viewport()->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_C);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_C);
    pumpEvents();
    if (!expect(sourceLineAt(mapTab->text(), selectedEscLineNumber).contains(QStringLiteral("-close on")),
                "C shortcut should toggle close on the selected line.")) {
        return 1;
    }
    if (!expect(hasVisibleLabelText(mapTab, QStringLiteral("Line"))
                    && mapTab->currentLineNumber() == selectedEscLineNumber,
                "C shortcut should keep the selected line focused.")) {
        return 1;
    }
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_R);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_R);
    pumpEvents();
    if (!expect(sourceLineAt(mapTab->text(), selectedEscLineNumber).contains(QStringLiteral("-reverse on")),
                "R shortcut should toggle reverse on the selected line.")) {
        return 1;
    }
    if (!expect(hasVisibleLabelText(mapTab, QStringLiteral("Line"))
                    && mapTab->currentLineNumber() == selectedEscLineNumber,
                "R shortcut should keep the selected line focused.")) {
        return 1;
    }

    const int lineDirectivesBeforeClosedShortcutDraft = countDirectiveLines(mapTab->text(), QStringLiteral("line"));
    mapTab->triggerAddLine();
    pumpEvents();
    const QPoint closedShortcutFirstVertex(viewportCenter.x() - 54, viewportCenter.y() + 46);
    const QPoint closedShortcutSecondVertex(viewportCenter.x() + 10, viewportCenter.y() + 50);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, closedShortcutFirstVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, closedShortcutFirstVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, closedShortcutSecondVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, closedShortcutSecondVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    mapView->viewport()->setFocus(Qt::OtherFocusReason);
    sendKey(mapView->viewport(), QEvent::KeyPress, Qt::Key_C);
    sendKey(mapView->viewport(), QEvent::KeyRelease, Qt::Key_C);
    pumpEvents();
    const int closedShortcutLineNumber = lastDirectiveLineNumber(mapTab->text(), QStringLiteral("line"));
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeClosedShortcutDraft + 1,
                "C shortcut while drawing a line should insert one closed line directive.")) {
        return 1;
    }
    if (!expect(sourceLineAt(mapTab->text(), closedShortcutLineNumber).contains(QStringLiteral("-close on")),
                "C shortcut while drawing a line should commit the draft as closed.")) {
        return 1;
    }
    if (!expect(hasVisibleLabelText(mapTab, QStringLiteral("Line"))
                    && mapTab->currentLineNumber() == closedShortcutLineNumber,
                "C shortcut while drawing a line should keep the inserted line focused.")) {
        return 1;
    }

    const QString textBeforeAreaClickClose = mapTab->text();
    const int areaDirectivesBeforeClickClose = countDirectiveLines(textBeforeAreaClickClose, QStringLiteral("area"));
    const int lineDirectivesBeforeAreaClickClose = countDirectiveLines(textBeforeAreaClickClose, QStringLiteral("line"));
    mapTab->triggerAddArea();
    pumpEvents();
    const QPoint clickCloseAreaFirst(viewportCenter.x() - 48, viewportCenter.y() + 8);
    const QPoint clickCloseAreaSecond(viewportCenter.x() - 10, viewportCenter.y() + 36);
    const QPoint clickCloseAreaThird(viewportCenter.x() + 18, viewportCenter.y() + 2);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, clickCloseAreaFirst, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, clickCloseAreaFirst, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, clickCloseAreaSecond, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, clickCloseAreaSecond, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, clickCloseAreaThird, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, clickCloseAreaThird, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, clickCloseAreaFirst, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, clickCloseAreaFirst, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("area")) == areaDirectivesBeforeClickClose + 1,
                "Clicking the first Area draft vertex again should commit the area.")) {
        return 1;
    }
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeAreaClickClose + 1,
                "Clicking the first Area draft vertex again should create the closed border line referenced by the area.")) {
        return 1;
    }

    const QString textBeforeAreaEscCommit = mapTab->text();
    const int areaDirectivesBeforeEscCommit = countDirectiveLines(textBeforeAreaEscCommit, QStringLiteral("area"));
    const int lineDirectivesBeforeAreaEscCommit = countDirectiveLines(textBeforeAreaEscCommit, QStringLiteral("line"));
    mapTab->triggerAddArea();
    pumpEvents();
    const QPoint firstAreaVertex(viewportCenter.x() - 22, viewportCenter.y() + 18);
    const QPoint secondAreaVertex(viewportCenter.x() + 14, viewportCenter.y() + 18);
    const QPoint secondAreaDrag(viewportCenter.x() + 14, viewportCenter.y() + 48);
    const QPoint thirdAreaVertex(viewportCenter.x() - 4, viewportCenter.y() - 14);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, firstAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, firstAreaVertex, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, secondAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, secondAreaDrag, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, secondAreaDrag, Qt::LeftButton, Qt::NoButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, thirdAreaVertex, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, thirdAreaVertex, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    textEditor->setFocus(Qt::OtherFocusReason);
    pumpEvents();
    sendKey(textEditor, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(textEditor, QEvent::KeyRelease, Qt::Key_Escape);
    pumpEvents();
    mapTab->triggerCompleteDraft();
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("area")) == areaDirectivesBeforeEscCommit + 1,
                "Esc in Area mode should commit captured vertices and return to Select mode.")) {
        return 1;
    }
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeAreaEscCommit + 1,
                "Area commit should create a closed border line referenced by the area block.")) {
        return 1;
    }
    if (!expect(hasVisibleLabelText(mapTab, QStringLiteral("Area")),
                "Esc after committing an area draft should leave the inserted area selected.")) {
        return 1;
    }
    if (!expect(mapTab->currentLineNumber() == lastDirectiveLineNumber(mapTab->text(), QStringLiteral("area")),
                "Esc after committing an area draft should move the Raw cursor to the inserted area.")) {
        return 1;
    }
    const QString textAfterAreaEscCommit = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeAreaEscCommit,
                "Undo after Area mode completion should remove the inserted area and border line blocks.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterAreaEscCommit,
                "Redo after undoing Area mode completion should restore the area and border line blocks.")) {
        return 1;
    }
    if (!expect(lastDraftAreaReferencesLineId(mapTab->text()),
                "Committed area should reference a border line id inside the area block.")) {
        return 1;
    }
    const auto areaBorderId = lastDraftAreaReferencedLineId(mapTab->text());
    if (!expect(areaBorderId.has_value() && lineBlockByIdHasBezierCoordinateRow(mapTab->text(), areaBorderId.value()),
                "Area click+drag draft should serialize bezier coordinate rows in the referenced border line.")) {
        return 1;
    }
    const QVector<QVector<double>> areaBorderRows = areaBorderId.has_value()
        ? lineBlockByIdNumericRows(mapTab->text(), areaBorderId.value())
        : QVector<QVector<double>>();
    if (!expect(areaBorderRows.size() >= 3
                && areaBorderRows.first().size() >= 2
                && areaBorderRows.at(1).size() >= 6,
                "Bezier area draft should keep the dragged segment as a cubic row in the referenced border line.")) {
        return 1;
    }

    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("area")) >= areaDirectivesBefore + 1,
                "Interactive area workflows should produce at least one additional area directive.")) {
        return 1;
    }

    const QString textBeforeFreehand = mapTab->text();
    const int lineDirectivesBeforeFreehand = countDirectiveLines(textBeforeFreehand, QStringLiteral("line"));
    const QPointF sceneCenterBeforeFreehand = mapView->mapToScene(mapView->viewport()->rect().center());
    mapTab->triggerAddFreehandLine();
    pumpEvents();
    const QPoint strokeStart(viewportCenter.x() - 30, viewportCenter.y() + 20);
    const QPoint strokeMid1(viewportCenter.x() - 10, viewportCenter.y() + 8);
    const QPoint strokeMid2(viewportCenter.x() + 16, viewportCenter.y() - 4);
    const QPoint strokeEnd(viewportCenter.x() + 34, viewportCenter.y() - 14);
    const int ellipseItemsBeforeFreehandPreview = countGraphicsEllipseItems(mapView->scene());
    sendMouse(mapView->viewport(), QEvent::MouseButtonPress, strokeStart, Qt::LeftButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseMove, strokeMid1, Qt::NoButton, Qt::LeftButton);
    pumpEvents();
    if (!expect(countGraphicsEllipseItems(mapView->scene()) == ellipseItemsBeforeFreehandPreview,
                "Freehand live preview should not render per-sample point markers.")) {
        return 1;
    }
    sendMouse(mapView->viewport(), QEvent::MouseMove, strokeMid2, Qt::NoButton, Qt::LeftButton);
    sendMouse(mapView->viewport(), QEvent::MouseButtonRelease, strokeEnd, Qt::LeftButton, Qt::NoButton);
    pumpEvents();
    if (!expect(countDirectiveLines(mapTab->text(), QStringLiteral("line")) == lineDirectivesBeforeFreehand + 1,
                "Freehand drag-and-release should insert one new line directive.")) {
        return 1;
    }
    const QPointF sceneCenterAfterFreehand = mapView->mapToScene(mapView->viewport()->rect().center());
    const QPointF sceneCenterDelta = sceneCenterAfterFreehand - sceneCenterBeforeFreehand;
    if (!expect(std::hypot(sceneCenterDelta.x(), sceneCenterDelta.y()) < 3.0,
                "Freehand completion should not recenter or refit the map viewport.")) {
        return 1;
    }
    if (!expect(lastDraftLineHasBezierCoordinateRow(mapTab->text()),
                "Freehand drag-and-release should serialize the simplified stroke as bezier line coordinate rows.")) {
        std::cerr << "Actual source after freehand:\n" << mapTab->text().toStdString() << '\n';
        return 1;
    }
    if (!expect(lastDraftLineNumericRows(mapTab->text()).size() >= 2,
                "Freehand drag-and-release should keep enough anchors for a valid simplified bezier line.")) {
        return 1;
    }
    const QString textAfterFreehand = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(mapTab->text() == textBeforeFreehand,
                "Undo after Freehand completion should remove the inserted bezier line block.")) {
        return 1;
    }
    mapTab->triggerRedo();
    pumpEvents();
    if (!expect(mapTab->text() == textAfterFreehand,
                "Redo after undoing Freehand completion should restore the bezier line block.")) {
        return 1;
    }

    hostWindow.close();
    pumpEvents();
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (!runFreehandBezierRowLogicSmoke()) {
        return 1;
    }
    if (!runSceneRefreshSelectionSourceSmoke()) {
        return 1;
    }
    if (!runLineSegmentPresentationSmoke()) {
        return 1;
    }
    if (const int rc = runInspectorObjectMoveSmoke(); rc != 0) {
        return rc;
    }
    if (const int rc = runTemplateBezierEditSmoke(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaBorderHitSelectionSmoke(); rc != 0) {
        return rc;
    }
    if (const int rc = runProjectValidationDiagnosticsStayOutOfMapEditorSmoke(); rc != 0) {
        return rc;
    }
    return runDragUndoRedoSmoke();
}
