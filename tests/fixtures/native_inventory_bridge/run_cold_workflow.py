#!/usr/bin/env python3
"""创建全新隔离工程，记录首自动导入及独立进程的旧oracle/生命周期验证。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def digest(path: Path) -> str:
    """对实际消费字节做SHA，不以文件名代替输入身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    """只使用现有显示会话；冷导入失败立即保留失败，不重试或改帧延时。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--cases-root", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mode", choices=["x11", "headless"], default="x11")
    args = parser.parse_args()
    if args.mode == "x11" and not os.environ.get("DISPLAY"):
        parser.error("此流程需从已有图形终端启动；没有DISPLAY不等于电脑没有桌面")
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    game = root / "game"
    game.mkdir()
    fixtures = game / "fixtures"
    fixtures.mkdir()
    manifest = json.loads((args.cases_root / "cases.json").read_text())
    paths = {"cases.json"}
    for row in manifest["cases"]:
        for kind in ("artifact", "queries", "oracle"):
            path = Path(row[kind])
            if path.is_absolute() or ".." in path.parts:
                raise ValueError("fixture路径必须留在原cases-root")
            expected = row.get(kind + "_sha256")
            if kind == "artifact":
                expected = row["input_sha256"]
            if expected and digest(args.cases_root / path) != expected:
                raise ValueError("冻结fixture摘要不符: " + str(path))
            paths.add(str(path))
        if Path(row["id"]).name != row["id"]:
            raise ValueError("fixture ID不能包含目录")
        paths.add("cli_audits/" + row["id"] + ".json")
    for name in sorted(paths):
        destination = fixtures / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.cases_root / name, destination)
    (fixtures / "expected_build.json").write_text(json.dumps({"source_commit": args.source_commit}))
    shutil.copy2(args.library, game / "libhanmo_world_bridge.so")
    shutil.copy2(Path(__file__).with_name("query_lifecycle.gd"), game / "query_lifecycle.gd")
    (game / "project.godot").write_text('[application]\nconfig/name="Hanmo cold native workflow"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    (game / "hanmo.gdextension").write_text('[configuration]\nentry_symbol="hanmo_world_library_init"\ncompatibility_minimum="4.7.2"\nreloadable=false\n[libraries]\nlinux.debug.x86_64="res://libhanmo_world_bridge.so"\n')
    env = os.environ.copy()
    for key, folder in (("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache")):
        path = root / folder
        path.mkdir()
        env[key] = str(path)
    options = ["--headless"] if args.mode == "headless" else ["--display-driver", "x11", "--rendering-method", "gl_compatibility"]
    report = {"engine_sha256": digest(args.godot), "library_sha256": digest(args.library), "source_commit": args.source_commit, "mode": args.mode, "cache_before": (game / ".godot").exists(), "fixture_sha256": {name: digest(fixtures / name) for name in sorted(paths)}, "stages": []}
    for name, stage_args in (("import", ["--editor", "--import"]), ("query", ["--script", "res://query_lifecycle.gd"])):
        command = [str(args.godot.resolve()), "--path", str(game), *options, *stage_args, "--audio-driver", "Dummy", "--verbose"]
        stage = {"stage": name, "command": command, "cache_before": (game / ".godot").exists()}
        started = time.monotonic()
        with (root / (name + ".log")).open("w") as log:
            try:
                stage["exit_code"] = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120).returncode
            except subprocess.TimeoutExpired:
                stage["timeout_seconds"] = 120
        stage["elapsed_seconds"] = time.monotonic() - started
        report["stages"].append(stage)
        (root / "result.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(stage), flush=True)
        if stage.get("exit_code") != 0:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
