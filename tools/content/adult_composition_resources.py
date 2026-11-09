"""核验冻结成人组合的实际选材闭包；只读当前包快照，不读取宿主素材或执行脚本。"""
from hashlib import sha256
import json
import math

# 包导入和直接运行tools/build.py沿同一解码器；不靠外部PYTHONPATH补齐入口。
try:
    from tools.presentation import _decode
except ModuleNotFoundError:
    from presentation import _decode
from .appearance_choices import _composition_domain
from .pack_errors import PackError

FEATURES = ('brows', 'eyes', 'nose', 'mouth')


def _require(valid, path):
    """结构或选材不合法时给出稳定拒绝，不能发布只有部分资源的新代。"""
    if not valid:
        raise PackError('InvalidAdultCompositionResource', '成人组合资源不满足冻结渲染合同', {'path': path})


def _resource(pack, path, kind):
    """每个实际被选资源必须显式必需、种类正确且真实字节匹配声明。"""
    _require(isinstance(path, str) and not path.startswith('/') and '\\' not in path
             and all(part not in ('', '.', '..') for part in path.split('/')), str(path))
    record = next((row for row in pack['manifest']['resources'] if row['path'] == path), None)
    raw = pack['files'].get(path)
    if record is None or record['kind'] != kind or record.get('required') is not True or raw is None:
        raise PackError('UndeclaredPackResource', '成人组合实际选材必须在当前包声明为必需资源', {'path': path})
    _require(sha256(raw).hexdigest() == record['sha256'], path)
    return raw


def _crop(value, width, height, square=False):
    """裁框必须由画布内正整数构成，头像另要求正方形。"""
    return (isinstance(value, list) and len(value) == 4 and all(type(x) is int for x in value)
            and min(value[:2]) >= 0 and min(value[2:]) > 0
            and value[0] + value[2] <= width and value[1] + value[3] <= height
            and (not square or value[2] == value[3]))


def _regions(value):
    """所有真实椭圆都限制在作者512坐标内，拒绝非有限数及越界采样。"""
    return (isinstance(value, list) and bool(value)
            and all(isinstance(row, list) and len(row) == 4
                    and all(type(x) in (int, float) and math.isfinite(x) for x in row)
                    and min(row[2:]) > 0 and row[0] - row[2] >= 0 and row[1] - row[3] >= 0
                    and row[0] + row[2] <= 512 and row[1] + row[3] <= 768 for row in value))


def validate_resources(entry, pack, parse, expected_catalog, accepted):
    """沿完整192元组反查族语义和骨相源；每张真正参与合成的PNG都解码一次。"""
    try:
        _validate(entry, pack, parse, expected_catalog, accepted)
    except (KeyError, TypeError, IndexError, ValueError, AttributeError) as error:
        if isinstance(error, PackError):
            raise
        raise PackError('InvalidAdultCompositionResource', '成人组合目录、语义或图像无法完整解析') from error


def _validate(entry, pack, parse, expected_catalog, accepted):
    """在改写namespace前消费原包逻辑路径，固定选件范围之外的资产不进入正式闭包。"""
    root = 'character_studio/'
    _require(entry['resource_root'] == 'res://' + root, 'resource_root')
    _require(entry['catalog'] == 'res://data/adult_appearance_compositions.json', 'catalog')
    _require(entry['fittings'] == 'res://' + root + 'fittings.json', 'fittings')
    _require(entry['representatives'] == 'res://' + root + 'representatives.json', 'representatives')
    catalog = parse(_resource(pack, entry['catalog'][6:], 'data'))
    # 正式渲染器也与引擎目录逐项核对；规范JSON保留bool/int等类型差异。
    _require(json.dumps(catalog, sort_keys=True, ensure_ascii=False)
             == json.dumps(expected_catalog, sort_keys=True, ensure_ascii=False), 'catalog')
    fittings = parse(_resource(pack, entry['fittings'][6:], 'data'))['fittings']
    samples = parse(_resource(pack, entry['representatives'][6:], 'data'))['samples']
    rows, families, faces = catalog['records'], catalog['families'], catalog['faces']
    _require(catalog['age_domain'] == [18, 59] and len(rows) == 192 and _composition_domain(rows), 'records')
    tuples = {(tuple(sorted(row['base_features'].items())), row['attire_style']) for row in rows}
    _require(len(tuples) == 192 and len({row['id'] for row in rows}) == 192, 'records')
    _require(len(families) == len(fittings) == 3 and len(faces) == len(samples) == 12, 'families')
    definitions, face_map = {}, {}
    for family in families:
        name = family['catalog']
        _require(isinstance(name, str) and '/' not in name and '\\' not in name
                 and '..' not in name and name.startswith('catalog_') and name.endswith('.json'), 'families/catalog')
        _require(sum(row['id'] == family['id'] and row['catalog'] == 'res://' + name for row in fittings) == 1, name)
        _require(family['id'] not in definitions, name)
        definition = parse(_resource(pack, root + name, 'data'))
        defaults, fields = definition.get('defaults'), definition.get('fields')
        _require(isinstance(defaults, dict) and isinstance(fields, dict)
                 and isinstance(definition.get('choice_semantics'), dict), name)
        _require(set(defaults) == {'face', *FEATURES, 'hair', 'beard', 'skin', 'hair_color', 'outfit', 'cloth_color'}, name)
        for field, index in defaults.items():
            _require(type(index) is int and isinstance(fields.get(field), dict)
                     and isinstance(fields[field].get('options'), list) and bool(fields[field]['options']), name + '/' + field)
        definitions[family['id']] = definition
    for face in faces:
        _require(face['id'] not in face_map and face['fitting_id'] in definitions
                 and type(face['face_index']) is int and 0 <= face['face_index'] <= 3, 'faces')
        _require(sum(row['id'] == face['id'] and row['fitting_id'] == face['fitting_id']
                     and row['selection']['face'] == face['face_index'] for row in samples) == 1, face['id'])
        face_map[face['id']] = face
        sample = next(row for row in samples if row['id'] == face['id'])
        original = next(row for row in accepted if row['id'] == face['id'])
        for field in FEATURES:
            index = sample['selection'][field]
            semantics = definitions[face['fitting_id']]['choice_semantics'][field]
            _require(type(index) is int and 0 <= index < len(semantics)
                     and semantics[index] == original['base_features'][field], face['id'] + '/' + field)
    decoded = set()
    for row in rows:
        face = face_map[row['base_features']['face_shape']]
        _require(row['fitting_id'] == face['fitting_id'], row['id'])
        definition = definitions[row['fitting_id']]
        selected = dict(definition['defaults'], face=face['face_index'])
        semantic = {field: row['base_features'][field] for field in FEATURES}
        semantic.update(hair='bun', beard='none', skin='warm', hair_color='black', outfit='plain', cloth_color='original')
        for field, token in semantic.items():
            choices = definition['choice_semantics'][field]
            _require(isinstance(choices, list) and choices.count(token) == 1, field)
            selected[field] = choices.index(token)
        _require(set(selected) == {'face', *semantic}, row['id'])
        for field, index in selected.items():
            _require(type(index) is int and 0 <= index < len(definition['fields'][field]['options']), field)
        rules = definition.get('compatibility', [])
        _require(isinstance(rules, list), 'compatibility')
        for rule in rules:
            _require(isinstance(rule, dict) and isinstance(rule.get('when'), dict)
                     and isinstance(rule.get('requires'), dict) and isinstance(rule.get('reason'), str), 'compatibility')
            _require(all(field in selected and type(index) is int
                         for condition in (rule['when'], rule['requires']) for field, index in condition.items()), 'compatibility')
            if all(selected[field] == index for field, index in rule['when'].items()):
                _require(all(selected[field] == index for field, index in rule['requires'].items()), 'compatibility')
        _require(isinstance(definition.get('face_profiles'), list), 'face_profiles')
        profile = definition['face_profiles'][selected['face']]
        _require(isinstance(profile, dict) and isinstance(profile.get('feature_sources'), dict)
                 and isinstance(profile.get('regions'), dict) and isinstance(definition.get('assets'), dict), 'face_profiles')
        ids = [f"head-{selected['face']}", f"hair-{selected['face']}", f"outfit-{selected['outfit']}"]
        for field in FEATURES:
            sources = profile['feature_sources'][field]
            _require(isinstance(sources, list) and len(sources) == len(definition['choice_semantics'][field])
                     and isinstance(sources[selected[field]], str) and bool(sources[selected[field]])
                     and _regions(profile['regions'][field]), field)
            ids.append(sources[selected[field]])
        crop = definition.get('portrait_crop', [0, 0, 1024, 1536])
        _require(_crop(crop, 1024, 1536) and crop[2] * 768 == crop[3] * 512, 'portrait_crop')
        _require(_crop(definition['avatar_crop'], 512, 768, square=True), 'avatar_crop')
        for asset in ids:
            path = definition['assets'][asset]
            _require(isinstance(path, str) and path.startswith('res://assets/runtime/') and path.endswith('.png'), str(asset))
            logical = root + path[6:]
            if logical not in decoded:
                raw = _resource(pack, logical, 'image')
                _require(len(raw) <= 33554432 and _decode(raw, 'png')[0] == [1024, 1536], logical)
                decoded.add(logical)
