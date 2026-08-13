"""Batch import drill data with configurable column mapping and FK validation."""

import argparse
import json
import os
import shutil
import sqlite3
import sys
from datetime import datetime

import pandas as pd

from db_common import DB_PATH, LOGS_DIR, EXTRA_DATA_COLUMN, IMPORT_TIME_COLUMN, ensure_logs_dir, ensure_schema, get_connection, stamp_drill_hole_import_time
from file_reader import read_dataframe, validate_file_path

RAW_DATA_DIR = r"D:\Desktop\MiniMineUI\data\raw"

TABLE_COLUMNS = {
    "DrillHoleInfo": [
        "borehole_id",
        "area_id",
        "x_coord",
        "y_coord",
        "z_coord",
        "total_depth",
        "azimuth",
        "dip_angle",
        EXTRA_DATA_COLUMN,
        IMPORT_TIME_COLUMN,
    ],
    "InclineInfo": [
        "borehole_id",
        "point_id",
        "area_id",
        "point_depth",
        "deviation_angle",
        "azimuth",
        EXTRA_DATA_COLUMN,
    ],
    "StrataInfo": [
        "borehole_id",
        "layer_order",
        "area_id",
        "layer_no",
        "bottom_depth",
        "rock_name",
        "dip_angle",
        EXTRA_DATA_COLUMN,
    ],
    "SampleRecord": [
        "sample_id",
        "borehole_id",
        "area_id",
        "start_depth",
        "end_depth",
        "sample_length",
        "core_length",
        "sample_type",
        EXTRA_DATA_COLUMN,
    ],
    "GradeInfo": ["sample_id", "element_name", "grade_value", EXTRA_DATA_COLUMN],
}

REQUIRED_FIELDS = {
    "DrillHoleInfo": ["borehole_id"],
    "InclineInfo": ["borehole_id", "point_id"],
    "StrataInfo": ["borehole_id", "layer_order"],
    "SampleRecord": ["sample_id"],
    "GradeInfo": ["sample_id", "element_name"],
}

REQUIRED_ROW_FIELDS = {
    "DrillHoleInfo": ["borehole_id", "x_coord", "y_coord", "z_coord", "total_depth"],
    "InclineInfo": ["borehole_id", "point_id"],
    "StrataInfo": ["borehole_id", "layer_order"],
    "SampleRecord": ["sample_id", "borehole_id"],
    "GradeInfo": ["sample_id", "element_name", "grade_value"],
}

CHILD_TABLES_REQUIRE_BOREHOLE = {"InclineInfo", "StrataInfo", "SampleRecord"}
IGNORE_TARGETS = {"ignore", "忽略", "__ignore__"}

VALID_CONFLICT_ACTIONS = {"skip", "overwrite", "merge"}

TABLE_PK = {
    "DrillHoleInfo": ["borehole_id"],
    "InclineInfo": ["borehole_id", "point_id"],
    "StrataInfo": ["borehole_id", "layer_order"],
    "SampleRecord": ["sample_id"],
    "GradeInfo": ["sample_id", "element_name"],
}

TABLE_COMPARE_FIELDS = {
    "DrillHoleInfo": ["area_id", "x_coord", "y_coord", "z_coord", "total_depth", "azimuth", "dip_angle"],
    "InclineInfo": ["point_depth", "deviation_angle", "azimuth"],
    "StrataInfo": ["bottom_depth", "rock_name", "dip_angle"],
    "SampleRecord": ["start_depth", "end_depth", "sample_length", "core_length", "sample_type"],
    "GradeInfo": ["grade_value"],
}

# Single-entry UI mandatory fields (red asterisk): empty means "keep existing", never NULL overwrite.
SINGLE_ENTRY_MANDATORY_FIELDS = {
    "DrillHoleInfo": ["x_coord", "y_coord", "z_coord", "total_depth"],
    "InclineInfo": ["point_id", "point_depth"],
    "StrataInfo": ["layer_order", "bottom_depth"],
    "SampleRecord": ["sample_id", "start_depth", "end_depth"],
    "GradeInfo": ["element_name", "grade_value"],
}


def is_mandatory_preserve_field(table_name, field_name):
    return field_name in SINGLE_ENTRY_MANDATORY_FIELDS.get(table_name, [])


def apply_single_entry_overwrite(existing, new_data, table_name):
    write_fields = TABLE_COLUMNS[table_name]
    result = dict(existing)
    for field_name in write_fields:
        if field_name == IMPORT_TIME_COLUMN:
            continue
        if field_name not in new_data:
            continue
        new_value = new_data[field_name]
        if is_mandatory_preserve_field(table_name, field_name) and is_empty_value(new_value):
            continue
        result[field_name] = new_value
    return result

TABLE_DISPLAY_NAMES = {
    "DrillHoleInfo": "钻孔概况表",
    "InclineInfo": "测斜记录表",
    "StrataInfo": "地层分层表",
    "SampleRecord": "样品记录表",
    "GradeInfo": "品位信息表",
}

FIELD_LABELS = {
    "borehole_id": "钻孔编号",
    "area_id": "勘探区编号",
    "x_coord": "X坐标",
    "y_coord": "Y坐标",
    "z_coord": "Z坐标",
    "total_depth": "终孔深度",
    "azimuth": "方位角",
    "dip_angle": "倾角",
    "point_id": "测点号",
    "point_depth": "测点深度",
    "deviation_angle": "偏斜角采用值",
    "layer_order": "岩层序号",
    "layer_no": "分层号",
    "bottom_depth": "岩石分层孔深（底深）",
    "rock_name": "岩石全名",
    "sample_id": "样品编号",
    "start_depth": "采样起始孔深",
    "end_depth": "采样终止孔深",
    "sample_length": "样长",
    "core_length": "岩矿心长度",
    "sample_type": "样品类型",
    "element_name": "元素名称",
    "grade_value": "元素品位值",
    EXTRA_DATA_COLUMN: "扩展字段",
    IMPORT_TIME_COLUMN: "导入时间",
}

TABLE_FIELD_LABELS = {
    "DrillHoleInfo": {
        "azimuth": "钻孔方位角",
        "dip_angle": "钻孔倾角",
    },
    "InclineInfo": {
        "azimuth": "方位角采用值",
        "deviation_angle": "偏斜角采用值",
    },
    "StrataInfo": {
        "dip_angle": "岩层倾角",
    },
}


def resolve_field_label(table_name, field_name):
    if table_name and table_name in TABLE_FIELD_LABELS:
        label = TABLE_FIELD_LABELS[table_name].get(field_name)
        if label:
            return label
    return FIELD_LABELS.get(field_name, field_name)

NUMERIC_COMPARE_FIELDS = {
    "x_coord",
    "y_coord",
    "z_coord",
    "total_depth",
    "azimuth",
    "dip_angle",
    "point_depth",
    "deviation_angle",
    "bottom_depth",
    "start_depth",
    "end_depth",
    "sample_length",
    "core_length",
    "grade_value",
}

NON_NEGATIVE_NUMERIC_FIELDS = {
    "total_depth",
    "point_depth",
    "bottom_depth",
    "start_depth",
    "end_depth",
    "sample_length",
    "core_length",
    "grade_value",
}

MAX_LOG_BYTES = 10 * 1024 * 1024


class ErrorRecord:
    def __init__(self, row_num, field_name, original_value, reason, target_to_source=None):
        self.row_num = row_num
        self.field_name = field_name or ""
        self.original_value = "" if original_value is None else str(original_value)
        self.reason = reason
        self.timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        if self.field_name and target_to_source:
            self.display_field_name = target_to_source.get(
                self.field_name,
                FIELD_LABELS.get(self.field_name, self.field_name),
            )
        elif self.field_name:
            self.display_field_name = FIELD_LABELS.get(self.field_name, self.field_name)
        else:
            self.display_field_name = "（整行）"

    def to_dict(self):
        return {
            "row_num": self.row_num,
            "field_name": self.display_field_name,
            "original_value": self.original_value,
            "reason": self.reason,
            "timestamp": self.timestamp,
        }

    def format_log_line(self):
        return (
            f'[{self.timestamp}] 行 {self.row_num}: 字段 "{self.display_field_name}" 错误 — '
            f'{self.reason} (原始值: "{self.original_value}")'
        )


def build_target_to_source_map(mapping):
    target_to_source = {}
    for source_col, target_col in mapping.items():
        if is_ignore_target(target_col):
            continue
        if target_col not in target_to_source:
            target_to_source[target_col] = source_col
    return target_to_source


def to_scalar(value):
    """Extract one Python scalar; avoid ambiguous truth tests on Series."""
    if value is None:
        return None
    if isinstance(value, pd.Series):
        if value.empty:
            return None
        return value.iloc[0]
    if isinstance(value, pd.DataFrame):
        if value.empty:
            return None
        return value.iloc[0, 0]
    if hasattr(value, "ndim") and getattr(value, "ndim", 0) > 0:
        try:
            flat = value.flat if hasattr(value, "flat") else None
            if flat is not None:
                return flat[0]
        except (TypeError, ValueError, IndexError):
            pass
    try:
        if hasattr(value, "item") and not isinstance(value, (str, bytes, dict, list, tuple)):
            return value.item()
    except (ValueError, TypeError):
        pass
    return value


def row_get(row, field_name, default=None):
    """Read one cell from a DataFrame row as a scalar."""
    if field_name not in row.index:
        return default
    return to_scalar(row[field_name])


def series_get(row, key, default=None):
    """Read one value from a Series (e.g. iterrows/raw row) as a scalar."""
    if key not in row.index:
        return default
    return to_scalar(row[key])


def is_na_scalar(value):
    value = to_scalar(value)
    if value is None:
        return True
    try:
        return bool(pd.isna(value))
    except (TypeError, ValueError):
        return False


def is_ignore_target(target):
    return target is None or str(target).strip().lower() in IGNORE_TARGETS


def is_empty_value(value):
    value = to_scalar(value)
    if value is None or is_na_scalar(value):
        return True
    if isinstance(value, str):
        return value.strip() == ""
    return False


def load_mapping(mapping_file):
    with open(mapping_file, "r", encoding="utf-8") as file:
        data = json.load(file)
    return data.get("columns", {}), data.get("grade_element_mapping", {})


def resolve_ignored_columns(file_columns, mapping):
    ignored = []
    for column in file_columns:
        if is_ignore_target(mapping.get(column)):
            ignored.append(column)
    return ignored


def json_safe_value(value):
    value = to_scalar(value)
    if value is None or is_na_scalar(value):
        return None
    if isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if value == int(value):
            return int(value)
        return value
    text = str(value).strip()
    return text if text else None


def build_extra_data(raw_row, ignored_columns):
    extra = {}
    for column in ignored_columns:
        value = json_safe_value(series_get(raw_row, column))
        if value is not None:
            extra[column] = value
    if not extra:
        return None
    return json.dumps(extra, ensure_ascii=False)


def get_grade_value_columns(mapping, file_columns):
    columns = []
    for column in file_columns:
        target = mapping.get(column)
        if target == "grade_value" and not is_ignore_target(target):
            columns.append(column)
    return columns


def should_pivot_grade(mapping, grade_element_mapping, file_columns):
    grade_cols = get_grade_value_columns(mapping, file_columns)
    if not grade_cols:
        return False
    if grade_element_mapping:
        return True
    return len(grade_cols) > 1


def pivot_grade_dataframe(df_raw, mapping, grade_element_mapping):
    file_columns = list(df_raw.columns)
    grade_cols = get_grade_value_columns(mapping, file_columns)
    if not grade_cols:
        return df_raw, False

    if not grade_element_mapping:
        raise ValueError(
            "检测到多个品位列映射到 grade_value，请在界面「元素名称映射」中为每列指定元素名称"
        )

    missing_elements = [col for col in grade_cols if not str(grade_element_mapping.get(col, "")).strip()]
    if missing_elements:
        raise ValueError(
            "以下品位列未指定元素名称: " + ", ".join(missing_elements)
        )

    base_rename = {
        source_col: target_col
        for source_col, target_col in mapping.items()
        if not is_ignore_target(target_col)
        and target_col != "grade_value"
        and source_col in df_raw.columns
    }

    records = []
    source_indices = []
    for idx, raw_row in df_raw.iterrows():
        base = {}
        for source_col, target_col in base_rename.items():
            base[target_col] = series_get(raw_row, source_col)

        for grade_col in grade_cols:
            value = series_get(raw_row, grade_col)
            if is_empty_value(value):
                continue
            row = dict(base)
            row["element_name"] = str(grade_element_mapping[grade_col]).strip()
            row["grade_value"] = value
            records.append(row)
            source_indices.append(idx)

    if not records:
        raise ValueError("品位数据为空，请检查文件内容和元素映射")

    df_pivoted = pd.DataFrame(records)
    df_pivoted.index = source_indices
    return df_pivoted, True


def apply_mapping(df, mapping):
    rename_map = {}
    for source_col, target_col in mapping.items():
        if not is_ignore_target(target_col):
            rename_map[source_col] = target_col

    missing_source_cols = [col for col in rename_map if col not in df.columns]
    if missing_source_cols:
        raise ValueError(f"映射配置中的列在文件中不存在: {', '.join(missing_source_cols)}")

    return df.rename(columns=rename_map)


def resolve_duplicate_sample_ids(df):
    """Auto-rename duplicate sample_id: first row keeps original, rest get _borehole_id suffix."""
    if "sample_id" not in df.columns or "borehole_id" not in df.columns:
        return df, None, {}

    result = df.copy()
    normalized_ids = result["sample_id"].apply(
        lambda value: normalize_id(value) if not is_empty_value(value) else ""
    )
    valid_mask = normalized_ids != ""
    if not valid_mask.any():
        return df, None, {}

    dup_index = normalized_ids.groupby(normalized_ids).cumcount()
    dup_counts = normalized_ids[valid_mask].value_counts()
    duplicate_original_ids = set(dup_counts[dup_counts > 1].index)
    if not duplicate_original_ids:
        return df, None, {}

    rename_map = {}
    rename_mask = dup_index > 0
    for idx in result.index:
        original_id = normalize_id(result.at[idx, "sample_id"])
        if original_id not in duplicate_original_ids:
            continue
        borehole_id = normalize_id(result.at[idx, "borehole_id"])
        key = f"{original_id}|{borehole_id}"
        if int(dup_index.at[idx]) > 0 and borehole_id:
            new_id = f"{original_id}_{borehole_id}"
            result.at[idx, "sample_id"] = new_id
            rename_map[key] = new_id
        else:
            rename_map[key] = original_id

    examples = []
    for original_id in list(duplicate_original_ids)[:5]:
        group_mask = normalized_ids == original_id
        records = []
        for idx in result.index[group_mask]:
            records.append(
                {
                    "sample_id": normalize_id(result.at[idx, "sample_id"]),
                    "borehole_id": normalize_id(result.at[idx, "borehole_id"]),
                }
            )
        examples.append(
            {
                "original_sample_id": original_id,
                "records": records,
            }
        )

    info = {
        "total_samples": len(result),
        "duplicate_original_ids": len(duplicate_original_ids),
        "renamed_records": int(rename_mask.sum()),
        "examples": examples,
    }
    return result, info, rename_map


def lookup_sample_id_in_db(cursor, borehole_id, raw_sample_id):
    """Match grade row to SampleRecord using borehole_id + sample_id prefix."""
    if not borehole_id or not raw_sample_id:
        return None
    cursor.execute(
        """
        SELECT sample_id FROM SampleRecord
        WHERE borehole_id = ? AND sample_id LIKE ?
        ORDER BY LENGTH(sample_id) ASC
        LIMIT 1
        """,
        (borehole_id, f"{raw_sample_id}%"),
    )
    row = cursor.fetchone()
    return normalize_id(row[0]) if row else None


def resolve_grade_sample_ids_from_db(conn, df):
    """Resolve GradeInfo sample_id by querying SampleRecord (no local rename logic)."""
    if "sample_id" not in df.columns:
        return df, None

    result = df.copy()
    if "borehole_id" not in df.columns:
        return result, {
            "grade_synced_from_db": False,
            "message": "品位表需映射钻孔编号(borehole_id)才能关联样品记录",
        }

    cursor = conn.cursor()
    resolved_count = 0
    examples = []

    for idx, row in result.iterrows():
        raw_sample_id = normalize_id(row_get(row, "sample_id"))
        borehole_id = normalize_id(row_get(row, "borehole_id"))
        if not raw_sample_id or not borehole_id:
            continue

        resolved_id = lookup_sample_id_in_db(cursor, borehole_id, raw_sample_id)
        if not resolved_id:
            continue

        if resolved_id != raw_sample_id:
            resolved_count += 1
            if len(examples) < 5:
                examples.append(
                    {
                        "original_sample_id": raw_sample_id,
                        "borehole_id": borehole_id,
                        "resolved_sample_id": resolved_id,
                    }
                )
        result.at[idx, "sample_id"] = resolved_id

    if resolved_count == 0 and not examples:
        return result, None

    return result, {
        "grade_synced_from_db": True,
        "resolved_records": resolved_count,
        "total_records": len(result),
        "examples": examples,
    }


def process_sample_id_renames(df, target_table):
    if target_table != "SampleRecord":
        return df, None
    df, sample_rename_info, _rename_map = resolve_duplicate_sample_ids(df)
    return df, sample_rename_info


def validate_required_fields(table_name, df):
    required = REQUIRED_FIELDS.get(table_name, [])
    missing = [field for field in required if field not in df.columns]
    if missing:
        raise ValueError(f"缺少必要字段映射: {', '.join(missing)}")


def normalize_id(value):
    value = to_scalar(value)
    if value is None or is_na_scalar(value):
        return ""
    return str(value).strip()


def format_original_value(value):
    value = to_scalar(value)
    if value is None or is_na_scalar(value):
        return ""
    return str(value).strip()


def load_borehole_ids(conn):
    cursor = conn.cursor()
    cursor.execute("SELECT borehole_id FROM DrillHoleInfo")
    return {normalize_id(row[0]) for row in cursor.fetchall() if normalize_id(row[0])}


def load_sample_ids(conn):
    cursor = conn.cursor()
    cursor.execute("SELECT sample_id FROM SampleRecord")
    return {normalize_id(row[0]) for row in cursor.fetchall() if normalize_id(row[0])}


def convert_value(column, value):
    value = to_scalar(value)
    if value is None or is_na_scalar(value):
        return None
    if column in ("point_id", "layer_order", "sample_type"):
        return int(float(value))
    if column in (
        "x_coord",
        "y_coord",
        "z_coord",
        "total_depth",
        "azimuth",
        "dip_angle",
        "point_depth",
        "deviation_angle",
        "bottom_depth",
        "start_depth",
        "end_depth",
        "sample_length",
        "core_length",
        "grade_value",
    ):
        return float(value)
    return str(value).strip()


def load_borehole_total_depths(conn):
    cursor = conn.cursor()
    cursor.execute("SELECT borehole_id, total_depth FROM DrillHoleInfo")
    depths = {}
    for borehole_id, total_depth in cursor.fetchall():
        normalized = normalize_id(borehole_id)
        if normalized and total_depth is not None:
            try:
                depths[normalized] = float(total_depth)
            except (TypeError, ValueError):
                continue
    return depths


def build_shape_error_records(bad_line_records, target_to_source=None):
    errors = []
    for info in bad_line_records or []:
        expected = info.get("expected", 0)
        actual = info.get("actual", 0)
        errors.append(
            ErrorRecord(
                info.get("row_num", 0),
                "",
                info.get("raw", ""),
                f"列数不匹配，期望 {expected} 列，实际 {actual} 列",
                target_to_source,
            )
        )
    return errors


def validate_required_row_fields(table_name, row, row_num, target_to_source=None):
    errors = []
    for field_name in REQUIRED_ROW_FIELDS.get(table_name, []):
        if field_name not in row.index:
            continue
        value = row_get(row, field_name)
        if is_empty_value(value):
            errors.append(
                ErrorRecord(
                    row_num,
                    field_name,
                    format_original_value(value),
                    "必填字段为空",
                    target_to_source,
                )
            )
    return errors


def validate_drill_hole_coordinates(row, row_num, target_to_source=None):
    errors = []
    x_value = row_get(row, "x_coord") if "x_coord" in row.index else None
    y_value = row_get(row, "y_coord") if "y_coord" in row.index else None
    z_value = row_get(row, "z_coord") if "z_coord" in row.index else None

    if all(not is_empty_value(value) for value in (x_value, y_value, z_value)):
        try:
            if float(x_value) == 0 and float(y_value) == 0 and float(z_value) == 0:
                errors.append(
                    ErrorRecord(
                        row_num,
                        "x_coord",
                        "0",
                        "钻孔坐标不能全为 0，请检查录入数据",
                        target_to_source,
                    )
                )
        except (TypeError, ValueError):
            pass
    return errors


def append_numeric_field_errors(row, row_num, field_names, target_to_source=None):
    errors = []
    for field_name in field_names:
        if field_name not in row.index:
            continue
        value = row_get(row, field_name)
        if is_empty_value(value):
            continue
        try:
            numeric_value = float(value)
            if field_name in NON_NEGATIVE_NUMERIC_FIELDS and numeric_value < 0:
                errors.append(
                    ErrorRecord(
                        row_num,
                        field_name,
                        format_original_value(value),
                        "数值超出合理范围",
                        target_to_source,
                    )
                )
        except (TypeError, ValueError):
            errors.append(
                ErrorRecord(
                    row_num,
                    field_name,
                    format_original_value(value),
                    "数值格式无效",
                    target_to_source,
                )
            )
    return errors


def validate_row(table_name, row, row_num, borehole_ids, sample_ids, target_to_source=None,
                 borehole_total_depths=None, incline_last_depths=None):
    errors = []

    if table_name == "DrillHoleInfo":
        errors.extend(validate_drill_hole_coordinates(row, row_num, target_to_source))
        errors.extend(
            append_numeric_field_errors(
                row,
                row_num,
                ["x_coord", "y_coord", "z_coord", "total_depth", "azimuth", "dip_angle"],
                target_to_source,
            )
        )

    if table_name in CHILD_TABLES_REQUIRE_BOREHOLE:
        borehole_id = normalize_id(row_get(row, "borehole_id"))
        if borehole_id and borehole_id not in borehole_ids:
            errors.append(
                ErrorRecord(
                    row_num,
                    "borehole_id",
                    borehole_id,
                    "钻孔编号在钻孔概况表中不存在",
                    target_to_source,
                )
            )

    if table_name == "GradeInfo":
        sample_id = normalize_id(row_get(row, "sample_id"))
        if sample_id and sample_id not in sample_ids:
            errors.append(
                ErrorRecord(
                    row_num,
                    "sample_id",
                    sample_id,
                    "样品编号在样品记录表中不存在",
                    target_to_source,
                )
            )
        errors.extend(
            append_numeric_field_errors(row, row_num, ["grade_value"], target_to_source)
        )

    if table_name == "InclineInfo":
        errors.extend(
            append_numeric_field_errors(row, row_num, ["point_depth"], target_to_source)
        )
        borehole_id = normalize_id(row_get(row, "borehole_id"))
        point_depth = row_get(row, "point_depth")
        if (
            incline_last_depths is not None
            and borehole_id
            and not is_empty_value(point_depth)
        ):
            try:
                depth_value = float(point_depth)
                last_depth = incline_last_depths.get(borehole_id)
                if last_depth is not None and depth_value <= last_depth:
                    errors.append(
                        ErrorRecord(
                            row_num,
                            "point_depth",
                            format_original_value(point_depth),
                            "测斜深度应随测点递增",
                            target_to_source,
                        )
                    )
                incline_last_depths[borehole_id] = depth_value
            except (TypeError, ValueError):
                pass

    if table_name == "StrataInfo":
        errors.extend(
            append_numeric_field_errors(row, row_num, ["bottom_depth"], target_to_source)
        )
        borehole_id = normalize_id(row_get(row, "borehole_id"))
        bottom_depth = row_get(row, "bottom_depth")
        if (
            borehole_total_depths is not None
            and borehole_id
            and not is_empty_value(bottom_depth)
        ):
            try:
                bottom_value = float(bottom_depth)
                total_depth = borehole_total_depths.get(borehole_id)
                if total_depth is not None and bottom_value > total_depth:
                    errors.append(
                        ErrorRecord(
                            row_num,
                            "bottom_depth",
                            format_original_value(bottom_depth),
                            "岩石分层孔深（底深）超过钻孔终孔深度",
                            target_to_source,
                        )
                    )
            except (TypeError, ValueError):
                pass

    if table_name == "SampleRecord":
        errors.extend(
            append_numeric_field_errors(
                row,
                row_num,
                ["start_depth", "end_depth", "sample_length", "core_length"],
                target_to_source,
            )
        )

    return errors


def convert_row_values(available_columns, row, row_num, extra_data=None):
    values = []
    for column in available_columns:
        if column == EXTRA_DATA_COLUMN:
            values.append(extra_data)
            continue
        raw_value = row_get(row, column)
        try:
            values.append(convert_value(column, raw_value))
        except (TypeError, ValueError) as exc:
            raise ErrorRecord(
                row_num,
                column,
                format_original_value(raw_value),
                "数值格式无效",
                None,
            ) from exc
    return values


def format_display_value(value):
    if is_empty_value(value):
        return ""
    if isinstance(value, float):
        if value == int(value):
            return str(int(value))
        return f"{value:.2f}"
    return str(value)


def parse_extra_data_dict(json_text):
    if not json_text:
        return {}
    try:
        data = json.loads(json_text)
    except json.JSONDecodeError:
        return {}
    if not isinstance(data, dict):
        return {}
    return data


def generic_values_equal(left, right):
    if is_empty_value(left) and is_empty_value(right):
        return True
    if is_empty_value(left) or is_empty_value(right):
        return False
    return str(left).strip() == str(right).strip()


def describe_overwrite_effect(existing_value, new_value):
    if is_empty_value(new_value):
        if is_empty_value(existing_value):
            return "保持为空"
        return "将清空"
    return f"将改为 {format_display_value(new_value)}"


def describe_merge_effect(existing_value, new_value):
    if is_empty_value(new_value):
        return "保留不变"
    if generic_values_equal(existing_value, new_value):
        return "保留不变"
    return f"将改为 {format_display_value(new_value)}"


def build_field_difference(field_name, field_label, existing_value, new_value):
    return {
        "field_name": field_name,
        "field_label": field_label,
        "existing_value": format_display_value(existing_value),
        "new_value": format_display_value(new_value),
        "overwrite_effect": describe_overwrite_effect(existing_value, new_value),
        "merge_effect": describe_merge_effect(existing_value, new_value),
    }


def get_extra_data_differences(existing_json, new_json):
    existing_dict = parse_extra_data_dict(existing_json)
    new_dict = parse_extra_data_dict(new_json)
    all_keys = sorted(set(existing_dict.keys()) | set(new_dict.keys()))
    differences = []

    for key in all_keys:
        in_existing = key in existing_dict
        in_new = key in new_dict
        existing_value = existing_dict.get(key) if in_existing else None
        new_value = new_dict.get(key) if in_new else None

        if in_existing and in_new and generic_values_equal(existing_value, new_value):
            continue

        field_name = f"{EXTRA_DATA_COLUMN}.{key}"
        field_label = f"扩展字段 ({key})"

        if in_existing and not in_new:
            differences.append(
                {
                    "field_name": field_name,
                    "field_label": field_label,
                    "existing_value": format_display_value(existing_value),
                    "new_value": "",
                    "overwrite_effect": "将删除此键",
                    "merge_effect": "保留不变",
                }
            )
            continue

        if in_new and not in_existing:
            if is_empty_value(new_value):
                continue
            differences.append(
                build_field_difference(field_name, field_label, None, new_value)
            )
            continue

        differences.append(
            build_field_difference(field_name, field_label, existing_value, new_value)
        )

    return differences


def collect_record_differences(table_name, existing, new_data):
    differences = []
    for field_name in TABLE_COMPARE_FIELDS.get(table_name, []):
        if field_name not in new_data:
            continue
        existing_value = existing.get(field_name)
        new_value = new_data.get(field_name)
        if is_empty_value(new_value):
            if is_empty_value(existing_value):
                continue
            if is_mandatory_preserve_field(table_name, field_name):
                continue
            field_display_label = resolve_field_label(table_name, field_name)
            differences.append(
                build_field_difference(field_name, field_display_label, existing_value, new_value)
            )
            continue
        if values_equal(field_name, existing_value, new_value):
            continue
        field_display_label = resolve_field_label(table_name, field_name)
        differences.append(
            build_field_difference(field_name, field_display_label, existing_value, new_value)
        )

    differences.extend(
        get_extra_data_differences(
            existing.get(EXTRA_DATA_COLUMN),
            new_data.get(EXTRA_DATA_COLUMN),
        )
    )
    return differences


def normalize_compare_value(field_name, value):
    if is_empty_value(value):
        return None
    if field_name in NUMERIC_COMPARE_FIELDS:
        return round(float(value), 4)
    if field_name in ("point_id", "layer_order", "sample_type"):
        return int(float(value))
    return str(value).strip()


def values_equal(field_name, left, right):
    return normalize_compare_value(field_name, left) == normalize_compare_value(field_name, right)


def normalize_pk_part(field_name, value):
    if is_empty_value(value):
        return ""
    if field_name in ("point_id", "layer_order", "sample_type"):
        return str(int(float(value)))
    return normalize_id(value)


def make_record_key(table_name, data):
    return "|".join(normalize_pk_part(field, data.get(field)) for field in TABLE_PK[table_name])


def format_record_label(table_name, data):
    if table_name == "DrillHoleInfo":
        return f"钻孔 {data.get('borehole_id')}"
    if table_name == "InclineInfo":
        return f"测斜 {data.get('borehole_id')} / 测点 {data.get('point_id')}"
    if table_name == "StrataInfo":
        return f"地层 {data.get('borehole_id')} / 岩层序号 {data.get('layer_order')}"
    if table_name == "SampleRecord":
        return f"样品 {data.get('sample_id')}"
    if table_name == "GradeInfo":
        return f"品位 {data.get('sample_id')} / {data.get('element_name')}"
    return make_record_key(table_name, data)


def format_conflict_description(table_name, data):
    table_label = TABLE_DISPLAY_NAMES.get(table_name, table_name)
    if table_name == "DrillHoleInfo":
        subject = f"钻孔编号 {data.get('borehole_id')}"
    elif table_name == "InclineInfo":
        subject = f"钻孔编号 {data.get('borehole_id')}（测点 {data.get('point_id')}）"
    elif table_name == "StrataInfo":
        subject = f"钻孔编号 {data.get('borehole_id')}（岩层序号 {data.get('layer_order')}）"
    elif table_name == "SampleRecord":
        subject = f"样品编号 {data.get('sample_id')}"
    elif table_name == "GradeInfo":
        subject = f"样品编号 {data.get('sample_id')} / 元素 {data.get('element_name')}"
    else:
        subject = format_record_label(table_name, data)
    return f"{subject} 已存在于【{table_label}】中"


def get_effective_compare_fields(table_name, row):
    configured = TABLE_COMPARE_FIELDS.get(table_name, [])
    return [field_name for field_name in configured if field_name in row.index]


def load_all_records(conn, table_name):
    columns = TABLE_COLUMNS[table_name]
    cursor = conn.cursor()
    cursor.execute(f"SELECT {', '.join(columns)} FROM {table_name}")
    records = {}
    for row in cursor.fetchall():
        record = {columns[index]: row[index] for index in range(len(columns))}
        key = make_record_key(table_name, record)
        if key and not all(part == "" for part in key.split("|")):
            records[key] = record
    return records


def get_field_differences(table_name, existing, row, extra_data=None):
    new_data = build_record_dict(table_name, row, extra_data)
    return collect_record_differences(table_name, existing, new_data)


def merge_extra_data_json(existing_json, new_json):
    existing_dict = parse_extra_data_dict(existing_json)
    new_dict = parse_extra_data_dict(new_json)

    if not new_dict:
        if not existing_dict:
            return None
        return json.dumps(existing_dict, ensure_ascii=False)

    merged = dict(existing_dict)
    for key, value in new_dict.items():
        if not is_empty_value(value):
            merged[key] = value
    if not merged:
        return None
    return json.dumps(merged, ensure_ascii=False)


def build_record_dict(table_name, row, extra_data):
    data = {}
    for field_name in TABLE_COLUMNS[table_name]:
        if field_name == EXTRA_DATA_COLUMN:
            data[field_name] = extra_data
            continue
        if field_name not in row.index:
            data[field_name] = None
            continue
        try:
            data[field_name] = convert_value(field_name, row_get(row, field_name))
        except (TypeError, ValueError):
            data[field_name] = format_original_value(row_get(row, field_name)) or None
    return data


def merge_records(existing, new_data, write_fields):
    merged = dict(existing)
    for field_name in write_fields:
        if field_name == EXTRA_DATA_COLUMN:
            merged[field_name] = merge_extra_data_json(existing.get(field_name), new_data.get(field_name))
            continue
        new_value = new_data.get(field_name)
        if not is_empty_value(new_value):
            merged[field_name] = new_value
        else:
            merged[field_name] = existing.get(field_name)
    return merged


def analyze_conflicts(conn, table_name, df, df_raw, ignored_columns):
    existing_records = load_all_records(conn, table_name)
    conflicts = []
    seen_keys = set()

    for index, row in df.iterrows():
        if index in df_raw.index:
            raw_row = df_raw.loc[index]
        else:
            raw_row = df_raw.iloc[int(index)]
        extra_data = build_extra_data(raw_row, ignored_columns)
        new_data = build_record_dict(table_name, row, extra_data)
        record_key = make_record_key(table_name, new_data)
        if not record_key or record_key in seen_keys:
            continue

        existing = existing_records.get(record_key)
        if not existing:
            continue

        differences = collect_record_differences(table_name, existing, new_data)
        if differences:
            conflicts.append(
                {
                    "row_num": int(index) + 2,
                    "record_key": record_key,
                    "record_label": format_record_label(table_name, new_data),
                    "conflict_description": format_conflict_description(table_name, new_data),
                    "target_table": table_name,
                    "differences": differences,
                }
            )
            seen_keys.add(record_key)

    conflicts.sort(key=lambda item: item["row_num"])
    return conflicts


def load_conflict_resolutions(resolution_file):
    if not resolution_file:
        return {}
    with open(resolution_file, "r", encoding="utf-8") as file:
        data = json.load(file)
    resolutions = {}
    for record_key, action in data.items():
        action_text = str(action).strip().lower()
        if action_text in VALID_CONFLICT_ACTIONS:
            resolutions[str(record_key)] = action_text
    return resolutions


def write_record(cursor, table_name, record):
    if table_name == "DrillHoleInfo":
        stamp_drill_hole_import_time(record)
    columns = TABLE_COLUMNS[table_name]
    placeholders = ", ".join(["?"] * len(columns))
    columns_str = ", ".join(columns)
    sql = f"INSERT OR REPLACE INTO {table_name} ({columns_str}) VALUES ({placeholders})"
    values = [record.get(column) for column in columns]
    cursor.execute(sql, values)


def import_table_rows(conn, table_name, df, df_raw, ignored_columns, conflict_resolutions,
                      target_to_source=None):
    existing_records = load_all_records(conn, table_name)
    write_fields = TABLE_COLUMNS[table_name]

    borehole_ids = set()
    sample_ids = set()
    borehole_total_depths = {}
    if table_name in CHILD_TABLES_REQUIRE_BOREHOLE:
        borehole_ids = load_borehole_ids(conn)
    if table_name == "StrataInfo":
        borehole_total_depths = load_borehole_total_depths(conn)
    if table_name == "GradeInfo":
        sample_ids = load_sample_ids(conn)

    incline_last_depths = {} if table_name == "InclineInfo" else None

    cursor = conn.cursor()
    total = len(df)
    inserted = 0
    overwritten = 0
    merged = 0
    failed = 0
    skipped = 0
    error_records = []

    for index, row in df.iterrows():
        row_num = int(index) + 2
        row_errors = validate_required_row_fields(table_name, row, row_num, target_to_source)
        row_errors.extend(
            validate_row(
                table_name,
                row,
                row_num,
                borehole_ids,
                sample_ids,
                target_to_source,
                borehole_total_depths,
                incline_last_depths,
            )
        )
        if row_errors:
            failed += 1
            error_records.extend(row_errors)
            continue

        try:
            if index in df_raw.index:
                raw_row = df_raw.loc[index]
            else:
                raw_row = df_raw.iloc[int(index)]
            extra_data = build_extra_data(raw_row, ignored_columns)
            new_data = build_record_dict(table_name, row, extra_data)
            record_key = make_record_key(table_name, new_data)
            existing = existing_records.get(record_key)

            if not existing:
                write_record(cursor, table_name, new_data)
                existing_records[record_key] = new_data
                inserted += 1
                continue

            differences = collect_record_differences(table_name, existing, new_data)
            if not differences:
                skipped += 1
                continue

            action = conflict_resolutions.get(record_key)
            if not action:
                failed += 1
                pk_field = TABLE_PK[table_name][0]
                error_records.append(
                    ErrorRecord(
                        row_num,
                        pk_field,
                        format_original_value(new_data.get(pk_field)),
                        "主键冲突，数据冲突未选择处理方式",
                        target_to_source,
                    )
                )
                continue

            if action == "skip":
                skipped += 1
                pk_field = TABLE_PK[table_name][0]
                error_records.append(
                    ErrorRecord(
                        row_num,
                        pk_field,
                        format_original_value(new_data.get(pk_field)),
                        "主键冲突，用户选择跳过",
                        target_to_source,
                    )
                )
                continue

            if action == "overwrite":
                write_record(cursor, table_name, new_data)
                existing_records[record_key] = new_data
                overwritten += 1
                continue

            merged_record = merge_records(existing, new_data, write_fields)
            write_record(cursor, table_name, merged_record)
            existing_records[record_key] = merged_record
            merged += 1
        except ErrorRecord as record:
            if target_to_source and record.field_name:
                record.display_field_name = target_to_source.get(
                    record.field_name,
                    FIELD_LABELS.get(record.field_name, record.field_name),
                )
            failed += 1
            error_records.append(record)
        except Exception as exc:
            failed += 1
            error_records.append(
                ErrorRecord(
                    row_num,
                    "",
                    "",
                    f"写入数据库失败（{exc}）",
                    target_to_source,
                )
            )

    conn.commit()
    return total, inserted, overwritten, merged, skipped, failed, error_records


def import_rows(conn, table_name, df, df_raw, ignored_columns, conflict_resolutions=None,
                target_to_source=None):
    return import_table_rows(
        conn,
        table_name,
        df,
        df_raw,
        ignored_columns,
        conflict_resolutions or {},
        target_to_source,
    )


def prepare_import_data(file_path, mapping_file, target_table):
    validate_file_path(file_path)
    if not os.path.exists(mapping_file):
        raise FileNotFoundError(f"映射文件不存在: {mapping_file}")
    if target_table not in TABLE_COLUMNS:
        raise ValueError(f"不支持的目标表: {target_table}")

    mapping, grade_element_mapping = load_mapping(mapping_file)
    if not mapping:
        raise ValueError("映射配置为空，请先配置字段映射")

    bad_line_records = []
    df_raw = read_dataframe(file_path, bad_line_records=bad_line_records)
    df_raw.columns = [str(col).strip() for col in df_raw.columns]
    file_columns = list(df_raw.columns)
    ignored_columns = resolve_ignored_columns(file_columns, mapping)
    target_to_source = build_target_to_source_map(mapping)
    shape_errors = build_shape_error_records(bad_line_records, target_to_source)

    pivoted = False
    if target_table == "GradeInfo" and should_pivot_grade(mapping, grade_element_mapping, file_columns):
        df_mapped, pivoted = pivot_grade_dataframe(df_raw, mapping, grade_element_mapping)
        grade_cols = set(get_grade_value_columns(mapping, file_columns))
        ignored_columns = [col for col in ignored_columns if col not in grade_cols]
    else:
        df_mapped = apply_mapping(df_raw, mapping)

    validate_required_fields(target_table, df_mapped)

    sample_rename_info = None
    if target_table == "SampleRecord":
        df_mapped, sample_rename_info = process_sample_id_renames(df_mapped, target_table)

    return mapping, df_raw, df_mapped, ignored_columns, sample_rename_info, shape_errors


def clean_old_logs(days=30):
    if not os.path.exists(LOGS_DIR):
        return

    now = datetime.now()
    for filename in os.listdir(LOGS_DIR):
        if not filename.startswith("import_errors_") or not filename.endswith(".log"):
            continue
        filepath = os.path.join(LOGS_DIR, filename)
        try:
            mtime = datetime.fromtimestamp(os.path.getmtime(filepath))
            if (now - mtime).days > days:
                os.remove(filepath)
        except OSError:
            pass


def generate_error_log_file(error_records, target_table):
    if not error_records:
        return ""

    ensure_logs_dir()
    clean_old_logs()

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(LOGS_DIR, f"import_errors_{timestamp}.log")

    header_text = (
        "导入错误日志\n"
        f"目标表: {target_table}\n"
        f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
        f"错误条数: {len(error_records)}\n"
        + "=" * 50
        + "\n\n"
    )
    lines = [record.format_log_line() for record in error_records]
    body_text = "\n".join(lines) + "\n"

    content = header_text + body_text
    encoded = content.encode("utf-8")
    if len(encoded) > MAX_LOG_BYTES:
        truncated_lines = []
        current_size = len(header_text.encode("utf-8"))
        notice = (
            f"\n[系统提示] 日志已超过 {MAX_LOG_BYTES // (1024 * 1024)}MB，"
            "后续错误记录已截断。请修正源文件后重新导入。\n"
        )
        budget = MAX_LOG_BYTES - current_size - len(notice.encode("utf-8"))
        for line in lines:
            line_bytes = (line + "\n").encode("utf-8")
            if current_size + len(line_bytes) > budget:
                break
            truncated_lines.append(line)
            current_size += len(line_bytes)
        body_text = "\n".join(truncated_lines) + notice
        content = header_text + body_text

    with open(log_path, "w", encoding="utf-8") as file:
        file.write(content)

    return log_path.replace("\\", "/")


def backup_original_file(file_path):
    """Copy imported source file to data/raw after successful DB commit."""
    os.makedirs(RAW_DATA_DIR, exist_ok=True)
    dst = os.path.join(RAW_DATA_DIR, os.path.basename(file_path))
    shutil.copy2(file_path, dst)
    return dst.replace("\\", "/")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--file_path", required=True)
    parser.add_argument("--mapping_file", required=True)
    parser.add_argument("--target_table", required=True)
    parser.add_argument("--analyze_conflicts", action="store_true")
    parser.add_argument("--conflict_resolutions", default="")
    args = parser.parse_args()

    ensure_logs_dir()

    try:
        mapping, df_raw, df_mapped, ignored_columns, sample_rename_info, shape_errors = prepare_import_data(
            args.file_path,
            args.mapping_file,
            args.target_table,
        )

        conn = get_connection()
        ensure_schema(conn)

        if args.target_table == "GradeInfo":
            df_mapped, grade_sync_info = resolve_grade_sample_ids_from_db(conn, df_mapped)
            if grade_sync_info:
                sample_rename_info = grade_sync_info

        if args.analyze_conflicts:
            conflicts = analyze_conflicts(
                conn,
                args.target_table,
                df_mapped,
                df_raw,
                ignored_columns,
            )
            conn.close()
            print(
                json.dumps(
                    {
                        "status": "success",
                        "conflicts": conflicts,
                        "table": args.target_table,
                        "shape_errors": [record.to_dict() for record in shape_errors],
                    },
                    ensure_ascii=False,
                )
            )
            return

        conflict_resolutions = load_conflict_resolutions(args.conflict_resolutions)
        target_to_source = build_target_to_source_map(mapping)
        total, inserted, overwritten, merged, skipped, failed, error_records = import_rows(
            conn,
            args.target_table,
            df_mapped,
            df_raw,
            ignored_columns,
            conflict_resolutions,
            target_to_source,
        )
        if shape_errors:
            error_records = shape_errors + error_records
            failed += len(shape_errors)
        success = inserted + overwritten + merged

        cursor = conn.cursor()
        cursor.execute(
            """
            INSERT INTO DataSourceInfo (source_file, target_table, import_time, row_count)
            VALUES (?, ?, ?, ?)
            """,
            (
                os.path.basename(args.file_path),
                args.target_table,
                datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                success,
            ),
        )
        conn.commit()
        conn.close()

        backup_original_file(args.file_path)

        log_path = ""
        if error_records:
            log_path = generate_error_log_file(error_records, args.target_table)
        serialized_errors = [record.to_dict() for record in error_records]

        response = {
            "status": "success",
            "total": total,
            "success": success,
            "inserted": inserted,
            "overwritten": overwritten,
            "merged": merged,
            "skipped": skipped,
            "failed": failed,
            "error_records": serialized_errors[:50],
            "log_path": log_path,
            "table": args.target_table,
            "has_failures": failed > 0,
            "has_error_log": bool(log_path),
        }
        if sample_rename_info is not None:
            response["sample_id_renames"] = sample_rename_info

        print(json.dumps(response, ensure_ascii=False))
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
