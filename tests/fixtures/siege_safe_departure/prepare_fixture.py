"""以既有正式军事作者准备器生成有真实离城道路和接纳营的短门；两张tiny图只满足内容资源合同。"""
import argparse
from copy import deepcopy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile


def write_json(path, value):
    """只写全新隔离工程，保留作者原件方便定位正式prepare失败。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def prepare(repository, source, commit, output):
    """精确物化源码，随后在其ROOT执行既有作者生成与真实prepare，不手写运行catalog。"""
    if output.exists() or not re.fullmatch(r'[0-9a-f]{7,40}', commit):
        raise ValueError('Require a fresh output and precise base commit')
    commit = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', commit + '^{commit}'], text=True).strip()
    output.mkdir(parents=True)
    members = ['tools', 'schemas', 'game/core', 'game/storage', 'game/data', 'game/project.godot',
               'content/foundation/alpha_life.json', 'tests/military_content_fixture.py',
               'game/tests/campaign_fixture.gd', 'game/tests/military_fixture.gd']
    raw = subprocess.check_output(['git', '-C', str(repository), 'archive', commit, *members])
    with tarfile.open(fileobj=BytesIO(raw), mode='r:') as archive:
        for member in archive:
            name = Path(member.name)
            if name.is_absolute() or '..' in name.parts or not (member.isdir() or member.isfile()):
                raise ValueError('Unexpected Git member')
            target = output / name
            if member.isdir(): target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(member).read())
    for relative in ['game/core/siege_rules.gd', 'game/core/siege_world.gd',
                     'game/core/domains/siege/safe_departure.gd', 'game/core/domains/siege/departure_allocation.gd', 'game/core/domains/siege/engine_use.gd',
                     'game/core/domains/siege/fortification_context.gd', 'game/core/domains/siege/integration_contract.gd',
                     'game/tests/test_siege_safe_departure.gd', 'game/tests/test_siege_strategy_contract.gd',
                     'game/tests/test_siege_departure_allocation.gd', 'game/tests/test_siege_integration_contract.gd',
                     'game/tests/test_siege_node_eligibility.gd', 'game/tests/test_siege_engine_use.gd',
                     'game/tests/test_siege_fortification_context.gd']:
        target = output / relative; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)
    (output / 'test-inputs').mkdir(exist_ok=True)
    shutil.copy2(source / 'tests/fixtures/siege_safe_departure/allocation.author.json', output / 'test-inputs/allocation.author.json')
    prefix = 'tests/fixtures/content_packs/compose/new_packs/base/'
    for relative in ['presentation.json', 'assets/art/pixel.png', 'assets/art/portrait.png']:
        target = output / ('game/data/presentation.json' if relative == 'presentation.json' else 'game/' + relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(['git', '-C', str(repository), 'show', commit + ':' + prefix + relative]))
    settings = output / 'game/project.godot'; text = settings.read_text()
    text = re.sub(r'^enabled=PackedStringArray\(.*\)$', 'enabled=PackedStringArray()', text, flags=re.M)
    text = re.sub(r'^run/main_scene=.*\n', '', text, flags=re.M)
    text = re.sub(r'^config/custom_user_dir_name=.*$', 'config/custom_user_dir_name="hanmo-siege-departure-tests"', text, flags=re.M)
    settings.write_text(text.replace('[application]', '[application]\nconfig/siege_departure_isolated=true'))
    # 子进程将模块ROOT绑定隔离工程，不能借主仓美术或已运行工坊输入偷偷补齐。
    generator = output / 'prepare_author.py'
    generator.write_text('''"""按现军事作者结构生成独有安全离城样本，只有定义变化，不造Kernel运行态。"""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
from tools.content.io import prepare_pack
from tools import build
root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('military_fixture',root/'tests/military_content_fixture.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
pack=module.military_pack()
for actor in pack['actors']:
    for field in ['military_plan','travel_plan','appearance_profile']: actor.pop(field,None)
    actor['portrait_kind']='merchant'
for table in ['places','battlefields','personal_battlefields']:
    for row in pack.get(table,[]): row['visual_tag']='city'
for row in pack.get('troop_types',[]): row['battle_portrait_kind']='merchant'
for row in pack['items']:
    if 'battle_portrait_kind' in row: row['battle_portrait_kind']='merchant'
pack['world_maps']=[]
for row in pack['scenarios']:
    row.pop('world_map_id',None); row['player_actor']='actor.military.a.commander'
for row in pack['routes']:
    row['cost_per_tick']=0; row['duration_ticks']=1
# 双军合法集结在西城；没有地形接触层的旧道路样本只隔离本次离城条款。
for actor in pack['actors']:
    if actor['id'].startswith('actor.military.b.'): actor['initial_place']='west.street'
for row in pack['roles']:
    if row['id']=='role.military.b': row['place_id']='west.street'
for row in pack['garrisons']:
    if row['id']=='garrison.military.b': row['place_id']='west.street'
for row in pack['strongholds']:
    if row['id']=='stronghold.military.b': row['settlement_id']='city.west'
for row in pack['military_orders']:
    row['target_stronghold_id']='stronghold.military.b'
    row['objective']={'kind':'occupy' if row['id']=='order.military.a' else 'relieve'}
for row in pack['inventories']:
    if row['id']=='inventory.military.b': row['goods']=[good for good in row['goods'] if good['item_id']!='item.military.b.food']
for row in pack['siege_topologies']:
    row['rules'].update(starvation_ticks=2,surrender_morale=40)
    if not any(edge['from']=='approach' and edge['to']=='supply' for edge in row['edges']): row['edges'].append({'from':'approach','to':'supply'})
# 东城为守方既有独立接纳营，位置、军粮、容量和守备均为正式作者定义。
spare=deepcopy(next(row for row in pack['garrisons'] if row['id']=='garrison.military.b'))
spare.update(id='garrison.refuge',place_id='east.entrance',inventory_id='inventory.refuge',wallet_id='wallet.refuge')
spare['units'][0].update(id='unit.refuge',commander_actor_id='actor.refuge',initial_soldiers=1)
actor=deepcopy(next(row for row in pack['actors'] if row['id']=='actor.military.b.commander'))
actor.update(id='actor.refuge',initial_place='east.entrance'); pack['actors'].append(actor)
pack['garrisons'].append(spare)
pack['inventories'].append({'id':'inventory.refuge','owner':{'kind':'organization','id':'org.military.b'},'capacity':10000,'goods':[{'item_id':'item.military.b.food','quantity':100}]})
pack['wallets'].append({'id':'wallet.refuge','owner':{'kind':'organization','id':'org.military.b'},'initial_balance':0})
# 两支真实守军同时离城，分别单独能入小营，合计容量必须整体判断。
for suffix in ['commander','provider']:
    person=deepcopy(next(row for row in pack['actors'] if row['id']=='actor.military.b.'+suffix))
    person['id']='actor.military.b2.'+suffix; pack['actors'].append(person)
role=deepcopy(next(row for row in pack['roles'] if row['id']=='role.military.b'))
role.update(id='role.military.b2',initial_actor='actor.military.b2.provider'); pack['roles'].append(role)
office=deepcopy(next(row for row in pack['offices'] if row['id']=='office.military.b'))
office.update(id='office.military.b2',appointing_role_id='role.military.b2'); pack['offices'].append(office)
order=deepcopy(next(row for row in pack['military_orders'] if row['id']=='order.military.b'))
order.update(id='order.military.b2',office_id='office.military.b2',issuer_role_id='role.military.b2',report_role_id='role.military.b2'); pack['military_orders'].append(order)
camp=next(row for row in pack['garrisons'] if row['id']=='garrison.military.b')
unit=deepcopy(camp['units'][0]); unit.update(id='unit.military.b2',commander_actor_id='actor.military.b2.commander',initial_soldiers=4); camp['units'].append(unit)

source=root/'test-inputs/safe-departure.author.json'; source.parent.mkdir(exist_ok=True)
source.write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\\n')
output=root/'build/safe-departure.hmcatalog.json'
prepare_pack(source,output)
(root/'content/foundation/alpha_life.json').write_bytes(source.read_bytes())
build.stage_content(root,foundation_sets=False)
for mode in ['capacity','no_refuge','blocked']:
    variant=deepcopy(pack)
    if mode=='capacity':
        bag=next(row for row in variant['inventories'] if row['id']=='inventory.refuge')
        bag['goods']=[]; bag['capacity']=40
    elif mode=='no_refuge':
        variant['garrisons']=[row for row in variant['garrisons'] if row['id']!='garrison.refuge']
    else:
        for row in variant['routes']: row.update(blocked_from_tick=0,blocked_until_tick=100000)
    path=root/('test-inputs/safe-departure-'+mode+'.author.json')
    path.write_text(json.dumps(variant,ensure_ascii=False,indent=2)+'\\n')
    prepare_pack(path,root/('build/safe-departure-'+mode+'.hmcatalog.json'))
prepare_pack(root/'test-inputs/allocation.author.json',root/'build/safe-departure-allocation.hmcatalog.json')
print(json.dumps({'content_path':str(output),'source':str(source)}))
''')
    subprocess.run([sys.executable, str(generator)], cwd=output, check=True)
    source_hashes = {str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in (output / 'game/core').rglob('*.gd')}
    write_json(output / 'prepare-receipt.json', {'base_commit': commit, 'content_path': str(output / 'build/safe-departure.hmcatalog.json'),
               'core_sha256': source_hashes, 'asset_count': 2, 'scope': '规则节点tiny输入，无视觉验收'})


def main():
    """调用者明确固定基底和新输出，不覆盖任何既有玩家存档或工坊目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--test-source', type=Path, required=True)
    parser.add_argument('--base-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repository.resolve(), args.test_source.resolve(), args.base_commit, args.output.absolute())


if __name__ == '__main__':
    main()
