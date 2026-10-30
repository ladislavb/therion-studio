#include "TherionCentreline.h"

#include "DocumentFile.h"
#include "TherionSourceLogicalDocument.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

enum class DataStyle
{
    None,
    Normal,
    Cartesian,
    NoSurvey,
    Ignored,
};

struct CalibrationValue
{
    double zero = 0.0;
    double scale = 1.0;
};

struct CentrelineSettings
{
    QHash<QString, double> unitFactors;
    QSet<QString> percentQuantities;
    QHash<QString, CalibrationValue> calibrations;
    bool hasDeclination = false;
    double declinationDegrees = 0.0;
    bool splay = false;
    bool duplicate = false;
    bool surface = false;
    QString coordinateSystem;
    DataStyle style = DataStyle::None;
    QStringList fields;
};

bool isAngleQuantity(const QString &quantity)
{
    return quantity == QLatin1String("compass") || quantity == QLatin1String("backcompass")
        || quantity == QLatin1String("clino") || quantity == QLatin1String("backclino")
        || quantity == QLatin1String("declination");
}

QString canonicalQuantity(const QString &token)
{
    const QString name = token.toLower();
    if (name == QLatin1String("tape") || name == QLatin1String("length") || name == QLatin1String("count")
        || name == QLatin1String("counter")) {
        return QStringLiteral("length");
    }
    if (name == QLatin1String("compass") || name == QLatin1String("bearing")) {
        return QStringLiteral("compass");
    }
    if (name == QLatin1String("backcompass") || name == QLatin1String("backbearing")) {
        return QStringLiteral("backcompass");
    }
    if (name == QLatin1String("clino") || name == QLatin1String("gradient")) {
        return QStringLiteral("clino");
    }
    if (name == QLatin1String("backclino") || name == QLatin1String("backgradient")) {
        return QStringLiteral("backclino");
    }
    if (name == QLatin1String("ceiling")) {
        return QStringLiteral("up");
    }
    if (name == QLatin1String("floor")) {
        return QStringLiteral("down");
    }
    if (name == QLatin1String("x")) {
        return QStringLiteral("easting");
    }
    if (name == QLatin1String("y")) {
        return QStringLiteral("northing");
    }
    if (name == QLatin1String("z")) {
        return QStringLiteral("altitude");
    }
    static const QSet<QString> plain = {
        QStringLiteral("from"), QStringLiteral("to"), QStringLiteral("left"), QStringLiteral("right"),
        QStringLiteral("up"), QStringLiteral("down"), QStringLiteral("easting"), QStringLiteral("northing"),
        QStringLiteral("altitude"), QStringLiteral("ignore"), QStringLiteral("ignoreall"),
        QStringLiteral("declination"),
    };
    return plain.contains(name) ? name : QString();
}

QStringList expandQuantityGroup(const QString &token)
{
    const QString name = token.toLower();
    if (name == QLatin1String("dimensions")) {
        return {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("up"), QStringLiteral("down")};
    }
    if (name == QLatin1String("position")) {
        return {QStringLiteral("easting"), QStringLiteral("northing"), QStringLiteral("altitude")};
    }
    if (name == QLatin1String("angle") || name == QLatin1String("angles")) {
        return {QStringLiteral("compass"), QStringLiteral("backcompass"), QStringLiteral("clino"),
                QStringLiteral("backclino")};
    }
    const QString quantity = canonicalQuantity(name);
    return quantity.isEmpty() ? QStringList() : QStringList{quantity};
}

bool lengthUnitFactor(const QString &unit, double *factor)
{
    static const QHash<QString, double> factors = {
        {QStringLiteral("m"), 1.0},           {QStringLiteral("meter"), 1.0},
        {QStringLiteral("meters"), 1.0},      {QStringLiteral("metre"), 1.0},
        {QStringLiteral("metres"), 1.0},      {QStringLiteral("cm"), 0.01},
        {QStringLiteral("centimeter"), 0.01}, {QStringLiteral("centimeters"), 0.01},
        {QStringLiteral("centimetre"), 0.01}, {QStringLiteral("centimetres"), 0.01},
        {QStringLiteral("mm"), 0.001},        {QStringLiteral("millimeter"), 0.001},
        {QStringLiteral("millimeters"), 0.001}, {QStringLiteral("millimetre"), 0.001},
        {QStringLiteral("millimetres"), 0.001}, {QStringLiteral("ft"), 0.3048},
        {QStringLiteral("foot"), 0.3048},     {QStringLiteral("feet"), 0.3048},
        {QStringLiteral("in"), 0.0254},       {QStringLiteral("inch"), 0.0254},
        {QStringLiteral("inches"), 0.0254},   {QStringLiteral("yd"), 0.9144},
        {QStringLiteral("yard"), 0.9144},     {QStringLiteral("yards"), 0.9144},
    };
    const auto it = factors.constFind(unit.toLower());
    if (it == factors.constEnd()) {
        return false;
    }
    *factor = it.value();
    return true;
}

bool angleUnitFactor(const QString &unit, double *factor, bool *percent)
{
    static const QHash<QString, double> factors = {
        {QStringLiteral("deg"), 1.0},     {QStringLiteral("degree"), 1.0},
        {QStringLiteral("degrees"), 1.0}, {QStringLiteral("grad"), 0.9},
        {QStringLiteral("grads"), 0.9},   {QStringLiteral("gon"), 0.9},
        {QStringLiteral("gons"), 0.9},    {QStringLiteral("mil"), 360.0 / 6400.0},
        {QStringLiteral("mils"), 360.0 / 6400.0}, {QStringLiteral("min"), 1.0 / 60.0},
        {QStringLiteral("minute"), 1.0 / 60.0},   {QStringLiteral("minutes"), 1.0 / 60.0},
    };
    const QString name = unit.toLower();
    *percent = name == QLatin1String("percent") || name == QLatin1String("percentage");
    if (*percent) {
        *factor = 1.0;
        return true;
    }
    const auto it = factors.constFind(name);
    if (it == factors.constEnd()) {
        return false;
    }
    *factor = it.value();
    return true;
}

bool parseNumber(const QString &token, double *value)
{
    bool ok = false;
    const double parsed = token.toDouble(&ok);
    if (ok && std::isfinite(parsed)) {
        *value = parsed;
        return true;
    }
    return false;
}

bool isAnonymousStation(const QString &token)
{
    return token == QLatin1String("-") || token == QLatin1String(".");
}

class CentrelineParser
{
public:
    explicit CentrelineParser(const TherionCentrelineFileReader &readFile)
        : readFile_(readFile)
    {
    }

    void parseFile(const QString &filePath, int depth);

    TherionCentrelineObservations observations;
    QSet<QString> stationsWithGeometry;
    QSet<QString> nosurveyStations;

private:
    void handleCommand(const QStringList &tokens, int lineNumber);
    void handleCentrelineCommand(const QString &directive, const QStringList &tokens, int lineNumber);
    void handleDataRow(const QStringList &tokens, int lineNumber);
    void handleUnits(const QStringList &tokens, int lineNumber);
    void handleCalibrate(const QStringList &tokens, int lineNumber);
    void handleDataHeader(const QStringList &tokens, int lineNumber);
    void handleFix(const QStringList &tokens, int lineNumber);
    void handleInput(const QString &target, int lineNumber);
    bool reading(const QString &quantity, const QString &token, double *value) const;
    QString stationKey(const QString &token) const;
    QString currentSurveyPath() const;
    void diagnose(int lineNumber, const QString &message);

    const TherionCentrelineFileReader &readFile_;
    QString currentFile_;
    QStringList surveyStack_;
    bool inCentreline_ = false;
    CentrelineSettings settings_;
    QVector<CentrelineSettings> groupStack_;
    QSet<QString> visitedFiles_;
    QSet<QString> knownSurveyPaths_;
    int depth_ = 0;
};

void CentrelineParser::diagnose(int lineNumber, const QString &message)
{
    observations.diagnostics.append({currentFile_, lineNumber, message});
}

QString CentrelineParser::currentSurveyPath() const
{
    QStringList innermostFirst;
    for (auto it = surveyStack_.crbegin(); it != surveyStack_.crend(); ++it) {
        innermostFirst.append(*it);
    }
    return innermostFirst.join(QLatin1Char('.'));
}

QString CentrelineParser::stationKey(const QString &token) const
{
    const int at = token.indexOf(QLatin1Char('@'));
    const QString name = at >= 0 ? token.left(at) : token;
    QStringList path;
    if (at >= 0 && at + 1 < token.size()) {
        path = token.mid(at + 1).split(QLatin1Char('.'), Qt::SkipEmptyParts);
    }
    const QString current = currentSurveyPath();
    if (!current.isEmpty()) {
        path.append(current);
    }
    return path.isEmpty() ? name : name + QLatin1Char('@') + path.join(QLatin1Char('.'));
}

bool CentrelineParser::reading(const QString &quantity, const QString &token, double *value) const
{
    double raw = 0.0;
    if (!parseNumber(token, &raw)) {
        return false;
    }
    const CalibrationValue calibration = settings_.calibrations.value(quantity);
    double corrected = (raw - calibration.zero) * calibration.scale;
    if (settings_.percentQuantities.contains(quantity)) {
        corrected = std::atan(corrected / 100.0) * 180.0 / kPi;
    } else {
        corrected *= settings_.unitFactors.value(quantity, 1.0);
    }
    *value = corrected;
    return true;
}

void CentrelineParser::parseFile(const QString &filePath, int depth)
{
    const QString canonical = QFileInfo(filePath).absoluteFilePath();
    if (depth > 32 || visitedFiles_.contains(canonical)) {
        return;
    }
    visitedFiles_.insert(canonical);
    const int previousDepth = depth_;
    depth_ = depth;

    QString contents;
    const QString previousFile = currentFile_;
    currentFile_ = canonical;
    if (!readFile_ || !readFile_(canonical, &contents)) {
        diagnose(0, QStringLiteral("Cannot read file."));
        currentFile_ = previousFile;
        depth_ = previousDepth;
        return;
    }
    observations.files.append(canonical);

    const bool isConfig = canonical.endsWith(QLatin1String(".thconfig"), Qt::CaseInsensitive);
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(contents);
    for (const TherionSourceLogicalCommand &command : document.commands()) {
        const QStringList &tokens = command.parsed.tokens;
        if (tokens.isEmpty()) {
            continue;
        }
        if (isConfig) {
            const QString directive = tokens.first().toLower();
            if ((directive == QLatin1String("source") || directive == QLatin1String("input")) && tokens.size() > 1) {
                handleInput(tokens.at(1), command.startLineNumber);
            }
            continue;
        }
        handleCommand(tokens, command.startLineNumber);
    }
    currentFile_ = previousFile;
    depth_ = previousDepth;
}

void CentrelineParser::handleInput(const QString &target, int lineNumber)
{
    const QString suffix = QFileInfo(target).suffix().toLower();
    if (suffix == QLatin1String("th2")) {
        const QString resolved = QFileInfo(currentFile_).dir().filePath(target);
        observations.th2References.append(TherionCentrelineTh2Reference{resolved, currentSurveyPath()});
        return;
    }
    if (suffix == QLatin1String("xvi") || suffix == QLatin1String("lox")
        || suffix == QLatin1String("3d") || suffix == QLatin1String("plt")) {
        return;
    }
    QString resolved = QFileInfo(currentFile_).dir().filePath(target);
    if (suffix.isEmpty() && !QFileInfo::exists(resolved) && QFileInfo::exists(resolved + QStringLiteral(".th"))) {
        resolved += QStringLiteral(".th");
    }
    Q_UNUSED(lineNumber);
    parseFile(resolved, depth_ + 1);
}

void CentrelineParser::handleCommand(const QStringList &tokens, int lineNumber)
{
    const QString directive = tokens.first().toLower();
    if (inCentreline_) {
        if (directive == QLatin1String("endcentreline") || directive == QLatin1String("endcenterline")) {
            inCentreline_ = false;
            return;
        }
        handleCentrelineCommand(directive, tokens, lineNumber);
        return;
    }

    if (directive == QLatin1String("survey")) {
        const QString name = tokens.size() > 1 ? tokens.at(1) : QString();
        surveyStack_.append(name);
        const QString path = currentSurveyPath();
        if (!knownSurveyPaths_.contains(path)) {
            knownSurveyPaths_.insert(path);
            observations.surveyPaths.append(path);
        }
    } else if (directive == QLatin1String("endsurvey")) {
        if (!surveyStack_.isEmpty()) {
            surveyStack_.removeLast();
        }
    } else if (directive == QLatin1String("centreline") || directive == QLatin1String("centerline")) {
        inCentreline_ = true;
        settings_ = CentrelineSettings();
        groupStack_.clear();
    } else if (directive == QLatin1String("input") && tokens.size() > 1) {
        handleInput(tokens.at(1), lineNumber);
    } else if (directive == QLatin1String("join") && tokens.size() >= 3) {
        int joinCount = 1;
        for (int index = 1; index + 1 < tokens.size(); ++index) {
            if (tokens.at(index) == QLatin1String("-count")) {
                bool countOk = false;
                const int count = tokens.at(index + 1).toInt(&countOk);
                if (countOk && count > 0) {
                    joinCount = count;
                }
                break;
            }
        }
        for (int index = 1; index + 1 < tokens.size(); index += 2) {
            if (tokens.at(index).startsWith(QLatin1Char('-'))
                || tokens.at(index + 1).startsWith(QLatin1Char('-'))) {
                break;
            }
            observations.joins.append({tokens.at(index), tokens.at(index + 1), currentSurveyPath(),
                                       currentFile_, lineNumber, joinCount});
        }
    } else if (directive == QLatin1String("equate") || directive == QLatin1String("fix")) {
        handleCentrelineCommand(directive, tokens, lineNumber);
    }
}

void CentrelineParser::handleCentrelineCommand(const QString &directive, const QStringList &tokens, int lineNumber)
{
    static const QSet<QString> ignoredCommands = {
        QStringLiteral("date"),       QStringLiteral("explo-date"), QStringLiteral("team"),
        QStringLiteral("explo-team"), QStringLiteral("instrument"), QStringLiteral("sd"),
        QStringLiteral("grade"),      QStringLiteral("grid-angle"), QStringLiteral("infer"),
        QStringLiteral("mark"),       QStringLiteral("station"),    QStringLiteral("break"),
        QStringLiteral("walls"),      QStringLiteral("vthreshold"), QStringLiteral("extend"),
        QStringLiteral("copyright"),  QStringLiteral("title"),      QStringLiteral("author"),
        QStringLiteral("encoding"),
    };

    if (ignoredCommands.contains(directive)) {
        return;
    }
    if (directive == QLatin1String("data")) {
        handleDataHeader(tokens, lineNumber);
    } else if (directive == QLatin1String("units")) {
        handleUnits(tokens, lineNumber);
    } else if (directive == QLatin1String("calibrate")) {
        handleCalibrate(tokens, lineNumber);
    } else if (directive == QLatin1String("declination")) {
        double value = 0.0;
        if (tokens.size() > 1 && parseNumber(tokens.at(1), &value)) {
            double factor = 1.0;
            bool percent = false;
            if (tokens.size() > 2 && !angleUnitFactor(tokens.at(2), &factor, &percent)) {
                diagnose(lineNumber, QStringLiteral("Unknown declination units \"%1\".").arg(tokens.at(2)));
            }
            settings_.hasDeclination = true;
            settings_.declinationDegrees = value * factor;
        } else {
            settings_.hasDeclination = false;
            settings_.declinationDegrees = 0.0;
        }
    } else if (directive == QLatin1String("flags")) {
        bool negate = false;
        for (int index = 1; index < tokens.size(); ++index) {
            const QString flag = tokens.at(index).toLower();
            if (flag == QLatin1String("not")) {
                negate = true;
                continue;
            }
            if (flag == QLatin1String("splay")) {
                settings_.splay = !negate;
            } else if (flag == QLatin1String("duplicate")) {
                settings_.duplicate = !negate;
            } else if (flag == QLatin1String("surface")) {
                settings_.surface = !negate;
            }
            negate = false;
        }
    } else if (directive == QLatin1String("cs")) {
        settings_.coordinateSystem = tokens.size() > 1 ? tokens.at(1) : QString();
    } else if (directive == QLatin1String("fix")) {
        handleFix(tokens, lineNumber);
    } else if (directive == QLatin1String("equate")) {
        QStringList keys;
        for (int index = 1; index < tokens.size(); ++index) {
            keys.append(stationKey(tokens.at(index)));
        }
        if (keys.size() >= 2) {
            observations.equates.append(keys);
        }
    } else if (directive == QLatin1String("group")) {
        groupStack_.append(settings_);
    } else if (directive == QLatin1String("endgroup")) {
        if (!groupStack_.isEmpty()) {
            settings_ = groupStack_.takeLast();
        }
    } else if (directive == QLatin1String("station-names")) {
        diagnose(lineNumber, QStringLiteral("station-names is not supported; station names may not resolve."));
    } else if (settings_.style != DataStyle::None) {
        handleDataRow(tokens, lineNumber);
    } else {
        diagnose(lineNumber, QStringLiteral("Survey data row without a preceding data command."));
    }
}

void CentrelineParser::handleUnits(const QStringList &tokens, int lineNumber)
{
    if (tokens.size() < 3) {
        diagnose(lineNumber, QStringLiteral("Incomplete units command."));
        return;
    }
    const QString unit = tokens.last();
    double multiplier = 1.0;
    int lastQuantity = tokens.size() - 2;
    double numeric = 0.0;
    if (parseNumber(tokens.at(lastQuantity), &numeric)) {
        multiplier = numeric;
        --lastQuantity;
    }
    for (int index = 1; index <= lastQuantity; ++index) {
        const QStringList quantities = expandQuantityGroup(tokens.at(index));
        if (quantities.isEmpty()) {
            diagnose(lineNumber, QStringLiteral("Unknown units quantity \"%1\".").arg(tokens.at(index)));
            continue;
        }
        for (const QString &quantity : quantities) {
            double factor = 1.0;
            bool percent = false;
            const bool known = isAngleQuantity(quantity) ? angleUnitFactor(unit, &factor, &percent)
                                                         : lengthUnitFactor(unit, &factor);
            if (!known) {
                diagnose(lineNumber, QStringLiteral("Unknown units \"%1\".").arg(unit));
                continue;
            }
            settings_.unitFactors.insert(quantity, factor * multiplier);
            if (percent) {
                settings_.percentQuantities.insert(quantity);
            } else {
                settings_.percentQuantities.remove(quantity);
            }
        }
    }
}

void CentrelineParser::handleCalibrate(const QStringList &tokens, int lineNumber)
{
    QStringList quantities;
    QVector<double> values;
    for (int index = 1; index < tokens.size(); ++index) {
        double numeric = 0.0;
        if (parseNumber(tokens.at(index), &numeric)) {
            values.append(numeric);
            continue;
        }
        if (!values.isEmpty()) {
            break; // trailing units are applied as already-converted readings
        }
        const QStringList expanded = expandQuantityGroup(tokens.at(index));
        if (expanded.isEmpty()) {
            diagnose(lineNumber, QStringLiteral("Unknown calibrate quantity \"%1\".").arg(tokens.at(index)));
        }
        quantities.append(expanded);
    }
    if (quantities.isEmpty() || values.isEmpty()) {
        diagnose(lineNumber, QStringLiteral("Incomplete calibrate command."));
        return;
    }
    for (const QString &quantity : quantities) {
        settings_.calibrations.insert(quantity, {values.at(0), values.size() > 1 ? values.at(1) : 1.0});
    }
}

void CentrelineParser::handleDataHeader(const QStringList &tokens, int lineNumber)
{
    settings_.fields.clear();
    const QString style = tokens.size() > 1 ? tokens.at(1).toLower() : QString();
    if (style == QLatin1String("normal")) {
        settings_.style = DataStyle::Normal;
    } else if (style == QLatin1String("cartesian")) {
        settings_.style = DataStyle::Cartesian;
    } else if (style == QLatin1String("nosurvey")) {
        settings_.style = DataStyle::NoSurvey;
    } else if (style == QLatin1String("dimensions")) {
        settings_.style = DataStyle::Ignored;
        return;
    } else {
        settings_.style = DataStyle::Ignored;
        diagnose(lineNumber, QStringLiteral("Data style \"%1\" is not supported; its shots are skipped.").arg(style));
        return;
    }

    for (int index = 2; index < tokens.size(); ++index) {
        const QString field = canonicalQuantity(tokens.at(index));
        if (field.isEmpty()) {
            settings_.style = DataStyle::Ignored;
            diagnose(lineNumber,
                     QStringLiteral("Data field \"%1\" is not supported; its shots are skipped.").arg(tokens.at(index)));
            return;
        }
        settings_.fields.append(field);
    }
    if (!settings_.fields.contains(QStringLiteral("from")) || !settings_.fields.contains(QStringLiteral("to"))) {
        settings_.style = DataStyle::Ignored;
        diagnose(lineNumber, QStringLiteral("Data command without from/to fields is not supported."));
    }
}

void CentrelineParser::handleFix(const QStringList &tokens, int lineNumber)
{
    if (tokens.size() < 2) {
        diagnose(lineNumber, QStringLiteral("fix without a station."));
        return;
    }
    TherionCentrelineFix fix;
    fix.stationKey = stationKey(tokens.at(1));
    fix.coordinateSystem = settings_.coordinateSystem;
    fix.filePath = currentFile_;
    fix.lineNumber = lineNumber;
    if (tokens.size() >= 5) {
        if (!parseNumber(tokens.at(2), &fix.position.x) || !parseNumber(tokens.at(3), &fix.position.y)
            || !parseNumber(tokens.at(4), &fix.position.z)) {
            diagnose(lineNumber, QStringLiteral("fix coordinates are not numeric."));
            return;
        }
    }
    observations.fixes.append(fix);
}

void CentrelineParser::handleDataRow(const QStringList &tokens, int lineNumber)
{
    if (settings_.style == DataStyle::Ignored) {
        return;
    }

    QHash<QString, QString> values;
    int tokenIndex = 0;
    for (const QString &field : std::as_const(settings_.fields)) {
        if (field == QLatin1String("ignoreall")) {
            break;
        }
        if (tokenIndex >= tokens.size()) {
            diagnose(lineNumber, QStringLiteral("Data row has fewer values than the data command."));
            return;
        }
        if (field != QLatin1String("ignore")) {
            values.insert(field, tokens.at(tokenIndex));
        }
        ++tokenIndex;
    }

    QString fromToken = values.value(QStringLiteral("from"));
    QString toToken = values.value(QStringLiteral("to"));
    if (settings_.style == DataStyle::NoSurvey) {
        for (const QString &token : {fromToken, toToken}) {
            if (!isAnonymousStation(token)) {
                nosurveyStations.insert(stationKey(token));
            }
        }
        return;
    }

    const bool fromAnonymous = isAnonymousStation(fromToken);
    const bool toAnonymous = isAnonymousStation(toToken);
    if (fromAnonymous && toAnonymous) {
        diagnose(lineNumber, QStringLiteral("Shot between two anonymous stations is skipped."));
        return;
    }

    ThreeDViewerVec3 delta;
    double length = 0.0;
    if (settings_.style == DataStyle::Cartesian) {
        if (!reading(QStringLiteral("easting"), values.value(QStringLiteral("easting")), &delta.x)
            || !reading(QStringLiteral("northing"), values.value(QStringLiteral("northing")), &delta.y)
            || !reading(QStringLiteral("altitude"), values.value(QStringLiteral("altitude")), &delta.z)) {
            diagnose(lineNumber, QStringLiteral("Cartesian shot values are not numeric."));
            return;
        }
        length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    } else {
        if (!reading(QStringLiteral("length"), values.value(QStringLiteral("length")), &length)) {
            diagnose(lineNumber, QStringLiteral("Shot length is missing or not numeric."));
            return;
        }

        auto verticalSign = [](const QString &token) {
            const QString lower = token.toLower();
            if (lower == QLatin1String("up") || lower == QLatin1String("u") || lower == QLatin1String("+v")) {
                return 1;
            }
            if (lower == QLatin1String("down") || lower == QLatin1String("d") || lower == QLatin1String("-v")) {
                return -1;
            }
            return 0;
        };

        bool hasClino = false;
        double clino = 0.0;
        int vertical = 0;
        const QString clinoToken = values.value(QStringLiteral("clino"));
        const QString backClinoToken = values.value(QStringLiteral("backclino"));
        double forwardClino = 0.0;
        double backClino = 0.0;
        const bool hasForwardClino = reading(QStringLiteral("clino"), clinoToken, &forwardClino);
        const bool hasBackClino = reading(QStringLiteral("backclino"), backClinoToken, &backClino);
        if (hasForwardClino && hasBackClino) {
            clino = (forwardClino - backClino) / 2.0;
            hasClino = true;
        } else if (hasForwardClino) {
            clino = forwardClino;
            hasClino = true;
        } else if (hasBackClino) {
            clino = -backClino;
            hasClino = true;
        } else {
            vertical = verticalSign(clinoToken);
            if (vertical == 0) {
                vertical = -verticalSign(backClinoToken);
            }
            if (vertical == 0 && clinoToken.compare(QLatin1String("level"), Qt::CaseInsensitive) == 0) {
                hasClino = true;
            }
        }

        if (vertical != 0 || (hasClino && std::abs(std::abs(clino) - 90.0) < 1e-9)) {
            const double sign = vertical != 0 ? vertical : (clino > 0 ? 1.0 : -1.0);
            delta = {0.0, 0.0, sign * length};
        } else {
            double compass = 0.0;
            double backCompass = 0.0;
            const bool hasCompass = reading(QStringLiteral("compass"), values.value(QStringLiteral("compass")), &compass);
            const bool hasBackCompass =
                reading(QStringLiteral("backcompass"), values.value(QStringLiteral("backcompass")), &backCompass);
            if (!hasClino && !settings_.fields.contains(QStringLiteral("clino"))
                && !settings_.fields.contains(QStringLiteral("backclino"))) {
                hasClino = true; // plan-only data
            }
            if (!hasClino || (!hasCompass && !hasBackCompass)) {
                diagnose(lineNumber, QStringLiteral("Shot direction is missing or not numeric."));
                return;
            }
            double bearing = compass;
            if (hasCompass && hasBackCompass) {
                const double a = compass * kPi / 180.0;
                const double b = (backCompass + 180.0) * kPi / 180.0;
                bearing = std::atan2(std::sin(a) + std::sin(b), std::cos(a) + std::cos(b)) * 180.0 / kPi;
            } else if (hasBackCompass) {
                bearing = backCompass + 180.0;
            }
            if (settings_.hasDeclination) {
                bearing += settings_.declinationDegrees;
            }
            const double bearingRad = bearing * kPi / 180.0;
            const double clinoRad = clino * kPi / 180.0;
            const double horizontal = length * std::cos(clinoRad);
            delta = {horizontal * std::sin(bearingRad), horizontal * std::cos(bearingRad), length * std::sin(clinoRad)};
        }
    }

    TherionCentrelineShot shot;
    if (fromAnonymous) {
        std::swap(fromToken, toToken);
        delta = {-delta.x, -delta.y, -delta.z};
    }
    shot.fromKey = stationKey(fromToken);
    shot.toKey = isAnonymousStation(toToken) ? QString() : stationKey(toToken);
    shot.delta = delta;
    shot.length = length;
    shot.splay = settings_.splay || shot.toKey.isEmpty();
    shot.duplicate = settings_.duplicate;
    shot.surface = settings_.surface;
    shot.surveyPath = currentSurveyPath();
    shot.filePath = currentFile_;
    shot.lineNumber = lineNumber;
    stationsWithGeometry.insert(shot.fromKey);
    if (!shot.toKey.isEmpty()) {
        stationsWithGeometry.insert(shot.toKey);
    }
    observations.shots.append(shot);
}
} // namespace

TherionCentrelineFileReader defaultTherionCentrelineFileReader()
{
    return [](const QString &filePath, QString *contents) {
        QString encodingName;
        QString encodingLabel;
        QString errorMessage;
        return DocumentFile::readTextFile(filePath, contents, &encodingName, &encodingLabel, &errorMessage);
    };
}

TherionCentrelineObservations parseTherionCentreline(const QStringList &entryFilePaths,
                                                     const TherionCentrelineFileReader &readFile)
{
    CentrelineParser parser(readFile);
    for (const QString &entry : entryFilePaths) {
        parser.parseFile(entry, 0);
    }
    for (const QString &key : std::as_const(parser.nosurveyStations)) {
        if (!parser.stationsWithGeometry.contains(key)) {
            parser.observations.stationKeysWithoutGeometry.append(key);
        }
    }
    parser.observations.stationKeysWithoutGeometry.sort();
    return parser.observations;
}

TherionCentrelineResult computeTherionCentreline(const QStringList &entryFilePaths,
                                                 const TherionCentrelineFileReader &readFile)
{
    const TherionCentrelineObservations obs = parseTherionCentreline(entryFilePaths, readFile);
    TherionCentrelineResult result = solveTherionCentreline(obs);

    QVector<TherionScrap3DModel> scrapModels;
    quint32 nextMeshId = quint32(result.scene.meshGroups.size());
    for (const auto &ref : obs.th2References) {
        QString th2Contents;
        if (!readFile(ref.filePath, &th2Contents)) {
            continue;
        }
        const auto models = TherionScrap3D::projectTh2File(
            ref.filePath, th2Contents, ref.surveyPath, result.stationPositions, nextMeshId, 0);
        nextMeshId += quint32(models.size());
        scrapModels += models;
    }

    const QVector<TherionScrapJoinDiagnostic> joinDiagnostics =
        TherionScrap3D::applyJoins(&scrapModels, obs.joins);
    for (const TherionScrapJoinDiagnostic &diagnostic : joinDiagnostics) {
        result.diagnostics.append({diagnostic.filePath, diagnostic.lineNumber, diagnostic.message});
    }

    for (const TherionScrap3DModel &scrap : scrapModels) {
        result.scrapModels.append(scrap);
        result.scene.scrapLinework += scrap.linework;
        result.scene.scrapPoints += scrap.pointFeatures;
        if (!scrap.meshGroup.vertices.isEmpty() && !scrap.meshGroup.triangles.isEmpty()) {
            result.scene.meshGroups.append(scrap.meshGroup);
        }
    }

    return result;
}

} // namespace TherionStudio
