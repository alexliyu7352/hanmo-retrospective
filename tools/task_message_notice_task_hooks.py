"""在隔离候选登记原Task受领接缝；每个锚点必须唯一，不直接写中央工区。"""
import argparse
import hashlib
import json
from pathlib import Path


def replace(source, old, new):
    """歧义或漂移停在原件，不宽松替换另一个业务分支。"""
    if source.count(old) != 1:
        raise ValueError(f'Expected one anchor: {old[:100]}')
    return source.replace(old, new)


def model(source):
    """新optional只由独立段持久化；旧Task visitor不扩写。"""
    source = replace(source, '#include "task_offer_response.hpp"', '#include "task_offer_response.hpp"\n#include "message_notices/task_message_notice_state.hpp"')
    source = replace(source, 'struct TaskExecutionServices;', 'struct TaskMessageNoticeServices;struct TaskExecutionServices;')
    source = replace(source, 'std::optional<TaskMessageHandoffState> message_handoffs;', 'std::optional<TaskMessageHandoffState> message_handoffs;std::optional<TaskMessageNoticeState> message_notices;')
    for name in ('preview_task', 'finish_task_action'):
        rows = [line for line in source.splitlines() if name + '(' in line and line.endswith(');')]
        if len(rows) != 1:
            raise ValueError(name)
        source = replace(source, rows[0], rows[0][:-2] + ',const TaskMessageNoticeServices* notices=nullptr);')
    return source


def rules(source):
    """同一AddLot前取证明、后绑原信；收件人死亡是原委托失效，不锁死整个世界钟。"""
    source = replace(source, '#include "message_handoffs/task_message_handoff.hpp"', '#include "message_handoffs/task_message_handoff.hpp"\n#include "message_notices/task_message_notice.hpp"')
    for name in ('preview_task', 'finish_task_action'):
        rows = [line for line in source.splitlines() if line.startswith('Outcome<') and name + '(' in line]
        if len(rows) != 1 or not rows[0].endswith('){'):
            raise ValueError(name)
        guard='\n    if(s.message_notices.has_value()!=(notices!=nullptr)||(notices&&notices->state!=&*s.message_notices))return {false,{},common_failure("TaskMessageNoticeServicesMismatch").error};'
        source = replace(source, rows[0], rows[0][:-2] + ',const TaskMessageNoticeServices* notices){' + guard)
    anchor='        if(d->template_id=="message"){auto weight=c.inventory_weight(person->inventory_id);'
    source=replace(source,anchor,'        auto notice_ready=preview_task_message_notice_acceptance(c,s,catalog,common,notices,d->id);if(!notice_ready.ok)return {false,{},notice_ready.error};\n'+anchor)
    anchor='    std::optional<TaskStaticPaymentPreparation> static_payment;'
    code='''    std::optional<TaskMessageNoticePreparation> message_notice;
    if(s.message_notices&&action.kind=="accept_offer"){if(!ic.common)return {false,{},common_failure("MissingInteractionCatalog").error};auto prepared=prepare_task_message_notice_acceptance(c,s,catalog,*ic.common,notices,action,finish_tick);if(!prepared.ok){if(prepared.error.original.code!="DeathNoticeLivingPartiesRequired")return {false,{},prepared.error};auto released=c.apply_resources({{ReleaseActionResources{action.id}}, {}});if(!released.ok)return {false,{},released.error};t->status="cancelled";TaskEffect effect;effect.success=false;effect.task_id=t->id;effect.code=prepared.error.original.code;effect.on_unavailable=d->on_unavailable;effect.retry="original_person_available";return {true,std::move(effect),{}};}message_notice=std::move(prepared.value);}
'''
    source=replace(source,anchor,code+anchor)
    anchor='    if(message_handoff){auto recorded=record_task_message_handoff_after_resources(c,*s.message_handoffs,*message_handoff);if(!recorded.ok)return {false,{},recorded.error};}'
    source=replace(source,anchor,anchor+'\n    if(message_notice){auto recorded=record_task_message_notice_after_resources(c,*s.message_notices,*message_notice);if(!recorded.ok)return {false,{},recorded.error};}')
    return source


def main():
    """两叶候选附原字节OID；调用方自行确认共享三方合流基准。"""
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False);records=[]
    for name,transform in [('task_model.hpp',model),('task_rules.cpp',rules)]:
        path='native/world/src/resident/domains/tasks/'+name;before=(args.source/path).read_bytes();after=transform(before.decode()).encode();out=args.output/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(after)
        oid=lambda data:hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        records.append({'path':path,'base_oid':oid(before),'result_oid':oid(after)})
    (args.output/'manifest.json').write_text(json.dumps({'source':str(args.source.resolve()),'files':records},indent=2))

if __name__=='__main__':
    main()
