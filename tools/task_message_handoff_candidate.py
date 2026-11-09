"""为原TaskState/送信完工生成具名交接接点；独立段未完整登记前不可单独安装。"""
from pathlib import Path
import argparse,json,hashlib
from task_static_public_candidate import replace,blob


def transform_model(source):
    """只追加新optional，旧Task codec不读写它；World复制仍由原TaskState值语义统一保护。"""
    source=replace(source,'#include "task_offer_response.hpp"','#include "task_offer_response.hpp"\n#include "message_handoffs/task_message_handoff_state.hpp"')
    return replace(source,'std::optional<TaskPublicAccountExecutionState> takeover_execution;', 'std::optional<TaskPublicAccountExecutionState> takeover_execution;std::optional<TaskMessageHandoffState> message_handoffs;')


def transform_rules(source):
    """货物仍仅原TransferLots执行一次；新增失败由原整个World事务撤回原货物与收据。"""
    source=replace(source,'#include "task_public_account_context.hpp"','#include "task_public_account_context.hpp"\n#include "message_handoffs/task_message_handoff.hpp"')
    anchor='ResourceRecipe recipe;auto owner=task_owner(t->id);auto bag=c.actor(action.actor_id)->inventory_id;'
    source=replace(source,anchor,'''std::optional<TaskMessageHandoffPreparation> message_handoff;
    if(s.message_handoffs&&action.kind=="deliver_message"){if(!ic.common)return {false,{},common_failure("MissingInteractionCatalog").error};const DynamicStepInput message_phase{c.tick(),finish_tick};auto prepared=prepare_task_message_handoff(c,s,catalog,*ic.common,*s.message_handoffs,action,finish_tick,&message_phase);if(!prepared.ok)return {false,{},prepared.error};message_handoff=std::move(prepared.value);}
    '''+anchor)
    anchor='recipe.operations.push_back(ReleaseActionResources{action.id});auto changed=c.apply_resources(recipe);if(!changed.ok)return {false,{},changed.error};'
    return replace(source,anchor,anchor+'\n    if(message_handoff){auto recorded=record_task_message_handoff_after_resources(c,*s.message_handoffs,*message_handoff);if(!recorded.ok)return {false,{},recorded.error};}')


def main():
    """基于调用方已冻结副本生成新两叶，绝不默认覆写中央或已交Task2快照。"""
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('output must be fresh')
    results=[]
    for leaf,transform in [('task_model.hpp',transform_model),('task_rules.cpp',transform_rules)]:
        path='native/world/src/resident/domains/tasks/'+leaf;before=(args.source/path).read_bytes();after=transform(before.decode()).encode();target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(after);results.append({'path':path,'source_oid':blob(before),'candidate_oid':blob(after),'sha256':hashlib.sha256(after).hexdigest()})
    (args.output/'manifest.json').write_text(json.dumps({'status':'WIP_NOT_FULLY_REGISTERED','shared':results,'pending':['explicit author profile and capability','independent task_message_handoffs/1 exact module/section','World JSON/binary/factory/validation/Common fact registration','Main query and page']},ensure_ascii=False,indent=2))

if __name__=='__main__':main()
