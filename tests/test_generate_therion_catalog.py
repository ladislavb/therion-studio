#!/usr/bin/env python3
"""Regression tests for therion command catalog generation."""

from __future__ import annotations

import importlib.util
import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
GENERATOR_PATH = REPO_ROOT / "scripts" / "generate_therion_catalog.py"
OVERRIDES_DIR = REPO_ROOT / "resources" / "therion_catalog" / "overrides"


def load_generator_module():
    spec = importlib.util.spec_from_file_location("generate_therion_catalog", GENERATOR_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load generator module spec")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TherionCatalogGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.generator = load_generator_module()
        cls.inputs = [
            REPO_ROOT / "therion" / "thbook" / "ch02.tex",
            REPO_ROOT / "therion" / "thbook" / "ch03.tex",
        ]
        cls.catalog = cls.generator.build_catalog(
            cls.inputs,
            source_repo="snapshot-copy",
            source_ref="snapshot-copy",
        )
        cls.catalog = cls.generator.apply_command_override_files(cls.catalog, OVERRIDES_DIR)
        cls.commands_by_name = {entry["name"]: entry for entry in cls.catalog["commands"]}

    def test_expected_core_commands_exist(self) -> None:
        expected = {
            "survey",
            "centreline",
            "scrap",
            "point",
            "line",
            "area",
            "map",
            "layout",
            "export",
            "encoding",
        }
        missing = sorted(expected - set(self.commands_by_name.keys()))
        self.assertFalse(missing, f"Missing expected commands: {missing}")

    def test_command_names_are_clean_keywords(self) -> None:
        keyword_re = re.compile(r"^[a-z0-9][a-z0-9-]*$")
        invalid = [
            name
            for name in self.commands_by_name
            if not keyword_re.fullmatch(name)
        ]
        self.assertFalse(invalid, f"Invalid generated command names: {invalid}")

    def test_centreline_alias_from_overrides(self) -> None:
        centreline = self.commands_by_name["centreline"]
        aliases = centreline.get("aliases", [])
        self.assertIn("centerline", aliases)

    def test_statistics_override_splits_item_and_value_arguments(self) -> None:
        arguments = self.commands_by_name["statistics"]["arguments"]
        self.assertEqual(len(arguments), 2)
        self.assertEqual(
            arguments[0]["allowed_values"],
            [
                "carto",
                "carto-count",
                "copyright",
                "copyright-count",
                "explo",
                "explo-length",
                "topo",
                "topo-length",
            ],
        )
        self.assertEqual(arguments[0]["value_arity"], "1")
        self.assertEqual(arguments[1]["allowed_values"], [])
        self.assertEqual(arguments[1]["value_arity"], "1")

    def test_known_context_examples(self) -> None:
        line_contexts = self.commands_by_name["line"].get("contexts", [])
        point_contexts = self.commands_by_name["point"].get("contexts", [])
        self.assertIn("scrap", line_contexts)
        self.assertIn("scrap", point_contexts)
        self.assertEqual(self.commands_by_name["encoding"].get("contexts", []), ["none"])
        import_contexts = self.commands_by_name["import"].get("contexts", [])
        self.assertIn("all", import_contexts)
        self.assertIn("survey", import_contexts)
        self.assertIn("layout", self.commands_by_name["cs"].get("contexts", []))
        self.assertEqual(self.commands_by_name["select"].get("contexts", []), ["none"])
        self.assertEqual(self.commands_by_name["export"].get("contexts", []), ["none"])

    def test_context_values_use_known_enums(self) -> None:
        allowed = {
            "all",
            "none",
            "survey",
            "scrap",
            "centerline",
            "map",
            "surface",
            "layout",
            "lookup",
        }
        invalid_contexts = []
        for command_name, command in self.commands_by_name.items():
            for context in command.get("contexts", []):
                if context not in allowed:
                    invalid_contexts.append((command_name, context))
        self.assertFalse(invalid_contexts, f"Unexpected context enums: {invalid_contexts}")

    def test_document_type_values_use_known_enums(self) -> None:
        allowed = {"all", "th", "th2", "thconfig"}
        invalid_document_types = []
        for command_name, command in self.commands_by_name.items():
            for document_type in command.get("document_types", []):
                if document_type not in allowed:
                    invalid_document_types.append((command_name, document_type))
        self.assertFalse(invalid_document_types, f"Unexpected document type enums: {invalid_document_types}")

    def test_known_document_type_examples(self) -> None:
        self.assertEqual(self.commands_by_name["encoding"].get("document_types", []), ["all"])
        self.assertEqual(self.commands_by_name["survey"].get("document_types", []), ["th"])
        self.assertEqual(self.commands_by_name["map"].get("document_types", []), ["th"])
        self.assertEqual(self.commands_by_name["source"].get("document_types", []), ["thconfig"])
        self.assertEqual(self.commands_by_name["export"].get("document_types", []), ["thconfig"])
        self.assertEqual(self.commands_by_name["scrap"].get("document_types", []), ["th2"])
        self.assertEqual(self.commands_by_name["point"].get("document_types", []), ["th2"])
        self.assertEqual(self.commands_by_name["line"].get("document_types", []), ["th2"])
        self.assertEqual(self.commands_by_name["area"].get("document_types", []), ["th2"])
        self.assertEqual(self.commands_by_name["join"].get("document_types", []), ["th", "th2"])

    def test_source_files_are_recorded(self) -> None:
        metadata = self.catalog.get("metadata", {})
        source_files = metadata.get("source_files", [])
        expected = [str(path) for path in self.inputs]
        self.assertEqual(source_files, expected)

    def test_option_shape_fields_exist(self) -> None:
        allowed_arity = {"0", "1", "N"}
        allowed_domain = {
            "none",
            "enum",
            "number",
            "date",
            "encoding",
            "path",
            "string",
            "station",
            "keyword",
            "projection",
            "unknown",
        }
        for command in self.catalog["commands"]:
            for option in command.get("options", []):
                self.assertIn("option_key", option)
                self.assertIn("value_arity", option)
                self.assertIn("value_domain", option)
                self.assertIn(option["value_arity"], allowed_arity)
                self.assertIn(option["value_domain"], allowed_domain)

    def test_known_option_shape_examples(self) -> None:
        export_options = self.commands_by_name["export"]["options"]
        dash_option = next((opt for opt in export_options if opt.get("option_key") == "-d"), None)
        self.assertIsNotNone(dash_option)
        self.assertEqual(dash_option["value_arity"], "0")
        self.assertEqual(dash_option["value_domain"], "none")

        select_options = self.commands_by_name["select"]["options"]
        recursive = next((opt for opt in select_options if opt.get("option_key") == "-recursive"), None)
        self.assertIsNotNone(recursive)
        self.assertEqual(recursive["value_arity"], "1")
        self.assertEqual(recursive["value_domain"], "enum")

    def test_survey_options_include_title_family(self) -> None:
        survey_options = self.commands_by_name["survey"]["options"]
        keys = {option.get("option_key") for option in survey_options}
        self.assertIn("-namespace", keys)
        self.assertIn("-declination", keys)
        self.assertIn("-person-rename", keys)
        self.assertIn("-title", keys)
        self.assertIn("-entrance", keys)

    def test_line_options_include_subtype(self) -> None:
        line_options = self.commands_by_name["line"]["options"]
        keys = {option.get("option_key") for option in line_options}
        self.assertIn("-subtype", keys)
        self.assertNotIn("-line", keys)

    def test_area_comopt_prose_is_not_parsed_as_option(self) -> None:
        area_options = self.commands_by_name["area"]["options"]
        keys = {option.get("option_key") for option in area_options}
        self.assertNotIn("-the", keys)
        prose = "the data lines consist of border line references (IDs)"
        self.assertFalse(any(option.get("raw", "").lower() == prose for option in area_options))

    def test_map_comopt_prose_is_not_parsed_as_option(self) -> None:
        map_options = self.commands_by_name["map"]["options"]
        keys = {option.get("option_key") for option in map_options}
        self.assertNotIn("-the", keys)
        self.assertNotIn("-if", keys)
        self.assertNotIn("-scraps", keys)
        self.assertIn("-preview", keys)

    def test_pipe_signatures_without_equals_are_split_from_descriptions(self) -> None:
        join = self.commands_by_name["join"]
        argument = join["arguments"][0]
        self.assertEqual(argument["signature"], "<pointX>")
        self.assertEqual(argument["name"], "<pointX>")
        self.assertEqual(argument["value_arity"], "N")
        self.assertIn("can be an ID of a point or line symbol", argument["description"])
        self.assertNotIn("can be an ID", argument["signature"])

        smooth_option = next((opt for opt in join["options"] if opt.get("option_key") == "-smooth"), None)
        self.assertIsNotNone(smooth_option)
        self.assertEqual(smooth_option["signature"], "smooth <on/off>")
        self.assertEqual(smooth_option["name"], "smooth <on/off>")
        self.assertIn("indicates whether two lines are to be connected smoothly", smooth_option["description"])
        self.assertNotIn("indicates whether", smooth_option["signature"])

        map_options = self.commands_by_name["map"]["options"]
        preview_option = next((opt for opt in map_options if opt.get("option_key") == "-preview"), None)
        self.assertIsNotNone(preview_option)
        self.assertEqual(preview_option["signature"], "preview <above/below> <other map id>")
        self.assertIn("will put the outline of the other map", preview_option["description"])

    def test_list_positional_arguments_are_variadic(self) -> None:
        equate = self.commands_by_name["equate"]
        self.assertEqual(equate["arguments"][0]["signature"], "<station list>")
        self.assertEqual(equate["arguments"][0]["value_arity"], "N")

    def test_centreline_inline_commands_are_structured(self) -> None:
        centreline = self.commands_by_name["centreline"]
        inline_commands = centreline.get("inline_commands", [])
        self.assertIn("data", inline_commands)
        self.assertIn("date", inline_commands)
        self.assertIn("team", inline_commands)
        self.assertNotIn("what", inline_commands)
        self.assertNotIn("x", inline_commands)
        self.assertNotIn("y", inline_commands)
        self.assertNotIn("z", inline_commands)
        self.assertNotIn("zero", inline_commands)

    def test_centerline_data_command_values_are_available(self) -> None:
        data = self.commands_by_name["data"]
        self.assertIn("centerline", data.get("contexts", []))
        values = set(data.get("allowed_values", []))
        self.assertIn("normal", values)
        self.assertIn("from", values)
        self.assertIn("to", values)
        self.assertIn("length", values)
        self.assertIn("compass", values)
        self.assertIn("clino", values)
        self.assertIn("backlength", values)
        self.assertIn("backcompass", values)
        self.assertIn("backclino", values)
        arguments = data.get("arguments", [])
        self.assertGreaterEqual(len(arguments), 2)
        style_values = set(arguments[0].get("allowed_values", []))
        reading_values = set(arguments[1].get("allowed_values", []))
        self.assertIn("normal", style_values)
        self.assertIn("topofil", style_values)
        self.assertIn("from", reading_values)
        self.assertIn("to", reading_values)
        self.assertIn("length", reading_values)
        self.assertIn("compass", reading_values)

    def test_centerline_inline_commands_have_command_entries(self) -> None:
        centreline = self.commands_by_name["centreline"]
        inline_commands = centreline.get("inline_commands", [])

        expected_inline_entries = {
            "date",
            "explo-date",
            "team",
            "explo-team",
            "instrument",
            "infer",
            "declination",
            "grid-angle",
            "sd",
            "units",
            "calibrate",
            "break",
            "mark",
            "flags",
            "station",
            "fix",
            "group",
            "endgroup",
            "walls",
            "vthreshold",
            "extend",
            "station-names",
            "data",
        }
        for name in expected_inline_entries:
            self.assertIn(name, inline_commands)
            self.assertIn(name, self.commands_by_name)
            self.assertIn("centerline", self.commands_by_name[name].get("contexts", []))

        infer_values = set(self.commands_by_name["infer"].get("allowed_values", []))
        self.assertIn("on", infer_values)
        self.assertIn("off", infer_values)

        walls_values = set(self.commands_by_name["walls"].get("allowed_values", []))
        self.assertIn("auto", walls_values)
        self.assertIn("on", walls_values)
        self.assertIn("off", walls_values)

        mark_values = set(self.commands_by_name["mark"].get("allowed_values", []))
        self.assertIn("fixed", mark_values)
        self.assertIn("painted", mark_values)
        self.assertIn("temporary", mark_values)

        flags_values = set(self.commands_by_name["flags"].get("allowed_values", []))
        self.assertIn("surface", flags_values)
        self.assertIn("duplicate", flags_values)
        self.assertIn("splay", flags_values)
        self.assertNotIn("data", flags_values)

    def test_map_body_break_is_available_in_map_context(self) -> None:
        break_command = self.commands_by_name["break"]
        self.assertIn("centerline", break_command.get("contexts", []))
        self.assertIn("map", break_command.get("contexts", []))

    def test_layout_settings_have_layout_command_entries(self) -> None:
        layout = self.commands_by_name["layout"]
        self.assertEqual(layout["arguments"][0]["signature"], "<id>")
        self.assertEqual(layout["arguments"][0]["value_arity"], "1")

        expected_settings = {
            "copy",
            "cs",
            "debug",
            "doc-author",
            "doc-keywords",
            "doc-subject",
            "doc-title",
            "legend-columns",
            "legend-width",
            "map-header",
            "symbol-assign",
            "symbol-hide",
        }
        missing = sorted(expected_settings - set(self.commands_by_name.keys()))
        self.assertFalse(missing, f"Missing generated layout setting commands: {missing}")

        for name in expected_settings:
            command = self.commands_by_name[name]
            self.assertIn("layout", command.get("contexts", []), name)
            self.assertIn("thconfig", command.get("document_types", []), name)

        map_header = self.commands_by_name["map-header"]
        self.assertEqual(map_header.get("allowed_values", []), [])
        self.assertEqual(map_header["arguments"][0].get("allowed_values", []), [])
        self.assertEqual(map_header["arguments"][1].get("allowed_values", []), [])
        self.assertEqual(
            map_header["arguments"][2].get("allowed_values", []),
            ["off", "n", "s", "e", "w", "ne", "nw", "se", "sw", "center"],
        )

        debug_values = self.commands_by_name["debug"]["arguments"][0].get("allowed_values", [])
        self.assertEqual(debug_values, ["on", "off", "all", "first", "second", "scrap-names", "station-names"])

        colour_model = self.commands_by_name["colour-model"]
        self.assertIn("color-model", colour_model.get("aliases", []))

    def test_symbol_type_subtype_matrix_presence(self) -> None:
        point = self.commands_by_name["point"]
        line = self.commands_by_name["line"]
        area = self.commands_by_name["area"]

        point_types = set(point.get("type_values", []))
        self.assertIn("station", point_types)
        self.assertIn("air-draught", point_types)

        line_types = set(line.get("type_values", []))
        self.assertIn("wall", line_types)
        self.assertIn("survey", line_types)

        area_types = set(area.get("type_values", []))
        self.assertIn("water", area_types)
        self.assertIn("u", area_types)

        point_subtypes = point.get("subtype_by_type", {})
        self.assertIn("fixed", point_subtypes.get("station", []))
        self.assertIn("winter", point_subtypes.get("air-draught", []))

        line_subtypes = line.get("subtype_by_type", {})
        self.assertIn("bedrock", line_subtypes.get("wall", []))
        self.assertIn("presumed", line_subtypes.get("wall", []))
        self.assertIn("surface", line_subtypes.get("survey", []))

        area_subtypes = area.get("subtype_by_type", {})
        self.assertIn("*", area_subtypes.get("u", []))

    def test_survey_allowed_values_are_not_polluted(self) -> None:
        survey_values = self.commands_by_name["survey"].get("allowed_values", [])
        self.assertNotIn("on", survey_values)
        self.assertNotIn("off", survey_values)
        self.assertNotIn("[]", survey_values)
        self.assertNotIn("-", survey_values)
        self.assertNotIn("cave-list", survey_values)

    def test_survey_summary_tex_artifacts_are_stripped(self) -> None:
        summary = self.commands_by_name["survey"].get("summary", "")
        self.assertNotIn("%", summary)
        self.assertNotIn("~", summary)
        self.assertNotIn("\\[", summary)
        self.assertNotIn("\\]", summary)
        self.assertNotIn("\\Nobreak", summary)
        self.assertNotIn("\\it", summary)

    def test_revise_contains_object_id_argument_and_object_options(self) -> None:
        revise = self.commands_by_name["revise"]
        arguments = revise.get("arguments", [])
        self.assertGreaterEqual(len(arguments), 1)
        self.assertEqual(arguments[0].get("signature"), "<id>")

        option_keys = {option.get("option_key") for option in revise.get("options", [])}
        self.assertIn("-stations", option_keys)


if __name__ == "__main__":
    unittest.main()
