"""Shared helpers for single-entry save scripts."""

import json
import sqlite3

from db_common import EXTRA_DATA_COLUMN, ensure_schema, get_connection, stamp_drill_hole_import_time
from batch_import import (
    TABLE_COLUMNS,
    TABLE_DISPLAY_NAMES,
    apply_single_entry_overwrite,
    collect_record_differences,
    merge_records,
    make_record_key,
    is_empty_value,
    format_conflict_description,
    format_record_label,
)


VALID_ACTIONS = {"skip", "overwrite", "merge"}


def borehole_exists(conn, borehole_id):
    cursor = conn.cursor()
    cursor.execute(
        "SELECT 1 FROM DrillHoleInfo WHERE borehole_id = ?",
        (borehole_id.strip(),),
    )
    return cursor.fetchone() is not None


def fetch_total_depth(conn, borehole_id):
    cursor = conn.cursor()
    cursor.execute(
        "SELECT total_depth FROM DrillHoleInfo WHERE borehole_id = ?",
        (borehole_id.strip(),),
    )
    row = cursor.fetchone()
    if not row or row[0] is None:
        return None
    return float(row[0])


def fetch_area_id(conn, borehole_id):
    cursor = conn.cursor()
    cursor.execute(
        "SELECT area_id FROM DrillHoleInfo WHERE borehole_id = ?",
        (borehole_id.strip(),),
    )
    row = cursor.fetchone()
    if not row or row[0] is None:
        return None
    value = str(row[0]).strip()
    return value or None


def resolve_area_id(conn, borehole_id, area_id_from_args):
    provided = str(area_id_from_args or "").strip()
    if provided:
        return provided
    return fetch_area_id(conn, borehole_id)


def require_borehole_exists(conn, borehole_id):
    if not borehole_exists(conn, borehole_id):
        raise ValueError("请先保存钻孔概况")


def build_extra_data_json(extra_fields):
    if not extra_fields:
        return None
    cleaned = {}
    for key, value in extra_fields.items():
        field_name = str(key).strip()
        if not field_name or is_empty_value(value):
            continue
        cleaned[field_name] = value
    if not cleaned:
        return None
    return json.dumps(cleaned, ensure_ascii=False)


def parse_extra_fields_json(extra_data_json):
    if not extra_data_json:
        return {}
    try:
        data = json.loads(extra_data_json)
    except json.JSONDecodeError as exc:
        raise ValueError(f"扩展字段 JSON 格式错误: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError("扩展字段必须是 JSON 对象")
    return data


def load_conflict_resolutions(resolution_file):
    if not resolution_file:
        return {}
    with open(resolution_file, "r", encoding="utf-8") as file:
        data = json.load(file)
    resolutions = {}
    for record_key, action in data.items():
        action_text = str(action).strip().lower()
        if action_text in VALID_ACTIONS:
            resolutions[str(record_key)] = action_text
    return resolutions


def fetch_record_by_key(conn, table_name, record_key):
    pk_fields = {
        "DrillHoleInfo": ["borehole_id"],
        "InclineInfo": ["borehole_id", "point_id"],
        "StrataInfo": ["borehole_id", "layer_order"],
        "SampleRecord": ["sample_id"],
        "GradeInfo": ["sample_id", "element_name"],
    }
    columns = TABLE_COLUMNS[table_name]
    pk = pk_fields[table_name]
    parts = record_key.split("|")
    if len(parts) != len(pk):
        return None

    where = " AND ".join(f"{field} = ?" for field in pk)
    cursor = conn.cursor()
    cursor.execute(
        f"SELECT {', '.join(columns)} FROM {table_name} WHERE {where}",
        parts,
    )
    row = cursor.fetchone()
    if not row:
        return None
    return {columns[index]: row[index] for index in range(len(columns))}


def make_conflict_payload(table_name, row_num, record_key, new_data, differences):
    return {
        "row_num": row_num,
        "record_key": record_key,
        "record_label": format_record_label(table_name, new_data),
        "conflict_description": format_conflict_description(table_name, new_data),
        "target_table": table_name,
        "differences": differences,
    }


def analyze_record_conflict(table_name, row_num, existing, new_data):
    if not existing:
        return None
    differences = collect_record_differences(table_name, existing, new_data)
    if not differences:
        return None
    return make_conflict_payload(
        table_name,
        row_num,
        make_record_key(table_name, new_data),
        new_data,
        differences,
    )


def resolve_record(existing, new_data, action, table_name):
    if action == "skip":
        return None
    write_fields = TABLE_COLUMNS[table_name]
    if action == "overwrite":
        return apply_single_entry_overwrite(existing, new_data, table_name)
    if action == "merge":
        return merge_records(existing, new_data, write_fields)
    raise ValueError(f"未知冲突处理方式: {action}")


def write_record(cursor, table_name, record):
    if table_name == "DrillHoleInfo":
        stamp_drill_hole_import_time(record)
    columns = TABLE_COLUMNS[table_name]
    placeholders = ", ".join(["?"] * len(columns))
    columns_str = ", ".join(columns)
    sql = f"INSERT OR REPLACE INTO {table_name} ({columns_str}) VALUES ({placeholders})"
    values = [record.get(column) for column in columns]
    cursor.execute(sql, values)


def save_existing_record(cursor, table_name, existing, record, resolutions):
    """Save one existing record using conflict resolutions. Returns write outcome."""
    record_key = make_record_key(table_name, record)
    differences = collect_record_differences(table_name, existing, record)
    if not differences:
        return "unchanged"

    action = resolutions.get(record_key)
    if not action:
        raise ValueError("数据冲突未选择处理方式")

    final_record = resolve_record(existing, record, action, table_name)
    if final_record is None:
        return "skipped"
    write_record(cursor, table_name, final_record)
    return "updated"


def success_response(message, **extra):
    payload = {"status": "success", "message": message}
    payload.update(extra)
    return payload


def error_response(message):
    return {"status": "error", "message": message}
