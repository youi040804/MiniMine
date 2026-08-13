"""Unified reader for CSV / XLSX / XLS batch import files."""

import csv
import os

import pandas as pd

SUPPORTED_EXTENSIONS = (".csv", ".xlsx", ".xls")
SUPPORTED_FORMAT_TEXT = ".csv / .xlsx / .xls"
CSV_ENCODINGS = ("utf-8-sig", "utf-8", "gbk", "gb2312", "latin-1")


def get_extension(file_path: str) -> str:
    return os.path.splitext(file_path)[1].lower()


def is_supported_file(file_path: str) -> bool:
    return get_extension(file_path) in SUPPORTED_EXTENSIONS


def validate_file_path(file_path: str) -> None:
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"文件不存在: {file_path}")
    ext = get_extension(file_path)
    if ext not in SUPPORTED_EXTENSIONS:
        raise ValueError(
            f"不支持的文件格式: {ext or '(无扩展名)'}，请选择 {SUPPORTED_FORMAT_TEXT} 文件"
        )


def _format_bad_line_value(row):
    if not row:
        return ""
    text = ",".join(str(cell) for cell in row)
    if len(text) > 200:
        return text[:200] + "..."
    return text


def read_csv_dataframe(file_path: str, nrows=None, bad_line_records=None):
    """Read CSV with encoding auto-detection; skip malformed rows and record them."""
    last_error = None

    for encoding in CSV_ENCODINGS:
        try:
            with open(file_path, "r", encoding=encoding, newline="") as file:
                reader = csv.reader(file)
                header = next(reader, None)
                if header is None:
                    raise ValueError("CSV 文件为空")

                expected_cols = len(header)
                columns = [str(col).strip() for col in header]
                rows = []

                for line_no, row in enumerate(reader, start=2):
                    if nrows is not None and len(rows) >= nrows:
                        break

                    if len(row) != expected_cols:
                        if bad_line_records is not None:
                            bad_line_records.append(
                                {
                                    "row_num": line_no,
                                    "expected": expected_cols,
                                    "actual": len(row),
                                    "raw": _format_bad_line_value(row),
                                }
                            )
                        continue

                    rows.append(row)

                return pd.DataFrame(rows, columns=columns)
        except UnicodeDecodeError as exc:
            last_error = exc
            continue
        except ValueError:
            raise
        except Exception as exc:
            last_error = exc
            continue

    raise ValueError(f"无法读取文件，请检查文件格式和编码。最后错误: {last_error}")


def read_dataframe(file_path: str, nrows=None, bad_line_records=None) -> pd.DataFrame:
    """Read full or partial dataframe from csv/xlsx/xls."""
    validate_file_path(file_path)
    ext = get_extension(file_path)
    read_kwargs = {"nrows": nrows} if nrows is not None else {}

    if ext == ".csv":
        return read_csv_dataframe(
            file_path,
            nrows=nrows,
            bad_line_records=bad_line_records,
        )

    if ext == ".xlsx":
        try:
            return pd.read_excel(file_path, engine="openpyxl", **read_kwargs)
        except ImportError as exc:
            raise ImportError("读取 .xlsx 需要安装 openpyxl: pip install openpyxl") from exc
        except Exception as exc:
            raise ValueError(f"Excel (.xlsx) 文件读取失败: {exc}") from exc

    if ext == ".xls":
        try:
            return pd.read_excel(file_path, engine="xlrd", **read_kwargs)
        except ImportError as exc:
            raise ImportError("读取 .xls 需要安装 xlrd: pip install xlrd") from exc
        except Exception as exc:
            raise ValueError(f"Excel (.xls) 文件读取失败: {exc}") from exc

    raise ValueError(f"不支持的文件格式: {ext}")


def read_columns(file_path: str) -> list[str]:
    """Read column names only."""
    df = read_dataframe(file_path, nrows=0)
    return [str(col).strip() for col in df.columns.tolist()]
