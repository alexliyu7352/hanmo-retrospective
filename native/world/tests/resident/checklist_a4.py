#!/usr/bin/env python3
"""A4 只驱动真实薄宿主两进程；不生成目录、世界事实或替代推进算法。"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def utc_now():
    """记录实际进程时点，耗时另用单调时钟而不相减墙钟。"""
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def file_ref(path):
    """固定实际文件身份；fixture 与 ELF 只读，不复制或改写。"""
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest()}


def unique_object(pairs):
    """拒绝重复 JSON 键，避免摘要忽略被后值覆盖的字段。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("DuplicateJsonKey:" + key)
        result[key] = value
    return result


def load_json(path):
    """读取完整原生输出，不投影或删去世界状态字段。"""
    return json.loads(path.read_text(encoding="utf-8"),
                      object_pairs_hook=unique_object)


def state_digest(value):
    """只规范对象键次序；数组顺序、每个字段和值全部参加摘要。"""
    data = json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def atomic_json(path, value):
    """原子封口外部报告，不改写原生存档或原始日志。"""
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def require(condition, message):
    """首个真实失败立即结束；测试不放宽原生拒绝或重试。"""
    if not condition:
        raise RuntimeError(message)


def execute_native(arguments, mode, report):
    """等待并回收生产进程后才允许独立冷读进程启动。"""
    command = [str(arguments.binary), mode, str(arguments.fixture),
               arguments.scenario, str(arguments.output),
               "--days", str(arguments.days)]
    record = {"mode": mode, "argv": command, "started_at": utc_now()}
    report["processes"].append(record)
    start = time.monotonic()
    with (arguments.output / (mode + ".stdout.log")).open("xb") as stdout:
        with (arguments.output / (mode + ".stderr.log")).open("xb") as stderr:
            process = subprocess.Popen(command, stdout=stdout, stderr=stderr)
            record["pid"] = process.pid
            record["exit_code"] = process.wait()
    record["ended_at"] = utc_now()
    record["seconds"] = time.monotonic() - start
    record["reaped_before_next_process"] = True
    require(record["exit_code"] == 0, "NativeProcessFailed:" + mode)
    result = load_json(arguments.output / (mode + ".result.json"))
    require(result.get("ok") is True, "NativeResultRejected:" + mode)
    require(result.get("requested_half_days") == 2 * arguments.days,
            "NativeRequestedSpanMismatch:" + mode)


def check_boundary(state, expected_tick, scenario):
    """校验真实整数半日边界，不能把调用次数冒充三十游戏日。"""
    require(state["scenario_id"] == scenario, "ScenarioChanged")
    require(state["tick"] == expected_tick, "ActualTickMismatch")
    timeline = state["world_timeline"]["state"]
    require(timeline["sub_half_day"] == {"numerator": "0", "denominator": "1"},
            "NonIntegralFinalMoment")
    require(timeline["boundary_state"] == 0, "UnfinishedBoundary")


def main():
    """在唯一配置上执行并记录新局、三十日、存档、冷读、再三十日。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--days", type=int, required=True)
    arguments = parser.parse_args()
    require(arguments.days == 30, "ChecklistA4RequiresThirtyDaysPerLeg")
    arguments.binary = arguments.binary.resolve(strict=True)
    arguments.fixture = arguments.fixture.resolve(strict=True)
    arguments.output = arguments.output.absolute()
    require(not arguments.output.exists(), "FreshA4OutputDirectoryRequired")
    arguments.output.mkdir(parents=True)
    report = {"status": "RUNNING", "started_at": utc_now(), "days_per_leg": 30,
              "fixture": file_ref(arguments.fixture),
              "binary": file_ref(arguments.binary), "processes": [],
              "scope": "Single exact-rest shared fixture; native time/checkpoint/cold continuation only"}
    started = time.monotonic()
    try:
        execute_native(arguments, "new", report)
        checkpoint = file_ref(arguments.output / "day.checkpoint")
        require(checkpoint["bytes"] > 0, "EmptyCheckpoint")
        execute_native(arguments, "binary", report)
        require(report["processes"][0]["pid"] != report["processes"][1]["pid"],
                "IndependentColdProcessRequired")
        states = [load_json(arguments.output / name) for name in
                  ["initial.json", "day.json", "restored.binary.json", "continued.binary.json"]]
        expected_ticks = [0, 60, 60, 120]
        for state, tick in zip(states, expected_ticks):
            check_boundary(state, tick, arguments.scenario)
        require(states[1] == states[2], "WholeRestoredStateMismatch")
        digests = [state_digest(state) for state in states]
        require(digests[1] == digests[2], "RestoredStateDigestMismatch")
        require(states[1]["revision"] > states[0]["revision"] and
                states[3]["revision"] > states[2]["revision"], "NoNativeProgress")
        receipt_counts = []
        for name in ["day.receipts.json", "continued.binary.receipts.json"]:
            receipts = load_json(arguments.output / name)
            require(receipts and all(r.get("ok") is True and r.get("kind") == "advance"
                                     for r in receipts), "RejectedAdvanceReceipt")
            receipt_counts.append(len(receipts))
        require(file_ref(arguments.output / "day.checkpoint") == checkpoint,
                "ColdReaderChangedCheckpoint")
        require(file_ref(arguments.fixture) == report["fixture"], "FixtureChanged")
        require(file_ref(arguments.binary) == report["binary"], "BinaryChanged")
        report.update(status="PASS", actual_ticks=expected_ticks,
                      state_digests=digests, whole_restore_state_equal=True,
                      raw_restore_bytes_equal=(arguments.output / "day.json").read_bytes() ==
                      (arguments.output / "restored.binary.json").read_bytes(),
                      checkpoint=checkpoint, accepted_advance_receipts=receipt_counts)
        code = 0
    except Exception as error:
        report.update(status="FAIL", error_type=type(error).__name__, error=str(error))
        code = 1
    report["ended_at"] = utc_now()
    report["seconds"] = time.monotonic() - started
    report["native_artifacts"] = [file_ref(path) for path in sorted(arguments.output.iterdir())
                                  if path.is_file()]
    atomic_json(arguments.output / "checklist_A4.result.json", report)
    print(json.dumps({"status": report["status"], "seconds": report["seconds"],
                      "result": str(arguments.output / "checklist_A4.result.json")},
                     ensure_ascii=False))
    return code


if __name__ == "__main__":
    sys.exit(main())
