"""只派生原首店1.2正常作者输入；旧包不改，不预制开工、接触、客户履约或死亡。"""
import argparse,json,subprocess
from pathlib import Path
from build_personal_appointment_author import emit
BASE='f612d843ea96f702b7460c8c0eab69663d4bff44'
def build(repo):
    """固定原首店terms和外部引用，仅新包身份、显式来源profile及生命历史能力。"""
    source='content/packs/authored/private_shops_1_2/'
    read=lambda name:json.loads(subprocess.check_output(['git','show',BASE+':'+source+name],cwd=repo))
    pack,content=read('pack.json'),read('content.json')
    pack.update(id='hanmo.task_shop_order_contacts',version='1.0.0',namespace='shop_contact')
    content.update(id='task_shop_order_contacts',version='1.0.0')
    for obj in (pack,content):
        for cap in ['task_shop_order_contacts_v1','family_life_v1']:
            if cap not in obj['required_capabilities']:obj['required_capabilities'].append(cap)
    for scene in content['scenarios']:
        scene['task_shop_order_contact_profile']='original_shop_order_contacts_v1'
        scene['family_life']={'profile':'verified_military_deaths_v1','player_death':'await_explicit_continuation'}
    target=repo/'content/packs/authored/task_shop_order_contacts_1_0';target.mkdir(parents=True,exist_ok=True)
    emit(target/'pack.json',pack);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/task-shop-order-contacts-1.0.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.8.0'},{'id':'hanmo.task_shop_order_contacts','version':'1.0.0'}]})
    (target/'README.md').write_text('''# 原店单真实交货接触 1.0

场景 shop_contact::scenario.first_shop。固定原首店 1.2 的三名客户、原采购履约、租铺、许可、订单与日租/实际结果合同；原 society 外部引用和所有原动态实体字符串不改。

本包显式选择 original_shop_order_contacts_v1 及真实生命历史。原本人接受客户店单后，实际原 provider SAY 开工才保存接触；原客户私款预约、店库实交、店金收入与失败回滚继续唯一原 writer。成功后的成本只读该次真实开工及原交付源。

没有预置商铺、三客户成功、Task、开工、死亡或完成记录。原三个 NPC subject 与多演员 API 保持，不为本包新增 NPC 交货生产者；本人实际开工需真实外层结果。

源码作者；尚未完整 World 或 GUI 验收。
''')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);build(parser.parse_args().repo)
