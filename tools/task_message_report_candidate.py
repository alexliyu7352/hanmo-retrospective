"""在已登记v1实交接点上追加显式v2当面返报；只写隔离副本，不改原共享规则。"""
from pathlib import Path
import argparse,json,hashlib
from task_static_public_candidate import replace,blob


def transform_rules(source):
    """先证当面报告，再等原钱货、关系与settlement全成功；末端失败由原World事务撤回。"""
    source=replace(source,'#include "message_handoffs/task_message_handoff.hpp"','#include "message_handoffs/task_message_handoff.hpp"\n#include "message_handoffs/task_message_report.hpp"')
    a='ResourceRecipe recipe;auto owner=task_owner(t->id);auto bag=c.actor(action.actor_id)->inventory_id;'
    source=replace(source,a,'''std::optional<TaskMessageReportPreparation> message_report;
    if(s.message_handoffs&&s.message_handoffs->version==2&&action.kind=="report_offer"&&d->template_id=="message"){if(!ic.common)return {false,{},common_failure("MissingInteractionCatalog").error};const DynamicStepInput message_phase{c.tick(),finish_tick};auto prepared=prepare_task_message_report(c,s,catalog,*ic.common,*s.message_handoffs,action,finish_tick,&message_phase);if(!prepared.ok)return {false,{},prepared.error};message_report=std::move(prepared.value);}
    '''+a)
    a='TaskEffect effect;effect.task_id=t->id;return {true,std::move(effect),{}};'
    return replace(source,a,'if(message_report){auto recorded=record_task_message_report_after_settlement(c,s,*s.message_handoffs,*message_report);if(!recorded.ok)return {false,{},recorded.error};}'+a)


def main():
    """v1来源必须具备原实交接点，未知锚或复用输出直接拒绝。"""
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('output must be fresh')
    p='native/world/src/resident/domains/tasks/task_rules.cpp';raw=(args.source/p).read_bytes();result=transform_rules(raw.decode()).encode();target=args.output/'candidate'/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(result)
    (args.output/'manifest.json').write_text(json.dumps({'status':'WIP_V2_NOT_REGISTERED','path':p,'source_oid':blob(raw),'candidate_oid':blob(result),'sha256':hashlib.sha256(result).hexdigest(),'pending':['explicit v2 author/marker/section selection','v2 Common fact and Main query registration','same-generation World transaction']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
