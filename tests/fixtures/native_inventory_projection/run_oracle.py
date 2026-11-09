"""外部驱动逐组调用native并对照原GDScript oracle；命令参数从不包含oracle或expected。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    """读取证据时拒绝重复JSON键，避免不同解析器覆盖出不同预期。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def invalid_constant(value: str) -> None:
    """证据禁止NaN和Infinity，读取失败必须真实返回非零。"""
    raise ValueError('Invalid JSON constant: ' + value)


def read_json(path: Path) -> dict:
    """严格解析单个JSON对象，原始字节摘要由独立函数保存。"""
    result = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_object, parse_constant=invalid_constant)
    if not isinstance(result, dict):
        raise ValueError('Expected JSON object: ' + str(path))
    return result


def digest(path: Path) -> str:
    """摘要绑定磁盘精确字节，不对JSON重排或重新序列化。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def input_path(root: Path, value: str) -> Path:
    """清单只能引用导出根内真实文件，禁止路径逃逸或引入外部oracle。"""
    if not isinstance(value, str) or Path(value).is_absolute():
        raise ValueError('Expected relative case path')
    result = (root / value).resolve(strict=True)
    if not result.is_relative_to(root) or not result.is_file():
        raise ValueError('Case path escapes export directory: ' + value)
    return result


def validate_result(value: dict, artifact_sha: str, query_sha: str, query_ids: list[str]) -> None:
    """结果必须精确协议、绑定两输入并保持查询顺序，count只接受规范非负int64十进制。"""
    if set(value) != {'format', 'input_sha256', 'queries_sha256', 'counts'}:
        raise ValueError('Unexpected result fields')
    if value['format'] != 'HM-NATIVE-INVENTORY-RESULT/1' or value['input_sha256'] != artifact_sha or value['queries_sha256'] != query_sha:
        raise ValueError('Result format or input binding mismatch')
    rows = value['counts']
    if not isinstance(rows, list) or len(rows) != len(query_ids):
        raise ValueError('Result query count mismatch')
    for expected_id, row in zip(query_ids, rows):
        if not isinstance(row, dict) or set(row) != {'query_id', 'count'} or row['query_id'] != expected_id:
            raise ValueError('Result query order or row fields mismatch')
        count = row['count']
        if not isinstance(count, str) or re.fullmatch(r'0|[1-9][0-9]*', count) is None or int(count) > 9223372036854775807:
            raise ValueError('Result count is not canonical nonnegative int64')


def tagged_value(node: object, tag: str, expected_type: type) -> object:
    """只读取审计所需tag节点，保持int字符串和附加字段原树，不把值解码后再猜原类型。"""
    if not isinstance(node, dict) or set(node) != {'t', 'v'} or node['t'] != tag or type(node['v']) is not expected_type:
        raise ValueError('Unexpected TypedValue shape for ' + tag)
    return node['v']


def expected_inventory_audit(artifact: Path) -> dict:
    """独立从输入原tag树推导lot序及组件摘要，不借用native结果或重算Inventory.count。"""
    envelope = read_json(artifact)
    payload_text = envelope['payload_json']
    if not isinstance(payload_text, str):
        raise ValueError('Artifact payload_json must be a string')
    payload_sha = hashlib.sha256(payload_text.encode('utf-8')).hexdigest()
    if envelope['payload_sha256'] != payload_sha:
        raise ValueError('Artifact embedded payload byte binding mismatch')
    payload = json.loads(payload_text, object_pairs_hook=unique_object, parse_constant=invalid_constant)
    fields = tagged_value(payload, 'dict', dict)
    rows = tagged_value(fields['inventories'], 'array', list)
    lot_orders = {}
    component_sha = {}
    for row in rows:
        inventory = tagged_value(row, 'dict', dict)
        inventory_id = tagged_value(inventory['id'], 'string', str)
        if inventory_id in lot_orders:
            raise ValueError('Duplicate inventory ID in artifact')
        lots = tagged_value(inventory['lots'], 'array', list)
        lot_orders[inventory_id] = [tagged_value(tagged_value(lot, 'dict', dict)['id'], 'string', str) for lot in lots]
        compact = json.dumps(row, sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False)
        component_sha[inventory_id] = hashlib.sha256(compact.encode('utf-8')).hexdigest()
    return {'payload_sha256': payload_sha, 'inventory_lot_ids': lot_orders, 'inventory_component_sha256': component_sha}


def validate_audit(audit: dict, artifact_sha: str, query_sha: str, result_sha: str, query_count: int, inventory: dict) -> None:
    """核对真实安装、完成数量、三份文件身份及全部Flecs组件内容，成功标记本身不足以通过。"""
    if audit.get('format') != 'HM-NATIVE-INVENTORY-AUDIT/1' or audit.get('binding_kind') != 'artifact':
        raise ValueError('Audit format or binding kind mismatch')
    if audit.get('ok') is not True or audit.get('installed') is not True or 'error' in audit:
        raise ValueError('Audit does not report an installed successful operation')
    for field, expected in [('exit_code', 0), ('install_count', 1), ('completed_queries', query_count)]:
        if type(audit.get(field)) is not int or audit[field] != expected:
            raise ValueError('Audit integer mismatch: ' + field)
    for field, expected in [('input_sha256', artifact_sha), ('queries_sha256', query_sha), ('result_sha256', result_sha)]:
        if audit.get(field) != expected:
            raise ValueError('Audit exact byte binding mismatch: ' + field)
    for field in ['payload_sha256', 'inventory_lot_ids', 'inventory_component_sha256']:
        if audit.get(field) != inventory[field]:
            raise ValueError('Audit installed component mismatch: ' + field)


def run_cases(exports: Path, executable: Path, output: Path, timeout: int = 120) -> dict:
    """每次用全新输出保留stdout、stderr和audit；失败场景保留证据，不能把后续成功覆盖成整体通过。"""
    exports, executable, output = exports.resolve(strict=True), executable.resolve(strict=True), output.resolve()
    if output.exists() or output == exports or output.is_relative_to(exports):
        raise ValueError('Choose a new result directory outside export inputs')
    manifest_path = exports / 'cases.json'
    manifest = read_json(manifest_path)
    if manifest.get('format') != 'HM-NATIVE-INVENTORY-CASES/1' or manifest.get('kernel_unchanged') is not True:
        raise ValueError('Unverified exporter manifest')
    records = manifest.get('cases')
    if not isinstance(records, list) or not records:
        raise ValueError('Empty case manifest')
    case_ids = [record.get('id') for record in records]
    if any(not isinstance(case_id, str) or re.fullmatch(r'[a-z0-9_]+', case_id) is None for case_id in case_ids) or len(set(case_ids)) != len(case_ids):
        raise ValueError('Invalid or duplicate case ID')
    manifest_sha = digest(manifest_path)
    executable_sha = digest(executable)
    output.mkdir(parents=True)
    reports = []
    for record in records:
        case_output = output / record['id']
        case_output.mkdir()
        artifact = input_path(exports, record['artifact'])
        queries = input_path(exports, record['queries'])
        oracle = input_path(exports, record['oracle'])
        before = {path: digest(path) for path in [artifact, queries, oracle]}
        if [before[artifact], before[queries], before[oracle]] != [record['input_sha256'], record['queries_sha256'], record['oracle_sha256']]:
            raise ValueError('Export manifest byte binding mismatch: ' + record['id'])
        expected = read_json(oracle)
        query_document = read_json(queries)
        query_ids = [row['query_id'] for row in query_document['queries']]
        validate_result(expected, before[artifact], before[queries], query_ids)
        expected_audit = expected_inventory_audit(artifact)
        result_path, audit_path = case_output / 'result.json', case_output / 'audit.json'
        command = [str(executable), 'readonly-count', '--input', str(artifact), '--queries', str(queries), '--output', str(result_path), '--audit', str(audit_path)]
        exit_code = None
        process_error = ''
        try:
            completed = subprocess.run(command, cwd=case_output, capture_output=True, timeout=timeout, check=False)
            stdout, stderr, exit_code = completed.stdout, completed.stderr, completed.returncode
        except subprocess.TimeoutExpired as exc:
            stdout, stderr = exc.stdout or b'', exc.stderr or b''
            process_error = 'native timed out after ' + str(timeout) + ' seconds'
        except OSError as exc:
            stdout, stderr = b'', str(exc).encode('utf-8')
            process_error = 'native could not start: ' + str(exc)
        (case_output / 'stdout.txt').write_bytes(stdout)
        (case_output / 'stderr.txt').write_bytes(stderr)
        unchanged = all(path.is_file() and digest(path) == sha for path, sha in {**before, manifest_path: manifest_sha, executable: executable_sha}.items())
        error = ''
        matches = False
        try:
            if process_error:
                raise ValueError(process_error)
            if exit_code != 0:
                raise ValueError('native exit code ' + str(exit_code))
            if not unchanged:
                raise ValueError('native changed an input, oracle, manifest, or executable')
            if not audit_path.is_file():
                raise ValueError('native did not publish audit evidence')
            actual = read_json(result_path)
            validate_result(actual, before[artifact], before[queries], query_ids)
            if actual != expected:
                raise ValueError('native result differs from original Inventory.count oracle')
            validate_audit(read_json(audit_path), before[artifact], before[queries], digest(result_path), len(query_ids), expected_audit)
            matches = True
        except (OSError, ValueError, KeyError, TypeError) as exc:
            error = str(exc)
        reports.append({'id': record['id'], 'source_kind': record['source_kind'], 'ok': matches, 'error': error, 'exit_code': exit_code, 'input_sha256': before[artifact], 'queries_sha256': before[queries], 'oracle_sha256': before[oracle], 'inputs_unchanged': unchanged, 'query_count': len(query_ids), 'result_sha256': digest(result_path) if result_path.is_file() else None, 'audit_sha256': digest(audit_path) if audit_path.is_file() else None})
    report = {'format': 'HM-NATIVE-INVENTORY-ORACLE-CHECK/1', 'ok': all(row['ok'] for row in reports), 'source_commit': manifest['source_commit'], 'executable_sha256': executable_sha, 'manifest_sha256': manifest_sha, 'oracle_passed_to_native': False, 'cases': reports}
    (output / 'oracle-check.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main() -> int:
    """仅执行调用方已获准构建的本地native文件，不查找或下载缺失依赖。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exports', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=120)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    report = run_cases(args.exports, args.executable, args.output, args.timeout)
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
