"""复制20冻结原oracle到新小工程，并准备明确纯wire深转换边界；不重跑来源链或覆旧库。"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def digest(path: Path) -> str:
    """精确文件字节身份，来源和candidate库均不在准备时改写。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def encoded(value: object) -> bytes:
    """测试wire显式UTF-8；只用于新纯边界，不重新编码20原工件。"""
    return (json.dumps(value, ensure_ascii=False, separators=(',', ':')) + '\n').encode('utf-8')


def tagged(value: object) -> dict:
    """独立期望构造只给测试原值加标签；bool先于int以免Python子类关系混型。"""
    if value is None: return {'t': 'null'}
    if isinstance(value, bool): return {'t': 'bool', 'v': value}
    if isinstance(value, int): return {'t': 'int', 'v': str(value)}
    if isinstance(value, str): return {'t': 'string', 'v': value}
    if isinstance(value, list): return {'t': 'array', 'v': [tagged(row) for row in value]}
    if isinstance(value, dict): return {'t': 'dict', 'v': {key: tagged(row) for key, row in value.items()}}
    raise TypeError('Unsupported expected test value')


def semantics(destination: Path, original: Path) -> list:
    """从一份原oracle做明确值替换，不调用native生成expected；全部新增样本标为纯wire。"""
    base = json.loads((original / 'artifact.json').read_bytes())
    base_payload = json.loads(base['payload_json'])
    original_query = json.loads((original / 'queries.json').read_bytes())
    original_oracle = json.loads((original / 'oracle.json').read_bytes())
    action = base_payload['v']['activities']['v'][0]['v']['id']['v']
    target_lot = next(lot['v']['id']['v'] for inventory in base_payload['v']['inventories']['v'] for lot in inventory['v']['lots']['v'] if lot['v']['reserved_by']['v'] == action)
    all_actors = original_oracle['results']['v'][-1]['v']['actors']['v']
    idle = next(row for row in all_actors if row['v']['current_activity']['t'] == 'null')
    idle_id = idle['v']['actor_id']['v']
    rows = []
    values = {'极值': [None, False, True, -9223372036854775808, 9223372036854775807, '9223372036854775807'], '深': {'原序': ['second', 'first'], '空': []}}
    for name in ('deep_values', 'params_nul', 'owner_nul', 'owner_key_nul'):
        folder = destination / 'semantics' / name
        folder.mkdir(parents=True)
        envelope, payload = deepcopy(base), deepcopy(base_payload)
        probe = tagged(values if name == 'deep_values' else {'层': [None, {'text': 'before\x00after'}]} if name != 'owner_key_nul' else {'before\x00after': [True, 9]})
        if name in ('deep_values', 'params_nul'):
            for row in payload['v']['activities']['v']:
                if row['v']['id']['v'] == action: row['v']['params']['v']['bridge_probe'] = deepcopy(probe)
        if name in ('deep_values', 'owner_nul', 'owner_key_nul'):
            for inventory in payload['v']['inventories']['v']:
                for lot in inventory['v']['lots']['v']:
                    if lot['v']['id']['v'] == target_lot: lot['v']['owner']['v']['bridge_probe'] = deepcopy(probe)
        raw = encoded(payload).decode('utf-8')
        envelope['payload_json'], envelope['payload_sha256'] = raw, hashlib.sha256(raw.encode()).hexdigest()
        artifact = encoded(envelope); binding = hashlib.sha256(artifact).hexdigest()
        (folder / 'artifact.json').write_bytes(artifact)
        query = deepcopy(original_query); query['input_sha256'] = binding
        if name != 'deep_values':
            query['queries'] = [{'query_id': 'first-valid', 'actor_ids': [idle_id]}, {'query_id': 'late-unrepresentable', 'actor_ids': ['actor.player']}]
        query_bytes = encoded(query); (folder / 'queries.json').write_bytes(query_bytes)
        if name == 'deep_values':
            expected = deepcopy(original_oracle)
            for batch in expected['results']['v']:
                for actor in batch['v']['actors']['v']:
                    current = actor['v']['current_activity']
                    if current['t'] == 'dict' and current['v']['id']['v'] == action: current['v']['params']['v']['bridge_probe'] = deepcopy(probe)
                    for reservation in actor['v']['goods_reservations']['v']:
                        lot = reservation['v']['lot']
                        if lot['v']['id']['v'] == target_lot: lot['v']['owner']['v']['bridge_probe'] = deepcopy(probe)
            expected['input_sha256'], expected['queries_sha256'] = binding, hashlib.sha256(query_bytes).hexdigest()
            (folder / 'oracle.json').write_bytes(encoded(expected))
        else:
            safe = {'format': original_query['format'], 'input_sha256': binding, 'queries': [{'query_id': 'safe', 'actor_ids': [idle_id]}]}
            safe_bytes = encoded(safe); (folder / 'safe-queries.json').write_bytes(safe_bytes)
            expected = {'format': original_oracle['format'], 'input_sha256': binding, 'queries_sha256': hashlib.sha256(safe_bytes).hexdigest(), 'results': {'t': 'array', 'v': [{'t': 'dict', 'v': {'query_id': {'t': 'string', 'v': 'safe'}, 'actors': {'t': 'array', 'v': [deepcopy(idle)]}}}]}}
            (folder / 'safe-oracle.json').write_bytes(encoded(expected))
        rows.append({'id': name, 'directory': str(folder.relative_to(destination)), 'kind': 'pure_wire_boundary', 'input_sha256': binding, 'queries_sha256': hashlib.sha256(query_bytes).hexdigest()})
    for name, bad in [('late_query_nul', 'a\x00b'), ('late_error_path_nul', 'missing\x00actor'), ('late_utf8', 'UTF8_SENTINEL')]:
        folder = destination / 'semantics' / name; folder.mkdir(parents=True)
        query = deepcopy(original_query)
        query['queries'] = [{'query_id': 'first-valid', 'actor_ids': [idle_id]}, {'query_id': 'late-invalid', 'actor_ids': ['actor.player']}]
        if name == 'late_error_path_nul': query['queries'][1]['actor_ids'] = [bad]
        else: query['queries'][1]['query_id'] = bad
        query_bytes = encoded(query)
        if name == 'late_utf8': query_bytes = query_bytes.replace(b'UTF8_SENTINEL', b'invalid\xffutf8')
        (folder / 'queries.json').write_bytes(query_bytes)
        rows.append({'id': name, 'directory': str(folder.relative_to(destination)), 'kind': 'pure_wire_query_boundary', 'queries_sha256': hashlib.sha256(query_bytes).hexdigest()})
    return rows


def prepare(exports: Path, inventory: Path, library: Path, output: Path) -> dict:
    """每个candidate建全新目录；不复制core/美术，不写入任何已冻结输入或库。"""
    exports, inventory, library = [p.resolve(strict=True) for p in (exports, inventory, library)]
    output = output.resolve()
    if output.exists() or any(source == output or source in output.parents for source in (exports, inventory, ROOT / 'game', ROOT / 'native', ROOT / 'tests')):
        raise ValueError('Use a new isolated candidate directory')
    case_dirs = sorted(p for p in exports.iterdir() if p.is_dir() and (p / 'artifact.json').is_file())
    if len(case_dirs) != 20: raise ValueError('Expected the frozen 20 original cases')
    game = output / 'game'; destination = game / 'native_actor_commitments_bridge'; destination.mkdir(parents=True)
    rows = []
    for source in case_dirs:
        oracle = json.loads((source / 'oracle.json').read_bytes())
        query = json.loads((source / 'queries.json').read_bytes())
        if digest(source / 'artifact.json') != oracle['input_sha256'] or oracle['input_sha256'] != query['input_sha256'] or digest(source / 'queries.json') != oracle['queries_sha256']:
            raise ValueError('Frozen oracle/input binding mismatch: ' + source.name)
        target = destination / 'cases' / source.name; target.mkdir(parents=True)
        for name in ('artifact.json', 'queries.json', 'oracle.json'): shutil.copy2(source / name, target / name)
        rows.append({'id': source.name, 'directory': str(target.relative_to(destination)), 'input_sha256': digest(target / 'artifact.json'), 'queries_sha256': digest(target / 'queries.json'), 'oracle_sha256': digest(target / 'oracle.json')})
    old = destination / 'inventory_v1'; old.mkdir()
    for name in ('artifact.json', 'queries.json', 'oracle.json'): shutil.copy2(inventory / name, old / name)
    special = semantics(destination, exports / 'actual_reporting')
    target_library = destination / 'bin' / library.name; target_library.parent.mkdir(); shutil.copy2(library, target_library)
    extension = destination / 'hanmo_world.gdextension'
    extension.write_text('[configuration]\nentry_symbol="hanmo_world_library_init"\ncompatibility_minimum="4.7.2"\nreloadable=false\n[libraries]\nlinux.debug.x86_64="res://native_actor_commitments_bridge/bin/' + library.name + '"\n')
    (game / 'tests').mkdir(); shutil.copy2(ROOT / 'game/tests/test_native_actor_commitments_bridge.gd', game / 'tests/test_native_actor_commitments_bridge.gd')
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Native actor commitments bridge isolation"\nconfig/native_actor_bridge_isolated=true\n[display]\nwindow/size/viewport_width=960\nwindow/size/viewport_height=540\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    manifest = {'format': 'HM-ACTOR-BRIDGE-FIXTURE/1', 'cases': rows, 'semantics': special, 'library_sha256': digest(target_library), 'expected_queries': 60}
    (destination / 'cases.json').write_bytes(encoded(manifest))
    receipt = {'game': str(game), 'library_sha256': digest(target_library), 'extension_sha256': digest(extension), 'test_sha256': digest(game / 'tests/test_native_actor_commitments_bridge.gd'), 'manifest_sha256': digest(destination / 'cases.json'), 'policy': '20 original tagged oracles copied byte-for-byte; explicit pure-wire additions are transformed from original expected values, never generated by native'}
    (output / 'preparation.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    return receipt


def main() -> None:
    """输入均显式绑定，准备不运行编辑器、不声称自动发现或GUI通过。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exports', 'inventory', 'library', 'output'): parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args(); print(json.dumps(prepare(args.exports, args.inventory, args.library, args.output), ensure_ascii=False))


if __name__ == '__main__': main()
