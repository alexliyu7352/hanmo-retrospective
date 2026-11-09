"""逐字段比较同来源的原生/GD有限页与草案；双端失败或规则红例不能冒充PASS。"""
import argparse
import hashlib
import json
from pathlib import Path


def compare(expected, actual, path=""):
    """保留数组顺序与全部键；JSON对象键序不属于公开页面协议。"""
    if type(expected) is not type(actual):
        return [f"{path}: type {type(expected).__name__} != {type(actual).__name__}"]
    if isinstance(expected, dict):
        if expected.keys() != actual.keys():
            return [f"{path}: keys differ {sorted(expected.keys() ^ actual.keys())}"]
        return [difference for key in expected for difference in compare(expected[key], actual[key], f"{path}/{key}")]
    if isinstance(expected, list):
        if len(expected) != len(actual):
            return [f"{path}: length {len(expected)} != {len(actual)}"]
        return [difference for index, pair in enumerate(zip(expected, actual)) for difference in compare(*pair, f"{path}/{index}")]
    return [] if expected == actual else [f"{path}: {expected!r} != {actual!r}"]


def main():
    """要求两端实际执行成功且零写，再输出原始输入与报告的完整摘要。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("gd", type=Path)
    parser.add_argument("native", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    gd, native = [json.loads(path.read_text()) for path in (args.gd, args.native)]
    differences = compare(gd["cases"], native["cases"], "cases")
    passed = gd.get("ok") is True and native.get("ok") is True and gd.get("zero_write") is True and native.get("zero_write") is True and not differences
    report = {"ok": passed, "cases": len(gd["cases"]), "differences": differences, "gd_failures": gd.get("failures", []), "sha256": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (args.input, args.gd, args.native)}}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
