import sqlite3
import os

DB_PATH = "minimine.db"

# ==================== 创建 5 张表（带 EXTRA_DATA） ====================
def create_tables(conn):
    cursor = conn.cursor()
    
    cursor.execute('''
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
    ''')
    
    cursor.execute('''
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
    ''')
    
    cursor.execute('''
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
    ''')
    
    cursor.execute('''
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
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS GradeInfo (
            sample_id TEXT,
            element_name TEXT,
            grade_value REAL,
            extra_data TEXT,
            PRIMARY KEY (sample_id, element_name)
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS DataSourceInfo (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_file TEXT,
            target_table TEXT,
            import_time TEXT,
            row_count INTEGER
        )
    ''')
    
    conn.commit()
    print("✅ 所有表创建完成（如已存在则保留）")


def main():
    print("🚀 开始创建数据库表结构...")
    
    # 确保目录存在
    db_dir = os.path.dirname(DB_PATH)
    if db_dir and not os.path.exists(db_dir):
        os.makedirs(db_dir)
        print(f"📁 创建目录: {db_dir}")
    
    conn = sqlite3.connect(DB_PATH)
    
    try:
        create_tables(conn)
        print(f"📊 数据库文件: {os.path.abspath(DB_PATH)}")
        
        # 验证表是否创建成功
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
        tables = cursor.fetchall()
        print("📋 已创建的表：")
        for table in tables:
            print(f"   - {table[0]}")
            
    except Exception as e:
        print(f"❌ 创建表失败: {e}")
        import traceback
        traceback.print_exc()
    finally:
        conn.close()
    
    print("🎉 数据库初始化完成！")


if __name__ == "__main__":
    main()