"""Generate telemetry_references.h from telemetry_public.json.

Reads a JSON file containing public telemetry entries and generates a C++ header
with inline constexpr TelemetryReference<T> definitions.
"""

import argparse
import json
import os
import pathlib
import sys

TELEMETRY_TYPE_CPP = {
    "bool": "bool",
    "f32": "float",
    "f64": "double",
    "i64": "int64_t",
    "i32": "int32_t",
    "u32": "uint32_t",
    "i16": "int16_t",
    "u16": "uint16_t",
    "i8": "int8_t",
    "u8": "uint8_t",
}

TELEMETRY_TYPE_SORT_ORDER = {
    "bool": -1,
    "f64": 0,
    "i64": 1,
    "f32": 2,
    "i32": 3,
    "u32": 4,
    "i16": 5,
    "u16": 6,
    "i8": 7,
    "u8": 8,
}


def main():
    parser = argparse.ArgumentParser(prog="generate_telemetry_refs")
    parser.add_argument("telemetry_json", type=pathlib.Path)
    parser.add_argument("--template", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    with open(args.telemetry_json, encoding="UTF-8") as f:
        data = json.load(f)

    # Sort entries by type order for consistent output
    entries = sorted(data.items(), key=lambda kv: (TELEMETRY_TYPE_SORT_ORDER.get(kv[1]["type"], 99), kv[0]))

    refs = ""
    for name, entry in entries:
        t_type = entry["type"]
        cpp_type = TELEMETRY_TYPE_CPP.get(t_type)
        if cpp_type is None:
            print(f"Unknown type '{t_type}' for telemetry '{name}'", file=sys.stderr)
            return 1

        desc = entry.get("desc", "")
        if desc:
            refs += f"/** {desc} */\n"
        refs += f'inline constexpr TelemetryReference<{cpp_type}> {name}{{ "{name}" }};\n\n'

    with open(args.template, encoding="UTF-8") as f:
        template_str = f.read()

    output = template_str.replace("/*TELEMETRY_REFERENCES_HERE*/", refs)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", newline="\n") as f:
        f.write(output)

    print(f"Generated {len(entries)} telemetry references -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
