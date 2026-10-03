import os
import sys

COMMON_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "common")
)
if COMMON_DIR not in sys.path:
    sys.path.insert(0, COMMON_DIR)

from db_common import DB_PATH, ensure_schema, get_connection


def main():
    print("🚀 开始初始化数据库...")

    conn = None
    try:
        conn = get_connection()
        ensure_schema(conn)

        print(f"📊 数据库文件: {os.path.abspath(DB_PATH)}")

        cursor = conn.cursor()
        cursor.execute(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' ORDER BY name"
        )
        tables = cursor.fetchall()

        print("📋 当前数据库表：")
        for table in tables:
            print(f"   - {table[0]}")

        print("🎉 数据库初始化完成！")

    except Exception as exc:
        print(f"❌ 数据库初始化失败: {exc}")
        raise

    finally:
        if conn is not None:
            conn.close()


if __name__ == "__main__":
    main()