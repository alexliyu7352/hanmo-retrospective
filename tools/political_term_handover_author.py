#!/usr/bin/env python3
"""从固定正常后任作者新增独立生前交接选择，不预置亲属、玩家控制、任职或签受。"""
import argparse,copy,json,subprocess
from pathlib import Path

def main():
    """前两次真实控制与原首任就职沿已有命令，新让职只新增明确条款。"""
    p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--repo',type=Path,default=Path('.'));a=p.parse_args()
    def read(path):
        """只读固定Git输入，不从别的开发位复制移动文件。"""
        return json.loads(subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo))
    root='content/packs/authored/political_successor_rounds_1_0/'
    c,m=read(root+'content.json'),read(root+'pack.json');c['id']='political_term_handovers';scenario=c['scenarios'][0];scenario['id']='scenario.qingchuan_term_handovers';c['texts'][scenario['name_key']]='青川后任首职生前交接';c['required_capabilities'].append('political_term_handovers_v1')
    d=scenario['dynamic_world'];org=d['organization'];g=copy.deepcopy(org['grants']['grant.successor_round']);g.update(id='grant.term_handover',allowed_command_kinds=['political_offer_term_handover','political_withdraw_term_handover']);org['grants'][g['id']]=g;org['offices']['office.city_steward']['grant_ids'].append(g['id'])
    d['political_term_handovers']={'version':1,'policies':{'policy.term_handover':{'id':'policy.term_handover','successor_round_policy_id':'policy.successor_round','handover_grant_id':g['id'],'response_ticks':80}}}
    for kind in g['allowed_command_kinds']:
        key='term_handover.command.'+kind;c['texts'][key]=kind;d['politics']['command_name_keys'][kind]=key
    m.update(id='hanmo.political_term_handovers',namespace='termhandover',version='1.0.0',required_capabilities=c['required_capabilities'])
    out=a.repo/'content/packs/authored/political_term_handovers_1_0';out.mkdir(parents=True,exist_ok=True)
    for name,value in [('content.json',c),('pack.json',m)]: (out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    selection=read('content/packs/selections/political-successor-rounds-1.0.0.json');selection['packs'][-1]={'id':m['id'],'version':'1.0.0'};(a.repo/'content/packs/selections/political-term-handovers-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')

if __name__=='__main__':main()
