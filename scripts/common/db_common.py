"""Shared database utilities for MiniMine Python scripts."""

import os
import sqlite3
from datetime import datetime


PROJECT_ROOT = os.environ.get(
    "MINIMINE_ROOT",
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")),
)

DB_PATH = os.environ.get(
    "MINIMINE_DB_PATH",
    os.path.join(PROJECT_ROOT, "runtime", "minimine.db"),
)

LOGS_DIR = os.environ.get(
    "MINIMINE_LOG_DIR",
    os.path.join(PROJECT_ROOT, "runtime", "logs"),
)


EXTRA_DATA_COLUMN = "EXTRA_DATA"
IMPORT_TIME_COLUMN = "import_time"

CORE_TABLES = [
    "DrillHoleInfo",
    "InclineInfo",
    "StrataInfo",
    "SampleRecord",
    "GradeInfo",
]
def create_core_tables(conn):
    """Create the MiniMine database schema when it does not exist."""
    cursor = conn.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS DrillHoleInfo (
            borehole_id TEXT PRIMARY KEY,
            area_id TEXT,
            x_coord REAL,
            y_coord REAL,
            z_coord REAL,
            total_depth REAL,
            azimuth REAL,
            dip_angle REAL,
            extra_data TEXT,
            import_time TEXT
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS InclineInfo (
            borehole_id TEXT,
            point_id INTEGER,
            area_id TEXT,
            point_depth REAL,
            deviation_angle REAL,
            azimuth REAL,
            extra_data TEXT,
            PRIMARY KEY (borehole_id, point_id)
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS StrataInfo (
            borehole_id TEXT,
            layer_order INTEGER,
            area_id TEXT,
            layer_no TEXT,
            bottom_depth REAL,
            rock_name TEXT,
            dip_angle REAL,
            extra_data TEXT,
            PRIMARY KEY (borehole_id, layer_order)
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS SampleRecord (
            sample_id TEXT PRIMARY KEY,
            borehole_id TEXT,
            area_id TEXT,
            start_depth REAL,
            end_depth REAL,
            sample_length REAL,
            core_length REAL,
            sample_type INTEGER,
            extra_data TEXT
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS GradeInfo (
            sample_id TEXT,
            element_name TEXT,
            grade_value REAL,
            extra_data TEXT,
            PRIMARY KEY (sample_id, element_name)
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS DataSourceInfo (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_file TEXT,
            target_table TEXT,
            import_time TEXT,
            row_count INTEGER
        )
    """)

    conn.commit()

def ensure_logs_dir():
    os.makedirs(LOGS_DIR, exist_ok=True)


def configure_connection(conn):
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA busy_timeout=30000")
    conn.execute("PRAGMA synchronous=NORMAL")


def get_connection():
    db_dir = os.path.dirname(DB_PATH)
    if db_dir:
        os.makedirs(db_dir, exist_ok=True)

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
    """Ensure the MiniMine database schema exists and is up to date."""
    close_after = False
    if conn is None:
        conn = get_connection()
        close_after = True

    create_core_tables(conn)

    for table_name in CORE_TABLES:
        ensure_extra_data_column(conn, table_name)

    ensure_import_time_column(conn, "DrillHoleInfo")

    if close_after:
        conn.close()

