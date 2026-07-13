#pragma once

#include <QString>

namespace TherionStudio
{
class TherionSourceFormatter final
{
public:
    // Formats only structural leading indentation. Each Therion block depth
    // is represented by one literal tab character; nonempty code-block body
    // rows are left byte-for-byte unchanged.
    [[nodiscard]] static QString formatIndentation(const QString &contents);
};
}
