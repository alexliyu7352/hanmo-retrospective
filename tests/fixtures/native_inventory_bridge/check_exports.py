#!/usr/bin/env python3
"""检查Linux隔离桥只导出C入口，避免静态C++运行库/Flecs符号进入宿主。"""
import argparse
import json
from pathlib import Path
import subprocess


def main() -> int:
    """检查真实ELF动态符号；工具错误或多余/缺失入口均拒绝通过。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    try:
        result = subprocess.run(
            [args.nm, "-D", "--defined-only", "--format=posix", str(args.library)],
            check=True, capture_output=True, text=True,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        print(json.dumps({"ok": False, "error": str(error)}))
        return 2
    rows = [line.split()[:2] for line in result.stdout.splitlines() if line.strip()]
    valid = rows == [["hanmo_world_library_init", "T"]]
    print(json.dumps({"ok": valid, "defined_exports": len(rows), "sample": rows[:12]}))
    return 0 if valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
