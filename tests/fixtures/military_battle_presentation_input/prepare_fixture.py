"""复用正式军事tiny输入，固定旧UI/Canvas算法作为有限读取与绘图参数的局部oracle。"""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASE = 'e31bf73f8cc363cb7f35a6bbb215a01dd9256eca'
SOURCES = {'military_battle_screen.gd': '6564bf4b9e4f39f6758c03a95848c78dfdf4cf802890d6917326d24aae73eb84', 'military_battle_canvas.gd': '069acb85865cd36a5b99d5becc154a586b7336600164ea95f5e538857c4127da'}
DRAW = {
    'rect': 'rect, color, filled=true, width=-1.0, antialiased=false',
    'line': 'from, to, color, width=-1.0, antialiased=false',
    'texture_rect': 'rect_texture, rect, tile, modulate=Color.WHITE, transpose=false',
    'polyline': 'points, color, width=-1.0, antialiased=false',
    'arc': 'center, radius, start_angle, end_angle, point_count, color, width=-1.0, antialiased=false',
    'circle': 'position, radius, color, filled=true, width=-1.0, antialiased=false',
    'string': 'font, position, text, alignment=0, width=-1.0, font_size=16, modulate=Color.WHITE',
    'colored_polygon': 'points, color, uv=PackedVector2Array(), texture=null',
    'dashed_line': 'from, to, color, width=-1.0, dash=2.0, aligned=true, antialiased=false',
}


def instrument(source: str) -> str:
    """仅记录原生绘图参数；原布局、文字、单位和输入算法保持原文。"""
    source = re.sub(r'\bdraw_(' + '|'.join(DRAW) + r')\(', r'_trace_\1(', source)
    source += '\n## 测试专用绘图参数记录，不产生截图或改写来源。\nvar trace_calls: Array=[]\n'
    for name, signature in DRAW.items():
        args = [value.split('=')[0].strip() for value in signature.split(',')]
        source += '\n## 记录原生绘制参数，供固定旧实现逐值对照。\nfunc _trace_' + name + '(' + signature + ') -> void:\n\ttrace_calls.append(["' + name + '",_trace_value([' + ','.join(args) + '])])\n'
    source += '''
## 只将资源和向量参数变成稳定可比较值，不生成替代画面。
static func _trace_value(value: Variant) -> Variant:
	if value is Vector2 or value is Vector2i: return [value.x,value.y]
	if value is Rect2: return [value.position.x,value.position.y,value.size.x,value.size.y]
	if value is Color: return [value.r,value.g,value.b,value.a]
	if value is Font: return {"font":value.get_class(),"path":value.resource_path}
	if value is Texture2D: return {"texture":value.resource_path,"size":[value.get_width(),value.get_height()]}
	if value is Array or value is PackedVector2Array:
		var result: Array=[]
		for item in value: result.append(_trace_value(item))
		return result
	return value
'''
    return source


def refresh(root: Path, output: Path) -> None:
    """只刷新候选源码和专测；固定旧oracle不从新源码重录。"""
    game = output / 'game'
    for folder in ['core', 'ui', 'storage']:
        shutil.copytree(root / 'game' / folder, game / folder, dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.uid', '*.pyc'))
    for name in ['military_fixture.gd', 'military_ui_fixture.gd', 'military_presentation_fixture.gd', 'test_military_battle_screen.gd', 'test_military_battle_presentation_input.gd']:
        shutil.copy2(root / 'game/tests' / name, game / 'tests' / name)
    oracle = game / 'tests/military_battle_oracle'; oracle.mkdir(exist_ok=True)
    (oracle / 'candidate_canvas.gd').write_text(instrument((root / 'game/ui/military_battle_canvas.gd').read_text()))


def prepare(root: Path, output: Path, art: Path) -> None:
    """正式军事作者经过原compose/stage；tiny图和音频只服务有界控件门。"""
    spec = importlib.util.spec_from_file_location('march_prepare', root / 'tests/fixtures/military_march_kernel/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    module.prepare(root, output, art, False)
    game = output / 'game'; refresh(root, output)
    oracle = game / 'tests/military_battle_oracle'
    hashes = {}
    for name in ['military_battle_screen.gd', 'military_battle_canvas.gd']:
        raw = subprocess.check_output(['git', 'show', BASE + ':game/ui/' + name], cwd=root)
        digest = hashlib.sha256(raw).hexdigest()
        if digest != SOURCES[name]: raise ValueError('Original source identity changed: ' + name)
        (oracle / name).write_bytes(raw); hashes[name] = digest
        if 'canvas' in name: (oracle / 'original_canvas.gd').write_text(instrument(raw.decode()))
    target = game / 'assets/art/alpha-military'; target.mkdir(parents=True)
    entries = []
    for identifier in ['battlefield_valley', 'troop_infantry', 'troop_archers', 'troop_cavalry']:
        shutil.copy2(art / ('pixel.png' if identifier == 'battlefield_valley' else 'portrait.png'), target / (identifier + '.png'))
        entries.append({'id': identifier, 'file': identifier + '.png'})
    (target / 'manifest.json').write_text(json.dumps({'assets': entries}))
    target = game / 'assets/audio'; target.mkdir(parents=True)
    for name in ['ambience.wav', 'paper.wav']: (target / name).write_bytes(subprocess.check_output(['git', 'show', BASE + ':game/assets/audio/' + name], cwd=root))
    (output / 'military-page-fixture.json').write_text(json.dumps({'baseline': BASE, 'original_sha256': hashes, 'scope': 'Military page/draft and real controls with tiny art; no visual/save/full-world acceptance'}, indent=2)+'\n')


def main() -> None:
    """新目录完整准备，或显式刷新候选；不覆盖原失败日志和固定oracle。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT); parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-art', type=Path); parser.add_argument('--refresh', action='store_true')
    args = parser.parse_args()
    if args.refresh: refresh(args.root.resolve(), args.output.resolve())
    else: prepare(args.root.resolve(), args.output.absolute(), args.fixture_art.resolve())


if __name__ == '__main__': main()
