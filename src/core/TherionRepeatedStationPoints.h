#pragma once

#include "TherionSourceValidator.h"

namespace TherionStudio
{
class TherionSourceDocument;
class TherionSourceLogicalDocument;

// Station points that repeat a station already drawn in the same file. Therion accepts them, but a station drawn
// twice in one scrap at different places pulls the morphing towards both. A station shared by two scraps of the same
// projection is how scraps join, so it is only information; cross sections (`-projection none`) are not compared.
void appendRepeatedStationPointDiagnostics(QVector<TherionSourceDiagnostic> *diagnostics,
                                           const TherionSourceDocument &sourceDocument,
                                           const TherionSourceLogicalDocument &logicalDocument);
}
