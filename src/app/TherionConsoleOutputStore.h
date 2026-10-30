#pragma once

#include <QFile>
#include <QString>

namespace TherionStudio
{
class TherionConsoleOutputStore final
{
public:
    explicit TherionConsoleOutputStore(const QString &filePath);
    bool clear();
    bool append(const QString &text);
    bool readAll(QString *text);
    [[nodiscard]] QString filePath() const;
    [[nodiscard]] QString errorString() const;

private:
    QFile file_;
    QString error_;
};
}
