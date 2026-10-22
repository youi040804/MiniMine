# 地质钻孔数据管理系统

基于 **C++ / Qt Widgets + Python / pandas + SQLite** 开发的地质钻孔数据录入与管理桌面应用，面向钻孔概况、测斜、地层、样品及品位等结构化数据，提供单孔录入、异构表格批量导入、字段映射、业务校验、冲突处理、数据查询和导出功能。

本仓库源于 MiniMine 数字矿山项目中的**钻孔数据录入与管理模块**，现以“地质钻孔数据管理系统”作为独立展示名称。仓库展示的是这一模块的实现，不代表包含完整数字矿山平台的全部功能。

## 一、主要功能

| 功能 | 实现内容 |
| --- | --- |
| 单孔录入 | 分步填写钻孔概况、测斜、地层、样品和品位数据 |
| 批量导入 | 使用 Python/pandas 读取 CSV、XLS、XLSX 文件 |
| 字段映射 | 根据源文件列名推荐标准字段映射，支持映射方案保存与复用 |
| 非标准字段保留 | 将未映射的附加列保存至 `EXTRA_DATA` JSON 字段 |
| 冲突处理 | 导入前分析已有记录，支持跳过、覆盖和合并 |
| 业务校验 | 检查必填值、数值合法性、数据关联及深度约束 |
| 查询与导出 | 查看钻孔相关记录，导出 CSV 或 XLSX |
| 错误反馈 | 返回导入统计、失败信息，并记录异常数据 |

## 二、技术栈

- **桌面端：** C++、Qt Widgets、Qt SQL
- **进程协作：** QProcess、JSON、环境变量
- **数据处理：** Python、pandas、openpyxl、xlrd
- **数据存储：** SQLite

## 三、系统架构

```text
┌──────────────────────────────────────────┐
│               Qt Widgets                 │
│ 单孔录入 / 批量导入 / 字段映射 / 查询与导出 │
└────────────────────┬─────────────────────┘
                     │
              QProcess + JSON
                     │
┌────────────────────▼─────────────────────┐
│              Python 脚本                 │
│ 文件解析 / 数据转换 / 校验 / 冲突处理      │
└────────────────────┬─────────────────────┘
                     │
                   SQLite
                     │
┌────────────────────▼─────────────────────┐
│                 数据库                   │
│ 钻孔 / 测斜 / 地层 / 样品 / 品位 / 数据来源 │
└──────────────────────────────────────────┘
```

Qt/C++ 负责界面交互、数据展示及部分数据库访问；Python 负责表格解析和批量数据处理。`bridge/PythonRunner.cpp` 通过 `QProcess` 启动 Python 脚本，传递参数与运行环境，并解析脚本输出的 JSON 结果。为降低跨进程访问 SQLite 时的连接冲突风险，进程桥在相关操作前后协调 C++ 侧数据库连接的释放和恢复。

## 四、核心实现

### 4.1 单孔录入与数据管理

通过 Qt Widgets 实现钻孔信息的分步录入，围绕钻孔概况、测斜、地层、样品及品位组织数据。Qt SQL 用于 SQLite 数据访问；数据查看界面支持读取钻孔及关联记录，并通过导出脚本生成 CSV、XLSX 文件。

相关代码：

- `ui/dialogs/entry/SingleEntryDialog.cpp`
- `ui/dialogs/view/DrillDetailDialog.cpp`
- `ui/dialogs/view/ExportDataDialog.cpp`
- `data/sqlite/DatabaseManager.cpp`
- `scripts/entry/`
- `scripts/export/export_data.py`

### 4.2 异构表格导入与字段映射

不同钻孔数据文件可能使用不同的列名、列顺序或附加字段。导入模块使用 pandas 解析 CSV、XLS、XLSX，并提供字段匹配建议及人工确认界面。已确认的映射方案可保存、加载和复用；无法映射到标准列的附加字段可保存为 `EXTRA_DATA` JSON，以减少源文件信息丢失。

对于品位等具有不同表格组织方式的数据，导入逻辑包含相应的字段转换和关联处理。

相关代码：

- `ui/dialogs/import/FieldMappingHelper.cpp`
- `ui/dialogs/import/BatchImportDialog.cpp`
- `scripts/import/read_csv_columns.py`
- `scripts/import/batch_import.py`
- `scripts/common/file_reader.py`

### 4.3 两阶段冲突处理

批量导入不是直接覆盖数据库中的同主键记录，而是分为两个阶段：

1. **冲突分析：** 比对待导入记录与现有记录，识别冲突并展示字段差异。
2. **用户决策与写入：** 用户选择跳过、覆盖或合并后，再执行相应的数据库操作。

| 策略 | 含义 |
| --- | --- |
| 跳过 | 保留数据库中的已有记录 |
| 覆盖 | 使用待导入记录替换相应已有记录 |
| 合并 | 按字段合并新旧数据，并处理扩展 JSON 字段 |

相关代码：

- `ui/dialogs/import/ImportConflictDialog.cpp`
- `ui/dialogs/import/BatchImportDialog.cpp`
- `scripts/import/batch_import.py`

### 4.4 业务校验与错误记录

数据导入过程中检查字段类型、必要数据关联和部分地质业务约束，例如：

- 测斜点深度的递增关系；
- 地层底深与钻孔终孔深度的关系；
- 测斜、地层、样品记录与钻孔之间的关联；
- 品位记录与样品之间的关联。

不符合校验要求的记录会被拦截并纳入错误反馈。这里的关联校验主要由**业务层逻辑**实现，不将其表述为完整的数据库外键约束体系。

### 4.5 SQLite 数据模型与访问

核心数据包括六类表：

| 表名 | 用途 |
| --- | --- |
| `DrillHoleInfo` | 钻孔概况 |
| `InclineInfo` | 测斜数据 |
| `StrataInfo` | 地层数据 |
| `SampleRecord` | 样品数据 |
| `GradeInfo` | 品位数据 |
| `DataSourceInfo` | 导入来源信息 |

数据库访问配置包含 SQLite WAL 模式、`busy_timeout=30000` 和 `synchronous=NORMAL`。这些配置用于改善数据库访问行为，不代表项目已经完成专门的并发性能优化或吞吐量测试。

## 五、真实钻孔数据验证

使用真实钻孔数据完成批量导入与业务校验验证，统计如下：

| 数据类别 | 原始记录 | 成功入库 | 校验拦截 |
| --- | ---: | ---: | ---: |
| 钻孔概况 | 60 | 60 | 0 |
| 测斜数据 A | 825 | 772 | 53 |
| 测斜数据 B | 283 | 246 | 37 |
| 测斜数据 C | 4,091 | 4,091 | 0 |
| 测斜数据 D | 8,404 | 8,404 | 0 |
| **合计** | **13,663** | **13,573** | **90** |

**说明：** 这些数字反映特定数据集上的导入与校验结果，不是系统吞吐量、响应时间或通用准确率指标。仓库不公开原始钻孔数据文件及运行时数据库。

## 六、仓库结构

```text
MiniMine/
├── app/                     # 应用入口、主窗口与路径配置
├── bridge/                  # QProcess 进程桥
├── data/sqlite/             # Qt SQL / SQLite 数据访问
├── ui/
│   ├── dialogs/
│   │   ├── entry/           # 单孔录入
│   │   ├── import/          # 批量导入、映射、冲突决策
│   │   └── view/            # 详情查询、导出
│   └── widgets/             # 自定义输入组件
├── scripts/
│   ├── common/              # 公共数据库与文件处理逻辑
│   ├── entry/               # 单孔保存
│   ├── import/              # 表格导入与校验
│   ├── export/              # 数据导出
│   ├── schema/              # 数据库初始化与迁移
│   └── tools/               # 数据库查询工具
├── images/                  # 项目架构图片
└── README.md
```

运行时数据库、日志和本地数据文件不作为公开源码的一部分。

## 七、运行环境与配置

### 7.1 环境依赖

- C++ 编译器及 Qt Widgets、Qt SQL 开发环境；
- Qt 的 QSQLITE 驱动；
- Python 3；
- Python 依赖：

```bash
pip install pandas openpyxl xlrd
```

### 7.2 获取仓库

```bash
git clone https://github.com/youi040804/MiniMine.git
cd MiniMine
```

### 7.3 路径配置

代码通过 `app/AppConfig.h` 及环境变量定位项目资源，可按运行环境设置：

| 环境变量 | 作用 |
| --- | --- |
| `MINIMINE_ROOT` | 项目根目录 |
| `MINIMINE_PYTHON` | Python 解释器路径 |
| `MINIMINE_DB_PATH` | SQLite 数据库路径 |
| `MINIMINE_LOG_DIR` | 日志目录 |

例如 Linux / WSL：

```bash
export MINIMINE_PYTHON=/usr/bin/python3
```

Windows PowerShell：

```powershell
$env:MINIMINE_PYTHON = "C:\Path\To\python.exe"
```

### 7.4 运行说明

Qt 桌面端需要在配置好 Qt Widgets、Qt SQL 和 QSQLITE 驱动的开发环境中构建。项目包含 Python 数据库初始化与查询脚本，可用于脱离 GUI 检查部分数据库逻辑。

例如，查看查询工具：

```bash
python scripts/tools/query.py
```

首次使用时应确认数据库路径及运行时目录配置正确。**仓库不提供一键跨平台安装包，也不将 Python 数据库脚本验证等同于完整 GUI 回归测试。**

## 八、项目定位与实现边界

本项目重点展示：

- Qt/C++ 桌面应用开发与数据库交互；
- QProcess 与 Python 脚本的跨进程协作；
- CSV/Excel 异构数据导入、字段映射与数据转换；
- 冲突决策、业务校验和真实钻孔数据验证。

项目不宣称实现完整数字矿山平台、三维建模、实时设备采集、数据库索引专项优化或经过基准测试的高并发导入能力。

---

**仓库地址：** https://github.com/youi040804/MiniMine
