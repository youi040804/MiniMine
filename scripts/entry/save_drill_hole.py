"""Save basic drill hole info to DrillHoleInfo with conflict handling."""

import argparse
import json
import sys

from db_common import EXTRA_DATA_COLUMN, ensure_schema, get_connection
from single_entry_common import (
    analyze_record_conflict,
    build_extra_data_json,
    fetch_record_by_key,
    load_conflict_resolutions,
    make_record_key,
    parse_extra_fields_json,
    save_existing_record,
    success_response,
    write_record,
)


REQUIRED_NUMERIC_FIELDS = {
    "x": "X坐标",
    "y": "Y坐标",
    "z": "Z坐标",
    "total_depth": "终孔深度",
}


def optional_float_from_args(args, field_name):
    if not hasattr(args, field_name):
        return None
    return getattr(args, field_name)


def build_record(args):
    extra_fields = parse_extra_fields_json(args.extra_data_json)
    record = {
        "borehole_id": args.borehole_id.strip(),
    }

    area_id = args.area_id.strip()
    if area_id:
        record["area_id"] = area_id

    if args.x is not None:
        record["x_coord"] = args.x
    if args.y is not None:
        record["y_coord"] = args.y
    if args.z is not None:
        record["z_coord"] = args.z
    if args.total_depth is not None:
        record["total_depth"] = args.total_depth

    if hasattr(args, "azimuth"):
        record["azimuth"] = optional_float_from_args(args, "azimuth")
    if hasattr(args, "dip_angle"):
        record["dip_angle"] = optional_float_from_args(args, "dip_angle")

    extra_json = build_extra_data_json(extra_fields)
    if extra_json is not None:
        record[EXTRA_DATA_COLUMN] = extra_json

    return record


def validate_borehole_id(args):
    if not args.borehole_id.strip():
        raise ValueError("钻孔编号为必填项")


def validate_for_insert(args):
    for field_name, label in REQUIRED_NUMERIC_FIELDS.items():
        if getattr(args, field_name) is None:
            raise ValueError(f"{label}为必填项")

    if args.total_depth < 0:
        raise ValueError("终孔深度不能为负数")

    if args.x == 0.0 and args.y == 0.0 and args.z == 0.0:
        raise ValueError("钻孔坐标不能全为 0，请检查录入数据")


def validate_for_update(args):
    if args.total_depth is not None and args.total_depth < 0:
        raise ValueError("终孔深度不能为负数")

    if (
        args.x is not None
        and args.y is not None
        and args.z is not None
        and args.x == 0.0
        and args.y == 0.0
        and args.z == 0.0
    ):
        raise ValueError("钻孔坐标不能全为 0，请检查录入数据")


def analyze(conn, new_data):
    record_key = make_record_key("DrillHoleInfo", new_data)
    existing = fetch_record_by_key(conn, "DrillHoleInfo", record_key)
    conflict = analyze_record_conflict("DrillHoleInfo", 1, existing, new_data)
    return existing, [conflict] if conflict else []


def save(conn, new_data, resolutions):
    record_key = make_record_key("DrillHoleInfo", new_data)
    existing = fetch_record_by_key(conn, "DrillHoleInfo", record_key)
    cursor = conn.cursor()

    if not existing:
        write_record(cursor, "DrillHoleInfo", new_data)
        conn.commit()
        return "钻孔概况保存成功"

    outcome = save_existing_record(cursor, "DrillHoleInfo", existing, new_data, resolutions)
    conn.commit()
    if outcome == "skipped":
        return "已跳过，保留数据库现有数据"
    if outcome == "unchanged":
        return "钻孔概况无变化"
    return "钻孔概况保存成功"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--borehole_id", required=True)
    parser.add_argument("--x", type=float, default=None)
    parser.add_argument("--y", type=float, default=None)
    parser.add_argument("--z", type=float, default=None)
    parser.add_argument("--total_depth", type=float, default=None)
    parser.add_argument("--azimuth", nargs="?", const=None, default=argparse.SUPPRESS, type=float)
    parser.add_argument("--dip_angle", nargs="?", const=None, default=argparse.SUPPRESS, type=float)
    parser.add_argument("--area_id", default="")
    parser.add_argument("--extra_data_json", default="")
    parser.add_argument("--analyze", action="store_true")
    parser.add_argument("--conflict_resolutions", default="")
    args = parser.parse_args()

    try:
        validate_borehole_id(args)
        new_data = build_record(args)
        conn = get_connection()
        ensure_schema(conn)

        existing, conflicts = analyze(conn, new_data)

        if args.analyze:
            conn.close()
            print(
                json.dumps(
                    success_response(
                        "分析完成",
                        conflicts=conflicts,
                        record_exists=existing is not None,
                    ),
                    ensure_ascii=False,
                )
            )
            return

        if not existing:
            validate_for_insert(args)
        else:
            validate_for_update(args)

        resolutions = load_conflict_resolutions(args.conflict_resolutions)
        message = save(conn, new_data, resolutions)
        conn.close()
        print(json.dumps(success_response(message), ensure_ascii=False))
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
