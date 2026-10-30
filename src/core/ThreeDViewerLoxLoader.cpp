#include "ThreeDViewerLoxLoader.h"

#include <QFile>
#include <QSet>
#include <QtEndian>

#include <cstring>
#include <limits>

namespace TherionStudio
{
namespace
{
constexpr quint32 chunkSurvey = 1;
constexpr quint32 chunkStation = 2;
constexpr quint32 chunkShot = 3;
constexpr quint32 chunkScrap = 4;
constexpr quint32 chunkSurface = 5;
constexpr quint32 chunkSurfaceBitmap = 6;

constexpr quint32 stationFlagSurface = 1;
constexpr quint32 stationFlagEntrance = 2;
constexpr quint32 stationFlagFixed = 4;
constexpr quint32 stationFlagContinuation = 8;
constexpr quint32 stationFlagHasWalls = 16;

constexpr quint32 shotFlagSurface = 1;
constexpr quint32 shotFlagDuplicate = 2;
constexpr quint32 shotFlagHidden = 4;
constexpr quint32 shotFlagNotLrud = 8;
constexpr quint32 shotFlagSplay = 16;

constexpr qsizetype headerSize = 16;
constexpr qsizetype dataPtrSize = 8;
constexpr qsizetype surveyRecordSize = 4 + dataPtrSize + 4 + dataPtrSize;
constexpr qsizetype stationRecordSize = 4 + 4 + dataPtrSize + dataPtrSize + 4 + 8 + 8 + 8;
constexpr qsizetype shotRecordSize = 4 + 4 + (8 * 4) + (8 * 4) + 4 + 4 + 4 + 8;
constexpr qsizetype scrapRecordSize = 4 + 4 + 4 + dataPtrSize + 4 + dataPtrSize;
constexpr qsizetype surfaceRecordSize = 4 + 4 + 4 + dataPtrSize + (8 * 6);
constexpr qsizetype surfaceBitmapRecordSize = 4 + 4 + dataPtrSize + (8 * 6);
constexpr qsizetype pointRecordSize = 8 * 3;
constexpr qsizetype triangleRecordSize = 4 * 3;

struct DataPtr
{
    quint32 position = 0;
    quint32 size = 0;
};

struct ChunkHeader
{
    quint32 type = 0;
    quint32 recordBytes = 0;
    quint32 recordCount = 0;
    quint32 dataBytes = 0;
};

class BinaryReader
{
public:
    BinaryReader(const QByteArray &bytes, qsizetype offset, qsizetype length)
        : m_bytes(bytes)
        , m_pos(offset)
        , m_end(offset + length)
    {
    }

    bool readUInt32(quint32 *value)
    {
        if (!canRead(4)) {
            return false;
        }
        *value = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(m_bytes.constData() + m_pos));
        m_pos += 4;
        return true;
    }

    bool readDouble(double *value)
    {
        if (!canRead(8)) {
            return false;
        }
        const quint64 bits = qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(m_bytes.constData() + m_pos));
        std::memcpy(value, &bits, sizeof(bits));
        m_pos += 8;
        return true;
    }

    bool readDataPtr(DataPtr *value)
    {
        return readUInt32(&value->position) && readUInt32(&value->size);
    }

    bool atEnd() const { return m_pos == m_end; }
    qsizetype position() const { return m_pos; }

private:
    bool canRead(qsizetype size) const
    {
        return size >= 0 && m_pos <= m_end && size <= m_end - m_pos;
    }

    const QByteArray &m_bytes;
    qsizetype m_pos = 0;
    qsizetype m_end = 0;
};

bool checkedSpan(qsizetype totalSize, qsizetype offset, qsizetype size)
{
    return offset >= 0 && size >= 0 && offset <= totalSize && size <= totalSize - offset;
}

QString readString(const QByteArray &data, const DataPtr &ptr, QString *error)
{
    if (ptr.size <= 1) {
        return {};
    }
    if (!checkedSpan(data.size(), qsizetype(ptr.position), qsizetype(ptr.size))) {
        *error = QStringLiteral("String pointer is outside the .lox chunk data.");
        return {};
    }
    const char *start = data.constData() + ptr.position;
    const qsizetype length = qsizetype(ptr.size) - 1;
    if (start[length] != '\0') {
        *error = QStringLiteral("String data is not null terminated.");
        return {};
    }
    return QString::fromUtf8(start, length);
}

bool validateRecordBlock(const ChunkHeader &header, qsizetype expectedRecordSize, QString *error)
{
    if (header.recordCount > quint32(std::numeric_limits<qsizetype>::max() / expectedRecordSize)) {
        *error = QStringLiteral(".lox chunk record count is too large.");
        return false;
    }
    const qsizetype expectedBytes = qsizetype(header.recordCount) * expectedRecordSize;
    if (qsizetype(header.recordBytes) != expectedBytes) {
        *error = QStringLiteral(".lox chunk has an unexpected record size.");
        return false;
    }
    return true;
}

ThreeDViewerShotSection shotSectionFromFile(quint32 value)
{
    switch (value) {
    case 1:
        return ThreeDViewerShotSection::Oval;
    case 2:
        return ThreeDViewerShotSection::Square;
    case 3:
        return ThreeDViewerShotSection::Diamond;
    case 4:
        return ThreeDViewerShotSection::Tunnel;
    default:
        return ThreeDViewerShotSection::None;
    }
}

bool readPoint(const QByteArray &data, qsizetype offset, ThreeDViewerVec3 *point)
{
    BinaryReader reader(data, offset, pointRecordSize);
    return reader.readDouble(&point->x) && reader.readDouble(&point->y) && reader.readDouble(&point->z) && reader.atEnd();
}

bool readTriangle(const QByteArray &data, qsizetype offset, std::array<quint32, 3> *triangle)
{
    BinaryReader reader(data, offset, triangleRecordSize);
    return reader.readUInt32(&(*triangle)[0]) && reader.readUInt32(&(*triangle)[1])
        && reader.readUInt32(&(*triangle)[2]) && reader.atEnd();
}
}

ThreeDViewerLoxLoader::Result ThreeDViewerLoxLoader::loadFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Result result;
        result.error = QStringLiteral("Unable to open .lox file: %1").arg(file.errorString());
        return result;
    }

    return loadBytes(file.readAll());
}

ThreeDViewerLoxLoader::Result ThreeDViewerLoxLoader::loadStationMetadataFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, QStringLiteral("Unable to open LOX station metadata.")};
    }
    QByteArray metadata;
    constexpr qint64 maximumMetadataBytes = 32 * 1024 * 1024;
    int chunks = 0;
    while (!file.atEnd()) {
        if (++chunks > 10000) {
            return {{}, QStringLiteral("LOX metadata chunk limit exceeded.")};
        }
        const QByteArray headerBytes = file.read(headerSize);
        if (headerBytes.size() != headerSize) {
            return {{}, QStringLiteral("LOX metadata header is truncated.")};
        }
        BinaryReader reader(headerBytes, 0, headerSize);
        ChunkHeader header;
        reader.readUInt32(&header.type);
        reader.readUInt32(&header.recordBytes);
        reader.readUInt32(&header.recordCount);
        reader.readUInt32(&header.dataBytes);
        const qint64 payloadSize = qint64(header.recordBytes) + qint64(header.dataBytes);
        if (payloadSize > file.size() - file.pos()) {
            return {{}, QStringLiteral("LOX metadata payload is truncated.")};
        }
        if (header.type == chunkSurvey || header.type == chunkStation) {
            if (metadata.size() + headerSize + payloadSize > maximumMetadataBytes) {
                return {{}, QStringLiteral("LOX station metadata limit exceeded.")};
            }
            metadata.append(headerBytes);
            const QByteArray payload = file.read(payloadSize);
            if (payload.size() != payloadSize) {
                return {{}, QStringLiteral("LOX metadata read failed.")};
            }
            metadata.append(payload);
        } else if (!file.seek(file.pos() + payloadSize)) {
            return {{}, QStringLiteral("LOX metadata seek failed.")};
        }
    }
    return loadBytes(metadata);
}

ThreeDViewerLoxLoader::Result ThreeDViewerLoxLoader::loadBytes(const QByteArray &bytes) const
{
    Result result;
    if (bytes.isEmpty()) {
        result.error = QStringLiteral(".lox file is empty.");
        return result;
    }

    qsizetype offset = 0;
    while (offset < bytes.size()) {
        if (!checkedSpan(bytes.size(), offset, headerSize)) {
            result.error = QStringLiteral(".lox chunk header is truncated.");
            return result;
        }

        BinaryReader headerReader(bytes, offset, headerSize);
        ChunkHeader header;
        headerReader.readUInt32(&header.type);
        headerReader.readUInt32(&header.recordBytes);
        headerReader.readUInt32(&header.recordCount);
        headerReader.readUInt32(&header.dataBytes);
        offset += headerSize;

        const qsizetype recordBytes = qsizetype(header.recordBytes);
        const qsizetype dataBytes = qsizetype(header.dataBytes);
        if (!checkedSpan(bytes.size(), offset, recordBytes)
            || !checkedSpan(bytes.size(), offset + recordBytes, dataBytes)) {
            result.error = QStringLiteral(".lox chunk payload is truncated.");
            return result;
        }

        const qsizetype recordsOffset = offset;
        const qsizetype dataOffset = offset + recordBytes;
        const QByteArray chunkData = bytes.mid(dataOffset, dataBytes);
        BinaryReader records(bytes, recordsOffset, recordBytes);

        switch (header.type) {
        case chunkSurvey:
            if (!validateRecordBlock(header, surveyRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                ThreeDViewerSurvey survey;
                DataPtr namePtr;
                DataPtr titlePtr;
                if (!records.readUInt32(&survey.id) || !records.readDataPtr(&namePtr)
                    || !records.readUInt32(&survey.parentId) || !records.readDataPtr(&titlePtr)) {
                    result.error = QStringLiteral(".lox survey record is truncated.");
                    return result;
                }
                survey.name = readString(chunkData, namePtr, &result.error);
                if (!result.error.isEmpty()) {
                    return result;
                }
                survey.title = readString(chunkData, titlePtr, &result.error);
                if (!result.error.isEmpty()) {
                    return result;
                }
                result.scene.surveys.push_back(survey);
            }
            break;

        case chunkStation:
            if (!validateRecordBlock(header, stationRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                ThreeDViewerStation station;
                DataPtr namePtr;
                DataPtr commentPtr;
                quint32 flags = 0;
                if (!records.readUInt32(&station.id) || !records.readUInt32(&station.surveyId)
                    || !records.readDataPtr(&namePtr) || !records.readDataPtr(&commentPtr)
                    || !records.readUInt32(&flags) || !records.readDouble(&station.position.x)
                    || !records.readDouble(&station.position.y) || !records.readDouble(&station.position.z)) {
                    result.error = QStringLiteral(".lox station record is truncated.");
                    return result;
                }
                station.name = readString(chunkData, namePtr, &result.error);
                if (!result.error.isEmpty()) {
                    return result;
                }
                station.comment = readString(chunkData, commentPtr, &result.error);
                if (!result.error.isEmpty()) {
                    return result;
                }
                station.surface = (flags & stationFlagSurface) != 0;
                station.entrance = (flags & stationFlagEntrance) != 0;
                station.fixed = (flags & stationFlagFixed) != 0;
                station.continuation = (flags & stationFlagContinuation) != 0;
                station.hasWalls = (flags & stationFlagHasWalls) != 0;
                result.scene.stations.push_back(station);
            }
            break;

        case chunkShot:
            if (!validateRecordBlock(header, shotRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                ThreeDViewerShot shot;
                quint32 flags = 0;
                quint32 section = 0;
                if (!records.readUInt32(&shot.fromStationId) || !records.readUInt32(&shot.toStationId)) {
                    result.error = QStringLiteral(".lox shot record is truncated.");
                    return result;
                }
                for (double &value : shot.fromLrud) {
                    if (!records.readDouble(&value)) {
                        result.error = QStringLiteral(".lox shot LRUD record is truncated.");
                        return result;
                    }
                }
                for (double &value : shot.toLrud) {
                    if (!records.readDouble(&value)) {
                        result.error = QStringLiteral(".lox shot LRUD record is truncated.");
                        return result;
                    }
                }
                if (!records.readUInt32(&flags) || !records.readUInt32(&section)
                    || !records.readUInt32(&shot.surveyId) || !records.readDouble(&shot.threshold)) {
                    result.error = QStringLiteral(".lox shot record is truncated.");
                    return result;
                }
                shot.surface = (flags & shotFlagSurface) != 0;
                shot.duplicate = (flags & shotFlagDuplicate) != 0;
                shot.hidden = (flags & shotFlagHidden) != 0;
                shot.notLrud = (flags & shotFlagNotLrud) != 0;
                shot.splay = (flags & shotFlagSplay) != 0;
                shot.section = shotSectionFromFile(section);
                result.scene.shots.push_back(shot);
            }
            break;

        case chunkScrap:
            if (!validateRecordBlock(header, scrapRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                ThreeDViewerMeshGroup mesh;
                quint32 pointCount = 0;
                quint32 triangleCount = 0;
                DataPtr pointsPtr;
                DataPtr trianglesPtr;
                if (!records.readUInt32(&mesh.id) || !records.readUInt32(&mesh.surveyId)
                    || !records.readUInt32(&pointCount) || !records.readDataPtr(&pointsPtr)
                    || !records.readUInt32(&triangleCount) || !records.readDataPtr(&trianglesPtr)) {
                    result.error = QStringLiteral(".lox scrap record is truncated.");
                    return result;
                }
                if (!checkedSpan(chunkData.size(), pointsPtr.position, qsizetype(pointCount) * pointRecordSize)
                    || pointsPtr.size != pointCount * pointRecordSize) {
                    result.error = QStringLiteral(".lox scrap point data is invalid.");
                    return result;
                }
                if (!checkedSpan(chunkData.size(), trianglesPtr.position, qsizetype(triangleCount) * triangleRecordSize)
                    || trianglesPtr.size != triangleCount * triangleRecordSize) {
                    result.error = QStringLiteral(".lox scrap triangle data is invalid.");
                    return result;
                }
                mesh.vertices.reserve(pointCount);
                for (quint32 pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
                    ThreeDViewerVec3 point;
                    if (!readPoint(chunkData, pointsPtr.position + qsizetype(pointIndex) * pointRecordSize, &point)) {
                        result.error = QStringLiteral(".lox scrap point data is truncated.");
                        return result;
                    }
                    mesh.vertices.push_back(point);
                }
                mesh.triangles.reserve(triangleCount);
                for (quint32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
                    std::array<quint32, 3> triangle = {};
                    if (!readTriangle(chunkData, trianglesPtr.position + qsizetype(triangleIndex) * triangleRecordSize, &triangle)) {
                        result.error = QStringLiteral(".lox scrap triangle data is truncated.");
                        return result;
                    }
                    if (triangle[0] >= pointCount || triangle[1] >= pointCount || triangle[2] >= pointCount) {
                        result.error = QStringLiteral(".lox scrap triangle references an invalid point.");
                        return result;
                    }
                    mesh.triangles.push_back(triangle);
                }
                mesh.computeNormals();
                result.scene.meshGroups.push_back(mesh);
            }
            break;

        case chunkSurface:
            if (!validateRecordBlock(header, surfaceRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                ThreeDViewerSurface surface;
                DataPtr dataPtr;
                if (!records.readUInt32(&surface.id) || !records.readUInt32(&surface.width)
                    || !records.readUInt32(&surface.height) || !records.readDataPtr(&dataPtr)) {
                    result.error = QStringLiteral(".lox surface record is truncated.");
                    return result;
                }
                for (double &value : surface.calibration) {
                    if (!records.readDouble(&value)) {
                        result.error = QStringLiteral(".lox surface calibration record is truncated.");
                        return result;
                    }
                }
                if (surface.width > 0
                    && surface.height > quint32(std::numeric_limits<qsizetype>::max() / surface.width / 8)) {
                    result.error = QStringLiteral(".lox surface grid is too large.");
                    return result;
                }
                const qsizetype elevationCount = qsizetype(surface.width) * qsizetype(surface.height);
                if (!checkedSpan(chunkData.size(), dataPtr.position, elevationCount * 8)
                    || dataPtr.size != elevationCount * 8) {
                    result.error = QStringLiteral(".lox surface data is invalid.");
                    return result;
                }
                surface.elevations.reserve(elevationCount);
                BinaryReader elevationReader(chunkData, dataPtr.position, elevationCount * 8);
                for (qsizetype elevationIndex = 0; elevationIndex < elevationCount; ++elevationIndex) {
                    double elevation = 0.0;
                    if (!elevationReader.readDouble(&elevation)) {
                        result.error = QStringLiteral(".lox surface data is truncated.");
                        return result;
                    }
                    surface.elevations.push_back(elevation);
                }
                result.scene.surfaces.push_back(surface);
            }
            break;

        case chunkSurfaceBitmap:
            if (!validateRecordBlock(header, surfaceBitmapRecordSize, &result.error)) {
                return result;
            }
            for (quint32 i = 0; i < header.recordCount; ++i) {
                DataPtr dataPtr;
                quint32 surfaceId = 0;
                quint32 type = 0;
                if (!records.readUInt32(&surfaceId) || !records.readUInt32(&type)
                    || !records.readDataPtr(&dataPtr)) {
                    result.error = QStringLiteral(".lox surface bitmap record is truncated.");
                    return result;
                }
                for (qsizetype calibrationIndex = 0; calibrationIndex < 6; ++calibrationIndex) {
                    double calibration = 0.0;
                    if (!records.readDouble(&calibration)) {
                        result.error = QStringLiteral(".lox surface bitmap calibration record is truncated.");
                        return result;
                    }
                    Q_UNUSED(calibration);
                }
                if (dataPtr.size > 0
                    && !checkedSpan(chunkData.size(), dataPtr.position, qsizetype(dataPtr.size))) {
                    result.error = QStringLiteral(".lox surface bitmap data is invalid.");
                    return result;
                }
                Q_UNUSED(surfaceId);
                Q_UNUSED(type);
            }
            break;

        default:
            result.error = QStringLiteral(".lox file contains an unsupported chunk type.");
            return result;
        }

        if (!records.atEnd()) {
            result.error = QStringLiteral(".lox chunk has unread record bytes.");
            return result;
        }

        offset = dataOffset + dataBytes;
    }

    if (result.scene.isEmpty()) {
        result.error = QStringLiteral(".lox file does not contain supported scene data.");
        return result;
    }

    QSet<quint32> stationIds;
    for (const ThreeDViewerStation &station : result.scene.stations) {
        stationIds.insert(station.id);
    }
    for (const ThreeDViewerShot &shot : result.scene.shots) {
        if (!stationIds.contains(shot.fromStationId) || !stationIds.contains(shot.toStationId)) {
            result.error = QStringLiteral(".lox shot references an unknown station.");
            return result;
        }
    }

    return result;
}

} // namespace TherionStudio
