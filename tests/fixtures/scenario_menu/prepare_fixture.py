"""将真实two_eras作者源经正式compose/stage制成双playable节点fixture；tiny PNG不代表生产美术。"""
from __future__ import annotations
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[3]
TOOLS_REF = 'ffeefef'
SCHEMA_REF = '7fc429717cbfeca7e835c11b6c1c5bdd9612fe77'
RUNTIME_REF = '36474ad'
DATA = ['module_contracts.json', 'pack_runtime.json', 'rule_capabilities.json', 'presentation.schema.json']
BASE = 'tests/fixtures/content_packs/compose/new_packs/base'
AUTHOR = 'tests/fixtures/content_packs/second_scenario/two_eras.json'


def export_files(ref: str, paths: list[str], destination: Path) -> None:
    """只从指定Git对象导出普通文件到新目录，不读取或覆盖活跃工区产物。"""
    raw = subprocess.check_output(['git', 'archive', ref, *paths], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        for member in archive.getmembers():
            if member.isdir() or member.name.endswith('.uid'):
                continue
            if not member.isfile() or Path(member.name).is_absolute() or '..' in Path(member.name).parts:
                raise ValueError('Expected regular trusted source member')
            path = destination / member.name
            path.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(member) as stream:
                path.write_bytes(stream.read())


def tiny_presentation() -> dict:
    """用已有2×2 PNG的真实几何声明年龄分段专绘；仅保证节点合同，不声明美术质量。"""
    profile = {'id': 'person', 'identity_group': 'identity', 'mode': 'handcrafted',
               'view': 'half_body', 'pose': 'idle', 'context': 'daily', 'age_domain': [0, 100],
               'child_max_age': 15, 'age_variants': [], 'attire_sets': [], 'overrides': [],
               'fallbacks': [], 'battle_profile': {'style': 'none'}}
    for age, ages in [('child', [0, 15]), ('adult', [16, 100])]:
        profile['age_variants'].append({'id': age, 'ages': ages, 'body_basis': age + '.front',
                                       'proportion': age, 'components': []})
        geometry = {'source_size': [2, 2], 'logical_canvas': [2, 2], 'alpha_bounds': [0, 0, 2, 2],
                    'safe_crop': [0, 0, 2, 2], 'support_kind': 'half_body_bottom', 'support_point': [1, 2],
                    'pose': 'idle', 'facing': 'front', 'view': 'half_body', 'mask_ids': [], 'palette_slots': []}
        profile['overrides'].append({'id': age + '.portrait', 'ages': ages, 'view': 'half_body',
                                    'pose': 'idle', 'context': 'daily', 'priority': 1,
                                    'asset_id': 'portraits/portrait', 'body_basis': age + '.front',
                                    'proportion': age, 'geometry': geometry})
    return {'version': 2, 'default_background': 'city', 'default_portrait': 'portrait',
            'backgrounds': {kind: {'path': 'res://assets/art/pixel.png'} for kind in ['city', 'hall']},
            'portraits': {'portrait': {'path': 'res://assets/art/portrait.png'}},
            'components': {}, 'character_profiles': {'person': profile}, 'scene_templates': {}}


def write_json(path: Path, value: dict) -> None:
    """只写作者/表现输入；锁和安装索引完全交正式工具产生。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def prepare() -> dict:
    """新建隔离工具根，核精确Schema/links，正式组合安装后准备真实UI运行工程。"""
    root = Path(tempfile.mkdtemp(prefix='hanmo-dual-playable-'))
    project = root / 'project'; project.mkdir()
    export_files(TOOLS_REF, ['tools', 'schemas', 'game/core', BASE, AUTHOR,
                            *['game/data/' + name for name in DATA]], project)
    export_files(SCHEMA_REF, ['schemas/content.schema.json', 'tools/content/links.py'], project)
    assert hashlib.sha256((project / 'schemas/content.schema.json').read_bytes()).hexdigest() == '55e99dc202436b56a2ee7bad8957c1185a680ad64f589a12e72dc5cbfc9f79f7'
    assert hashlib.sha256((project / 'tools/content/links.py').read_bytes()).hexdigest() == 'df6882468061df5f2a0480b3b50bffe979979e62964ca3b84cbdf646a86b9d1b'
    original = (project / AUTHOR).read_bytes()
    author = json.loads(original)
    # 只补表现绑定；two_eras既有双人物、初态、年龄和玩法关系全部原样保留。
    for actor in author['actors']:
        actor['presentation_profile_id'] = 'person'
    manifest = json.loads((project / BASE / 'pack.json').read_text())
    manifest.update(id='scenario-menu', namespace='menu', required_capabilities=author['required_capabilities'])
    pack = root / 'author'; pack.mkdir()
    shutil.copytree(project / BASE / 'assets', pack / 'assets')
    write_json(pack / 'content.json', author)
    write_json(pack / 'presentation.json', tiny_presentation())
    write_json(pack / 'pack.json', manifest)
    write_json(project / 'content/foundation/two_eras.json', author)
    write_json(project / 'game/data/presentation.json', tiny_presentation())
    shutil.copytree(pack / 'assets', project / 'game/assets')
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                 'packs': [{'id': 'scenario-menu', 'version': '1.0.0'}]}
    write_json(root / 'selection.json', selection)
    command = "from pathlib import Path; import json; from tools.content.pack_compose import compose_pack_set; from tools import build; r=Path.cwd().parent; result=compose_pack_set([r/'author'],json.loads((r/'selection.json').read_text()),r/'composed'); build.stage_content(Path.cwd(),content_sets=[r/'composed'],foundation_sets=False); print(json.dumps(result))"
    run = subprocess.run([sys.executable, '-c', command], cwd=project, capture_output=True, text=True, timeout=90)
    (root / 'compose-stage.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError('Real compose/stage failed; preserved ' + str(root / 'compose-stage.log') + '\n' + run.stdout + run.stderr)
    game_root = root / 'runtime'; game_root.mkdir()
    export_files(RUNTIME_REF, ['game/core', 'game/storage', 'game/ui',
                              *['game/data/' + name for name in DATA],
                              'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav',
                              'game/assets/audio/LICENSE.json'], game_root)
    game = game_root / 'game'
    shutil.copytree(project / 'game/data/generated', game / 'data/generated')
    (game / 'tests').mkdir()
    shutil.copyfile(ROOT / 'game/tests/test_installed_scenario_menu.gd', game / 'tests/test_installed_scenario_menu.gd')
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="A2 dual playable nodes"\nconfig/version="test"\nconfig/restore_menu_isolated=true\n[display]\nwindow/size/viewport_width=1280\nwindow/size/viewport_height=800\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    index = json.loads((game / 'data/generated/installed-content.json').read_text())
    result = {'root': str(root), 'game': str(game), 'tools_ref': TOOLS_REF, 'schema_ref': SCHEMA_REF,
              'runtime_ref': RUNTIME_REF, 'pack_set_digest': index['sets'][0]['pack_set_digest'],
              'two_eras_original_sha256': hashlib.sha256(original).hexdigest(),
              'fixture_scope': 'real compose/stage; original dual-playable gameplay plus explicit tiny presentation binding; no production art claim'}
    write_json(root / 'preparation.json', result)
    return result


if __name__ == '__main__':
    print(json.dumps(prepare(), ensure_ascii=False, indent=2))
