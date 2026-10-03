import os
import sys

COMMON_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "common")
)
if COMMON_DIR not in sys.path:
    sys.path.insert(0, COMMON_DIR)

from db_common import ensure_schema, get_connection

conn = get_connection()
ensure_schema(conn)
cursor = conn.cursor()

# 查询所有表记录数
tables = ['DrillHoleInfo', 'InclineInfo', 'StrataInfo', 'SampleRecord', 'GradeInfo']
for table in tables:
    cursor.execute(f'SELECT COUNT(*) FROM {table}')
    print(f'{table}: {cursor.fetchone()[0]} 条')

# 查 InclineInfo 前10条
print('\nInclineInfo 前10条:')
cursor.execute('SELECT borehole_id, point_id, point_depth, deviation_angle, azimuth FROM InclineInfo LIMIT 10')
for row in cursor.fetchall():
    print(row)

conn.close()