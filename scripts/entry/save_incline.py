"""Save incline survey data to InclineInfo with validation and conflict handling."""

import argparse
import json
import sys

from db_common import EXTRA_DATA_COLUMN, ensure_schema, get_connection
from single_entry_common import (
    analyze_record_conflict,
    build_extra_data_json,
    fetch_record_by_key,
    fetch_total_depth,
    load_conflict_resolutions,
    make_record_key,
    parse_extra_fields_json,
    require_borehole_exists,
    resolve_area_id,
    save_existing_record,
    success_response,
    write_record,
)


def build_rows(borehole_id, area_id, rows, extra_fields):
    extra_json = build_extra_data_json(extra_fields)
    result = []
    for row in rows:
        result.append(
            {
                "borehole_id": borehole_id,
                "point_id": int(row.get("point_id", 0)),
                "area_id": area_id or None,
                "point_depth": float(row.get("point_depth", 0)),
                "deviation_angle": float(row.get("deviation_angle", 0))
                if row.get("deviation_angle") not in (None, "")
                else None,
                "azimuth": float(row.get("azimuth", 0))
                if row.get("azimuth") not in (None, "")
                else None,
                EXTRA_DATA_COLUMN: extra_json,
            }
        )
    return result


def validate_rows(conn, borehole_id, records):
    if not records:
        raise ValueError("无数据可保存")

    require_borehole_exists(conn, borehole_id)
    total_depth = fetch_total_depth(conn, borehole_id)

    seen_points = set()
    previous_depth = None
    for index, record in enumerate(records, start=1):
        point_id = record["point_id"]
        point_depth = record["point_depth"]

        if point_id in seen_points:
            raise ValueError(f"第 {index} 行：测点号 {point_id} 重复")
        seen_points.add(point_id)

        if previous_depth is not None and point_depth <= previous_depth:
            raise ValueError(f"第 {index} 行：测点深度必须递增")
        previous_depth = point_depth

        if total_depth is not None and point_depth > total_depth:
            raise ValueError(f"第 {index} 行：测点深度不能超过终孔深度 {total_depth}")


def analyze(conn, records):
    conflicts = []
    for index, record in enumerate(records, start=1):
        record_key = make_record_key("InclineInfo", record)
        existing = fetch_record_by_key(conn, "InclineInfo", record_key)
        conflict = analyze_record_conflict("InclineInfo", index + 1, existing, record)
        if conflict:
            conflicts.append(conflict)
    return conflicts


def save(conn, records, resolutions):
    inserted = 0
    updated = 0
    skipped = 0
    cursor = conn.cursor()
    for record in records:
        record_key = make_record_key("InclineInfo", record)
        existing = fetch_record_by_key(conn, "InclineInfo", record_key)
        if not existing:
            write_record(cursor, "InclineInfo", record)
            inserted += 1
            continue

        outcome = save_existing_record(cursor, "InclineInfo", existing, record, resolutions)
        if outcome == "updated":
            updated += 1
        elif outcome == "skipped":
            skipped += 1
    conn.commit()
    message = f"测斜数据保存成功，新增 {inserted} 条，更新 {updated} 条"
    if skipped:
        message += f"，跳过 {skipped} 条"
    return message


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--borehole_id", required=True)
    parser.add_argument("--data_json", required=True)
    parser.add_argument("--area_id", default="")
    parser.add_argument("--extra_data_json", default="")
    parser.add_argument("--analyze", action="store_true")
    parser.add_argument("--conflict_resolutions", default="")
    args = parser.parse_args()

    try:
        rows = json.loads(args.data_json)
        extra_fields = parse_extra_fields_json(args.extra_data_json)
        borehole_id = args.borehole_id.strip()

        conn = get_connection()
        ensure_schema(conn)
        area_id = resolve_area_id(conn, borehole_id, args.area_id.strip())
        records = build_rows(borehole_id, area_id, rows, extra_fields)
        validate_rows(conn, borehole_id, records)

        if args.analyze:
            conflicts = analyze(conn, records)
            conn.close()
            print(json.dumps(success_response("分析完成", conflicts=conflicts), ensure_ascii=False))
            return

        resolutions = load_conflict_resolutions(args.conflict_resolutions)
        message = save(conn, records, resolutions)
        conn.close()
        print(json.dumps(success_response(message), ensure_ascii=False))
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
