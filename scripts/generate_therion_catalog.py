#!/usr/bin/env python3
"""Generate Therion command catalog JSON from thbook TeX sources.

Current parser scope:
- therion/thbook/ch02.tex
- therion/thbook/ch03.tex

The parser is intentionally conservative: it extracts section-level command
metadata and preserves textual details for downstream tooling.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
from pathlib import Path
from typing import Any


SECTION_RE = re.compile(r"^\\subsubchapter\s+`([^'\n]+)'\.?\s*$", re.MULTILINE)
PIPE_TOKEN_RE = re.compile(r"\|([^|]+)\|")
NEW_TAG_RE = re.compile(r"\\NEW\{[^}]*\}")
BRACKET_NOTE_RE = re.compile(r"\\\[[^\]]*\\\]")
TEX_CMD_RE = re.compile(r"\\[a-zA-Z]+\*?(?:\{[^}]*\})?")
DESCRIPTION_LEAD_RE = re.compile(
    r"\s+(is|are|can|indicates|determines?|specif(?:y|ies)|will|must|should|allows?|contains?|defines?|sets?|gives?|means?)\b",
    re.IGNORECASE,
)
SIGNATURE_MARKER_RE = re.compile(r"<[^>]+>|\[[^\]]+\]")
TRAILING_SIGNATURE_CONNECTOR_RE = re.compile(r"\s+(?:and|or)$", re.IGNORECASE)
CONTEXT_ORDER = [
    "none",
    "all",
    "survey",
    "scrap",
    "centerline",
    "map",
    "surface",
    "layout",
    "lookup",
]
PROCESSING_DATA_TOP_LEVEL_DIRECTIVES = {
    "cs",
    "export",
    "language",
    "layout",
    "log",
    "lookup",
    "maps",
    "maps-offset",
    "select",
    "setup3d",
    "sketch-colors",
    "sketch-warp",
    "source",
    "system",
    "text",
    "unselect",
}
CENTERLINE_INLINE_COMMAND_NAMES = {
    "date",
    "explo-date",
    "team",
    "explo-team",
    "instrument",
    "infer",
    "declination",
    "grid-angle",
    "sd",
    "grade",
    "units",
    "calibrate",
    "break",
    "mark",
    "flags",
    "station",
    "cs",
    "fix",
    "equate",
    "data",
    "group",
    "endgroup",
    "walls",
    "vthreshold",
    "extend",
    "station-names",
}
MAP_BODY_INLINE_COMMAND_NAMES = {
    "break",
    "preview",
}
TH2_MAP_OBJECT_DIRECTIVES = {
    "area",
    "line",
    "point",
    "scrap",
}
ALL_DOCUMENT_TYPES = ["all"]
THERION_SOURCE_DOCUMENT_TYPES = ["th"]
THERION_MAP_DOCUMENT_TYPES = ["th2"]
THERION_CONFIG_DOCUMENT_TYPES = ["thconfig"]
COMMAND_DOCUMENT_TYPE_OVERRIDES = {
    "join": ["th", "th2"],
}
COMMAND_CONTEXT_OVERRIDES = {
    "cs": ["layout"],
}


def normalize_whitespace(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def normalize_directive_token(value: str) -> str:
    normalized = normalize_whitespace(value).lower()
    if normalized == "centreline":
        return "centerline"
    if normalized == "endcentreline":
        return "endcenterline"
    return normalized


def append_unique(items: list[str], value: str) -> None:
    normalized = normalize_whitespace(value).lower()
    if not normalized or normalized in items:
        return
    items.append(normalized)


def symbol_keyword_from_token(token: str) -> str:
    normalized = normalize_whitespace(token).lower()
    if not re.fullmatch(r"[a-z][a-z0-9-]*", normalized):
        return ""
    return normalized


def extract_signature_core(signature: str) -> str:
    pipe_tokens = [normalize_whitespace(token) for token in PIPE_TOKEN_RE.findall(signature)]
    if pipe_tokens:
        return pipe_tokens[0]
    return normalize_whitespace(signature)


def infer_block_closing_directive(command_directive: str, syntax_rows: list[str]) -> str:
    if not command_directive or command_directive.startswith("end"):
        return ""

    candidate_end_tokens: list[str] = []
    for syntax_row in syntax_rows:
        lowered_row = normalize_whitespace(syntax_row).lower()
        for match in re.findall(r"\bend[a-z][a-z0-9-]*\b", lowered_row):
            candidate = normalize_directive_token(match)
            if candidate and candidate not in candidate_end_tokens:
                candidate_end_tokens.append(candidate)

    if not candidate_end_tokens:
        return ""

    expected_candidate = f"end{command_directive}"
    if expected_candidate in candidate_end_tokens:
        return expected_candidate

    for candidate in candidate_end_tokens:
        opening_candidate = normalize_directive_token(candidate[3:]) if candidate.startswith("end") else ""
        if opening_candidate == command_directive:
            return candidate

    if len(candidate_end_tokens) == 1:
        return candidate_end_tokens[0]
    return ""


def extract_option_key(signature_core: str) -> str:
    if not signature_core:
        return ""
    tokens = [token.strip().split(",")[0] for token in signature_core.split()]
    first_token = tokens[0] if tokens else ""
    if not first_token:
        return ""
    first_alias = first_token.split("/")[0].strip()
    if not first_alias:
        return ""
    if first_alias.startswith("<") or first_alias.startswith("["):
        return ""
    normalized = first_alias.lower().replace("[", "").replace("]", "")
    if normalized.startswith("-"):
        normalized = normalized.lstrip("-")

    if not re.fullmatch(r"[a-z][a-z0-9-]*", normalized):
        for token in tokens[1:]:
            candidate = token.split("/")[0].strip().lower().replace("[", "").replace("]", "")
            if "-" not in candidate:
                continue
            if re.fullmatch(r"[a-z][a-z0-9-]*", candidate):
                return f"-{candidate}"
        return ""
    return f"-{normalized}"


def infer_value_arity(signature_core: str, raw_signature: str) -> str:
    if not signature_core:
        return "0"

    core = signature_core
    parts = core.split(maxsplit=1)
    payload = parts[1] if len(parts) > 1 else ""
    payload = payload.strip()
    if not payload:
        return "0"

    if "..." in payload or "..." in raw_signature:
        return "N"

    placeholder_count = len(re.findall(r"<[^>]+>", payload))
    bracket_count = len(re.findall(r"\[[^\]]+\]", payload))
    token_count = len(payload.split())
    if placeholder_count + bracket_count > 1:
        return "N"
    if placeholder_count + bracket_count == 1:
        return "1"
    if token_count > 1:
        return "N"
    return "1"


def infer_positional_value_arity(signature_core: str, raw_signature: str) -> str:
    if not signature_core:
        return "0"
    if "..." in signature_core or "..." in raw_signature:
        return "N"
    if re.search(r"<[^>]*\blist\b[^>]*>", signature_core, re.IGNORECASE):
        return "N"

    marker_count = len(SIGNATURE_MARKER_RE.findall(signature_core))
    if marker_count > 1:
        return "N"
    if marker_count == 1:
        return "1"

    return infer_value_arity(signature_core, raw_signature)


def infer_value_domain(signature_core: str, raw_signature: str, allowed_values: list[str], value_arity: str) -> str:
    if value_arity == "0":
        return "none"

    if allowed_values:
        return "enum"

    text = f"{signature_core} {raw_signature}".lower()
    if "<on/off>" in text or "on/off" in text:
        return "enum"
    if "<number>" in text or "<n>" in text or "<num>" in text or "<value>" in text:
        return "number"
    if "<date>" in text or "date" in text:
        return "date"
    if "<encoding" in text or "encoding" in text:
        return "encoding"
    if "<file" in text or "file-name" in text or "filename" in text or "directory" in text or "path" in text:
        return "path"
    if "<string>" in text or "string" in text or "text" in text:
        return "string"
    if "station" in text:
        return "station"
    if "keyword" in text:
        return "keyword"
    if "projection" in text:
        return "projection"
    return "unknown"


def infer_allowed_values(signature_core: str, raw_signature: str) -> list[str]:
    allowed_values: list[str] = []
    text = f"{signature_core} {raw_signature}"

    # Enumerations commonly appear as <a/b/c>.
    for match in re.findall(r"<([^>]+)>", text):
        candidate = normalize_whitespace(match)
        if "/" not in candidate:
            continue
        parts = [part.strip() for part in candidate.split("/") if part.strip()]
        for part in parts:
            if not re.fullmatch(r"[A-Za-z0-9_.:-]+", part):
                continue
            if part not in allowed_values:
                allowed_values.append(part)

    # Some options use syntax like (none)/horizontal/vertical.
    for match in re.findall(r"\(([^)]+)\)((?:/[A-Za-z0-9_.:-]+)+)", text):
        first_token = match[0].strip()
        tail_tokens = [token.strip() for token in match[1].split("/") if token.strip()]
        candidates = [first_token, *tail_tokens]
        for part in candidates:
            if not re.fullmatch(r"[A-Za-z0-9_.:-]+", part):
                continue
            if part not in allowed_values:
                allowed_values.append(part)

    if "on" in allowed_values and "off" in allowed_values:
        # Keep deterministic order for common boolean enum.
        allowed_values = [value for value in allowed_values if value not in {"on", "off"}]
        allowed_values = ["on", "off", *allowed_values]

    return allowed_values


def clean_tex_text(value: str) -> str:
    stripped = value.replace("\r\n", "\n")
    stripped = NEW_TAG_RE.sub("", stripped)
    # Strip TeX comments while preserving escaped percent signs.
    stripped = re.sub(r"(?<!\\)%[^\n]*", "", stripped)
    # Convert \[...\] and \[...] note wrappers to plain text content.
    stripped = re.sub(r"\\\[\s*([^\]]*?)\s*\\\]", r" (\1) ", stripped, flags=re.DOTALL)
    stripped = re.sub(r"\\\[\s*([^\]]*?)\s*\]", r" (\1) ", stripped, flags=re.DOTALL)
    stripped = BRACKET_NOTE_RE.sub("", stripped)
    stripped = stripped.replace("\\hfill\\break", "\n")
    stripped = stripped.replace("\\qquad", " ")
    stripped = stripped.replace("$", " ")
    stripped = stripped.replace("``", "\"")
    stripped = stripped.replace("''", "\"")
    stripped = re.sub(r"`([^`']+)'", r'"\1"', stripped)
    stripped = re.sub(r"\\([#$%&_{}])", r"\1", stripped)
    stripped = re.sub(r"\\[,;:!]", " ", stripped)
    stripped = stripped.replace("~", " ")
    stripped = stripped.replace("---", " - ")
    stripped = PIPE_TOKEN_RE.sub(lambda match: match.group(1), stripped)
    stripped = TEX_CMD_RE.sub(" ", stripped)
    stripped = stripped.replace("\\\\", " ")
    # Plain TeX terminates a macro name with a control space, as in "\MP\ variable"
    # or "\TeX\ sections". TEX_CMD_RE removes the macro itself but leaves that
    # trailing backslash behind, so drop any backslash left before whitespace.
    stripped = re.sub(r"\\(?=\s|$)", " ", stripped)
    stripped = stripped.replace("|", " ")
    stripped = stripped.replace("{", " ").replace("}", " ")
    stripped = stripped.replace("with - print-encodings", "with --print-encodings")
    return normalize_whitespace(stripped)


def normalize_catalog_text_fields(catalog: dict[str, Any]) -> dict[str, Any]:
    commands = catalog.get("commands", [])
    if not isinstance(commands, list):
        return catalog

    for command in commands:
        if not isinstance(command, dict):
            continue
        command["summary"] = clean_tex_text(str(command.get("summary", "")))

        syntax_rows = command.get("syntax", [])
        if isinstance(syntax_rows, list):
            normalized_syntax_rows = [clean_tex_text(str(row)) for row in syntax_rows]
            command["syntax"] = [row for row in normalized_syntax_rows if row]

        for field_name in ("arguments", "options"):
            entries = command.get(field_name, [])
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                for text_key in ("raw", "name", "signature", "description"):
                    entry[text_key] = clean_tex_text(str(entry.get(text_key, "")))

                dependencies = entry.get("dependencies", [])
                if isinstance(dependencies, list):
                    normalized_dependencies = [clean_tex_text(str(dep)) for dep in dependencies]
                    entry["dependencies"] = [dep for dep in normalized_dependencies if dep]

    return catalog


def item_starts_with_pipe_signature(raw_item: str) -> bool:
    # Most structured command/option signatures in thbook use |...| wrappers.
    # Keep support for leading \NEW{...} annotation before the signature token.
    if re.match(r"^\s*(?:\\NEW\{[^}]*\}\s*)*\|", raw_item) is not None:
        return True

    stripped = raw_item.lstrip()
    # Some entries start with TeX formatting wrappers (e.g. {\rightskip ...})
    # before the first |signature| token.
    if not stripped.startswith(("{", "\\")):
        return False

    first_pipe_index = stripped.find("|")
    if first_pipe_index < 0:
        return False

    equals_index = stripped.find("=")
    if equals_index >= 0 and first_pipe_index > equals_index:
        return False

    return True


def strip_trailing_signature_connector(signature: str) -> str:
    normalized = normalize_whitespace(signature)
    if SIGNATURE_MARKER_RE.search(normalized):
        normalized = TRAILING_SIGNATURE_CONNECTOR_RE.sub("", normalized).strip()
    return normalized


def split_item_signature_description(item: str, raw_item: str) -> tuple[str, str]:
    if "=" in item:
        lhs, rhs = item.split("=", 1)
        return strip_trailing_signature_connector(clean_tex_text(lhs)), clean_tex_text(rhs)

    lhs = clean_tex_text(item)
    if item_starts_with_pipe_signature(raw_item) or SIGNATURE_MARKER_RE.search(lhs):
        match = DESCRIPTION_LEAD_RE.search(lhs)
        if match:
            signature = strip_trailing_signature_connector(lhs[: match.start()])
            description = lhs[match.start(1) :].strip()
            if signature:
                return signature, description

    return strip_trailing_signature_connector(lhs), ""


def parse_item_block(
    block_text: str,
    require_leading_pipe_signature: bool = False,
    infer_option_key: bool = True,
) -> list[dict[str, Any]]:
    items: list[str] = []
    current: list[str] = []
    for raw_line in block_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("*"):
            if current:
                items.append(" ".join(current))
            current = [line[1:].strip()]
        else:
            if current:
                current.append(line)
    if current:
        items.append(" ".join(current))

    parsed_items: list[dict[str, Any]] = []
    for raw_item in items:
        if require_leading_pipe_signature and not item_starts_with_pipe_signature(raw_item):
            continue
        item = clean_tex_text(raw_item)
        if not item:
            continue
        lhs, rhs = split_item_signature_description(item, raw_item)

        signature_core = extract_signature_core(lhs)
        name = signature_core if signature_core else lhs
        allowed_values = infer_allowed_values(signature_core, lhs)

        option_key = extract_option_key(signature_core) if infer_option_key else ""
        dependencies = []
        if option_key:
            lowered_rhs = rhs.lower()
            if "valid only" in lowered_rhs or "only when" in lowered_rhs or "inside" in lowered_rhs:
                dependencies.append(rhs)

        value_arity = infer_value_arity(signature_core, lhs) if option_key else infer_positional_value_arity(signature_core, lhs)
        value_domain = infer_value_domain(signature_core, lhs, allowed_values, value_arity)

        parsed_items.append(
            {
                "raw": item,
                "name": name,
                "signature": lhs,
                "option_key": option_key,
                "value_arity": value_arity,
                "value_domain": value_domain,
                "description": rhs,
                "allowed_values": allowed_values,
                "dependencies": dependencies,
            }
        )
    return parsed_items


def extract_block(section_text: str, tag: str) -> list[str]:
    pattern = re.compile(rf"\\{tag}\b(.*?)(?:\\end{tag})", re.DOTALL)
    return [match.group(1).strip() for match in pattern.finditer(section_text)]


def extract_contexts(section_text: str) -> list[str]:
    contexts: list[str] = []
    for block in extract_block(section_text, "context"):
        if not block:
            continue

        normalized_block = clean_tex_text(block).lower()
        if not normalized_block:
            continue

        if "very first command" in normalized_block or "first command in the file" in normalized_block:
            contexts.append("none")
        if re.search(r"\bnone\b", normalized_block):
            contexts.append("none")

        if re.search(r"\ball\b", normalized_block):
            contexts.append("all")
        if re.search(r"\bsurvey\b", normalized_block):
            contexts.append("survey")
        if re.search(r"\bscrap\b", normalized_block):
            contexts.append("scrap")
        if re.search(r"\bcentreline\b|\bcenterline\b", normalized_block):
            contexts.append("centerline")
        if re.search(r"\bmap\b", normalized_block):
            contexts.append("map")
        if re.search(r"\bsurface\b", normalized_block):
            contexts.append("surface")
        if re.search(r"\blayout\b", normalized_block):
            contexts.append("layout")
        if re.search(r"\blookup\b", normalized_block):
            contexts.append("lookup")

    deduped: list[str] = []
    for context_name in CONTEXT_ORDER:
        if context_name in contexts:
            deduped.append(context_name)
    return deduped


def infer_document_types(source_file: str, command_directive: str) -> list[str]:
    if command_directive == "encoding":
        return ALL_DOCUMENT_TYPES.copy()
    if command_directive in COMMAND_DOCUMENT_TYPE_OVERRIDES:
        return COMMAND_DOCUMENT_TYPE_OVERRIDES[command_directive].copy()

    source_name = Path(source_file).name.lower()
    if source_name == "ch03.tex":
        return THERION_CONFIG_DOCUMENT_TYPES.copy()
    if command_directive in TH2_MAP_OBJECT_DIRECTIVES:
        return THERION_MAP_DOCUMENT_TYPES.copy()
    if source_name == "ch02.tex":
        return THERION_SOURCE_DOCUMENT_TYPES.copy()
    return []


def extract_section_description(section_text: str) -> str:
    description_blocks = extract_block(section_text, "description")
    if description_blocks:
        return clean_tex_text(description_blocks[0])

    first_macro = re.search(r"\\(syntax|arguments|options|context)\b", section_text)
    prose = section_text[: first_macro.start()] if first_macro else section_text
    prose = re.sub(r"\\subsubchapter\s+`[^`]+'\.?", "", prose)
    return clean_tex_text(prose)


def extract_symbol_type_values(arguments: list[dict[str, Any]]) -> list[str]:
    type_values: list[str] = []
    type_argument = None
    for item in arguments:
        if not isinstance(item, dict):
            continue
        candidate_text = " ".join(
            str(item.get(field_name, ""))
            for field_name in ("name", "signature", "raw", "description")
        ).lower()
        if "<type>" in candidate_text:
            type_argument = item
            break
    if type_argument is None:
        return type_values

    type_text = f"{type_argument.get('signature', '')} {type_argument.get('description', '')}"
    type_text = NEW_TAG_RE.sub("", type_text)
    type_text = re.sub(r"(?<!\\)%[^\n]*", "", type_text)
    type_text = re.sub(r"\\\[\s*([^\]]*?)\s*\\\]", " ", type_text, flags=re.DOTALL)
    type_text = re.sub(r"\\\[\s*([^\]]*?)\s*\]", " ", type_text, flags=re.DOTALL)
    type_text = BRACKET_NOTE_RE.sub(" ", type_text)
    for token in PIPE_TOKEN_RE.findall(type_text):
        keyword = symbol_keyword_from_token(token)
        if not keyword:
            continue
        append_unique(type_values, keyword)

    # Newer parsed argument payloads can be prose-only (without |keyword| tokens).
    # Recover symbol type lists from "one of following:" / "supported:" style text.
    prose = clean_tex_text(type_text).lower()
    list_tail = ""
    for marker in (
        "one of following:",
        "following types are supported:",
        "the following types are supported:",
        "types are supported:",
        "following types:",
    ):
        marker_index = prose.find(marker)
        if marker_index >= 0:
            list_tail = prose[marker_index + len(marker):]
            break
    if not list_tail:
        return type_values

    # Remove explanatory parenthetical prose and cut common prose tails that
    # follow the enumerated symbol list.
    list_tail = re.sub(r"\([^)]*\)", " ", list_tail)
    for stop_marker in (
        "the subtype may be specified",
        "the following subtypes",
        "any subtype specification",
        "type-specific options",
    ):
        stop_index = list_tail.find(stop_marker)
        if stop_index >= 0:
            list_tail = list_tail[:stop_index]
            break
    segments = [segment.strip() for segment in list_tail.split(";") if segment.strip()]
    stop_words = {
        "the",
        "type",
        "types",
        "following",
        "supported",
        "is",
        "are",
        "for",
        "and",
        "or",
        "of",
        "use",
        "see",
    }
    for segment in segments:
        if ":" in segment:
            segment = segment.split(":", 1)[1].strip()
        for item in segment.split(","):
            token = item.strip().strip(".")
            if not token:
                continue
            first_word = token.split()[0].strip()
            keyword = symbol_keyword_from_token(first_word)
            if not keyword or keyword in stop_words:
                continue
            append_unique(type_values, keyword)
    return type_values


def extract_symbol_subtype_matrix(section_text: str) -> dict[str, list[str]]:
    subtype_matrix: dict[str, list[str]] = {}
    subtype_anchor = re.search(r"\|\s*subtype\s*<restr_keyword>\s*\|", section_text, re.IGNORECASE)
    if subtype_anchor is None:
        return subtype_matrix

    tail = section_text[subtype_anchor.start():]
    italic_groups = re.finditer(r"\{\\it\s+([^:}]+):\}\s*(.*?);", tail, re.DOTALL)
    for group in italic_groups:
        type_name = symbol_keyword_from_token(group.group(1))
        if not type_name:
            continue

        subtype_values: list[str] = subtype_matrix.get(type_name, [])
        for token in PIPE_TOKEN_RE.findall(group.group(2)):
            keyword = symbol_keyword_from_token(token)
            if not keyword:
                continue
            append_unique(subtype_values, keyword)
        if subtype_values:
            subtype_matrix[type_name] = subtype_values

    return subtype_matrix


def extract_centerline_data_value_groups(description: str) -> tuple[list[str], list[str]]:
    style_values: list[str] = []
    reading_values: list[str] = []
    if not description:
        return style_values, reading_values

    style_match = re.search(r"style\s*\(([^)]+)\)", description, re.IGNORECASE)
    if style_match:
        for token in style_match.group(1).split(","):
            keyword = symbol_keyword_from_token(token)
            if keyword:
                append_unique(style_values, keyword)

    readings_match = re.search(
        r"keywords:\s*(.*?)(?:\.\s*See\s+Survex\s+manual|\.\s*For\s+interleaved\s+data|$)",
        description,
        re.IGNORECASE | re.DOTALL,
    )
    if not readings_match:
        return style_values, reading_values

    readings_text = readings_match.group(1)

    # Expand [back]token forms into both forward and backward variants
    # (e.g. [back]tape -> tape, backtape) before generic token splitting.
    back_token_pattern = re.compile(r"\[back\]\s*([a-z][a-z0-9-]*)", re.IGNORECASE)
    for match in back_token_pattern.finditer(readings_text):
        base = symbol_keyword_from_token(match.group(1))
        if not base:
            continue
        append_unique(reading_values, base)
        append_unique(reading_values, f"back{base}")

    readings_text = re.sub(r"\\\[\s*dimension.*?shot\]", " ", readings_text, flags=re.IGNORECASE | re.DOTALL)
    readings_text = re.sub(r"\\[a-zA-Z]+(?:\d+)?(?:\{[^}]*\})?", " ", readings_text)
    readings_text = readings_text.replace("[back]", "back")
    readings_text = readings_text.replace("/", ",")
    readings_text = readings_text.replace("\\", " ")

    for token in readings_text.split(","):
        normalized = normalize_whitespace(token).lower()
        if not normalized:
            continue
        normalized = normalized.strip("`'\"|.:-[]()")
        normalized = re.sub(r"^\d+", "", normalized)
        keyword = symbol_keyword_from_token(normalized)
        if keyword:
            append_unique(reading_values, keyword)

    return style_values, reading_values


def extract_centerline_data_allowed_values(description: str) -> list[str]:
    style_values, reading_values = extract_centerline_data_value_groups(description)
    allowed_values: list[str] = []
    for keyword in style_values:
        append_unique(allowed_values, keyword)
    for keyword in reading_values:
        append_unique(allowed_values, keyword)
    return allowed_values


def extract_supported_flag_keywords(description: str, skip: set[str]) -> list[str]:
    keywords: list[str] = []
    for match in re.finditer(r"supported flags?(?: are)?:\s*([^.]*)", description, re.IGNORECASE):
        clause = match.group(1)
        candidate_tokens = PIPE_TOKEN_RE.findall(clause)
        if not candidate_tokens:
            candidate_tokens = [
                re.sub(r"^and\s+", "", normalize_whitespace(re.sub(r"\([^)]*\)", "", token)).lower())
                for token in clause.split(",")
            ]
        for token in candidate_tokens:
            keyword = symbol_keyword_from_token(token)
            if not keyword or keyword in skip:
                continue
            append_unique(keywords, keyword)
    return keywords


def extract_one_of_keywords_from_description(description: str, skip: set[str]) -> list[str]:
    keywords: list[str] = []
    match = re.search(r"is one of:\s*([^.]*)", description, re.IGNORECASE)
    if not match:
        return keywords

    candidate = match.group(1).replace(" and ", ",")
    for token in candidate.split(","):
        normalized = normalize_whitespace(re.sub(r"\([^)]*\)", "", token)).lower()
        keyword = symbol_keyword_from_token(normalized)
        if not keyword or keyword in skip:
            continue
        append_unique(keywords, keyword)
    return keywords


def extract_centerline_inline_allowed_values(inline_command_name: str, description: str, seed_values: list[str]) -> list[str]:
    allowed_values: list[str] = []
    for token in seed_values:
        append_unique(allowed_values, token)

    if inline_command_name == "data":
        style_values, reading_values = extract_centerline_data_value_groups(description)
        for token in style_values:
            append_unique(allowed_values, token)
        for token in reading_values:
            append_unique(allowed_values, token)
        return allowed_values

    skip = {inline_command_name, *CENTERLINE_INLINE_COMMAND_NAMES}
    for token in extract_one_of_keywords_from_description(description, skip):
        append_unique(allowed_values, token)
    for token in extract_supported_flag_keywords(description, skip):
        append_unique(allowed_values, token)
    return allowed_values


def build_argument_entries_from_signature(signature_core: str) -> list[dict[str, Any]]:
    arguments: list[dict[str, Any]] = []
    if not signature_core:
        return arguments

    parts = signature_core.split(maxsplit=1)
    payload = parts[1] if len(parts) > 1 else ""
    payload = payload.strip()
    if not payload:
        return arguments

    for match in re.finditer(r"\[[^\]]+\]|<[^>]+>", payload):
        signature = normalize_whitespace(match.group(0))
        if not signature:
            continue
        allowed_values = infer_allowed_values(signature, signature)
        value_arity = infer_positional_value_arity(signature, signature)
        arguments.append(
            {
                "raw": signature,
                "name": signature,
                "signature": signature,
                "option_key": "",
                "value_arity": value_arity,
                "value_domain": "enum" if allowed_values else "keyword",
                "description": "",
                "allowed_values": allowed_values,
                "dependencies": [],
            }
        )
    return arguments


def strip_cpp_line_comments(text: str) -> str:
    return re.sub(r"//[^\n]*", "", text)


def parse_thstok_token_map(header_text: str, array_name: str) -> dict[str, str]:
    tokens_by_enum = parse_thstok_tokens_by_enum(header_text, array_name)
    return {enum_name: tokens[0] for enum_name, tokens in tokens_by_enum.items() if tokens}


def parse_thstok_tokens_by_enum(header_text: str, array_name: str) -> dict[str, list[str]]:
    cleaned = strip_cpp_line_comments(header_text)
    array_match = re.search(
        rf"static\s+const\s+thstok\s+{re.escape(array_name)}\[\]\s*=\s*\{{(?P<body>.*?)\n\}};",
        cleaned,
        re.DOTALL,
    )
    if array_match is None:
        return {}

    tokens_by_enum: dict[str, list[str]] = {}
    for match in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(TT_[A-Z0-9_]+)\s*\}', array_match.group("body")):
        tokens_by_enum.setdefault(match.group(2), []).append(match.group(1))
    return tokens_by_enum


def parse_option_invalid_type_tokens_from_source(
    source_text: str,
    option_case: str,
    token_by_enum: dict[str, str],
) -> list[str]:
    case_match = re.search(
        rf"\n    case\s+{re.escape(option_case)}\s*:(?P<body>.*?)(?:\n    case\s+TT_[A-Z0-9_]+\s*:|\n    default\s*:)",
        source_text,
        re.DOTALL,
    )
    if case_match is None:
        return []

    invalid_types: list[str] = []
    for match in re.finditer(
        r"case\s+(TT_[A-Z0-9_]+)\s*:\s*throw\s+thexception\s*\([^;]*not valid with type",
        case_match.group("body"),
        re.DOTALL,
    ):
        token = token_by_enum.get(match.group(1))
        if token:
            append_unique(invalid_types, token)
    return invalid_types


def apply_source_derived_option_applicability(catalog: dict[str, Any], source_root: Path) -> None:
    point_header = source_root / "thpoint.h"
    point_source = source_root / "thpoint.cxx"
    if not point_header.exists() or not point_source.exists():
        return

    point_header_text = point_header.read_text(encoding="utf-8", errors="replace")
    point_source_text = point_source.read_text(encoding="utf-8", errors="replace")
    point_type_tokens = parse_thstok_token_map(point_header_text, "thtt_point_types")
    point_option_tokens = parse_thstok_tokens_by_enum(point_header_text, "thtt_point_opt")
    if not point_type_tokens or not point_option_tokens:
        return

    excluded_types_by_option_key: dict[str, list[str]] = {}
    for option_case, option_tokens in point_option_tokens.items():
        excluded_types = parse_option_invalid_type_tokens_from_source(point_source_text, option_case, point_type_tokens)
        if not excluded_types:
            continue
        for option_token in option_tokens:
            excluded_types_by_option_key[f"-{option_token}"] = excluded_types
    if not excluded_types_by_option_key:
        return

    for command in catalog.get("commands", []):
        if command.get("directive") != "point":
            continue
        for option in command.get("options", []):
            excluded_types = excluded_types_by_option_key.get(option.get("option_key"))
            if not excluded_types:
                continue
            existing = option.get("excluded_types", [])
            if not isinstance(existing, list):
                existing = []
            for type_token in excluded_types:
                append_unique(existing, type_token)
            option["excluded_types"] = existing
        return


def parse_sections(tex_text: str, source_file: str, known_command_names: set[str] | None = None) -> list[dict[str, Any]]:
    matches = list(SECTION_RE.finditer(tex_text))
    results = []
    for index, match in enumerate(matches):
        command_name = normalize_whitespace(match.group(1))
        if "xtherion" in command_name.lower():
            continue
        command_directive = normalize_directive_token(command_name)

        start = match.start()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(tex_text)
        section_text = tex_text[start:end]

        syntax_blocks = [clean_tex_text(block) for block in extract_block(section_text, "syntax")]
        argument_blocks = extract_block(section_text, "arguments")
        option_blocks = extract_block(section_text, "options")
        comopt_blocks = extract_block(section_text, "comopt")
        contexts = extract_contexts(section_text)
        if not contexts and command_directive in PROCESSING_DATA_TOP_LEVEL_DIRECTIVES:
            contexts = ["none"]
        document_types = infer_document_types(source_file, command_directive)
        description = extract_section_description(section_text)

        arguments = []
        for block in argument_blocks:
            arguments.extend(parse_item_block(block, infer_option_key=False))
        options = []
        for block in option_blocks:
            options.extend(parse_item_block(block, require_leading_pipe_signature=True))
        comopt_items: list[dict[str, Any]] = []
        for block in comopt_blocks:
            comopt_items.extend(parse_item_block(block, require_leading_pipe_signature=True))

        normalized_command_name = command_name.strip().lower()
        if normalized_command_name not in {"centreline", "centerline"}:
            options.extend(comopt_items)
        deduped_options = []
        seen_option_signatures: set[str] = set()
        for option in options:
            signature = normalize_whitespace(option.get("signature", "")).lower()
            if not signature or signature in seen_option_signatures:
                continue
            seen_option_signatures.add(signature)
            deduped_options.append(option)
        options = deduped_options

        dependencies = []
        if contexts:
            dependencies.append(f"context: {', '.join(contexts)}")
        for opt in options:
            for dep in opt.get("dependencies", []):
                if dep not in dependencies:
                    dependencies.append(dep)

        # Command-level allowed values must describe command argument domains.
        # Option-level enum values stay attached to each option entry and must not
        # be merged into the parent command, otherwise identifier arguments (e.g.
        # survey <id>) get polluted with unrelated option enums like on/off.
        allowed_values: list[str] = []
        if normalized_command_name == "join":
            for argument in arguments:
                argument["value_arity"] = "N"
        if normalized_command_name == "layout" and not arguments:
            arguments = build_argument_entries_from_signature("layout <id>")

        result_entry = {
            "name": command_name,
            "directive": command_directive,
            "source": {
                "file": source_file,
                "line": tex_text.count("\n", 0, start) + 1,
            },
            "summary": description,
            "syntax": syntax_blocks,
            "contexts": contexts,
            "document_types": document_types,
            "arguments": arguments,
            "options": options,
            "dependencies": dependencies,
            "allowed_values": allowed_values,
        }

        if normalized_command_name in {"centreline", "centerline"}:
            inline_commands: list[str] = []
            for item in comopt_items:
                option_key = normalize_whitespace(item.get("option_key", ""))
                if not option_key.startswith("-"):
                    continue
                append_unique(inline_commands, option_key[1:])
            if inline_commands:
                result_entry["inline_commands"] = inline_commands

            for item in comopt_items:
                option_key = normalize_whitespace(item.get("option_key", ""))
                if not option_key.startswith("-"):
                    continue

                inline_command_name = option_key[1:].strip().lower()
                if not inline_command_name:
                    continue

                signature = normalize_whitespace(item.get("signature", ""))
                signature_core = extract_signature_core(signature)
                arguments_for_command = build_argument_entries_from_signature(signature_core)
                description_for_command = clean_tex_text(item.get("description", ""))
                if inline_command_name == "data":
                    style_values, reading_values = extract_centerline_data_value_groups(item.get("description", ""))
                    if arguments_for_command:
                        arguments_for_command[0]["allowed_values"] = style_values
                        arguments_for_command[0]["value_domain"] = "enum" if style_values else "keyword"
                        arguments_for_command[0]["value_arity"] = "1"
                    if len(arguments_for_command) >= 2:
                        arguments_for_command[1]["allowed_values"] = reading_values
                        arguments_for_command[1]["value_domain"] = "enum" if reading_values else "keyword"
                        arguments_for_command[1]["value_arity"] = "N"
                allowed_for_command = extract_centerline_inline_allowed_values(
                    inline_command_name,
                    item.get("description", ""),
                    list(item.get("allowed_values", [])),
                )

                command_line = tex_text.count("\n", 0, start) + 1
                signature_anchor = section_text.lower().find(signature.lower())
                if signature and signature_anchor >= 0:
                    command_line = tex_text.count("\n", 0, start + signature_anchor) + 1

                results.append(
                    {
                        "name": inline_command_name,
                        "directive": normalize_directive_token(inline_command_name),
                        "source": {
                            "file": source_file,
                            "line": command_line,
                        },
                        "summary": description_for_command,
                        "syntax": [signature] if signature else [],
                        "contexts": ["centerline"],
                        "document_types": THERION_SOURCE_DOCUMENT_TYPES.copy(),
                        "arguments": arguments_for_command,
                        "options": [],
                        "dependencies": ["context: centerline"],
                        "allowed_values": allowed_for_command,
                    }
                )

        if normalized_command_name in {"point", "line", "area"}:
            type_values = extract_symbol_type_values(arguments)

            subtype_matrix = extract_symbol_subtype_matrix(section_text)
            if normalized_command_name == "area":
                type_argument = None
                for item in arguments:
                    if not isinstance(item, dict):
                        continue
                    candidate_text = " ".join(
                        str(item.get(field_name, ""))
                        for field_name in ("name", "signature", "raw", "description")
                    ).lower()
                    if "<type>" in candidate_text:
                        type_argument = item
                        break
                type_description = (
                    f"{type_argument.get('signature', '')} {type_argument.get('description', '')}".lower()
                    if type_argument
                    else ""
                )
                if "arbitrary subtype" in type_description:
                    subtype_matrix.setdefault("u", ["*"])

            if subtype_matrix:
                for type_key in subtype_matrix.keys():
                    append_unique(type_values, type_key)

            if type_values:
                result_entry["type_values"] = type_values

            if subtype_matrix:
                result_entry["subtype_by_type"] = subtype_matrix

        results.append(result_entry)
    return results


def annotate_block_metadata(commands: list[dict[str, Any]]) -> list[dict[str, str]]:
    open_to_close: dict[str, str] = {}
    close_to_open: dict[str, str] = {}

    for command in commands:
        directive = normalize_directive_token(command.get("directive", command.get("name", "")))
        command["directive"] = directive
        closing_directive = infer_block_closing_directive(directive, command.get("syntax", []))
        if closing_directive:
            open_to_close[directive] = closing_directive
            close_to_open[closing_directive] = directive

    for command in commands:
        directive = normalize_directive_token(command.get("directive", command.get("name", "")))
        block_meta: dict[str, Any] = {"role": "leaf"}
        if directive in open_to_close:
            block_meta = {
                "role": "container_open",
                "close_directive": open_to_close[directive],
            }
        elif directive in close_to_open:
            block_meta = {
                "role": "container_close",
                "open_directive": close_to_open[directive],
            }
        command["block"] = block_meta

    pairs = [
        {"open_directive": opening, "close_directive": closing}
        for opening, closing in sorted(open_to_close.items())
    ]
    return pairs


def apply_map_body_command_contexts(catalog: dict[str, Any]) -> None:
    commands = catalog.get("commands", [])
    commands_by_name = {
        normalize_directive_token(command.get("name", "")): command
        for command in commands
        if isinstance(command, dict)
    }
    map_command = commands_by_name.get("map")
    if not map_command:
        return

    map_syntax = " ".join(map_command.get("syntax", [])).lower()
    if not map_syntax:
        return

    for keyword in sorted(MAP_BODY_INLINE_COMMAND_NAMES):
        if re.search(rf"\b{re.escape(keyword)}\b", map_syntax) is None:
            continue
        target_command = commands_by_name.get(keyword)
        if target_command is None:
            continue

        contexts = target_command.setdefault("contexts", [])
        append_unique(contexts, "map")
        dependencies = target_command.setdefault("dependencies", [])
        append_unique(dependencies, "context: map")


def synthesize_layout_setting_commands(commands: list[dict[str, Any]]) -> list[dict[str, Any]]:
    layout_command = next(
        (
            command
            for command in commands
            if isinstance(command, dict)
            and normalize_directive_token(str(command.get("directive", command.get("name", "")))) == "layout"
        ),
        None,
    )
    if layout_command is None:
        return []

    synthesized: list[dict[str, Any]] = []
    for option in layout_command.get("options", []):
        if not isinstance(option, dict):
            continue
        option_key = normalize_whitespace(str(option.get("option_key", ""))).lower()
        if not option_key.startswith("-"):
            continue

        command_name = option_key[1:]
        if not command_name:
            continue

        signature = clean_tex_text(str(option.get("signature", "")))
        signature_core = extract_signature_core(signature)
        aliases: list[str] = []
        if "colour" in command_name:
            append_unique(aliases, command_name.replace("colour", "color"))

        synthesized.append(
            {
                "name": command_name,
                "directive": normalize_directive_token(command_name),
                "aliases": aliases,
                "source": layout_command.get("source", {}),
                "summary": clean_tex_text(str(option.get("description", ""))),
                "syntax": [signature] if signature else [],
                "contexts": ["layout"],
                "document_types": THERION_CONFIG_DOCUMENT_TYPES.copy(),
                "arguments": build_argument_entries_from_signature(signature_core),
                "options": [],
                "dependencies": ["context: layout"],
                "allowed_values": [],
            }
        )
    return synthesized


def apply_command_context_overrides(catalog: dict[str, Any]) -> None:
    commands = catalog.get("commands", [])
    commands_by_name = {
        normalize_directive_token(command.get("name", "")): command
        for command in commands
        if isinstance(command, dict)
    }
    for command_name, context_names in COMMAND_CONTEXT_OVERRIDES.items():
        target_command = commands_by_name.get(command_name)
        if target_command is None:
            continue
        contexts = target_command.setdefault("contexts", [])
        dependencies = target_command.setdefault("dependencies", [])
        for context_name in context_names:
            append_unique(contexts, context_name)
            append_unique(dependencies, f"context: {context_name}")


def choose_revise_option_arity(arity_tokens: set[str]) -> str:
    normalized: set[str] = set()
    for token in arity_tokens:
        raw = str(token).strip().upper()
        if not raw:
            continue
        if raw in {"0", "NONE"}:
            normalized.add("NONE")
        elif raw in {"1", "EXACTLY_ONE"}:
            normalized.add("EXACTLY_ONE")
        elif raw in {"N", "ONE_OR_MORE"}:
            normalized.add("ONE_OR_MORE")
        elif raw in {"*", "ZERO_OR_MORE"}:
            normalized.add("ZERO_OR_MORE")
        else:
            normalized.add(raw)
    if not normalized:
        return "1"
    if "ONE_OR_MORE" in normalized:
        return "N"
    if "EXACTLY_ONE" in normalized:
        return "1"
    if "NONE" in normalized:
        return "0"
    return "1"


def apply_revise_command_option_union(catalog: dict[str, Any]) -> None:
    commands = catalog.get("commands", [])
    if not isinstance(commands, list):
        return

    revise_command = None
    for command in commands:
        if not isinstance(command, dict):
            continue
        if normalize_directive_token(str(command.get("directive", command.get("name", "")))) == "revise":
            revise_command = command
            break
    if revise_command is None:
        return

    arguments = revise_command.setdefault("arguments", [])
    if isinstance(arguments, list) and not arguments:
        arguments.append(
            {
                "raw": "<id>",
                "name": "<id>",
                "signature": "<id>",
                "option_key": "",
                "value_arity": "1",
                "value_domain": "keyword",
                "description": "identifier of the object to revise",
                "allowed_values": [],
                "dependencies": [],
            }
        )

    options_by_key: dict[str, dict[str, Any]] = {}
    for command in commands:
        if not isinstance(command, dict):
            continue
        command_directive = normalize_directive_token(str(command.get("directive", command.get("name", ""))))
        if command_directive == "revise":
            continue

        for option in command.get("options", []):
            if not isinstance(option, dict):
                continue
            option_key = normalize_whitespace(str(option.get("option_key", ""))).lower()
            if not option_key.startswith("-"):
                continue

            bucket = options_by_key.setdefault(
                option_key,
                {
                    "signatures": set(),
                    "descriptions": set(),
                    "allowed_values": set(),
                    "arity_tokens": set(),
                    "value_domains": set(),
                },
            )
            signature = clean_tex_text(str(option.get("signature", "")))
            description = clean_tex_text(str(option.get("description", "")))
            value_arity = str(option.get("value_arity", "")).strip()
            value_domain = str(option.get("value_domain", "")).strip()

            if signature:
                bucket["signatures"].add(signature)
            if description:
                bucket["descriptions"].add(description)
            if value_arity:
                bucket["arity_tokens"].add(value_arity)
            if value_domain:
                bucket["value_domains"].add(value_domain)
            for allowed_value in option.get("allowed_values", []):
                normalized_allowed = normalize_whitespace(str(allowed_value))
                if normalized_allowed:
                    bucket["allowed_values"].add(normalized_allowed)

    synthesized_options: list[dict[str, Any]] = []
    for option_key in sorted(options_by_key.keys()):
        bucket = options_by_key[option_key]
        signatures = sorted(bucket["signatures"])
        description_candidates = sorted(bucket["descriptions"])
        allowed_values = sorted(bucket["allowed_values"], key=lambda value: value.lower())
        value_domains = {domain.lower() for domain in bucket["value_domains"]}

        signature = signatures[0] if signatures else f"{option_key} <value>"
        description = (
            "object-specific option accepted by revise; semantics depend on the revised object type."
            if not description_candidates
            else description_candidates[0]
        )

        synthesized_options.append(
            {
                "raw": signature if not description else f"{signature} = {description}",
                "name": signature,
                "signature": signature,
                "option_key": option_key,
                "value_arity": choose_revise_option_arity(bucket["arity_tokens"]),
                "value_domain": "enum" if allowed_values else ("unknown" if not value_domains else sorted(value_domains)[0]),
                "description": description,
                "allowed_values": allowed_values,
                "dependencies": [],
            }
        )

    if synthesized_options:
        revise_command["options"] = synthesized_options


def merge_command_entries(target: dict[str, Any], source: dict[str, Any]) -> None:
    list_union_fields = (
        "aliases",
        "contexts",
        "document_types",
        "dependencies",
        "inline_commands",
        "allowed_values",
        "type_values",
    )
    for field in list_union_fields:
        source_values = source.get(field)
        if not isinstance(source_values, list) or not source_values:
            continue
        target_values = target.setdefault(field, [])
        if not isinstance(target_values, list):
            target_values = []
            target[field] = target_values
        for value in source_values:
            append_unique(target_values, value)

    source_subtype_by_type = source.get("subtype_by_type")
    if isinstance(source_subtype_by_type, dict):
        target_subtype_by_type = target.setdefault("subtype_by_type", {})
        if not isinstance(target_subtype_by_type, dict):
            target_subtype_by_type = {}
            target["subtype_by_type"] = target_subtype_by_type
        for type_key, subtype_values in source_subtype_by_type.items():
            if not isinstance(subtype_values, list):
                continue
            target_values = target_subtype_by_type.setdefault(type_key, [])
            if not isinstance(target_values, list):
                target_values = []
                target_subtype_by_type[type_key] = target_values
            for value in subtype_values:
                append_unique(target_values, value)

    for field in ("summary", "syntax", "arguments", "options"):
        source_value = source.get(field)
        if field == "summary":
            if isinstance(source_value, str) and source_value.strip() and not str(target.get("summary", "")).strip():
                target["summary"] = source_value
            continue
        if isinstance(source_value, list) and source_value and not target.get(field):
            target[field] = source_value


def merge_object_patch(target: dict[str, Any], patch: dict[str, Any]) -> dict[str, Any]:
    for key, value in patch.items():
        if isinstance(value, dict) and isinstance(target.get(key), dict):
            merge_object_patch(target[key], value)
            continue
        target[key] = value
    return target


def apply_command_override_payload(command: dict[str, Any], payload: dict[str, Any], command_name: str, override_path: Path) -> None:
    special_keys = {"patch", "options_by_key", "arguments_by_name", "arguments_by_index"}
    direct_patch = {key: value for key, value in payload.items() if key not in special_keys}
    if "patch" in payload:
        patch_object = payload.get("patch")
        if not isinstance(patch_object, dict):
            raise ValueError(f"{override_path}: 'patch' must be an object for command '{command_name}'.")
        merge_object_patch(command, patch_object)
    elif direct_patch:
        merge_object_patch(command, direct_patch)

    options_by_key = payload.get("options_by_key", {})
    if options_by_key:
        if not isinstance(options_by_key, dict):
            raise ValueError(f"{override_path}: 'options_by_key' must be an object for command '{command_name}'.")
        option_entries = command.get("options", [])
        if not isinstance(option_entries, list):
            raise ValueError(f"{override_path}: command '{command_name}' has non-list options.")
        for option_key, option_patch in options_by_key.items():
            if not isinstance(option_patch, dict):
                raise ValueError(f"{override_path}: patch for option '{option_key}' must be an object.")
            normalized_option_key = normalize_whitespace(str(option_key)).lower()
            target_option = next(
                (
                    option
                    for option in option_entries
                    if isinstance(option, dict)
                    and normalize_whitespace(str(option.get("option_key", ""))).lower() == normalized_option_key
                ),
                None,
            )
            if target_option is None:
                raise ValueError(
                    f"{override_path}: command '{command_name}' has no option '{option_key}' for override."
                )
            merge_object_patch(target_option, option_patch)

    arguments_by_name = payload.get("arguments_by_name", {})
    if arguments_by_name:
        if not isinstance(arguments_by_name, dict):
            raise ValueError(f"{override_path}: 'arguments_by_name' must be an object for command '{command_name}'.")
        arguments = command.get("arguments", [])
        if not isinstance(arguments, list):
            raise ValueError(f"{override_path}: command '{command_name}' has non-list arguments.")
        for argument_name, argument_patch in arguments_by_name.items():
            if not isinstance(argument_patch, dict):
                raise ValueError(f"{override_path}: patch for argument '{argument_name}' must be an object.")
            normalized_argument_name = normalize_whitespace(str(argument_name)).lower()
            target_argument = next(
                (
                    argument
                    for argument in arguments
                    if isinstance(argument, dict)
                    and normalize_whitespace(str(argument.get("name", ""))).lower() == normalized_argument_name
                ),
                None,
            )
            if target_argument is None:
                raise ValueError(
                    f"{override_path}: command '{command_name}' has no argument '{argument_name}' for override."
                )
            merge_object_patch(target_argument, argument_patch)

    arguments_by_index = payload.get("arguments_by_index", {})
    if arguments_by_index:
        if not isinstance(arguments_by_index, dict):
            raise ValueError(f"{override_path}: 'arguments_by_index' must be an object for command '{command_name}'.")
        arguments = command.get("arguments", [])
        if not isinstance(arguments, list):
            raise ValueError(f"{override_path}: command '{command_name}' has non-list arguments.")
        for index_token, argument_patch in arguments_by_index.items():
            if not isinstance(argument_patch, dict):
                raise ValueError(f"{override_path}: patch for argument index '{index_token}' must be an object.")
            try:
                index = int(str(index_token))
            except ValueError as error:
                raise ValueError(
                    f"{override_path}: argument index '{index_token}' is not an integer for command '{command_name}'."
                ) from error
            if index < 0 or index >= len(arguments):
                raise ValueError(
                    f"{override_path}: argument index '{index}' is out of range for command '{command_name}'."
                )
            target_argument = arguments[index]
            if not isinstance(target_argument, dict):
                raise ValueError(
                    f"{override_path}: argument at index '{index}' is not an object for command '{command_name}'."
                )
            merge_object_patch(target_argument, argument_patch)


def apply_command_override_files(catalog: dict[str, Any], overrides_dir: Path) -> dict[str, Any]:
    if not overrides_dir.exists():
        return catalog
    if not overrides_dir.is_dir():
        raise ValueError(f"{overrides_dir}: overrides path exists but is not a directory.")

    commands_by_name = {command["name"]: command for command in catalog.get("commands", []) if isinstance(command, dict)}
    for override_path in sorted(overrides_dir.glob("*.override.json")):
        suffix = ".override.json"
        if not override_path.name.endswith(suffix):
            continue
        command_name = override_path.name[: -len(suffix)]
        if not command_name:
            continue
        command = commands_by_name.get(command_name)
        if command is None:
            raise ValueError(f"{override_path}: command '{command_name}' not found in generated catalog.")
        payload = json.loads(override_path.read_text(encoding="utf-8"))
        if not isinstance(payload, dict):
            raise ValueError(f"{override_path}: override payload must be a JSON object.")
        apply_command_override_payload(command, payload, command_name, override_path)

    catalog["commands"] = sorted(catalog["commands"], key=lambda item: item["name"].lower())
    return catalog


def finalize_catalog(catalog: dict[str, Any]) -> dict[str, Any]:
    catalog = normalize_catalog_text_fields(catalog)
    commands = sorted(catalog.get("commands", []), key=lambda item: item["name"].lower())
    catalog["commands"] = commands
    catalog["block_pairs"] = annotate_block_metadata(commands)
    return catalog


def write_command_files(catalog: dict[str, Any], commands_dir: Path) -> None:
    commands = catalog.get("commands", [])
    if not isinstance(commands, list):
        return

    commands_dir.mkdir(parents=True, exist_ok=True)
    stale_files = {path.name for path in commands_dir.glob("*.json")}
    for command in commands:
        if not isinstance(command, dict):
            continue
        command_name = normalize_directive_token(str(command.get("name", "")))
        if not command_name:
            continue
        file_name = f"{command_name}.json"
        file_path = commands_dir / file_name
        file_path.write_text(json.dumps(command, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        stale_files.discard(file_name)

    for stale_file in sorted(stale_files):
        (commands_dir / stale_file).unlink()


def build_catalog(inputs: list[Path], source_repo: str, source_ref: str) -> dict[str, Any]:
    commands = []
    source_texts: list[tuple[Path, str]] = []
    known_command_names: set[str] = set()
    for input_path in inputs:
        text = input_path.read_text(encoding="utf-8", errors="replace")
        source_texts.append((input_path, text))
        for match in SECTION_RE.finditer(text):
            command_name = normalize_whitespace(match.group(1)).lower()
            if "xtherion" in command_name:
                continue
            known_command_names.add(command_name)

    for input_path, text in source_texts:
        commands.extend(parse_sections(text, str(input_path), known_command_names))

    commands.extend(synthesize_layout_setting_commands(commands))

    # Deduplicate by command name while preserving first occurrence precedence.
    # For duplicate command definitions (e.g. top-level plus centerline inline),
    # merge context-oriented fields so scope metadata is not lost.
    deduped = {}
    for command in commands:
        existing = deduped.get(command["name"])
        if existing is None:
            deduped[command["name"]] = command
            continue
        merge_command_entries(existing, command)

    sorted_commands = sorted(deduped.values(), key=lambda item: item["name"].lower())
    block_pairs = annotate_block_metadata(sorted_commands)

    catalog = {
        "metadata": {
            "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "generator_version": "4",
            "source_repository": source_repo,
            "source_reference": source_ref,
            "source_files": [str(path) for path in inputs],
        },
        "block_pairs": block_pairs,
        "commands": sorted_commands,
    }
    apply_map_body_command_contexts(catalog)
    apply_command_context_overrides(catalog)
    apply_revise_command_option_union(catalog)
    return catalog


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Therion command metadata catalog from thbook TeX.")
    parser.add_argument(
        "--input",
        action="append",
        default=[],
        help="Input TeX file. Can be supplied multiple times.",
    )
    parser.add_argument(
        "--output",
        default="resources/therion_command_catalog.json",
        help="Output JSON path.",
    )
    parser.add_argument(
        "--overrides-dir",
        default="resources/therion_catalog/overrides",
        help="Directory with per-command override files (*.override.json).",
    )
    parser.add_argument(
        "--commands-dir",
        default="resources/therion_catalog/commands",
        help="Directory where per-command JSON files are written.",
    )
    parser.add_argument(
        "--source-repo",
        default="https://github.com/therion/therion",
        help="Upstream source repository metadata.",
    )
    parser.add_argument(
        "--source-ref",
        default="snapshot",
        help="Upstream source reference metadata (tag/commit/etc.).",
    )
    parser.add_argument(
        "--source-root",
        default="therion",
        help="Therion source root used for source-derived metadata.",
    )
    args = parser.parse_args()

    input_paths = [Path(p) for p in args.input] if args.input else [
        Path("therion/thbook/ch02.tex"),
        Path("therion/thbook/ch03.tex"),
    ]

    for input_path in input_paths:
        if not input_path.exists():
            raise FileNotFoundError(f"Missing input file: {input_path}")

    catalog = build_catalog(input_paths, args.source_repo, args.source_ref)
    apply_source_derived_option_applicability(catalog, Path(args.source_root))

    overrides_dir = Path(args.overrides_dir)
    catalog = apply_command_override_files(catalog, overrides_dir)

    catalog = finalize_catalog(catalog)

    write_command_files(catalog, Path(args.commands_dir))

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Generated {output_path} with {len(catalog['commands'])} commands.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
