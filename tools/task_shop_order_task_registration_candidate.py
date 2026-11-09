"""店单到原Task消费者的精确候选；只写新目录，供S5整体注册与中央三方合流。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
BASE = 'c277862'
PREFIX = 'native/world/src/resident/domains/'
SHARED = [PREFIX+'tasks/'+name for name in ['task_model.hpp','task_rules.cpp','task_journal.cpp','task_actor_pending_responsibilities.cpp','task_inheritance_history.cpp','task_public_account_dependencies.cpp','task_lifecycle_view.cpp','task_route_obligations.cpp']]+[PREFIX+'director/director_rules.cpp']


def once(source, old, new):
    """锚点漂移立即停止，不把旧整叶覆盖新后继。"""
    if source.count(old) != 1:
        raise ValueError(f'anchor count {source.count(old)}: {old[:100]}')
    return source.replace(old, new, 1)


def transform(path, source):
    """旧字节visitor不改；只有真实独立sidecar才可绕开普通Offer假设。"""
    if path.endswith('/task_model.hpp'):
        source=once(source,'#pragma once','#pragma once\n#include "shop_orders/task_shop_order_state.hpp"')
        return once(source,'std::optional<TaskStaticAssetState> static_assets;','std::optional<TaskStaticAssetState> static_assets;std::optional<TaskShopOrderState> shop_orders;')
    if path.endswith('/director_rules.cpp'):
        source='#include "resident/domains/tasks/shop_orders/task_shop_order_sources.hpp"\n'+source
        return once(source,'    // 先按真实已受领侧账分流；不得把authority/proposal ID伪装成普通Offer。','    // 店单只能走具名财货授权；首版没有NPC当面履约策略时明确等候，禁止假Offer或自动受领。\n    if(obligation&&tasks.shop_orders&&tasks.shop_orders->by_task.count(obligation->id)){auto valid=validate_task_shop_order_ledger(c,tasks,*tasks.shop_orders);if(!valid.ok)return {false,{},valid.error};next.template_id="maintain_commitment";next.goal_ref=obligation->id;next.commitment_ids={obligation->id};next.phase=PlanPhase::waiting;next.wait_reason="ShopOrderContinuationNotConfigured";return {true,{std::move(p)},{}};}\n    // 先按真实已受领侧账分流；不得把authority/proposal ID伪装成普通Offer。')
    source='#include "shop_orders/task_shop_order_sources.hpp"\n'+source
    if path.endswith('/task_lifecycle_view.cpp'):
        source=once(source,'    const auto& shared=*interactions.common;std::vector<StableId> ids;std::set<StableId> seen;', '    if(tasks.shop_orders){auto shop_source=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders);if(!shop_source.ok)return {false,{},shop_source.error};}\n    const auto& shared=*interactions.common;std::vector<StableId> ids;std::set<StableId> seen;')
        return once(source,'for(const auto& row:tasks.tasks.rows())if(row.second.assignee==observer&&seen.insert(row.second.offer_id).second)ids.push_back(row.second.offer_id);', '// 已验店单由本人专页办理，不占普通委托的有限扫描槽，也不伪造普通Offer定义。\n    for(const auto& row:tasks.tasks.rows())if(row.second.assignee==observer&&!(tasks.shop_orders&&tasks.shop_orders->by_task.count(row.first))&&seen.insert(row.second.offer_id).second)ids.push_back(row.second.offer_id);')
    if path.endswith('/task_route_obligations.cpp'):
        source=once(source,'    const auto identity=common.state().identity;Integer offset=0;', '    if(tasks.shop_orders){auto shop_source=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders);if(!shop_source.ok)return {false,{},shop_source.error};}\n    const auto identity=common.state().identity;Integer offset=0;')
        return once(source,'        if(governance&&governance->governance_contracts.find(id))', '        // 店单不借普通Offer推远方客户地址；原客户当面办理仍在本人店单页。\n        if(tasks.shop_orders&&tasks.shop_orders->by_task.count(id))continue;\n        if(governance&&governance->governance_contracts.find(id))')
    if path.endswith('/task_rules.cpp'):
        return once(source,'const PoliticalTakeoverRead* authority=nullptr){for(const auto& row:s.tasks.rows()){const auto& t=row.second;', 'const PoliticalTakeoverRead* authority=nullptr){if(s.shop_orders){auto valid=validate_task_shop_order_ledger(c,s,*s.shop_orders);if(!valid.ok)return valid;}for(const auto& row:s.tasks.rows()){const auto& t=row.second;if(s.shop_orders&&s.shop_orders->by_task.count(row.first)){if((governance&&governance->governance_contracts.find(row.first))||(envoy&&envoy->envoy_contracts.find(row.first)))return common_failure("DuplicateTaskResponsibility",row.first);continue;}')
    if path.endswith('/task_journal.cpp'):
        return once(source,'accepted.insert(task.offer_id);if(governance', 'accepted.insert(task.offer_id);if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id)){auto lines_for_order=task_shop_order_journal_lines(common,tasks,*tasks.shop_orders,catalog,id,task.id);if(!lines_for_order.ok)return {false,{},lines_for_order.error};for(const auto& line:lines_for_order.value)lines.append(line);if(task.status=="success")++finished;lines.append("");if(lines.full)return rejected("ResidentFrameLimit");continue;}if(governance')
    if path.endswith('/task_actor_pending_responsibilities.cpp'):
        return once(source,'    return {true,std::move(out),{}};', '    if(tasks.shop_orders){auto pending_orders=read_task_shop_order_actor_pending(c,tasks,*tasks.shop_orders,actor);if(!pending_orders.ok)return {false,{},pending_orders.error};for(const auto& source:pending_orders.value)pending(source);}\n    return {true,std::move(out),{}};')
    if path.endswith('/task_inheritance_history.cpp'):
        source=once(source,'if(!deceased||deceased->alive)return unknown(actor);','if(!deceased||deceased->alive)return unknown(actor);if(tasks.shop_orders){auto valid=validate_task_shop_order_actor_history(c,tasks,*tasks.shop_orders,actor,boundary,tick);if(!valid.ok)return valid;}')
        return once(source,'if(!*prior)continue;const auto& task=row.second;', 'if(!*prior)continue;const auto& task=row.second;if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id))continue;')
    if path.endswith('/task_fulfilled_clients.cpp'):
        source=once(source,'std::map<std::pair<int,StableId>,TaskFulfilledClientSource> clients;', 'std::map<std::pair<int,StableId>,TaskFulfilledClientSource> clients;if(x.tasks.shop_orders){auto shop_clients=read_task_shop_order_fulfilled_clients(x.common,x.tasks,*x.tasks.shop_orders,actor,before);if(!shop_clients.ok)return {false,{},shop_clients.error};for(auto& source:shop_clients.value.clients)clients.emplace(std::make_pair(static_cast<int>(source.original_client.kind),source.original_client.id),std::move(source));}')
        return once(source,'const auto& task=row.second;if(task.assignee!=actor||task.status!="success")continue;', 'const auto& task=row.second;if(x.tasks.shop_orders&&x.tasks.shop_orders->by_task.count(task.id))continue;if(task.assignee!=actor||task.status!="success")continue;')
    if path.endswith('/task_public_account_dependencies.cpp'):
        source=once(source,'TaskPublicAccountDependencies out;out.source_identity=common.state().identity;', 'if(tasks.shop_orders){auto valid=validate_task_shop_order_acceptances(common,tasks,*tasks.shop_orders);if(!valid.ok)return {false,{},valid.error};}TaskPublicAccountDependencies out;out.source_identity=common.state().identity;')
        source=once(source,'if(gov&&mission)return fail("DuplicateTaskResponsibility",task.id);', 'if(gov&&mission)return fail("DuplicateTaskResponsibility",task.id);if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id)){if(gov||mission)return fail("DuplicateTaskResponsibility",task.id);const auto* b=tasks.shop_orders->bindings.find(tasks.shop_orders->by_task.at(task.id));if(!b)return fail("TaskShopOrderIndexMismatch",task.id);if(chosen(selection.wallet_ids,b->terms.shop_wallet_id)||chosen(selection.wallet_ids,b->terms.client_wallet_id)||chosen(selection.inventory_ids,b->terms.stock_inventory_id)||chosen(selection.inventory_ids,b->terms.client_inventory_id))return fail("TaskShopOrderPrivateAssetsRequired",task.id);continue;}')
        source=once(source,'auto boundary=occupation_sequence(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};\n    if(governance', 'auto boundary=occupation_sequence(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};\n    if(tasks.shop_orders){auto valid=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders,phase);if(!valid.ok)return {false,{},valid.error};}\n    if(governance')
        return once(source,'if(gov&&mission)return {false,{},common_failure("DuplicateTaskResponsibility",task.id).error};','if(gov&&mission)return {false,{},common_failure("DuplicateTaskResponsibility",task.id).error};if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id)){if(gov||mission)return {false,{},common_failure("DuplicateTaskResponsibility",task.id).error};continue;}')
    raise ValueError(path)


def main():
    """固定Git前像逐OID保留，生成目录只供评审，绝不直接写中央工作树。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--base',default=BASE);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    base=subprocess.check_output(['git','-C',str(args.repo),'rev-parse',args.base],text=True).strip();rows=[]
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        old=subprocess.check_output(['git','-C',str(args.repo),'show',base+':'+path]);data=transform(path,old.decode()).encode();target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(data)})
    (args.output/'manifest.json').write_text(json.dumps({'base':base,'state':'SOURCE_ONLY_TASK_CONSUMER_FRAGMENT','shared':rows,'excluded':'S5 holds complete World/Codec/phase/author/navigation grouping; this is not registration closure','npc':'explicit wait for already accepted shop orders; no automatic new acceptance or delivery strategy'},indent=2)+'\n');print(args.output/'manifest.json')

if __name__=='__main__':main()
