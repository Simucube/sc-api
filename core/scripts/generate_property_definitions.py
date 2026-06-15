"""Generate SC-API property definition headers and sources from JSON.

Extracted from gorilla's generate_property_definitions.py (--scapi code path).
Generates {GroupName}PropertyRef<T> definitions for each property group.
"""

from __future__ import annotations
import argparse
import json
import os
import pathlib
import sys

SCRIPT_DIR = pathlib.Path(os.path.realpath(os.path.dirname(__file__)))
SCAPI_TEMPLATE_DIR = SCRIPT_DIR / "template_scapi"

PROPERTY_TYPES = {
    "bool",
    "f32",
    "f64",
    "i64",
    "i32",
    "i8",
    "string",
    "enum",
}

PROPERTY_TYPE_SCAPI_CPP = {
    "bool": "bool",
    "f32": "double",
    "f64": "double",
    "i64": "int64_t",
    "i32": "int32_t",
    "i8": "int32_t",
    "string": "std::string_view",
    "enum": "std::string_view",
}

PROPERTY_TYPE_SORT_ORDER = {
    "bool": 6,
    "f32": 3,
    "f64": 1,
    "i64": 0,
    "i32": 2,
    "i8": 4,
    "string": 7,
    "enum": 5,
}

PROPERTY_GROUP_CLASS_NAMES = {
    "vehicle": "Vehicle",
    "sim": "Sim",
    "participant": "Participant",
    "track": "Track",
    "tire": "Tire",
    "session": "Session",
}

PROPERTY_GROUP_HEADER_GUARD_NAMES = {
    "vehicle": "VEHICLE",
    "sim": "SIM",
    "participant": "PARTICIPANT",
    "track": "TRACK",
    "tire": "TIRE",
    "session": "SESSION",
}

PROPERTY_GROUP_NAMESPACE_NAMES_SCAPI = {
    "vehicle": "sc_api::core::sim_data::vehicle",
    "sim": "sc_api::core::sim_data::sim",
    "participant": "sc_api::core::sim_data::participant",
    "track": "sc_api::core::sim_data::track",
    "tire": "sc_api::core::sim_data::tire",
    "session": "sc_api::core::sim_data::session",
}


class ConfigProperty:
    def __init__(self, name, type):
        self.id = 0
        self.type = type
        self.name = name
        self.description = ""


def parse_properties(properties_file):
    d = json.load(properties_file)

    properties = []
    for t_name, t_data in d.items():
        t_type = t_data["type"]

        if t_type not in PROPERTY_TYPES:
            print("Property name=", t_name, " unknown type:" + t_type)
            continue

        t = ConfigProperty(t_name, t_type)

        if "desc" in t_data:
            t.description = t_data["desc"]

        properties.append(t)

    properties.sort(key=lambda t: PROPERTY_TYPE_SORT_ORDER[t.type])

    id_counter = 1
    for prop in properties:
        prop.id = id_counter
        id_counter += 1
    return properties


def generate_scapi_definitions_header(properties: list[ConfigProperty], property_group: str, output_dir: pathlib.Path):
    header_filename = property_group + ".h"
    with open(SCAPI_TEMPLATE_DIR / "properties.h") as template_f:
        template_str = template_f.read()

    prop_class = PROPERTY_GROUP_CLASS_NAMES[property_group]

    gen_value_refs = ""
    for prop in properties:
        if prop.description:
            gen_value_refs += "/** " + prop.description + " */\n"
        gen_value_refs += "extern const " + prop_class + "PropertyRef<" + PROPERTY_TYPE_SCAPI_CPP[prop.type] + "> " + prop.name + ";\n\n"

    output = template_str.replace("HEADER_GUARD_NAME_HERE", "SC_API_CORE_SIM_DATA_" + PROPERTY_GROUP_HEADER_GUARD_NAMES[property_group] + "_PROPERTIES_GENERATED_H_")
    output = output.replace("NAMESPACE_NAME_HERE", PROPERTY_GROUP_NAMESPACE_NAMES_SCAPI[property_group])
    output = output.replace("/*PROPERTY_REFERENCES_HERE*/", gen_value_refs)

    os.makedirs(output_dir, exist_ok=True)
    with open(output_dir / header_filename, "w") as out_f:
        out_f.write(output)


def generate_scapi_definitions_source(properties: list[ConfigProperty], property_group: str, output_dir: pathlib.Path):
    source_filename = property_group + ".cpp"
    with open(SCAPI_TEMPLATE_DIR / "properties.cpp") as template_f:
        template_str = template_f.read()

    prop_class = PROPERTY_GROUP_CLASS_NAMES[property_group]

    gen_value_refs = ""
    for prop in properties:
        gen_value_refs += "const " + prop_class + "PropertyRef<" + PROPERTY_TYPE_SCAPI_CPP[prop.type] + "> " + prop.name + "{\"" + prop.name + "\"};\n"

    output = template_str.replace("NAMESPACE_NAME_HERE", PROPERTY_GROUP_NAMESPACE_NAMES_SCAPI[property_group])
    output = output.replace("/*HEADER_INCLUDE_HERE*/", "#include \"sc-api/core/sim_data/" + property_group + ".h\"")
    output = output.replace("/*PROPERTY_REFERENCES_HERE*/", gen_value_refs)

    os.makedirs(output_dir, exist_ok=True)
    with open(output_dir / source_filename, "w") as out_f:
        out_f.write(output)


def main():
    parser = argparse.ArgumentParser(prog="generate_property_definitions")
    parser.add_argument("property_list", type=argparse.FileType("r", encoding="UTF-8"))
    parser.add_argument("--group", "-g", required=True)
    parser.add_argument("--header_dir", required=True, type=pathlib.Path)
    parser.add_argument("--source_dir", required=True, type=pathlib.Path)
    args = parser.parse_args()

    properties = parse_properties(args.property_list)

    generate_scapi_definitions_header(properties, args.group, args.header_dir)
    generate_scapi_definitions_source(properties, args.group, args.source_dir)

    print(f"Generated {len(properties)} property definitions for group '{args.group}'")


if __name__ == "__main__":
    main()
