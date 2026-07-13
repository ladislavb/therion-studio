#pragma once

#include "TherionSourceLogicalDocument.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
enum class Th2GeometryObjectKind
{
    Point,
    Line,
    Area,
    Scrap,
    Map,
    Background,
};

struct Th2SourceRange
{
    int startLineNumber = 0;
    int endLineNumber = 0;
    int startOffset = 0;
    int endOffset = 0;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool containsLine(int lineNumber) const;
};

struct Th2CommandObject
{
    Th2GeometryObjectKind kind = Th2GeometryObjectKind::Point;
    QString directive;
    QString type;
    QString subtype;
    QString id;
    QString stableKey;
    QHash<QString, QStringList> optionsByName;
    Th2SourceRange sourceRange;
};

struct Th2PointObject
{
    Th2CommandObject command;
    QPointF position;
    bool hasPosition = false;
};

struct Th2LinePointRow
{
    int lineNumber = 0;
    QString text;
    QVector<QPointF> coordinatePoints;
    bool smoothOff = false;
    QString subtype;
    Th2SourceRange sourceRange;
};

struct Th2LineObject
{
    Th2CommandObject command;
    QVector<Th2LinePointRow> pointRows;
    bool closed = false;
};

struct Th2AreaObject
{
    Th2CommandObject command;
    QStringList borderReferences;
};

struct Th2BackgroundObject
{
    Th2CommandObject command;
    QString path;
    QString metadataFormat;
};

struct Th2GeometryObjectRef
{
    Th2GeometryObjectKind kind = Th2GeometryObjectKind::Point;
    int index = -1;

    [[nodiscard]] bool isValid() const;
};

class Th2GeometryProjection final
{
public:
    [[nodiscard]] static Th2GeometryProjection fromDocuments(
        const TherionSourceDocument &sourceDocument,
        const TherionSourceLogicalDocument &logicalDocument);
    [[nodiscard]] static Th2GeometryProjection fromText(
        const QString &contents,
        const TherionSourceDocumentMetadata &metadata = {});

    [[nodiscard]] const QVector<Th2CommandObject> &blockObjects() const;
    [[nodiscard]] const QVector<Th2PointObject> &points() const;
    [[nodiscard]] const QVector<Th2LineObject> &lines() const;
    [[nodiscard]] const QVector<Th2AreaObject> &areas() const;
    [[nodiscard]] const QVector<Th2BackgroundObject> &backgrounds() const;
    [[nodiscard]] const Th2PointObject *pointAtLineNumber(int lineNumber) const;
    [[nodiscard]] const Th2LineObject *lineAtLineNumber(int lineNumber) const;
    [[nodiscard]] const Th2AreaObject *areaAtLineNumber(int lineNumber) const;
    [[nodiscard]] const Th2BackgroundObject *backgroundAtLineNumber(int lineNumber) const;
    [[nodiscard]] Th2GeometryObjectRef objectRefAtLineNumber(int lineNumber) const;
    [[nodiscard]] const Th2CommandObject *commandForObjectRef(Th2GeometryObjectRef ref) const;

private:
    QVector<Th2CommandObject> blockObjects_;
    QVector<Th2PointObject> points_;
    QVector<Th2LineObject> lines_;
    QVector<Th2AreaObject> areas_;
    QVector<Th2BackgroundObject> backgrounds_;
};
}
