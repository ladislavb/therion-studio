#include "../src/core/TherionSourceValidator.h"
#include "../src/core/TherionCommandSyntax.h"
#include "../src/core/TherionDocumentEditor.h"

#include <QCoreApplication>

#include <cstdio>
#include <cstdlib>

using TherionStudio::TherionSourceValidationResult;
using TherionStudio::TherionSourceValidationCatalog;
using TherionStudio::TherionSourceValidator;
using TherionStudio::TherionSourceDiagnosticSeverity;
using TherionStudio::TherionSourceDiagnostic;
using TherionStudio::TherionSourceDocumentMetadata;
using TherionStudio::TherionSourceDocumentType;
using TherionStudio::TherionSourceTextEdit;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionSourceValidatorTest failed: %s\n", message);
        std::exit(1);
    }
}

QString diagnosticSourceRange(const TherionSourceDiagnostic &diagnostic)
{
    if (diagnostic.columnNumber <= 0 || diagnostic.columnLength <= 0) {
        return QString();
    }
    return diagnostic.currentText.mid(diagnostic.columnNumber - 1, diagnostic.columnLength);
}

void reportsAndFixesMalformedClipTokens()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off \"-clip off\" \"-clip off\"\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Malformed quoted -clip off tokens should produce one line-level diagnostic.");
    require(result.diagnostics.first().lineNumber == 1,
            "Malformed quoted -clip off diagnostic should point at the source line.");
    require(result.diagnostics.first().severity == TherionSourceDiagnosticSeverity::Error,
            "Malformed quoted -clip off tokens should be reported as an error.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Malformed quoted -clip off tokens should be removed from the suggested line.");
    require(diagnosticSourceRange(result.diagnostics.first()).startsWith(QStringLiteral("\"-clip off\"")),
            "Malformed quoted -clip off diagnostics should point at the first malformed token.");

    const QString fixed = TherionSourceValidator::applyFixes(contents, {result.diagnostics.first().fix});
    require(fixed == QStringLiteral("line rock-border -close on -clip off\nendline\n"),
            "Applying malformed quoted -clip off fix should preserve line endings and remove duplicate tokens.");
}

void plansValidationFixesAsSourceEdits()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off \"-clip off\"\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);
    require(result.diagnostics.size() == 1,
            "Malformed quoted -clip off tokens should produce a source-edit-plannable diagnostic.");

    const QVector<TherionSourceTextEdit> edits = TherionSourceValidator::validationFixEdits(contents,
                                                                                            {result.diagnostics.first().fix});
    require(edits.size() == 1,
            "Validation fix planner should expose one valid source edit.");

    QString updated = contents;
    require(TherionStudio::TherionDocumentEditor::applySourceTextEdits(&updated, edits),
            "Validation source edits should apply through the shared source edit helper.");
    require(updated == TherionSourceValidator::applyFixes(contents, {result.diagnostics.first().fix}),
            "Validation source edits should preserve existing applyFixes behavior.");

    TherionStudio::TherionSourceDiagnosticFix invalidFix;
    invalidFix.startOffset = contents.size() + 1;
    invalidFix.length = 1;
    invalidFix.replacementText = QStringLiteral("ignored");
    require(TherionSourceValidator::validationFixEdits(contents, {invalidFix}).isEmpty(),
            "Validation fix planner should ignore invalid source ranges.");
}

void reportsAndFixesDuplicateOptionRows()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off -clip off\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Duplicate option/value pairs should produce one line-level diagnostic.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Duplicate option/value pairs should be removed from the suggested line.");
    require(diagnosticSourceRange(result.diagnostics.first()) == QStringLiteral("-clip off"),
            "Duplicate option diagnostics should point at the duplicate option/value token range.");
}

void reportsAndFixesDuplicateLinePointSmoothOffRows()
{
    const QString contents = QStringLiteral("line rock-border\n"
                                            "  0 0\n"
                                            "  smooth off\n"
                                            "  smooth off\n"
                                            "  1 1\n"
                                            "  smooth off\n"
                                            "  smooth off\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 2,
            "Duplicate line-point smooth off rows should produce one warning per redundant row.");
    require(result.diagnostics.at(0).code == QStringLiteral("duplicate-line-point-smooth-off"),
            "Duplicate line-point smooth off diagnostics should use a dedicated code.");
    require(result.diagnostics.at(0).severity == TherionSourceDiagnosticSeverity::Warning,
            "Duplicate line-point smooth off diagnostics should be warnings.");
    require(result.diagnostics.at(0).lineNumber == 4
                && result.diagnostics.at(1).lineNumber == 7,
            "Duplicate line-point smooth off diagnostics should point at redundant rows.");
    require(result.diagnostics.at(0).hasFix && result.diagnostics.at(1).hasFix,
            "Duplicate line-point smooth off diagnostics should provide safe removal fixes.");

    const QString fixed = TherionSourceValidator::applyFixes(contents,
                                                             {result.diagnostics.at(0).fix,
                                                              result.diagnostics.at(1).fix});
    require(fixed == QStringLiteral("line rock-border\n"
                                    "  0 0\n"
                                    "  smooth off\n"
                                    "  1 1\n"
                                    "  smooth off\n"
                                    "endline\n"),
            "Applying duplicate line-point smooth off fixes should remove only redundant rows.");
}

void acceptsSingleLinePointSmoothOffBeforeEndline()
{
    const QString contents = QStringLiteral("line rock-border\n"
                                            "  0 0\n"
                                            "  1 1\n"
                                            "  smooth off\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        require(diagnostic.code != QStringLiteral("duplicate-line-point-smooth-off"),
                "A single smooth off before endline is valid XTherion line-point metadata.");
    }
}

void reportsAndFixesOptionLikeSubtype()
{
    const QString contents = QStringLiteral("line rock-border -close on -clip off -subtype \"-clip off\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 1,
            "Option-like subtype values should produce one line-level diagnostic.");
    require(result.diagnostics.first().suggestedText == QStringLiteral("line rock-border -close on -clip off"),
            "Option-like subtype values should be removed from the suggested line.");
}

void keepsQuotedTextValuesStartingWithDash()
{
    const QString contents = QStringLiteral("line label -text \"-clip off\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.isEmpty(),
            "Quoted text values that start with '-' should not be reported as malformed option tokens.");
}

void reportsDuplicateObjectIdsInSameSourceScope()
{
    const QString contents = QStringLiteral("scrap test\n"
                                            "line wall -id line-1\n"
                                            "  0 0\n"
                                            "  1 1\n"
                                            "endline\n"
                                            "line border -id line-1\n"
                                            "  2 2\n"
                                            "  3 3\n"
                                            "endline\n"
                                            "point 0 0 label -id line-1\n"
                                            "endscrap\n"
                                            "\n"
                                            "scrap other\n"
                                            "line wall -id line-1\n"
                                            "  4 4\n"
                                            "  5 5\n"
                                            "endline\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 2,
            "Duplicate object ids should be reported for later objects in the same source scope, regardless of object type.");
    const TherionSourceDiagnostic &diagnostic = result.diagnostics.at(0);
    require(diagnostic.code == QStringLiteral("duplicate-object-id"),
            "Duplicate explicit object ids should use the duplicate-object-id diagnostic code.");
    require(diagnostic.lineNumber == 6,
            "Duplicate explicit object id diagnostic should point at the second object declaration.");
    require(diagnostic.severity == TherionSourceDiagnosticSeverity::Error,
            "Duplicate explicit object ids in the same source scope should be reported as errors.");
    require(diagnosticSourceRange(diagnostic) == QStringLiteral("line-1"),
            "Duplicate explicit object id diagnostic should point at the repeated id value.");
    require(result.diagnostics.at(1).lineNumber == 10
                && diagnosticSourceRange(result.diagnostics.at(1)) == QStringLiteral("line-1"),
            "Duplicate explicit IDs should share one namespace for point, line, and area objects in a scrap.");
}

void reportsDuplicateNamespaceObjectNamesInSameParentScope()
{
    const QString contents = QStringLiteral("survey root\n"
                                            "map shared\n"
                                            "endmap\n"
                                            "scrap shared\n"
                                            "endscrap\n"
                                            "survey shared\n"
                                            "endsurvey\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    require(result.diagnostics.size() == 2,
            "Map, scrap, and survey names should share one namespace in the same parent scope.");
    require(result.diagnostics.at(0).code == QStringLiteral("duplicate-object-id")
                && result.diagnostics.at(0).lineNumber == 4
                && diagnosticSourceRange(result.diagnostics.at(0)) == QStringLiteral("shared"),
            "Duplicate scrap names should be reported against the repeated namespace token.");
    require(result.diagnostics.at(1).code == QStringLiteral("duplicate-object-id")
                && result.diagnostics.at(1).lineNumber == 6
                && diagnosticSourceRange(result.diagnostics.at(1)) == QStringLiteral("shared"),
            "Duplicate survey names should be reported against the repeated namespace token.");
}

void reportsUnknownAreaLineReferencesInCurrentScrap()
{
    const QString contents = QStringLiteral("scrap test\n"
                                            "line wall -id line-2\n"
                                            "endline\n"
                                            "area water\n"
                                            "  line-1\n"
                                            "  line-2\n"
                                            "endarea\n"
                                            "endscrap\n"
                                            "\n"
                                            "scrap other\n"
                                            "line wall -id line-1\n"
                                            "endline\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents);

    bool foundExpectedDiagnostic = false;
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        foundExpectedDiagnostic = foundExpectedDiagnostic
            || (diagnostic.code == QStringLiteral("unknown-area-line-reference")
                && diagnostic.lineNumber == 5
                && diagnostic.severity == TherionSourceDiagnosticSeverity::Error
                && diagnosticSourceRange(diagnostic) == QStringLiteral("line-1"));
    }
    require(foundExpectedDiagnostic,
            "Unknown area line reference diagnostic should point at the missing area boundary id.");
}

TherionSourceValidationCatalog basicCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("encoding"),
        QStringLiteral("survey"),
        QStringLiteral("input"),
        QStringLiteral("map"),
        QStringLiteral("export"),
        QStringLiteral("centerline"),
        QStringLiteral("revise"),
        QStringLiteral("scrap"),
        QStringLiteral("line"),
        QStringLiteral("point"),
        QStringLiteral("area"),
        QStringLiteral("map-header"),
        QStringLiteral("join"),
        QStringLiteral("layout"),
    };
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("survey"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("input"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("map"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("export"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("revise"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("scrap"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("line"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("point"), 3);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("area"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("map-header"), 3);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("join"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("layout"), 1);
    catalog.commandMaxPositionalCount.insert(QStringLiteral("export"), 1);
    catalog.commandMaxPositionalCount.insert(QStringLiteral("map-header"), 3);
    catalog.commandMaxPositionalCount.insert(QStringLiteral("layout"), 1);
    catalog.commandOptionNames.insert(QStringLiteral("survey"), {QStringLiteral("-title")});
    catalog.commandOptionNames.insert(QStringLiteral("map"), {QStringLiteral("-title")});
    catalog.commandOptionNames.insert(QStringLiteral("export"), {QStringLiteral("-output"), QStringLiteral("-o"), QStringLiteral("-layout")});
    catalog.commandOptionNames.insert(QStringLiteral("revise"), {QStringLiteral("-stations")});
    catalog.commandOptionNames.insert(QStringLiteral("point"), {QStringLiteral("-name"), QStringLiteral("-text")});
    catalog.commandOptionNames.insert(QStringLiteral("line"),
                                      {QStringLiteral("-close"), QStringLiteral("-clip"), QStringLiteral("-subtype")});
    catalog.commandOptionNames.insert(QStringLiteral("scrap"), {QStringLiteral("-projection"), QStringLiteral("-scale")});
    catalog.commandTypeValues.insert(QStringLiteral("line"), {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandTypeValues.insert(QStringLiteral("area"), {QStringLiteral("water"), QStringLiteral("sand")});
    catalog.commandArgumentAllowedValuesByKey.insert(TherionStudio::commandArgumentValueKey(QStringLiteral("line"), 0),
                                                     {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandArgumentAllowedValuesByKey.insert(TherionStudio::commandArgumentValueKey(QStringLiteral("map-header"), 2),
                                                     {QStringLiteral("off"), QStringLiteral("n"), QStringLiteral("center")});
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionAllowedValuesByKey.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                   {QStringLiteral("on"), QStringLiteral("off"), QStringLiteral("auto")});
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-subtype")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandSubtypeValuesByTypeKey.insert(TherionStudio::commandSubtypeValueKey(QStringLiteral("line"), QStringLiteral("wall")),
                                                 {QStringLiteral("bedrock"), QStringLiteral("blocks")});
    catalog.commandSubtypeValuesByTypeKey.insert(TherionStudio::commandSubtypeValueKey(QStringLiteral("line"), QStringLiteral("border")),
                                                 {QStringLiteral("visible"), QStringLiteral("invisible")});
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-projection")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-scale")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("export"), QStringLiteral("-o")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("export"), QStringLiteral("-layout")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionFixedArityByKey.insert(TherionStudio::commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-text")),
                                                0);
    catalog.commandOptionValueArityTokens.insert(TherionStudio::commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-name")),
                                                 QStringLiteral("EXACTLY_ONE"));
    return catalog;
}

bool containsDiagnostic(const TherionSourceValidationResult &result, const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

const TherionSourceDiagnostic *diagnosticForCode(const TherionSourceValidationResult &result,
                                                 const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return &diagnostic;
        }
    }
    return nullptr;
}

TherionSourceDiagnosticSeverity severityForDiagnostic(const TherionSourceValidationResult &result, const QString &code)
{
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return diagnostic.severity;
        }
    }
    std::fprintf(stderr, "TherionSourceValidatorTest failed: missing diagnostic %s\n", qPrintable(code));
    std::exit(1);
}

void reportsUnknownCommandWithoutSafeFix()
{
    const QString contents = QStringLiteral("scrap test\n"
                                            "bogus value\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Unknown command should produce a review-only diagnostic.");
    for (const auto &diagnostic : result.diagnostics) {
        if (diagnostic.code == QStringLiteral("unknown-command")) {
            require(!diagnostic.hasFix, "Unknown command should not be marked as a safe fix.");
        }
    }
}

void reportsUnknownOptionAndMissingOptionValue()
{
    const QString contents = QStringLiteral("line wall -bogus x -clip\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Unknown option should produce a diagnostic.");
    const TherionSourceDiagnostic *unknownOption =
        diagnosticForCode(result, QStringLiteral("unknown-option"));
    require(unknownOption != nullptr && diagnosticSourceRange(*unknownOption) == QStringLiteral("-bogus"),
            "Unknown option diagnostics should point at the unknown option token.");
    require(severityForDiagnostic(result, QStringLiteral("unknown-option")) == TherionSourceDiagnosticSeverity::Warning,
            "Unknown option should remain a warning because the catalog may be incomplete.");
    require(containsDiagnostic(result, QStringLiteral("missing-option-value")),
            "Known option without a required value should produce a diagnostic.");
    const TherionSourceDiagnostic *missingOptionValue =
        diagnosticForCode(result, QStringLiteral("missing-option-value"));
    require(missingOptionValue != nullptr && diagnosticSourceRange(*missingOptionValue) == QStringLiteral("-clip"),
            "Missing option value diagnostics should point at the option token.");
    require(severityForDiagnostic(result, QStringLiteral("missing-option-value")) == TherionSourceDiagnosticSeverity::Error,
            "Known option without a required value should be reported as an error.");
}

void reportsMissingRequiredArgument()
{
    const QString contents = QStringLiteral("scrap -projection plan\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("missing-argument")),
            "Missing required positional argument should produce a diagnostic.");
    require(severityForDiagnostic(result, QStringLiteral("missing-argument")) == TherionSourceDiagnosticSeverity::Error,
            "Missing required positional argument should be reported as an error.");
}

void allowsStationPointWithoutName()
{
    const TherionSourceValidationResult unnamed =
        TherionSourceValidator::validate(QStringLiteral("scrap test\n"
                                                        "point 10 20 station\n"
                                                        "endscrap\n"),
                                         basicCatalog());
    require(!containsDiagnostic(unnamed, QStringLiteral("missing-station-reference")),
            "Station points without -name are valid Therion map objects.");

    const TherionSourceValidationResult named =
        TherionSourceValidator::validate(QStringLiteral("scrap test\n"
                                                        "point 10 20 station -name 1@survey\n"
                                                        "endscrap\n"),
                                         basicCatalog());
    require(!containsDiagnostic(named, QStringLiteral("missing-station-reference")),
            "Station points with -name should not produce missing station reference diagnostics.");

    const TherionSourceValidationResult label =
        TherionSourceValidator::validate(QStringLiteral("scrap test\n"
                                                        "point 10 20 label -text label\n"
                                                        "endscrap\n"),
                                         basicCatalog());
    require(!containsDiagnostic(label, QStringLiteral("missing-station-reference")),
            "Non-station points should not require -name.");
}

void acceptsBracketedOptionValueAsSingleLogicalValue()
{
    const QString contents = QStringLiteral("scrap vchod_02.s -projection plan -scale [0 0 1600 0 0.0 0.0 40.64 0.0 m]\n"
                                            "endscrap\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Bracketed option values should count as one logical value for fixed arity validation.");
}

void acceptsLineContinuationAfterOptionValue()
{
    const QString contents =
        QStringLiteral("survey 1318 -title \"1318 Vetrna propast\" \\")
        + QLatin1Char('\n')
        + QStringLiteral("  -person-rename \"Lenka Souckova\" \"Lenka Blazkova\"\n"
                         "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Line continuation markers should not count as option values.");
}

void validatesContinuedCommandAsOneCatalogCommand()
{
    const QString contents =
        QStringLiteral("survey 1318 -title \"1318 Vetrna propast\" \\")
        + QLatin1Char('\n')
        + QStringLiteral("  -person-rename \"Lenka Souckova\" \"Lenka Blazkova\"\n"
                         "endsurvey\n");
    TherionSourceValidationCatalog catalog = basicCatalog();
    catalog.commandOptionNames[QStringLiteral("survey")].insert(QStringLiteral("-person-rename"));
    catalog.commandOptionFixedArityByKey.insert(
        TherionStudio::commandOptionValueKey(QStringLiteral("survey"), QStringLiteral("-person-rename")),
        2);

    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, catalog);

    require(!containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Continuation rows should be validated as part of the logical command, not as standalone commands.");
    require(!containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Known options on continuation rows should be associated with the opening command.");
    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Continuation rows should share logical option value counting with the opening command.");
}

void mapsContinuedCommandDiagnosticsToPhysicalLine()
{
    const QString contents =
        QStringLiteral("survey 1318 -title \"1318 Vetrna propast\" \\")
        + QLatin1Char('\n')
        + QStringLiteral("  -bogus value\n"
                         "endsurvey\n");

    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());
    const TherionSourceDiagnostic *unknownOption =
        diagnosticForCode(result, QStringLiteral("unknown-option"));

    require(unknownOption != nullptr,
            "Unknown options on continuation rows should still produce diagnostics.");
    require(unknownOption->lineNumber == 2,
            "Continuation-row diagnostics should point at the physical continuation line.");
    require(unknownOption->columnNumber == 3,
            "Continuation-row diagnostics should point at the physical source column.");
    require(diagnosticSourceRange(*unknownOption) == QStringLiteral("-bogus"),
            "Continuation-row diagnostics should carry the physical source line for token previews.");
}

void doesNotTreatZeroFixedArityAsValidationError()
{
    const QString contents = QStringLiteral("point 389.5 355.5 label -text \"Sifon I\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("wrong-option-value-count")),
            "Fixed arity zero from catalog metadata is not reliable enough to reject existing option values.");
}

void keepsDashPrefixedPointTextAsTextValue()
{
    const QString contents = QStringLiteral("point 4505.0 -1446.0 label -text \"-21 m\"\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Dash-prefixed quoted point text values should not be reported as unknown options.");
}

void acceptsOptionAliasesExtractedFromCatalogSignature()
{
    const QStringList aliases = TherionStudio::extractOptionKeysFromSignatureAliases(QStringLiteral("output/o <file>"));
    require(aliases.contains(QStringLiteral("-output")),
            "Option alias extraction should include the full option name.");
    require(aliases.contains(QStringLiteral("-o")),
            "Option alias extraction should include the short option alias.");

    const TherionSourceValidationResult result =
        TherionSourceValidator::validate(QStringLiteral("export map -o map.pdf -layout moj\n"), basicCatalog());
    require(!containsDiagnostic(result, QStringLiteral("unknown-option")),
            "Known option aliases such as export -o should not be reported as unknown options.");
}

void reportsUnknownArgumentValue()
{
    const QString contents = QStringLiteral("line mystery\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-argument-value")),
            "Unknown first argument value should produce a diagnostic when allowed values are known.");
    const TherionSourceDiagnostic *unknownArgumentValue =
        diagnosticForCode(result, QStringLiteral("unknown-argument-value"));
    require(unknownArgumentValue != nullptr && diagnosticSourceRange(*unknownArgumentValue) == QStringLiteral("mystery"),
            "Unknown argument value diagnostics should point at the argument token.");
}

void reportsUnknownLaterArgumentValue()
{
    const QString contents = QStringLiteral("map-header 70 100 sideways\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-argument-value")),
            "Unknown later positional argument values should produce a diagnostic when allowed values are known.");
    const TherionSourceDiagnostic *unknownArgumentValue =
        diagnosticForCode(result, QStringLiteral("unknown-argument-value"));
    require(unknownArgumentValue != nullptr && diagnosticSourceRange(*unknownArgumentValue) == QStringLiteral("sideways"),
            "Unknown later argument value diagnostics should point at the invalid argument token.");
}

void reportsExtraPositionalArgument()
{
    const QString contents = QStringLiteral("map-header 70 100 nw bbb\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("extra-argument")),
            "Extra positional arguments should produce a diagnostic when a catalog max is known.");
    const TherionSourceDiagnostic *extraArgument =
        diagnosticForCode(result, QStringLiteral("extra-argument"));
    require(extraArgument != nullptr && diagnosticSourceRange(*extraArgument) == QStringLiteral("bbb"),
            "Extra positional argument diagnostics should point at the first extra argument token.");
    require(extraArgument != nullptr && extraArgument->severity == TherionSourceDiagnosticSeverity::Warning,
            "Extra positional argument diagnostics should be warnings.");
}

void doesNotCountOptionsAsExtraPositionalArguments()
{
    const QString contents = QStringLiteral("export map -o map.pdf -layout moj\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("extra-argument")),
            "Option tokens and option values should not count as extra positional arguments.");
}

void acceptsVariadicJoinArguments()
{
    const QString contents = QStringLiteral("join mnp_01.s mnp_02.s zeleza_zvon_01.s -count 2\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("extra-argument")),
            "Variadic join arguments should not be reported as extra positional arguments.");
}

void acceptsLayoutIdArgument()
{
    const QString contents = QStringLiteral("layout l_plan\n"
                                            "endlayout\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("extra-argument")),
            "Layout id should count as the declared positional argument.");
}

void reportsUnknownOptionValue()
{
    const QString contents = QStringLiteral("line wall -close maybe\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-option-value")),
            "Unknown option values should produce a diagnostic when allowed values are known.");
    const TherionSourceDiagnostic *unknownOptionValue =
        diagnosticForCode(result, QStringLiteral("unknown-option-value"));
    require(unknownOptionValue != nullptr && diagnosticSourceRange(*unknownOptionValue) == QStringLiteral("maybe"),
            "Unknown option value diagnostics should point at the invalid option value token.");
    require(severityForDiagnostic(result, QStringLiteral("unknown-option-value")) == TherionSourceDiagnosticSeverity::Warning,
            "Unknown option values should remain warnings because catalog values may be incomplete.");
}

void reportsUnknownSubtypeValueForCurrentSymbolType()
{
    const QString contents = QStringLiteral("line wall -subtype visible\n"
                                            "endline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(containsDiagnostic(result, QStringLiteral("unknown-option-value")),
            "Subtype values incompatible with the current symbol type should produce a validator diagnostic.");
    const TherionSourceDiagnostic *unknownOptionValue =
        diagnosticForCode(result, QStringLiteral("unknown-option-value"));
    require(unknownOptionValue != nullptr && diagnosticSourceRange(*unknownOptionValue) == QStringLiteral("visible"),
            "Subtype compatibility diagnostics should point at the subtype value token.");
}

void reportsInvalidCommandContextWhenCatalogListsContexts()
{
    TherionSourceValidationCatalog catalog = basicCatalog();
    catalog.commandContexts.insert(QStringLiteral("line"), {QStringLiteral("scrap")});

    const QString contents = QStringLiteral("survey cave\n"
                                            "line wall\n"
                                            "endline\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, catalog);

    require(containsDiagnostic(result, QStringLiteral("invalid-command-context")),
            "Known commands should produce a context diagnostic when the catalog excludes the current block.");
    const TherionSourceDiagnostic *invalidContext =
        diagnosticForCode(result, QStringLiteral("invalid-command-context"));
    require(invalidContext != nullptr && diagnosticSourceRange(*invalidContext) == QStringLiteral("line"),
            "Invalid context diagnostics should point at the command token.");
    require(severityForDiagnostic(result, QStringLiteral("invalid-command-context")) == TherionSourceDiagnosticSeverity::Warning,
            "Invalid context should remain a warning because catalog context metadata may be incomplete.");

    const TherionSourceValidationResult validResult = TherionSourceValidator::validate(
        QStringLiteral("scrap s1\n"
                       "line wall\n"
                       "endline\n"
                       "endscrap\n"),
        catalog);
    require(!containsDiagnostic(validResult, QStringLiteral("invalid-command-context")),
            "Commands in a catalog-listed context should not produce context diagnostics.");
}

void skipsContextValidationWhenCatalogOmitsContexts()
{
    const TherionSourceValidationResult result =
        TherionSourceValidator::validate(QStringLiteral("line wall\n"
                                                        "endline\n"),
                                         basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("invalid-command-context")),
            "Catalogs without command context metadata should not produce context diagnostics.");
}

void reportsInvalidDocumentTypeWhenCatalogListsTypes()
{
    TherionSourceValidationCatalog catalog = basicCatalog();
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    catalog.commandNames.insert(QStringLiteral("source"));

    TherionSourceDocumentMetadata mapMetadata;
    mapMetadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceValidationResult invalidResult =
        TherionSourceValidator::validate(QStringLiteral("source index.th\n"),
                                         catalog,
                                         mapMetadata);

    require(containsDiagnostic(invalidResult, QStringLiteral("invalid-document-type")),
            "Known commands should produce a document-type diagnostic when the catalog excludes the current document type.");
    const TherionSourceDiagnostic *invalidDocumentType =
        diagnosticForCode(invalidResult, QStringLiteral("invalid-document-type"));
    require(invalidDocumentType != nullptr && diagnosticSourceRange(*invalidDocumentType) == QStringLiteral("source"),
            "Invalid document-type diagnostics should point at the command token.");
    require(severityForDiagnostic(invalidResult, QStringLiteral("invalid-document-type")) == TherionSourceDiagnosticSeverity::Warning,
            "Invalid document type should remain a warning because catalog document-type metadata may be incomplete.");

    TherionSourceDocumentMetadata configMetadata;
    configMetadata.sourceType = TherionSourceDocumentType::TherionConfig;
    const TherionSourceValidationResult validResult =
        TherionSourceValidator::validate(QStringLiteral("source index.th\n"),
                                         catalog,
                                         configMetadata);
    require(!containsDiagnostic(validResult, QStringLiteral("invalid-document-type")),
            "Commands in a catalog-listed document type should not produce document-type diagnostics.");
}

void skipsDocumentTypeValidationWithoutKnownSourceType()
{
    TherionSourceValidationCatalog catalog = basicCatalog();
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    catalog.commandNames.insert(QStringLiteral("source"));

    const TherionSourceValidationResult result =
        TherionSourceValidator::validate(QStringLiteral("source index.th\n"), catalog);

    require(!containsDiagnostic(result, QStringLiteral("invalid-document-type")),
            "Document-type diagnostics should not be reported when validation has no known source type.");
}

void reportsBlockPairProblems()
{
    const TherionSourceValidationResult unclosed =
        TherionSourceValidator::validate(QStringLiteral("scrap test\n"), basicCatalog());
    require(containsDiagnostic(unclosed, QStringLiteral("unclosed-block")),
            "Unclosed block should produce a diagnostic.");
    require(severityForDiagnostic(unclosed, QStringLiteral("unclosed-block")) == TherionSourceDiagnosticSeverity::Error,
            "Unclosed block should be reported as an error.");

    const TherionSourceValidationResult unmatched =
        TherionSourceValidator::validate(QStringLiteral("endscrap\n"), basicCatalog());
    require(containsDiagnostic(unmatched, QStringLiteral("unmatched-block-close")),
            "Unmatched block close should produce a diagnostic.");
    require(severityForDiagnostic(unmatched, QStringLiteral("unmatched-block-close")) == TherionSourceDiagnosticSeverity::Error,
            "Unmatched block close should be reported as an error.");
    require(!containsDiagnostic(unmatched, QStringLiteral("unknown-command")),
            "Known block close directives should not be reported as unknown commands.");
}

void keepsInputAsStandaloneCommandAndMapReferencesAsMapContent()
{
    const QString contents = QStringLiteral("encoding utf-8\n"
                                            "survey 1303_1974 -title \"1303 old map\"\n"
                                            "\n"
                                            "input stara_dvanactka_1974/00index.th\n"
                                            "\n"
                                            "map 1303_1974.m -title \"1303 old map\"\n"
                                            "  stara_dvanactka_1974.m@stara_dvanactka_1974\n"
                                            "endmap\n"
                                            "\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "Standalone input commands should not corrupt block matching.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "Standalone input commands should not make later block closes look unmatched.");
    require(!containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Map object reference rows should not be reported as unknown commands.");
}

void keepsCenterlineDataRowsOutOfCommandCatalogValidation()
{
    const QString contents = QStringLiteral("centerline\n"
                                            "  data normal from to compass clino tape\n"
                                            "  1 2 0 0 1\n"
                                            "endcenterline\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unknown-command")),
            "Centerline survey data rows should not be reported as unknown commands.");
    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "Centerline blocks should still be matched.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "Centerline block closes should still be matched.");
}

void keepsOneLineReviseFromOpeningBlock()
{
    const QString contents = QStringLiteral("survey cave\n"
                                            "revise hipds@hip.hip_dom -stations 19@dur.dur_dom\n"
                                            "endsurvey\n");
    const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());

    require(!containsDiagnostic(result, QStringLiteral("unclosed-block")),
            "One-line revise commands should not open a block.");
    require(!containsDiagnostic(result, QStringLiteral("unmatched-block-close")),
            "One-line revise commands should not make the enclosing block close look unmatched.");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    reportsAndFixesMalformedClipTokens();
    plansValidationFixesAsSourceEdits();
    reportsAndFixesDuplicateOptionRows();
    reportsAndFixesDuplicateLinePointSmoothOffRows();
    acceptsSingleLinePointSmoothOffBeforeEndline();
    reportsAndFixesOptionLikeSubtype();
    keepsQuotedTextValuesStartingWithDash();
    reportsDuplicateObjectIdsInSameSourceScope();
    reportsDuplicateNamespaceObjectNamesInSameParentScope();
    reportsUnknownAreaLineReferencesInCurrentScrap();
    reportsUnknownCommandWithoutSafeFix();
    reportsUnknownOptionAndMissingOptionValue();
    reportsMissingRequiredArgument();
    allowsStationPointWithoutName();
    acceptsBracketedOptionValueAsSingleLogicalValue();
    acceptsLineContinuationAfterOptionValue();
    validatesContinuedCommandAsOneCatalogCommand();
    mapsContinuedCommandDiagnosticsToPhysicalLine();
    doesNotTreatZeroFixedArityAsValidationError();
    keepsDashPrefixedPointTextAsTextValue();
    acceptsOptionAliasesExtractedFromCatalogSignature();
    reportsUnknownArgumentValue();
    reportsUnknownLaterArgumentValue();
    reportsExtraPositionalArgument();
    doesNotCountOptionsAsExtraPositionalArguments();
    acceptsVariadicJoinArguments();
    acceptsLayoutIdArgument();
    reportsUnknownOptionValue();
    reportsUnknownSubtypeValueForCurrentSymbolType();
    reportsInvalidCommandContextWhenCatalogListsContexts();
    skipsContextValidationWhenCatalogOmitsContexts();
    reportsInvalidDocumentTypeWhenCatalogListsTypes();
    skipsDocumentTypeValidationWithoutKnownSourceType();
    reportsBlockPairProblems();
    keepsInputAsStandaloneCommandAndMapReferencesAsMapContent();
    keepsCenterlineDataRowsOutOfCommandCatalogValidation();
    keepsOneLineReviseFromOpeningBlock();
    return 0;
}
