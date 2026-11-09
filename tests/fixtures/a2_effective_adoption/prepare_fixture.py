"""原two_eras只增加监护人真实钱包，正式compose/stage后执行两tick收养及独立冷恢复。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.build import run_checked
from tools.content.io import canonical
from tools.content.pack_compose import compose_pack_set
from tools.content.pack_preview import prepare_preview


def write_json(path: Path, value: dict) -> None:
    """只向自有新作者或证据目录写规范JSON，不改源夹具及任何游戏state。"""
    path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(canonical(value))


def hashes(directory: Path) -> dict:
    """按原文件字节记录输入身份，Godot生成诊断只留独立目录。"""
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def prepare() -> dict:
    """保留已有两年代/独立图/初态/家庭角色，只加入实际支付所需作者钱包。"""
    spec = json.loads((Path(__file__).parent / 'author_changes.json').read_text())
    raw = (ROOT / spec['source']).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == spec['source_sha256']
    source = json.loads(raw); wallet = spec['wallet']
    source['wallets'].append(deepcopy(wallet))
    for scenario in source['scenarios']:
        scenario['enabled']['wallets'].append(wallet['id'])
        scenario['initial']['wallets'].append(deepcopy(wallet))
    restored_source = deepcopy(source); restored_source['wallets'].pop()
    for scenario in restored_source['scenarios']:
        scenario['enabled']['wallets'].pop(); scenario['initial']['wallets'].pop()
    assert restored_source == json.loads(raw)
    folder = Path(tempfile.mkdtemp(prefix='hanmo-a2-effective-adoption-'))
    author = folder / 'author'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base', author)
    manifest = json.loads((author / 'pack.json').read_text())
    manifest.update(id='effective-adoption', namespace='adopt', required_capabilities=source['required_capabilities'])
    write_json(author / 'pack.json', manifest); write_json(author / 'content.json', source)
    before = hashes(author)
    composed = folder / 'composed'
    result = compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                              'packs': [{'id': 'effective-adoption', 'version': '1.0.0'}]}, composed)
    prepared = prepare_preview(composed, folder / 'runtime', expected_pack_set_digest=result['pack_set_digest'])
    game = Path(prepared['game_path']); (game / 'tests').mkdir()
    shutil.copyfile(ROOT / 'game/tests/test_pack_effective_adoption.gd', game / 'tests/test_pack_effective_adoption.gd')
    evidence = folder / 'evidence'; evidence.mkdir()
    write_json(evidence / 'fixture.json', {'pack_set_digest': result['pack_set_digest'],
               'scenario': 'adopt::scenario.east', 'role': 'adopt::role.east', 'place': 'adopt::east.office',
               'prototype': 'adopt::actor.prototype', 'deceased': 'adopt::actor.deceased',
               'recipient_wallet': 'adopt::wallet.east', 'fee': 20, 'duration': 2, 'child_age': 6, 'guardian_age': 44})
    assert hashes(author) == before
    result = {'root': str(folder), **prepared, 'evidence': str(evidence), 'source_sha256': spec['source_sha256'],
              'author_files': before, 'only_gameplay_author_difference': 'guardian wallet enabled and initialized in both original scenarios'}
    write_json(folder / 'preparation.json', result)
    return result


def run(prepared: dict) -> dict:
    """仅一次有界runner：正式收养两tick进程与完成后独立v2恢复进程，不成长或继任。"""
    engine = os.environ['GODOT_BIN']; evidence = Path(prepared['evidence']); game = prepared['game_path']
    version = run_checked([engine, '--version'], timeout=10, log=evidence / 'engine-version.log').strip()
    assert version == '4.7.2.stable.official.ed1daf0bf'
    run_checked([engine, '--headless', '--editor', '--path', game, '--import', '--quit'], timeout=90, log=evidence / 'import.log')
    for mode in ['create', 'restore']:
        run_checked([engine, '--headless', '--path', game, '--script', 'res://tests/test_pack_effective_adoption.gd',
                     '--', '--mode=' + mode, '--evidence=' + str(evidence)], timeout=45, log=evidence / (mode + '.log'))
    results = [json.loads((evidence / (mode + '-result.json')).read_text()) for mode in ['create', 'restore']]
    assert len({row['process_id'] for row in results}) == 2 and all(row['failures'] == 0 for row in results)
    receipt = {'prepared': prepared, 'results': results, 'evidence_sha256': hashes(evidence),
               'scope': 'new-mode effective east scenario; real family_adopt plus two ticks; prototype remains definition-only; completed v2 cold restore; no direct instantiate, synthetic world, growth, succession, GUI or prior suites'}
    write_json(evidence / 'receipt.json', receipt)
    return receipt


def main() -> int:
    """默认仅准备真实新包；--run才执行限定短门，所有捕获原档和日志不提交。"""
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--run', action='store_true')
    args = parser.parse_args(); prepared = prepare()
    print(json.dumps(prepared, ensure_ascii=False, indent=2), flush=True)
    if args.run: print(json.dumps(run(prepared), ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
