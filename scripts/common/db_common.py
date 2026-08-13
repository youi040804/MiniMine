"""Shared database utilities for MiniMine Python scripts."""

import os
import sqlite3
from datetime import datetime

DB_PATH = r"D:\Desktop\MiniMineUI\minimine.db"
LOGS_DIR = r"D:\Desktop\MiniMineUI\logs"
EXTRA_DATA_COLUMN = "EXTRA_DATA"
IMPORT_TIME_COLUMN = "import_time"

CORE_TABLES = [
    "DrillHoleInfo",
    "InclineInfo",
    "StrataInfo",
    "SampleRecord",
    "GradeInfo",
]


def ensure_logs_dir():
    os.makedirs(LOGS_DIR, exist_ok=True)


def configure_connection(conn):
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA busy_timeout=30000")
    conn.execute("PRAGMA synchronous=NORMAL")


def get_connection():
    conn = sqlite3.connect(DB_PATH, timeout=30)
    configure_connection(conn)
    return conn


def table_has_column(cursor, table_name, column_name):
    cursor.execute(f"PRAGMA table_info({table_name})")
    target = column_name.lower()
    return any(row[1].lower() == target for row in cursor.fetchall())


def ensure_extra_data_column(conn, table_name):
    cursor = conn.cursor()
    if not table_has_column(cursor, table_name, EXTRA_DATA_COLUMN):
        cursor.execute(f"ALTER TABLE {table_name} ADD COLUMN {EXTRA_DATA_COLUMN} TEXT")
        conn.commit()


def ensure_import_time_column(conn, table_name="DrillHoleInfo"):
    cursor = conn.cursor()
    if not table_has_column(cursor, table_name, IMPORT_TIME_COLUMN):
        cursor.execute(f"ALTER TABLE {table_name} ADD COLUMN {IMPORT_TIME_COLUMN} TEXT")
        conn.commit()


def current_import_time():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def stamp_drill_hole_import_time(record):
    if record is None:
        return record
    record[IMPORT_TIME_COLUMN] = current_import_time()
    return record


def ensure_schema(conn=None):
    """Ensure all core tables contain the EXTRA_DATA column."""
    close_after = False
    if conn is None:
        conn = get_connection()
        close_after = True

    for table_name in CORE_TABLES:
        ensure_extra_data_column(conn, table_name)

    ensure_import_time_column(conn, "DrillHoleInfo")

    if close_after:
        conn.close()
