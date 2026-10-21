#pragma once

#include <QPointF>

#include <cmath>

namespace TherionStudio
{
// Handles of the points next to a selected line point change length only, as in xTherion.

// Projects candidate onto the ray from anchor through control, keeping at least minimumLength.
inline QPointF lengthOnlyLineControlPoint(const QPointF &anchor,
                                          const QPointF &control,
                                          const QPointF &candidate,
                                          qreal minimumLength)
{
    const QPointF direction = control - anchor;
    const qreal length = std::hypot(direction.x(), direction.y());
    if (length <= 1e-9) {
        return control;
    }

    const QPointF unit = direction / length;
    const QPointF offset = candidate - anchor;
    const qreal projectedLength = qMax(minimumLength, (offset.x() * unit.x()) + (offset.y() * unit.y()));
    return anchor + (unit * projectedLength);
}

// True when newControl lies on the same ray from anchor as oldControl.
inline bool lineControlDirectionUnchanged(const QPointF &anchor, const QPointF &oldControl, const QPointF &newControl)
{
    const QPointF oldVector = oldControl - anchor;
    const QPointF newVector = newControl - anchor;
    const qreal oldLength = std::hypot(oldVector.x(), oldVector.y());
    const qreal newLength = std::hypot(newVector.x(), newVector.y());
    if (oldLength <= 1e-9 || newLength <= 1e-9) {
        return false;
    }

    const qreal sine = ((oldVector.x() * newVector.y()) - (oldVector.y() * newVector.x())) / (oldLength * newLength);
    const qreal cosine = ((oldVector.x() * newVector.x()) + (oldVector.y() * newVector.y())) / (oldLength * newLength);
    return cosine > 0.0 && std::abs(sine) <= 1e-6;
}
} // namespace TherionStudio
