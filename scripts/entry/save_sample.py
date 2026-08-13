"""Save sample records and grade info with validation and conflict handling."""

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


def build_sample_record(borehole_id, area_id, sample):
    extra_object = sample.get("extra_data")
    if isinstance(extra_object, dict):
        extra_json = build_extra_data_json(extra_object)
    else:
        extra_json = None
    return {
        "sample_id": sample.get("sample_id"),
        "borehole_id": borehole_id,
        "area_id": area_id or None,
        "start_depth": float(sample.get("start_depth", 0))
        if sample.get("start_depth") not in (None, "")
        else None,
        "end_depth": float(sample.get("end_depth", 0))
        if sample.get("end_depth") not in (None, "")
        else None,
        "sample_length": float(sample.get("sample_length", 0))
        if sample.get("sample_length") not in (None, "")
        else None,
        "core_length": float(sample.get("core_length", 0))
        if sample.get("core_length") not in (None, "")
        else None,
        "sample_type": int(float(sample.get("sample_type", 0)))
        if sample.get("sample_type") not in (None, "")
        else None,
        EXTRA_DATA_COLUMN: extra_json,
    }


def build_grade_record(grade):
    return {
        "sample_id": grade.get("sample_id"),
        "element_name": grade.get("element_name"),
        "grade_value": float(grade.get("grade_value", 0)),
        EXTRA_DATA_COLUMN: None,
    }


def validate_payload(conn, borehole_id, samples, grades):
    if not samples and not grades:
        raise ValueError("无数据可保存")

    require_borehole_exists(conn, borehole_id)
    total_depth = fetch_total_depth(conn, borehole_id)

    seen_samples = set()
    for index, sample in enumerate(samples, start=1):
        sample_id = str(sample.get("sample_id", "")).strip()
        if not sample_id:
            raise ValueError(f"第 {index} 个样品：样品编号不能为空")

        if sample_id in seen_samples:
            raise ValueError(f"样品编号 {sample_id} 重复")
        seen_samples.add(sample_id)

        start_depth = sample.get("start_depth")
        end_depth = sample.get("end_depth")
        if start_depth in (None, "") or end_depth in (None, ""):
            raise ValueError(f"样品 {sample_id}：采样起始孔深和采样终止孔深为必填项")

        start_value = float(start_depth)
        end_value = float(end_depth)
        if start_value >= end_value:
            raise ValueError(f"样品 {sample_id}：采样起始孔深必须小于采样终止孔深")
        if total_depth is not None and end_value > total_depth:
            raise ValueError(f"样品 {sample_id}：采样终止孔深不能超过终孔深度 {total_depth}")

    sample_ids = {str(sample.get("sample_id", "")).strip() for sample in samples}
    for index, grade in enumerate(grades, start=1):
        sample_id = str(grade.get("sample_id", "")).strip()
        element_name = str(grade.get("element_name", "")).strip()
        grade_value = grade.get("grade_value")

        if not sample_id:
            raise ValueError(f"第 {index} 条品位：缺少样品编号")
        if sample_id not in sample_ids:
            raise ValueError(f"品位记录引用了未定义的样品编号 {sample_id}")
        if not element_name:
            raise ValueError(f"样品 {sample_id}：元素名称不能为空")
        if grade_value in (None, ""):
            raise ValueError(f"样品 {sample_id} / 元素 {element_name}：元素品位值不能为空")


def analyze(conn, sample_records, grade_records):
    conflicts = []
    for index, record in enumerate(sample_records, start=1):
        record_key = make_record_key("SampleRecord", record)
        existing = fetch_record_by_key(conn, "SampleRecord", record_key)
        conflict = analyze_record_conflict("SampleRecord", index + 1, existing, record)
        if conflict:
            conflicts.append(conflict)

    for index, record in enumerate(grade_records, start=1):
        record_key = make_record_key("GradeInfo", record)
        existing = fetch_record_by_key(conn, "GradeInfo", record_key)
        conflict = analyze_record_conflict("GradeInfo", index + 1, existing, record)
        if conflict:
            conflicts.append(conflict)
    return conflicts


def save_records(conn, table_name, records, resolutions):
    inserted = 0
    updated = 0
    skipped = 0
    cursor = conn.cursor()
    for record in records:
        record_key = make_record_key(table_name, record)
        existing = fetch_record_by_key(conn, table_name, record_key)
        if not existing:
            write_record(cursor, table_name, record)
            inserted += 1
            continue

        outcome = save_existing_record(cursor, table_name, existing, record, resolutions)
        if outcome == "updated":
            updated += 1
        elif outcome == "skipped":
            skipped += 1
    return inserted, updated, skipped


def save(conn, sample_records, grade_records, resolutions):
    sample_inserted, sample_updated, sample_skipped = save_records(
        conn, "SampleRecord", sample_records, resolutions
    )
    grade_inserted, grade_updated, grade_skipped = save_records(
        conn, "GradeInfo", grade_records, resolutions
    )
    conn.commit()
    message = (
        f"样品数据保存成功，样品新增 {sample_inserted} 条/更新 {sample_updated} 条，"
        f"品位新增 {grade_inserted} 条/更新 {grade_updated} 条"
    )
    skipped_total = sample_skipped + grade_skipped
    if skipped_total:
        message += f"，跳过 {skipped_total} 条"
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
        payload = json.loads(args.data_json)
        samples = payload.get("samples", [])
        grades = payload.get("grades", [])
        borehole_id = args.borehole_id.strip()

        conn = get_connection()
        ensure_schema(conn)
        area_id = resolve_area_id(conn, borehole_id, args.area_id.strip())

        legacy_extra_fields = parse_extra_fields_json(args.extra_data_json)
        sample_records = []
        for sample in samples:
            if legacy_extra_fields and not sample.get("extra_data"):
                sample = dict(sample)
                sample["extra_data"] = legacy_extra_fields
            sample_records.append(build_sample_record(borehole_id, area_id, sample))
        grade_records = [build_grade_record(grade) for grade in grades]

        validate_payload(conn, borehole_id, samples, grades)

        if args.analyze:
            conflicts = analyze(conn, sample_records, grade_records)
            conn.close()
            print(json.dumps(success_response("分析完成", conflicts=conflicts), ensure_ascii=False))
            return

        resolutions = load_conflict_resolutions(args.conflict_resolutions)
        message = save(conn, sample_records, grade_records, resolutions)
        conn.close()
        print(json.dumps(success_response(message), ensure_ascii=False))
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
