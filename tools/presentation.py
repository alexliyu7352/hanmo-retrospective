"""校验唯一表现清单与真实资源；只处理声明，不生成身份或运行组合渲染。"""
import json
import math
import os
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path
from functools import lru_cache
from io import BytesIO
from PIL import Image, UnidentifiedImageError
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 1048576
GROUPS = ('components', 'character_profiles', 'scene_templates')
CLOTHING = ('garment', 'headwear', 'accessory')
ANATOMY = ('body', 'face', 'hair', 'beard')
MAX_FEATURE_COMBINATIONS = 256


def issue(path, code):
    """返回跨语言共享的码与JSON指针，文本不参与契约判断。"""
    return {'code': code, 'path': path, 'message': code}


def pointer(*parts):
    """转义所有动态段，避免斜线或波浪号改变诊断字段身份。"""
    return ''.join('/' + str(p).replace('~', '~0').replace('/', '~1') for p in parts)


def _pairs(pairs):
    """拒绝解析时被覆盖的同名键。"""
    result = {}
    for key, value in pairs:
        if key in result: raise ValueError('DuplicateKey')
        result[key] = value
    return result


def _constant(_value):
    """拒绝非标准非有限JSON数字。"""
    raise ValueError('InvalidNumber')


def _bounded(value):
    """在序列化或递归Schema之前检查深度、类型及有限精确整数边界。"""
    todo = [(value, 0)]; count = 0
    while todo:
        value, depth = todo.pop(); count += 1
        if depth > 32 or count > 200000: raise ValueError('ManifestLimits')
        if type(value) is dict:
            if len(value) > 512 or any(type(k) is not str or len(k) > 16384 for k in value): raise ValueError('ManifestLimits')
            todo.extend((v, depth + 1) for v in value.values())
        elif type(value) is list: todo.extend((v, depth + 1) for v in value)
        elif type(value) is str:
            if len(value) > 16384: raise ValueError('ManifestLimits')
            value.encode('utf8')
        elif type(value) is int:
            if not -2147483647 <= value <= 2147483647: raise ValueError('InvalidNumber')
        elif type(value) is float:
            if not math.isfinite(value) or abs(value) > 2147483647: raise ValueError('InvalidNumber')
        elif value is not None and type(value) is not bool: raise ValueError('ManifestType')



def _key_issue(data):
    """提前定位Schema不允许的路径分隔键，诊断本身仍使用正确转义指针。"""
    todo = [(data, [])]
    while todo:
        value, path = todo.pop()
        if isinstance(value, dict):
            for key, child in value.items():
                if '/' in key or '~' in key: return issue(pointer(*path, key), 'InvalidKey')
                todo.append((child, path + [key]))
        elif isinstance(value, list): todo.extend((child, path + [n]) for n, child in enumerate(value))
    return None

def read_registry(path):
    """读取有界严格JSON；不将无效输入降级成空注册表。"""
    with Path(path).open('rb') as stream: raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES: raise ValueError('ManifestTooLarge')
    data = json.loads(raw.decode('utf8'), object_pairs_hook=_pairs, parse_constant=_constant)
    _bounded(data)
    if type(data) is not dict: raise ValueError('ManifestObjectExpected')
    return data


def _asset_path(game, raw):
    """只接受美术目录内无符号链接的正规资源路径，防止宿主边界不同。"""
    if not raw.startswith('res://assets/art/'): raise ValueError('AssetPath')
    parts = raw[6:].split('/')
    if any(p in ('', '.', '..') for p in parts): raise ValueError('AssetPath')
    path = Path(game)
    for part in parts:
        path = path / part
        if path.is_symlink(): raise ValueError('AssetPath')
    if not path.is_file(): raise ValueError('MissingAsset')
    if path.stat().st_size > 16 * 1024 * 1024: raise ValueError('AssetTooLarge')
    return path


def _asset_bytes(path):
    """资源读取以实际字节限制为准，不能靠先前stat结果允许并发增长越界。"""
    with path.open('rb') as stream: raw = stream.read(16 * 1024 * 1024 + 1)
    if len(raw) > 16 * 1024 * 1024: raise ValueError('AssetTooLarge')
    return raw


@lru_cache(maxsize=128)
def _decode(raw, extension):
    """按完整不可变资源字节缓存解码元数据；校验和读取像素都不可省略。"""
    try:
        with Image.open(BytesIO(raw)) as image:
            if image.format != {'png': 'PNG', 'webp': 'WEBP', 'jpg': 'JPEG', 'jpeg': 'JPEG'}.get(extension): raise ValueError('AssetDecode')
            if image.width * image.height > 16 * 1024 * 1024: raise ValueError('AssetTooLarge')
            image.verify()
        with Image.open(BytesIO(raw)) as image:
            image.load()
            bounds = image.convert('RGBA').getchannel('A').getbbox()
            return list(image.size), ([] if bounds is None else [bounds[0], bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]])
    except Image.DecompressionBombError as exc:
        raise ValueError('AssetTooLarge') from exc
    except (OSError, UnidentifiedImageError, SyntaxError) as exc:
        raise ValueError('AssetDecode') from exc



@lru_cache(maxsize=4)
def _svg_engine(engine):
    """旧SVG专用探针只接受固定Godot版本，不下载安装或执行作者提供的脚本。"""
    try:
        result = subprocess.run([engine, '--version'], capture_output=True, text=True, timeout=10)
        if result.returncode or result.stdout.strip() != '4.7.2.stable.official.ed1daf0bf': raise ValueError('SVGDecoderUnavailable')
        return engine
    except (OSError, subprocess.TimeoutExpired) as exc: raise ValueError('SVGDecoderUnavailable') from exc


def _decode_svg(path):
    """保留旧SVG格式并实际解码；外部引用/实体拒绝，缺固定解码器明确失败。"""
    raw = _asset_bytes(path)
    if b'<!DOCTYPE' in raw.upper() or b'<!ENTITY' in raw.upper(): raise ValueError('AssetDecode')
    try:
        root = ET.fromstring(raw)
        if root.tag.split('}')[-1] != 'svg': raise ValueError('AssetDecode')
        for node in root.iter():
            if node.tag.split('}')[-1] == 'script': raise ValueError('AssetDecode')
            for key, value in node.attrib.items():
                if key.split('}')[-1] == 'href' and not value.startswith('#'): raise ValueError('AssetDecode')
    except ET.ParseError as exc: raise ValueError('AssetDecode') from exc
    engine = os.environ.get('GODOT_BIN') or shutil.which('godot') or shutil.which('godot4')
    if not engine: raise ValueError('SVGDecoderUnavailable')
    engine = _svg_engine(engine)
    try:
        # 独立空工程防止--script仍触发被检查工程的autoload。
        with tempfile.TemporaryDirectory(prefix='hanmo-svg-') as folder:
            project = Path(folder)
            (project / 'project.godot').write_text('config_version=5\n', encoding='utf8')
            result = subprocess.run([engine, '--headless', '--audio-driver', 'Dummy', '--path', str(project),
                                     '--script', str(Path(__file__).resolve().with_name('presentation_svg_probe.gd')), '--', str(path.resolve())],
                                    capture_output=True, text=True, timeout=15)
        replies = [line[7:] for line in result.stdout.splitlines() if line.startswith('HM_SVG=')]
        if len(replies) != 1: raise ValueError('AssetDecode')
        reply = json.loads(replies[0])
        if result.returncode or not reply.get('ok'): raise ValueError(reply.get('code', 'AssetDecode'))
    except (OSError, subprocess.TimeoutExpired, json.JSONDecodeError) as exc: raise ValueError('AssetDecode') from exc
    # SVG只保留旧纹理兼容；新分层几何必须使用可核对透明边界的栅格格式。
    return [], []

def _rect(rect, size):
    """矩形用起点与正宽高表示，必须完整包含在指定画布内。"""
    return rect[2] > 0 and rect[3] > 0 and rect[0] + rect[2] <= size[0] and rect[1] + rect[3] <= size[1]


def _cycle(graph):
    """有界迭代消除无依赖节点，剩余节点表示自环或多节点循环。"""
    remaining = {key: set(values) for key, values in graph.items()}
    while remaining:
        ready = {key for key, values in remaining.items() if not values & remaining.keys()}
        if not ready: return True
        remaining = {key: values for key, values in remaining.items() if key not in ready}
    return False


def _geometry(geo, asset, path, assets, components):
    """核对声明像素、透明边界、裁切和遮罩引用，不推断图像审美质量。"""
    if asset not in assets: return issue(path.rsplit('/', 1)[0] + '/asset_id', 'MissingAssetReference')
    size, bounds = assets[asset]
    if not size: return issue(path, 'LayeredRasterRequired')
    if geo['source_size'] != size: return issue(path + '/source_size', 'SourceSize')
    if not bounds or geo['alpha_bounds'] != bounds: return issue(path + '/alpha_bounds', 'AlphaBounds')
    if not _rect(geo['safe_crop'], size): return issue(path + '/safe_crop', 'GeometryBounds')
    for index, mask in enumerate(geo['mask_ids']):
        if mask not in components: return issue(path + '/mask_ids/' + str(index), 'MissingComponent')
        if components[mask]['slot'] != 'mask': return issue(path + '/mask_ids/' + str(index), 'MaskSlot')
    return None


def _composition(ids, variant, profile, data, path):
    """检查一个年龄和独立衣着的完整组合，所有查询均来自本次候选。"""
    components = data.get('components', {}); selected = []
    for id in ids:
        if id not in components: return issue(path, 'MissingComponent')
        selected.append(components[id])
    slots = [c['slot'] for c in selected]
    if len(ids) != len(set(ids)) or len([s for s in slots if s != 'mask']) != len(set(slots) - {'mask'}): return issue(path, 'DuplicateSlot')
    if 'body' not in slots or 'garment' not in slots: return issue(path, 'RequiredSlot')
    base = selected[slots.index('body')]['geometry']; tags = {tag for c in selected for tag in c['tags']}
    graph = {}
    for c in selected:
        g = c['geometry']
        if c['body_basis'] != variant['body_basis'] or c['proportion'] != variant['proportion']: return issue(path, 'IncompatibleBody')
        if g['view'] != profile['view'] or g['pose'] != profile['pose'] or any(g[k] != base[k] for k in ('logical_canvas', 'facing')): return issue(path, 'IncompatibleGeometry')
        if not set(c['compatible_tags']) <= tags or set(c['incompatible_tags']) & tags: return issue(path, 'IncompatibleTags')
        edges = c['after'] + g['mask_ids']
        if any(edge not in ids for edge in edges): return issue(path, 'MissingCompositionDependency')
        graph[c['id']] = edges
    if _cycle(graph): return issue(path, 'CompositionCycle')
    return None


def _feature_combinations(variant, components, path):
    """仅替换现有解剖槽；同槽谓词必须互斥，枚举前限制候选笛卡尔积。"""
    bindings = variant.get('feature_bindings', [])
    choices = {}; predicates = {}; seen = set()
    for n, binding in enumerate(bindings):
        location = path + '/feature_bindings/' + str(n)
        if binding['id'] in seen: return None, issue(location + '/id', 'RecordIdentity')
        seen.add(binding['id'])
        component_id = binding['component_id']
        if component_id not in components: return None, issue(location + '/component_id', 'MissingComponent')
        slot = components[component_id]['slot']
        if slot not in ANATOMY: return None, issue(location + '/component_id', 'AnatomySlot')
        indexes = [i for i, id in enumerate(variant['components']) if components[id]['slot'] == slot]
        if not indexes: return None, issue(location + '/component_id', 'FeatureBindingSlot')
        if len(indexes) != 1: return None, issue(path + '/components', 'DuplicateSlot')
        features = binding['features']
        for previous in predicates.get(slot, []):
            if all(key not in previous or previous[key] == value for key, value in features.items()):
                return None, issue(location + '/features', 'FeatureBindingOverlap')
        predicates.setdefault(slot, []).append(features)
        index = indexes[0]
        options = choices.setdefault(index, [variant['components'][index]])
        if component_id not in options: options.append(component_id)
    count = 1
    for options in choices.values():
        count *= len(options)
        if count > MAX_FEATURE_COMBINATIONS: return None, issue(path + '/feature_bindings', 'FeatureCombinationLimit')
    combinations = [variant['components'][:]]
    for index, options in choices.items():
        combinations = [ids[:index] + [component_id] + ids[index+1:] for ids in combinations for component_id in options]
    return combinations, None


def _compatible(source, target):
    """回退只允许相同作者身份组、视图与儿童界限，并完整覆盖支持年龄。"""
    return all(source[k] == target[k] for k in ('identity_group', 'view', 'pose', 'context', 'child_max_age')) and target['age_domain'][0] <= source['age_domain'][0] and target['age_domain'][1] >= source['age_domain'][1]


def _battle_compatible(source, target):
    """战斗目标独立选择姿势；身份组、儿童界限、年龄覆盖和终点约束仍必须一致。"""
    return (target['view'] == 'battle' and target['context'] == 'battle'
            and target['battle_profile']['style'] == 'none'
            and all(source[k] == target[k] for k in ('identity_group', 'child_max_age'))
            and target['age_domain'][0] <= source['age_domain'][0]
            and target['age_domain'][1] >= source['age_domain'][1])


def _profile(profile, data, path, assets):
    """检查年龄覆盖、儿童骨架、独立衣着、专绘优先级与显式回退引用。"""
    lo, hi = profile['age_domain']; cursor = lo; variants = profile['age_variants']; components = data.get('components', {})
    if lo > hi: return issue(path + '/age_domain', 'AgeCoverage')
    seen = set(); candidates = []
    for n, v in enumerate(variants):
        p = path + '/age_variants/' + str(n)
        if v['id'] in seen: return issue(p + '/id', 'RecordIdentity')
        seen.add(v['id'])
        a, b = v['ages']
        if a != cursor or a > b or b > hi: return issue(p + '/ages', 'AgeCoverage')
        cursor = b + 1
        if (a <= profile['child_max_age'] < b) or v['proportion'] != ('child' if b <= profile['child_max_age'] else 'adult'): return issue(p, 'ChildBasis')
        for i, id in enumerate(v['components']):
            if id not in components: return issue(p + '/components/' + str(i), 'MissingComponent')
            if components[id]['slot'] in CLOTHING: return issue(p + '/components/' + str(i), 'AnatomySlot')
        if profile['mode'] == 'handcrafted' and 'feature_bindings' in v: return issue(p + '/feature_bindings', 'HandcraftedComposition')
        combinations, error = _feature_combinations(v, components, p)
        if error: return error
        candidates.append(combinations)
    if cursor != hi + 1: return issue(path + '/age_domain', 'AgeCoverage')
    if profile['mode'] == 'composite':
        if not profile['attire_sets']: return issue(path + '/attire_sets', 'RequiredAttire')
        attire_ids = set()
        for n, attire in enumerate(profile['attire_sets']):
            p = path + '/attire_sets/' + str(n)
            if attire['id'] in attire_ids: return issue(p + '/id', 'RecordIdentity')
            attire_ids.add(attire['id']); bindings = {}
            for i, binding in enumerate(attire['bindings']):
                q = p + '/bindings/' + str(i)
                if binding['body_basis'] in bindings: return issue(q, 'DuplicateBasis')
                bindings[binding['body_basis']] = (binding, q)
                for j, id in enumerate(binding['components']):
                    if id not in components: return issue(q + '/components/' + str(j), 'MissingComponent')
                    if components[id]['slot'] not in CLOTHING + ('mask',): return issue(q + '/components/' + str(j), 'AttireSlot')
            if set(bindings) != {v['body_basis'] for v in variants}: return issue(p + '/bindings', 'AttireCoverage')
            for i, v in enumerate(variants):
                binding, q = bindings[v['body_basis']]
                for anatomy in candidates[i]:
                    error = _composition(anatomy + binding['components'], v, profile, data, q)
                    if error: return error
    elif profile['attire_sets'] or any(v['components'] for v in variants): return issue(path, 'HandcraftedComposition')
    overrides = profile['overrides']; seen = set()
    for n, item in enumerate(overrides):
        p = path + '/overrides/' + str(n); a, b = item['ages']
        if item['id'] in seen: return issue(p + '/id', 'RecordIdentity')
        seen.add(item['id'])
        matching = [v for v in variants if v['ages'][0] <= a <= b <= v['ages'][1] and v['body_basis'] == item['body_basis'] and v['proportion'] == item['proportion']]
        if not matching or any(item[k] != profile[k] for k in ('view', 'pose', 'context')): return issue(p, 'OverrideAge')
        if item['geometry']['view'] != item['view'] or item['geometry']['pose'] != item['pose'] or item['geometry']['mask_ids']: return issue(p, 'IncompatibleGeometry')
        error = _geometry(item['geometry'], item['asset_id'], p + '/geometry', assets, components)
        if error: return error
        for old in overrides[:n]:
            if old['priority'] == item['priority'] and all(old[k] == item[k] for k in ('view', 'pose', 'context')) and max(a, old['ages'][0]) <= min(b, old['ages'][1]): return issue(p, 'OverrideOverlap')
    if profile['mode'] == 'handcrafted':
        cursor = lo
        for item in sorted(overrides, key=lambda i: i['ages'][0]):
            if item['ages'][0] > cursor: return issue(path + '/overrides', 'OverrideCoverage')
            cursor = max(cursor, item['ages'][1] + 1)
        if cursor <= hi: return issue(path + '/overrides', 'OverrideCoverage')
    profiles = data.get('character_profiles', {})
    for n, id in enumerate(profile['fallbacks']):
        p = path + '/fallbacks/' + str(n)
        if id not in profiles: return issue(p, 'MissingProfile')
        if not _compatible(profile, profiles[id]): return issue(p, 'FallbackCompatibility')
    battle = profile['battle_profile']
    if battle['style'] == 'simple':
        target = profiles.get(battle['profile_id']); p = path + '/battle_profile/profile_id'
        if target is None: return issue(p, 'MissingProfile')
        if not _battle_compatible(profile, target): return issue(p, 'BattleCompatibility')
    return None


def scene_selector_valid(selector, target_ref, path=''):
    """校验唯一受支持的候选形状；只描述匹配能力，不证明真实对象存在或可见。"""
    def rejected(suffix, code):
        """公开结果保留相对字段路径，便于清单和运行期共同定位。"""
        return dict(ok=False, **issue(path + suffix, code))
    if type(selector) is not dict or set(selector) != {'command_kind', 'params'}:
        return rejected('', 'InvalidSceneSelector')
    kind = selector['command_kind']; params = selector['params']
    if type(kind) is not str or not 1 <= len(kind) <= 128:
        return rejected('/command_kind', 'InvalidSceneSelector')
    targets = {'move_local': 'place', 'inspect': 'prop', 'talk': 'actor', 'estate_build': 'estate'}
    if kind not in targets: return rejected('/command_kind', 'UnsupportedSceneCommand')
    if type(params) is not dict: return rejected('/params', 'InvalidSceneSelector')
    required = {'role_id'} if kind == 'talk' else {'facility'} if kind == 'estate_build' else set()
    if set(params) != required: return rejected('/params', 'InvalidSceneSelector')
    if kind == 'talk' and (type(params['role_id']) is not str or not 1 <= len(params['role_id']) <= 128):
        return rejected('/params', 'InvalidSceneSelector')
    if kind == 'estate_build' and params['facility'] not in ('farm', 'granary', 'palisade'):
        return rejected('/params', 'InvalidSceneSelector')
    if type(target_ref) is not dict or set(target_ref) != {'kind', 'id'} or target_ref['kind'] not in targets.values() or type(target_ref['id']) is not str or not 1 <= len(target_ref['id']) <= 128:
        return rejected('/target_ref', 'InvalidSceneTarget')
    if target_ref['kind'] != targets[kind]: return rejected('/target_ref', 'SceneTargetKind')
    return {'ok': True}


def _scene(scene, data, path, assets):
    """验证静态分层、人物位置和热点目标，不执行跳转或修改世界状态。"""
    for n, rect in enumerate(scene['ui_safe_areas']):
        if not _rect(rect, scene['logical_canvas']): return issue(path + '/ui_safe_areas/' + str(n), 'GeometryBounds')
    layers = {}; graph = {}; backgrounds = 0
    for n, layer in enumerate(scene['layers']):
        p = path + '/layers/' + str(n)
        if layer['id'] in layers: return issue(p + '/id', 'DuplicateLayer')
        layers[layer['id']] = layer
    for n, layer in enumerate(scene['layers']):
        p = path + '/layers/' + str(n); kind = layer['kind']; graph[layer['id']] = layer['after']
        if kind == 'background' and 'display_width' in layer: return issue(p + '/display_width', 'IncompatibleGeometry')
        if any(id not in layers for id in layer['after']): return issue(p + '/after', 'MissingLayer')
        if 'visible_if' in layer and layer.get('target_ref', {}).get('kind') != 'estate': return issue(p + '/visible_if', 'ScenePredicateTarget')
        if kind == 'actor':
            if 'target_ref' in layer and layer['target_ref']['kind'] != 'actor': return issue(p + '/target_ref', 'SceneTargetKind')
            if ('target_ref' in layer and 'extent' not in layer) or ('extent' in layer and any(value <= 0 for value in layer['extent'])): return issue(p + '/extent', 'SceneActorExtent')
            if 'profile_id' in layer and layer['profile_id'] not in data.get('character_profiles', {}): return issue(p + '/profile_id', 'MissingProfile')
        elif kind == 'hotspot':
            if layer['target_layer_id'] not in layers or layer['target_layer_id'] == layer['id']: return issue(p + '/target_layer_id', 'MissingLayer')
            if not _rect(layer['rect'], [100, 100]): return issue(p + '/rect', 'GeometryBounds')
            if 'candidate_selector' in layer:
                target = layers[layer['target_layer_id']].get('target_ref', {})
                checked = scene_selector_valid(layer['candidate_selector'], target, p + '/candidate_selector')
                if not checked['ok']: return issue(checked['path'], checked['code'])
        else:
            backgrounds += kind == 'background'
            error = _geometry(layer['geometry'], layer['asset_id'], p + '/geometry', assets, data.get('components', {}))
            if error: return error
            if layer['geometry']['view'] != 'scene' or layer['geometry']['logical_canvas'] != scene['logical_canvas'] or layer['geometry']['mask_ids']: return issue(p + '/geometry', 'IncompatibleGeometry')
    if backgrounds != 1: return issue(path + '/layers', 'RequiredBackground')
    if _cycle(graph): return issue(path + '/layers', 'SceneCycle')
    return None


def validate_registry(data: dict, game: Path) -> list[dict]:
    """共享Schema检查后校验整个候选；不修改传入对象或现有输出。"""
    try:
        _bounded(data)
        if len(json.dumps(data, ensure_ascii=False, allow_nan=False, separators=(',', ':')).encode('utf8')) > MAX_BYTES: return [issue('', 'ManifestTooLarge')]
        schema = read_registry(Path(game) / 'data/presentation.schema.json')
    except (OSError, ValueError, TypeError, UnicodeError, RecursionError, OverflowError) as exc: return [issue('', str(exc))]
    key_error = _key_issue(data)
    if key_error: return [key_error]
    errors = list(Draft202012Validator(schema).iter_errors(data))
    if errors: return [issue(pointer(*e.absolute_path), 'Schema') for e in errors]
    for group in GROUPS:
        if data['version'] == 1 and group in data: return [issue('/' + group, 'VersionField')]
        if data['version'] == 2 and group not in data: return [issue('/' + group, 'RequiredGroup')]
        if len(data.get(group, {})) > (512 if group == 'components' else 128): return [issue('/' + group, 'TooManyRecords')]
    assets = {}
    for group, default in [('backgrounds', 'default_background'), ('portraits', 'default_portrait')]:
        if data[default] not in data[group]: return [issue('/' + default, 'ContentReference')]
        for id in sorted(data[group]):
            entry = data[group][id]
            # 取景范围不成为规则坐标；非法宽高在资源发布前拒绝。
            if 'map_world_rect' in entry and (entry['map_world_rect'][2] <= 0 or entry['map_world_rect'][3] <= 0):
                return [issue(pointer(group, id, 'map_world_rect'), 'MapWorldRect')]
            try:
                path = _asset_path(game, entry['path'])
                assets[group + '/' + id] = _decode_svg(path) if path.suffix == '.svg' else _decode(_asset_bytes(path), path.suffix[1:])
            except (ValueError, OSError) as exc: return [issue(pointer(group, id, 'path'), str(exc))]
    for group in GROUPS:
        for id in sorted(data.get(group, {})):
            record = data[group][id]
            if id != record['id']: return [issue(pointer(group, id, 'id'), 'RecordIdentity')]
    for id in sorted(data.get('components', {})):
        component = data['components'][id]
        if set(component['compatible_tags']) & set(component['incompatible_tags']): return [issue(pointer('components', id), 'ContradictoryTags')]
        for n, dependency in enumerate(component['after']):
            if dependency not in data['components']: return [issue(pointer('components', id, 'after', n), 'MissingComponent')]
        error = _geometry(component['geometry'], component['asset_id'], pointer('components', id, 'geometry'), assets, data['components'])
        if error: return [error]
    for id in sorted(data.get('character_profiles', {})):
        profile = data['character_profiles'][id]
        error = _profile(profile, data, pointer('character_profiles', id), assets)
        if error: return [error]
    component_graph = {id: c['after'] + c['geometry']['mask_ids'] for id, c in data.get('components', {}).items()}
    if _cycle(component_graph): return [issue('/components', 'CompositionCycle')]
    graph = {id: data['character_profiles'][id]['fallbacks'] for id in sorted(data.get('character_profiles', {}))}
    if _cycle(graph):
        # 逐起点定位参与循环的第一个稳定ID，不将依赖环当作顺序问题。
        for id in graph:
            todo = list(graph[id]); seen = set()
            while todo:
                next_id = todo.pop()
                if next_id == id: return [issue(pointer('character_profiles', id, 'fallbacks'), 'FallbackCycle')]
                if next_id not in seen: seen.add(next_id); todo.extend(graph[next_id])
    for id in sorted(data.get('scene_templates', {})):
        scene = data['scene_templates'][id]
        error = _scene(scene, data, pointer('scene_templates', id), assets)
        if error: return [error]
    return []


def _reference_rule(schema, rule):
    """沿受信本地defs取真实字段约束，不因提取共用ID而丢失表现引用注解。"""
    seen = set()
    while '$ref' in rule:
        ref = rule['$ref']
        if (not isinstance(ref, str) or not ref.startswith('#/$defs/') or ref in seen
                or ref.removeprefix('#/$defs/') not in schema['$defs']):
            raise ValueError('Invalid presentation reference schema')
        seen.add(ref)
        rule = schema['$defs'][ref.removeprefix('#/$defs/')]
    return rule


def reference_specs():
    """从作者Schema注解导出表字段类别，不再维护第二份表现字段元组。"""
    schema = json.loads((ROOT / 'schemas/content.schema.json').read_text())
    for table in schema['x-hanmo-tables']:
        rule = schema['properties'][table]['items']
        rule = _reference_rule(schema, rule)
        for field, prop in rule.get('properties', {}).items():
            prop = _reference_rule(schema, prop)
            if 'x-hanmo-presentation' in prop: yield table, field, prop['x-hanmo-presentation'], prop.get('type')
            elif prop.get('type') == 'object':
                groups = {_reference_rule(schema, child).get('x-hanmo-presentation')
                          for child in prop.get('properties', {}).values()}
                if len(groups) == 1 and None not in groups: yield table, field, groups.pop(), 'object'


def validate_references(data: dict, pack: dict) -> list[dict]:
    """旧空纹理ID沿用默认值；新增显式引用必须非空且场景尺度相符。"""
    issues = []
    for table, field, group, shape in reference_specs():
        records = pack.get(table, [])
        if isinstance(records, dict): records = records.values()
        for record in records:
            if field not in record: continue
            key = record[field]; path = pointer(table, record.get('id', ''), field)
            if group == 'scene_templates' and shape == 'object':
                if not isinstance(key, dict): issues.append(issue(path, 'SceneScale')); continue
                for view, template_id in key.items():
                    location = path + pointer(view)
                    if view not in ('world', 'city', 'interior'): issues.append(issue(location, 'SceneScale')); continue
                    if not isinstance(template_id, str) or template_id not in data.get(group, {}): issues.append(issue(location, 'UnknownPresentation')); continue
                    if data[group][template_id]['view_kind'] != view: issues.append(issue(location, 'SceneScale'))
                continue
            if key == '' and group in ('backgrounds', 'portraits'): continue
            if not isinstance(key, str) or key not in data.get(group, {}): issues.append(issue(path, 'UnknownPresentation')); continue
            if group == 'scene_templates' and ((table == 'world_maps') != (data[group][key]['view_kind'] == 'world')): issues.append(issue(path, 'SceneScale'))
    return issues


def validate_project_pack(pack, root=ROOT):
    """作者校验和正式prepare共用严格资源门槛，源文件路径不决定项目身份。"""
    try: data = read_registry(Path(root) / 'game/data/presentation.json')
    except (OSError, ValueError, UnicodeError, RecursionError) as exc: return [issue('/presentation', str(exc))]
    return validate_registry(data, Path(root) / 'game') or validate_references(data, pack)


def project_inputs(root):
    """列出本代清单、Schema和全部资源快照，供暂存发布前重检。"""
    root = Path(root); manifest = root / 'game/data/presentation.json'; schema = root / 'game/data/presentation.schema.json'
    data = read_registry(manifest); errors = validate_registry(data, root / 'game')
    if errors: raise ValueError('Invalid presentation: ' + json.dumps(errors))
    return [manifest, schema] + sorted({_asset_path(root / 'game', e['path']) for g in ('backgrounds', 'portraits') for e in data[g].values()})


def check_project(root: Path) -> dict:
    """验证当前全部作者包与唯一表现清单，错误时阻止发布。"""
    data = read_registry(root / 'game/data/presentation.json'); issues = validate_registry(data, root / 'game')
    if not issues:
        for path in sorted((root / 'content/foundation').glob('*.json')):
            issues.extend(dict(i, source=str(path.relative_to(root))) for i in validate_references(data, json.loads(path.read_text())))
    if issues: raise RuntimeError('表现检查未通过：' + json.dumps(issues, ensure_ascii=False))
    return {'ok': True, 'backgrounds': len(data['backgrounds']), 'portraits': len(data['portraits'])}
