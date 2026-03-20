"""Generate device_info_definitions.h from device_info_enums.json.

Reads a JSON file containing device_info enum definitions and generates a C++ header
with enum classes, string id arrays, toString() and fromString() functions.
"""

import argparse
import json
import os
import pathlib
import sys


def generate_enum(enum_def):
    """Generate C++ code for a single enum definition."""
    lines = []
    name = enum_def["name"]
    description = enum_def["description"]
    ids_array = enum_def["ids_array_name"]
    from_string = enum_def["from_string_name"]
    fallback = enum_def["fallback_value"]
    values = enum_def["values"]

    # Enum class docstring
    doc_lines = description.split("\n")
    if len(doc_lines) == 1:
        lines.append(f"/** {doc_lines[0]} */")
    else:
        lines.append("/** " + doc_lines[0])
        for dl in doc_lines[1:]:
            if dl:
                lines.append(f" * {dl}")
            else:
                lines.append(" *")
        lines.append(" */")

    lines.append(f"enum class {name} {{")

    for val in values:
        desc = val.get("description")
        if desc:
            lines.append("")
            lines.append(f"    /** {desc} */")
        lines.append(f"    {val['name']},")

    lines.append("};")

    # String ids array
    lines.append("")
    lines.append(f"inline constexpr const char* {ids_array}[] = {{")
    for val in values:
        lines.append(f'    "{val["name"]}",')
    lines.append("};")

    # toString function
    lines.append("")
    lines.append(f"constexpr std::string_view toString({name} r) {{ return {ids_array}[static_cast<int>(r)]; }}")

    # fromString function
    lines.append("")
    lines.append(f"constexpr {name} {from_string}(std::string_view s) {{")
    lines.append(f"    for (int i = 0; i < static_cast<int>({name}::{fallback}); ++i) {{")
    lines.append(f"        if (s == {ids_array}[i]) return static_cast<{name}>(i);")
    lines.append("    }")
    lines.append(f"    return {name}::{fallback};")
    lines.append("}")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(prog="generate_device_info_defs")
    parser.add_argument("enums_json", type=pathlib.Path)
    parser.add_argument("--template", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    with open(args.enums_json, encoding="UTF-8") as f:
        data = json.load(f)

    blocks = []
    for enum_def in data["enums"]:
        blocks.append(generate_enum(enum_def))

    generated = "\n\n".join(blocks)

    with open(args.template, encoding="UTF-8") as f:
        template_str = f.read()

    output = template_str.replace("/*DEVICE_INFO_ENUMS_HERE*/", generated)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", newline="\n") as f:
        f.write(output)

    print(f"Generated {len(data['enums'])} device_info enums -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
