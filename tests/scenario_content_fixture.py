"""通过正式准备入口生成双年代短门目录；不写玩家内容或绕过表现/Schema检查。"""
import argparse
import copy
import json
from pathlib import Path
from tools.content.io import prepare_pack

ROOT=Path(__file__).resolve().parents[1]


def prepare_scenario_fixture(folder: Path) -> Path:
    """使用已保存作者fixture生成独立目录；失败保留原源，不向正式generated写产物。"""
    folder=Path(folder)
    folder.mkdir(parents=True,exist_ok=True)
    output=folder/'two-eras.hmcatalog.json'
    prepare_pack(ROOT/'tests/fixtures/content_packs/second_scenario/two_eras.json',output)
    return output


def combat_source():
    """在真实双年代源加入现有Alpha武器定义与显式个人初货，不改基础默认货物。"""
    pack=json.loads((ROOT/'tests/fixtures/content_packs/second_scenario/two_eras.json').read_text())
    alpha=json.loads((ROOT/'content/foundation/alpha_life.json').read_text())
    weapon=copy.deepcopy(next(row for row in alpha['items'] if row['id']=='item.sword'))
    pack['items'].append(weapon);pack['texts'][weapon['name_key']]=alpha['texts'][weapon['name_key']]
    next(row for row in pack['actors'] if row['id']=='actor.player')['initial_goods']=[{'item_id':weapon['id'],'quantity':3}]
    pack['required_capabilities'].append('personal_combat')
    for scenario in pack['scenarios']:
        row=next(row for row in scenario['initial']['actors'] if row['actor_id']=='actor.player')
        row['combat']={'weapon_id':weapon['id'],'learned':[]}
        row['initial_goods']=[{'item_id':weapon['id'],'quantity':1},{'item_id':'item.grain','quantity':1}]
    return pack



def prepare_goods_fixtures(folder: Path) -> list[Path]:
    """两种显式初货顺序经同一正式prepare生成，整体内容身份仍各自保留。"""
    folder=Path(folder);folder.mkdir(parents=True,exist_ok=True)
    result=[]
    for label in ['a','b']:
        pack=combat_source()
        if label=='b':
            for scenario in pack['scenarios']:
                for actor in scenario['initial']['actors']:
                    actor.get('initial_goods',[]).reverse()
        source=folder/('goods-'+label+'.author.json')
        source.write_text(json.dumps(pack,ensure_ascii=False)+'\n')
        output=folder/('goods-'+label+'.hmcatalog.json')
        prepare_pack(source,output);result.append(output)
    return result


def main() -> int:
    """输出可交给Godot短门的实际文件路径，任何准备异常保持非零失败。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    parser.add_argument('--goods',action='store_true')
    args=parser.parse_args()
    result={'goods_content':[str(p) for p in prepare_goods_fixtures(args.folder)]} if args.goods else {'scenario_content':str(prepare_scenario_fixture(args.folder))}
    print(json.dumps(result,ensure_ascii=False))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
