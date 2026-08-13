"""Export a database table to CSV or Excel."""

import argparse
import json
import os
import sys

import pandas as pd

from db_common import get_connection

EXPORT_TABLES = {
    "DrillHoleInfo",
    "InclineInfo",
    "StrataInfo",
    "SampleRecord",
    "GradeInfo",
}


def export_table(target_table, output_path, file_format):
    if target_table not in EXPORT_TABLES:
        raise ValueError(f"不支持导出的表: {target_table}")

    normalized_format = file_format.strip().lower()
    if normalized_format not in {"csv", "xlsx"}:
        raise ValueError(f"不支持的导出格式: {file_format}")

    output_path = os.path.abspath(output_path)
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    conn = get_connection()
    try:
        df = pd.read_sql_query(f"SELECT * FROM {target_table}", conn)
    finally:
        conn.close()

    if normalized_format == "csv":
        df.to_csv(output_path, index=False, encoding="utf-8-sig")
    else:
        df.to_excel(output_path, index=False, engine="openpyxl")

    return len(df), output_path.replace("\\", "/")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target_table", required=True)
    parser.add_argument("--format", required=True, choices=["csv", "xlsx"])
    parser.add_argument("--output_path", required=True)
    args = parser.parse_args()

    try:
        row_count, output_path = export_table(
            args.target_table,
            args.output_path,
            args.format,
        )
        print(
            json.dumps(
                {
                    "status": "success",
                    "message": f"成功导出 {row_count} 条记录",
                    "row_count": row_count,
                    "output_path": output_path,
                    "target_table": args.target_table,
                },
                ensure_ascii=False,
            )
        )
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
