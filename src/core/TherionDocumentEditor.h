#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct TherionDraftObjectOptions
{
    QString type;
    QString subtype;
    QString identifier;
    QString name;
    QString text;
    QString value;
    QString targetScrapIdentifier;
    bool nameEnabled = false;
    bool textEnabled = false;
    bool valueEnabled = false;
};

struct TherionReferencedAreaBoundaryLine
{
    int lineNumber = 0;
    QString identifier;
};

struct TherionSourceTextEdit
{
    int startOffset = 0;
    int length = 0;
    QString replacementText;
};

class TherionDocumentEditor final
{
public:
    [[nodiscard]] static bool applySourceTextEdits(QString *contents,
                                                   QVector<TherionSourceTextEdit> edits,
                                                   QString *errorMessage = nullptr);

    // Edits for several vertices of one line/area, all expressed against `contents`.
    // Fails (so callers can fall back to sequential rewrites) when the combined edits overlap.
    [[nodiscard]] static bool lineAreaVerticesRewriteEdits(const QString &contents,
                                                           int lineNumber,
                                                           const QString &kind,
                                                           const QVector<std::pair<int, QPointF>> &vertexMoves,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           QString *errorMessage = nullptr);

    [[nodiscard]] static bool structureEntryNameRewriteEdits(const QString &contents,
                                                             int lineNumber,
                                                             const QString &category,
                                                             const QString &newName,
                                                             QVector<TherionSourceTextEdit> *edits,
                                                             QString *errorMessage = nullptr);
    [[nodiscard]] static bool appendScrapBlockEdits(const QString &contents,
                                                    const QString &preferredName,
                                                    QVector<TherionSourceTextEdit> *edits,
                                                    int *insertedLineNumber = nullptr,
                                                    QString *errorMessage = nullptr,
                                                    const QString &options = QString());
    [[nodiscard]] static bool appendDraftGeometryEdits(const QString &contents,
                                                       const QString &kind,
                                                       const QVector<QPointF> &vertices,
                                                       QVector<TherionSourceTextEdit> *edits,
                                                       int *insertedLineNumber = nullptr,
                                                       QString *errorMessage = nullptr,
                                                       const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendDraftLineGeometryEdits(const QString &contents,
                                                           const QStringList &coordinateRows,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           int *insertedLineNumber = nullptr,
                                                           QString *errorMessage = nullptr,
                                                           const QString &lineOptions = QString(),
                                                           const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendDraftAreaGeometryEdits(const QString &contents,
                                                           const QStringList &coordinateRows,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           int *insertedLineNumber = nullptr,
                                                           QString *errorMessage = nullptr,
                                                           const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendReferencedAreaEdits(const QString &contents,
                                                        int scrapLineNumber,
                                                        const QVector<TherionReferencedAreaBoundaryLine> &boundaryLines,
                                                        QVector<TherionSourceTextEdit> *edits,
                                                        int *insertedLineNumber = nullptr,
                                                        QString *errorMessage = nullptr,
                                                        const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool pointCoordinateRewriteEdits(const QString &contents,
                                                          int lineNumber,
                                                          const QPointF &point,
                                                          QVector<TherionSourceTextEdit> *edits,
                                                          QString *errorMessage = nullptr);
    [[nodiscard]] static bool lineAreaVertexRewriteEdits(const QString &contents,
                                                         int lineNumber,
                                                         const QString &kind,
                                                         int vertexIndex,
                                                         const QPointF &point,
                                                         QVector<TherionSourceTextEdit> *edits,
                                                         QString *errorMessage = nullptr);
    [[nodiscard]] static bool lineOptionToggleRewriteEdits(const QString &contents,
                                                           int lineNumber,
                                                           const QString &optionName,
                                                           bool enabled,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           QString *errorMessage = nullptr);
    [[nodiscard]] static bool pointOrientationRewriteEdits(const QString &contents,
                                                           int lineNumber,
                                                           bool enabled,
                                                           qreal orientationDegrees,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           QString *errorMessage = nullptr);
    [[nodiscard]] static bool mapObjectClipDisabledRewriteEdits(const QString &contents,
                                                                int lineNumber,
                                                                bool disabled,
                                                                QVector<TherionSourceTextEdit> *edits,
                                                                QString *errorMessage = nullptr);
    [[nodiscard]] static bool pointAlignRewriteEdits(const QString &contents,
                                                     int lineNumber,
                                                     const QString &align,
                                                     QVector<TherionSourceTextEdit> *edits,
                                                     QString *errorMessage = nullptr);
    [[nodiscard]] static bool linePointOrientationRewriteEdits(const QString &contents,
                                                               int lineNumber,
                                                               int sourceVertexIndex,
                                                               bool enabled,
                                                               qreal orientationDegrees,
                                                               QVector<TherionSourceTextEdit> *edits,
                                                               QString *errorMessage = nullptr);
    [[nodiscard]] static bool linePointLeftSizeRewriteEdits(const QString &contents,
                                                            int lineNumber,
                                                            int sourceVertexIndex,
                                                            bool enabled,
                                                            qreal sizeValue,
                                                            QVector<TherionSourceTextEdit> *edits,
                                                            QString *errorMessage = nullptr);
    [[nodiscard]] static bool scrapScaleRewriteEdits(const QString &contents,
                                                     int lineNumber,
                                                     const QString &scaleExpression,
                                                     QVector<TherionSourceTextEdit> *edits,
                                                     QString *errorMessage = nullptr);
    [[nodiscard]] static bool scrapProjectionRewriteEdits(const QString &contents,
                                                          int lineNumber,
                                                          const QString &projectionExpression,
                                                          QVector<TherionSourceTextEdit> *edits,
                                                          QString *errorMessage = nullptr);
    [[nodiscard]] static bool mapObjectQuickFieldsRewriteEdits(const QString &contents,
                                                               int lineNumber,
                                                               const QString &type,
                                                               const QString &subtype,
                                                               const QString &identifier,
                                                               const QString &name,
                                                               bool nameEnabled,
                                                               QVector<TherionSourceTextEdit> *edits,
                                                               QString *errorMessage = nullptr);
    [[nodiscard]] static bool mapObjectTextOptionRewriteEdits(const QString &contents,
                                                              int lineNumber,
                                                              const QString &text,
                                                              QVector<TherionSourceTextEdit> *edits,
                                                              QString *errorMessage = nullptr);
    [[nodiscard]] static bool mapObjectValueOptionRewriteEdits(const QString &contents,
                                                               int lineNumber,
                                                               const QString &value,
                                                               QVector<TherionSourceTextEdit> *edits,
                                                               QString *errorMessage = nullptr);
    [[nodiscard]] static bool lineCoordinateRowsRewriteEdits(const QString &contents,
                                                             int lineNumber,
                                                             const QStringList &coordinateRows,
                                                             QVector<TherionSourceTextEdit> *edits,
                                                             QString *errorMessage = nullptr);
};
}
