"""准备有限地图门：原tiny图、正式双向道路作者夹具，以及只记录draw参数的固定原Canvas副本。"""
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
ORIGINAL_COMMIT = '7652d3829085b83039dba29ceaeeae1c17847ca7'
ORIGINAL_SHA = '70eecc70402f4dd3ba4bf64f7198bd3e7af9ca01e71704c22ca59229feb9a550'
DRAW_ARGUMENTS = {
    'rect': 'rect, color, filled=true, width=-1.0, antialiased=false',
    'line': 'from, to, color, width=-1.0, antialiased=false',
    'texture_rect': 'texture, rect, tile, modulate=Color.WHITE, transpose=false',
    'polyline': 'points, color, width=-1.0, antialiased=false',
    'arc': 'center, radius, start_angle, end_angle, point_count, color, width=-1.0, antialiased=false',
    'circle': 'position, radius, color, filled=true, width=-1.0, antialiased=false',
    'style_box': 'style_box, rect',
    'string': 'font, position, text, alignment=0, width=-1.0, font_size=16, modulate=Color.WHITE',
    'multiline_string': 'font, position, text, alignment=0, width=-1.0, font_size=16, max_lines=-1, modulate=Color.WHITE',
}


def instrument(source: str) -> str:
    """仅替换原生draw调用为参数记录；原几何、布局、颜色、文字、输入和字段读取一字不改。"""
    names = '|'.join(sorted(DRAW_ARGUMENTS, key=len, reverse=True))
    source = re.sub(r'\bdraw_(' + names + r')\(', r'_trace_draw_\1(', source)
    source += '\n## 测试副本只记录原生绘图参数，不绘制截图或反写游戏状态。\nvar trace_calls: Array=[]\n'
    for name, signature in DRAW_ARGUMENTS.items():
        args = [x.split('=')[0].strip() for x in signature.split(',')]
        source += f'\n## 原draw_{name}的无绘制参数记录适配，不包含几何算法。\nfunc _trace_draw_{name}({signature}) -> void:\n\ttrace_calls.append(["{name}",trace_value([{",".join(args)}])])\n'
    source += '''
## 将原参数变为稳定可比值；资源仅记录本门真正使用的公开样式与尺寸。
static func trace_value(value: Variant) -> Variant:
	if value is Vector2 or value is Vector2i: return [value.x,value.y]
	if value is Rect2: return [value.position.x,value.position.y,value.size.x,value.size.y]
	if value is Color: return [value.r,value.g,value.b,value.a]
	if value is Texture2D: return {"texture":value.resource_path,"size":[value.get_width(),value.get_height()]}
	if value is Font: return {"font":value.get_class(),"path":value.resource_path}
	if value is StyleBoxFlat:
		return {"style":value.get_class(),"color":trace_value(value.bg_color),"corners":[value.corner_radius_top_left,value.corner_radius_top_right,value.corner_radius_bottom_left,value.corner_radius_bottom_right]}
	if value is Dictionary:
		var result: Dictionary={}
		for key in value: result[key]=trace_value(value[key])
		return result
	if value is Array or value is PackedVector2Array:
		var result: Array=[]
		for item in value: result.append(trace_value(item))
		return result
	return value
'''
    return source


def write_candidate(root: Path, output: Path) -> None:
    """只刷新当前候选记录适配和专测，不重stage或重录固定旧oracle。"""
    target = output / 'game/tests/map_oracle'; target.mkdir(parents=True, exist_ok=True)
    (target / 'candidate_canvas.gd').write_text(instrument((root / 'game/ui/world_map_canvas.gd').read_text()))
    shutil.copy2(root / 'game/tests/test_kernel_map_view.gd', output / 'game/tests/test_kernel_map_view.gd')


def prepare(root: Path, output: Path, original_art: Path | None) -> dict:
    """固定原Git源摘要后只在新隔离根扩展测试道路；所有作者仍经正式stage。"""
    root, output = root.resolve(), output.resolve()
    original = subprocess.check_output(['git', 'show', ORIGINAL_COMMIT + ':game/ui/world_map_canvas.gd'], cwd=root, env={**os.environ, 'GIT_NO_LAZY_FETCH': '1'})
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA: raise ValueError('Original Canvas source hash mismatch')
    spec = importlib.util.spec_from_file_location('scene_prepare', root / 'tests/fixtures/m0_scene_context/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    report = module.prepare(root, output, original_art)
    path = output / 'content/foundation/procurement.json'; author = json.loads(path.read_text())
    author['settlements'].append({'id':'village.test','kind':'village','map_xy':[340,180],'entry_place':'test.entry','name_key':'test.village'})
    place = deepcopy(author['places'][0]); place.update(id='test.entry',settlement_id='village.test',passages=['test.room'],name_key='test.entry')
    author['places'].append(place)
    room = deepcopy(place); room.update(id='test.room',parent_place='test.entry',passages=['test.entry'],name_key='test.room'); author['places'].append(room)
    author['routes'] = [
        {'id':'road.out','origin':'city.west','destination':'village.test','duration_ticks':3,'cost_per_tick':1,'fatigue_per_tick':1,'risk_bp':0,'shape':[[200,240],[200,240],[280,240]]},
        {'id':'road.back','origin':'village.test','destination':'city.west','duration_ticks':3,'cost_per_tick':1,'fatigue_per_tick':1,'risk_bp':0}]
    author['texts'].update({'test.village':'试验村','test.entry':'试验村口','test.room':'试验村舍'})
    path.write_text(json.dumps(author, ensure_ascii=False, indent=2) + '\n')
    subprocess.run([sys.executable, '-c', 'from pathlib import Path; from tools import build; build.stage_content(Path.cwd(), foundation_sets=False)'], cwd=output, check=True)
    target = output / 'game/tests/map_oracle'; target.mkdir(parents=True)
    (target / 'original_canvas.gd').write_text(instrument(original.decode()))
    write_candidate(root, output)
    shutil.copy2(root / 'game/tests/test_map_geometry.gd', output / 'game/tests/test_map_geometry.gd')
    project = output / 'game/project.godot'; project.write_text(project.read_text().replace('Hanmo M0 isolated scene context','Hanmo M0 isolated map view'))
    report.update(original_canvas_sha256=ORIGINAL_SHA, scope='Finite map draw/input, fixed draw-argument oracle; no GUI/save acceptance')
    return report


def main() -> int:
    """明确区分新准备和只刷新候选，旧oracle记录不接受新实现重录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    parser.add_argument('--candidate-only', action='store_true')
    args = parser.parse_args()
    if args.candidate_only: write_candidate(args.source_root.resolve(), args.output.resolve())
    else: print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__': raise SystemExit(main())
