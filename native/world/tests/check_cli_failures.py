"""对已获准构建的同库CLI做文件边界与整批失败检查，不运行旧世界全门或下载工具。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path: Path) -> str:
    """绑定精确输入及原保留文件，不把重排JSON的摘要当作原字节。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(executable: Path, artifact: Path, queries: Path, output: Path) -> dict:
    """所有负例放新目录；原输入只读，错误不能覆盖原文件或留下部分counts。"""
    executable, artifact, queries = [p.resolve(strict=True) for p in (executable, artifact, queries)]
    output = output.resolve()
    if output.exists() or output in [artifact.parent, queries.parent]:
        raise ValueError('Choose a new output directory')
    output.mkdir(parents=True)
    original = {p: sha(p) for p in (artifact, queries, executable)}
    source = artifact.read_bytes()
    query_bytes = queries.read_bytes()
    rows = []
    variants = ['duplicate_json_key', 'wrong_payload_sha', 'wrong_source', 'unknown_inventory', 'duplicate_query', 'output_alias', 'existing_result', 'existing_audit', 'occupied_stage']
    for name in variants:
        directory = output / name
        directory.mkdir()
        a, q, result, audit = [directory / p for p in ('artifact.json', 'queries.json', 'result.json', 'audit.json')]
        a.write_bytes(source)
        q.write_bytes(query_bytes)
        expected = ''
        installed = 1
        if name == 'duplicate_json_key':
            a.write_text('{"format":"one","format":"two"}')
            expected, installed = 'DuplicateJsonKey', 0
        elif name == 'wrong_payload_sha':
            value = json.loads(source)
            value['payload_sha256'] = '0' * 64
            a.write_text(json.dumps(value))
            expected, installed = 'PayloadDigestMismatch', 0
        elif name == 'wrong_source':
            value = json.loads(query_bytes)
            value['input_sha256'] = '0' * 64
            q.write_text(json.dumps(value))
            expected = 'SourceMismatch'
        elif name == 'unknown_inventory':
            value = json.loads(query_bytes)
            value['queries'][-1]['inventory_id'] = 'missing'
            q.write_text(json.dumps(value))
            expected = 'UnknownInventory'
        elif name == 'duplicate_query':
            value = json.loads(query_bytes)
            value['queries'].append(value['queries'][0])
            q.write_text(json.dumps(value))
            expected = 'DuplicateQueryId'
        elif name == 'output_alias':
            result = a
            expected, installed = 'AliasedPaths', 0
        elif name in ['existing_result', 'existing_audit']:
            (result if name == 'existing_result' else audit).write_text('preserve-original\n')
            expected, installed = 'OutputExistsOrUnavailable', 0
        else:
            (directory / 'result.json.pending').mkdir()
            (directory / 'result.json.pending/keep').write_text('preserve-stage\n')
            expected = 'OutputStageFailed'
        protected = {p: sha(p) for p in (a, q, result, audit) if p.exists()}
        command = [str(executable), 'readonly-count', '--input', str(a), '--queries', str(q), '--output', str(result), '--audit', str(audit)]
        process = subprocess.run(command, capture_output=True, timeout=60, check=False)
        (directory / 'stdout.txt').write_bytes(process.stdout)
        (directory / 'stderr.txt').write_bytes(process.stderr)
        preserved = all(p.exists() and sha(p) == digest for p, digest in protected.items())
        no_partial = result in protected or not result.exists()
        checked_audit = True
        if name not in ['output_alias', 'existing_result', 'existing_audit']:
            data = json.loads(audit.read_text()) if audit.is_file() else {}
            checked_audit = data.get('ok') is False and data.get('exit_code') == 1 and data.get('install_count') == installed and data.get('completed_queries') == 0 and data.get('error', {}).get('code') == expected and 'counts' not in data and 'result_sha256' not in data
        if name == 'occupied_stage':
            preserved = preserved and (directory / 'result.json.pending/keep').read_text() == 'preserve-stage\n'
        ok = process.returncode != 0 and expected.encode() in process.stderr and preserved and no_partial and checked_audit
        rows.append({'case': name, 'ok': ok, 'expected': expected, 'exit_code': process.returncode, 'protected_unchanged': preserved, 'no_partial_result': no_partial, 'audit_checked': checked_audit})
    report = {'ok': all(row['ok'] for row in rows) and all(sha(p) == digest for p, digest in original.items()), 'cases': rows, 'original_files_unchanged': all(sha(p) == digest for p, digest in original.items())}
    (output / 'cli-failure-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return report


def main() -> int:
    """调用者必须先取得执行许可并提供已构建路径，本脚本不自行准备依赖。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--queries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = run(args.executable, args.artifact, args.queries, args.output)
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
