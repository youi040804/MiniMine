"""Apply database schema updates for MiniMine."""

import json
import sys

from db_common import CORE_TABLES, EXTRA_DATA_COLUMN, ensure_schema, get_connection, table_has_column


def main():
    try:
        conn = get_connection()
        ensure_schema(conn)

        cursor = conn.cursor()
        status = {}
        for table_name in CORE_TABLES:
            status[table_name] = table_has_column(cursor, table_name, EXTRA_DATA_COLUMN)

        conn.close()
        print(
            json.dumps(
                {
                    "status": "success",
                    "message": "数据库结构已更新，所有核心表均包含 EXTRA_DATA 字段",
                    "tables": status,
                },
                ensure_ascii=False,
            )
        )
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
