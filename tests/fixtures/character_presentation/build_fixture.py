"""生成独立原创几何小样，验证分层渲染；不冒充历史肖像或发行美术。"""
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / 'game/assets/art/character-fixtures'
OUT = Path(__file__).with_name('manifest.json')
SIZE = (96, 128)


def image_for(age, slot, option):
    """以独立儿童/成人绘画坐标生成透明图层，不缩小成人图片。"""
    image = Image.new('RGBA', SIZE)
    draw = ImageDraw.Draw(image)
    child = age == 'child'
    face = (29, 32, 67, 69) if child else (32, 10, 64, 49)
    if slot == 'body':
        body = (25, 63, 71, 126) if child else (20, 45, 76, 127)
        if option == 'broad': body = (body[0] - 6, body[1], body[2] + 6, body[3])
        draw.rounded_rectangle(body, 10, '#ba8d64')
    elif slot == 'face':
        if option == 'round': draw.ellipse(face, '#dfb17f')
        else: draw.polygon([(face[0]+5, face[1]), (face[2]-5, face[1]), (face[2], face[3]-10), (48, face[3]), (face[0], face[3]-10)], fill='#dfb17f')
        y = face[1] + (20 if child else 21)
        draw.rectangle((39, y, 42, y+2), fill='#332820')
        draw.rectangle((54, y, 57, y+2), fill='#332820')
        draw.line((44, y+12, 52, y+12), '#7e403a', 2)
    elif slot == 'hair':
        top = 27 if child else 4
        draw.pieslice((27 if child else 30, top, 69 if child else 66, top+31), 180, 360, fill='#352b2b')
        if option == 'bun': draw.ellipse((39, top-3, 57, top+10), '#352b2b')
    elif slot == 'garment':
        top = 75 if child else 56
        color = '#466e91' if option == 'plain' else '#a94746'
        draw.polygon([(34, top), (62, top), (80, 125), (16, 125)], fill=color)
        draw.line([(35, top), (53, top+25), (62, top)], fill='#e5c792', width=3)
        draw.rectangle((25, 99, 71, 105), '#c4a268')
    return image


def save(name, image):
    """写PNG并计算真实alpha边界，清单几何由像素导出。"""
    path = ART / f'{name}.png'
    image.save(path)
    box = image.getbbox()
    return {'path': 'res://assets/art/character-fixtures/' + path.name}, [box[0], box[1], box[2]-box[0], box[3]-box[1]]


def main():
    """产出唯一候选清单、两年龄独立部件与双专绘；正式清单保持不变。"""
    ART.mkdir(parents=True, exist_ok=True)
    backgrounds = {'fixture.background': {'path': 'res://assets/art/presentation-fixtures/scene.png'}}
    legacy = json.loads((ROOT / 'tests/fixtures/presentation/layered-v2.json').read_text())
    backgrounds = legacy['backgrounds']
    portraits, bounds = {}, {}
    for age in ('child', 'adult'):
        for slot, options in (('body', ('slender', 'broad')), ('face', ('round', 'angular')), ('hair', ('short', 'bun')), ('garment', ('plain', 'formal'))):
            for option in options:
                name = f'{age}.{slot}.{option}'
                portraits[name], bounds[name] = save(name, image_for(age, slot, option))
        for who, color in (('a', '#dbc16a'), ('b', '#98b07a')):
            image = Image.new('RGBA', SIZE)
            for slot, option in [('body', 'slender'), ('garment', 'formal'), ('face', 'round'), ('hair', 'bun')]:
                image.alpha_composite(image_for(age, slot, option))
            ImageDraw.Draw(image).rectangle((19, 108, 76, 114), fill=color)
            name = f'{age}.dedicated.{who}'
            portraits[name], bounds[name] = save(name, image)
    data = {'version': 2, 'default_background': legacy['default_background'], 'default_portrait': 'adult.dedicated.a', 'backgrounds': backgrounds, 'portraits': portraits, 'components': {}, 'character_profiles': {}, 'scene_templates': {}}
    for view, pose in [('half_body', 'idle'), ('battle', 'combat')]:
        suffix = '.battle' if view == 'battle' else ''
        variants = []
        for age, ages in [('child', [0, 15]), ('adult', [16, 100])]:
            for asset, boundary in bounds.items():
                if not asset.startswith(age+'.') or '.dedicated.' in asset: continue
                slot = asset.split('.')[1]
                identifier = asset + suffix
                geometry = {'source_size': list(SIZE), 'logical_canvas': list(SIZE), 'alpha_bounds': boundary, 'safe_crop': [20, 20 if age == 'child' else 0, 56, 57], 'support_kind': 'foot' if view == 'battle' else 'half_body_bottom', 'support_point': [50, 100], 'pose': pose, 'facing': 'front', 'view': view, 'mask_ids': [], 'palette_slots': []}
                data['components'][identifier] = {'id': identifier, 'slot': slot, 'asset_id': 'portraits/'+asset, 'body_basis': age+'.front', 'proportion': age, 'anchor': [50, 100], 'z_index': {'body': 0, 'garment': 1, 'face': 2, 'hair': 3}[slot], 'tags': [age], 'compatible_tags': [age], 'incompatible_tags': [], 'after': [], 'geometry': geometry}
            components = [f'{age}.{slot}.{option}{suffix}' for slot, option in [('body', 'slender'), ('face', 'round'), ('hair', 'short')]]
            bindings = [{'id': slot+'.'+option, 'features': {feature: option}, 'component_id': f'{age}.{slot}.{option}{suffix}'} for slot, feature, options in [('body', 'build', ['slender', 'broad']), ('face', 'face_shape', ['round', 'angular']), ('hair', 'hair', ['short', 'bun'])] for option in options]
            variants.append({'id': age, 'ages': ages, 'body_basis': age+'.front', 'proportion': age, 'components': components, 'feature_bindings': bindings})
        profile_id = 'fixture.battle' if view == 'battle' else 'fixture.person'
        data['character_profiles'][profile_id] = {'id': profile_id, 'identity_group': 'fixture.identity', 'mode': 'composite', 'view': view, 'pose': pose, 'context': 'battle' if view == 'battle' else 'daily', 'age_domain': [0, 100], 'child_max_age': 15, 'age_variants': variants, 'attire_sets': [{'id': option, 'bindings': [{'body_basis': age+'.front', 'components': [f'{age}.garment.{option}{suffix}']} for age in ('child', 'adult')]} for option in ('plain', 'formal')], 'overrides': [], 'fallbacks': [], 'battle_profile': {'style': 'none'} if view == 'battle' else {'style': 'simple', 'profile_id': 'fixture.battle'}}
    for who in ('a', 'b'):
        profile = json.loads(json.dumps(data['character_profiles']['fixture.person']))
        profile['id'] = 'fixture.dedicated.'+who
        profile['mode'] = 'handcrafted'; profile['attire_sets'] = []
        for variant in profile['age_variants']:
            variant['components'] = []; variant.pop('feature_bindings')
            age = variant['id']; asset = age+'.dedicated.'+who
            geometry = json.loads(json.dumps(data['components'][age+'.body.slender']['geometry']))
            geometry['alpha_bounds'] = bounds[asset]
            profile['overrides'].append({'id': age+'.portrait', 'ages': variant['ages'], 'view': 'half_body', 'pose': 'idle', 'context': 'daily', 'priority': 1, 'asset_id': 'portraits/'+asset, 'body_basis': age+'.front', 'proportion': age, 'geometry': geometry})
        data['character_profiles'][profile['id']] = profile
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    main()
