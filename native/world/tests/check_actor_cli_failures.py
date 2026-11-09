"""仅核actor-commitments CLI的整批失败与文件保护；原工件只读，不改decoder或生成新世界。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path: Path) -> str:
    """绑定实际文件字节，保留原输入与二进制身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    """只写本例新文件，明确UTF-8紧凑编码用于重签负例。"""
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')


def run(executable: Path, artifact: Path, queries: Path, inventory_artifact: Path, output: Path) -> dict:
    """每例独占目录；检查失败审计、无部分结果、无覆盖和暂存目录清理。"""
    executable, artifact, queries, inventory_artifact = [p.resolve(strict=True) for p in (executable, artifact, queries, inventory_artifact)]
    output = output.resolve()
    if output.exists() or output in {artifact.parent, queries.parent, inventory_artifact.parent}:
        raise ValueError('Choose a fresh output directory owned by this run')
    source, query_bytes, inventory_bytes = [p.read_bytes() for p in (artifact, queries, inventory_artifact)]
    original_query = json.loads(query_bytes)
    if json.loads(source).get('format') != 'HM-NATIVE-LEGACY-ARTIFACT/1' or original_query.get('format') != 'HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1':
        raise ValueError('Expected real legacy artifact and actor query inputs')
    if original_query.get('input_sha256') != hashlib.sha256(source).hexdigest():
        raise ValueError('Query must bind the exact original artifact')
    if json.loads(inventory_bytes).get('format') != 'HM-NATIVE-INVENTORY-ARTIFACT/1':
        raise ValueError('Expected authentic preserved inventory-v1 artifact')
    valid = next((deepcopy(row) for row in original_query['queries'] if row.get('actor_ids')), None)
    if valid is None:
        raise ValueError('Need a genuine nonempty valid query before the invalid batch row')
    output.mkdir(parents=True)
    originals = {p: sha(p) for p in (executable, artifact, queries, inventory_artifact)}
    cases = [
        'unknown_actor_after_valid', 'duplicate_query', 'duplicate_actor', 'wrong_source',
        'inventory_v1_profile', 'dangling_wallet', 'output_alias_input', 'output_alias_queries',
        'output_alias_audit', 'existing_result', 'existing_audit', 'occupied_result_stage', 'occupied_audit_stage',
    ]
    records = []
    for name in cases:
        directory = output / name
        directory.mkdir()
        a, q, result, audit = [directory / part for part in ('artifact.json', 'queries.json', 'result.json', 'audit.json')]
        a.write_bytes(source)
        q.write_bytes(query_bytes)
        expected, installed, audit_expected = '', 1, True
        pending_keeps = []
        changed = deepcopy(original_query)
        if name == 'unknown_actor_after_valid':
            changed['queries'] = [deepcopy(valid), {'query_id': 'late-unknown', 'actor_ids': ['missing.actor.cli.probe']}]
            write_json(q, changed)
            expected = 'UnknownActor'
        elif name == 'duplicate_query':
            changed['queries'] = [deepcopy(valid), deepcopy(valid)]
            write_json(q, changed)
            expected = 'DuplicateQueryId'
        elif name == 'duplicate_actor':
            actor = valid['actor_ids'][0]
            changed['queries'] = [deepcopy(valid), {'query_id': 'late-duplicate', 'actor_ids': [actor, actor]}]
            write_json(q, changed)
            expected = 'DuplicateQueryActor'
        elif name == 'wrong_source':
            changed['input_sha256'] = '0' * 64
            write_json(q, changed)
            expected = 'SourceMismatch'
        elif name == 'inventory_v1_profile':
            a.write_bytes(inventory_bytes)
            changed['input_sha256'] = sha(a)
            write_json(q, changed)
            expected = 'UnsupportedProjectionProfile'
        elif name == 'dangling_wallet':
            envelope = json.loads(source)
            payload = json.loads(envelope['payload_json'])
            payload['v']['actors']['v'][0]['v']['wallet_id'] = {'t': 'string', 'v': 'missing.wallet.cli.probe'}
            raw = json.dumps(payload, ensure_ascii=False, separators=(',', ':'))
            envelope['payload_json'] = raw
            envelope['payload_sha256'] = hashlib.sha256(raw.encode('utf-8')).hexdigest()
            write_json(a, envelope)
            changed['input_sha256'] = sha(a)
            write_json(q, changed)
            expected, installed = 'DanglingProjectionReference', 0
        elif name.startswith('output_alias_'):
            result = {'output_alias_input': a, 'output_alias_queries': q, 'output_alias_audit': audit}[name]
            expected, installed, audit_expected = 'AliasedPaths', 0, False
        elif name in {'existing_result', 'existing_audit'}:
            (result if name == 'existing_result' else audit).write_bytes(b'preserve-original\n')
            expected, installed, audit_expected = 'OutputExistsOrUnavailable', 0, False
        else:
            blocked = result if name == 'occupied_result_stage' else audit
            pending = Path(str(blocked) + '.pending')
            pending.mkdir()
            keep = pending / 'keep'
            keep.write_bytes(b'preserve-existing-stage\n')
            pending_keeps.append(keep)
            expected = 'OutputStageFailed'
            audit_expected = name != 'occupied_audit_stage'
        protected = {p: sha(p) for p in (a, q, result, audit, *pending_keeps) if p.is_file()}
        command = [str(executable), 'actor-commitments', '--input', str(a), '--queries', str(q), '--output', str(result), '--audit', str(audit)]
        process = subprocess.run(command, capture_output=True, timeout=60, check=False)
        (directory / 'stdout.txt').write_bytes(process.stdout)
        (directory / 'stderr.txt').write_bytes(process.stderr)
        unchanged = all(p.is_file() and sha(p) == digest for p, digest in protected.items())
        no_result = result in protected or not result.exists()
        no_new_audit = audit in protected or not audit.exists()
        checked_audit = no_new_audit
        if audit_expected:
            body = json.loads(audit.read_bytes()) if audit.is_file() else {}
            checked_audit = (
                body.get('format') == 'HM-NATIVE-ACTOR-COMMITMENTS-AUDIT/1'
                and body.get('binding_kind') == 'artifact' and body.get('ok') is False
                and body.get('exit_code') == 1 and body.get('install_count') == installed
                and body.get('installed') is (installed == 1) and body.get('completed_queries') == 0
                and body.get('error', {}).get('code') == expected
                and body.get('input_sha256') == sha(a) and body.get('queries_sha256') == sha(q)
                and not {'results', 'counts', 'result_sha256'}.intersection(body)
            )
        pending_clean = all(
            not Path(str(path) + '.pending').exists() or any(keep.parent == Path(str(path) + '.pending') for keep in pending_keeps)
            for path in (result, audit)
        )
        ok = process.returncode == 1 and expected.encode() in process.stderr and not process.stdout and unchanged and no_result and checked_audit and pending_clean
        records.append({'case': name, 'ok': ok, 'expected': expected, 'exit_code': process.returncode,
                        'installed_expected': installed, 'protected_unchanged': unchanged, 'no_partial_result': no_result,
                        'audit_expected': audit_expected, 'audit_contract': checked_audit, 'pending_cleanup': pending_clean,
                        'input_sha256': sha(a), 'queries_sha256': sha(q), 'command': command})
    original_unchanged = all(p.is_file() and sha(p) == digest for p, digest in originals.items())
    report = {'ok': original_unchanged and all(row['ok'] for row in records), 'checks': len(records),
              'failures': sum(not row['ok'] for row in records), 'original_files_unchanged': original_unchanged,
              'original_sha256': {str(path): digest for path, digest in originals.items()}, 'cases': records,
              'scope': 'development CLI negative supplement; not independent review, no old decoder gates rerun'}
    (output / 'actor-cli-failure-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main() -> int:
    """只接受已有构建与明确来源；不下载、编译或调用旧世界导出器。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--queries', type=Path, required=True)
    parser.add_argument('--inventory-artifact', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = run(args.executable, args.artifact, args.queries, args.inventory_artifact, args.output)
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
