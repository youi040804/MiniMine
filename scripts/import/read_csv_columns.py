"""Read column names from CSV/Excel files."""

import argparse
import json
import sys

from file_reader import SUPPORTED_FORMAT_TEXT, read_columns, validate_file_path


def main():
    parser = argparse.ArgumentParser(
        description=f"Read column names from supported files ({SUPPORTED_FORMAT_TEXT})"
    )
    parser.add_argument("--file_path", required=True)
    args = parser.parse_args()

    try:
        validate_file_path(args.file_path)
        columns = read_columns(args.file_path)
        print(json.dumps({"status": "success", "columns": columns}, ensure_ascii=False))
    except Exception as exc:
        print(json.dumps({"status": "error", "message": str(exc)}, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
