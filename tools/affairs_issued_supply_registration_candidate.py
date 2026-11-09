"""独立发行供给适配的原Affairs尾参候选；仅写隔离输出，不改变中央或固定生产源。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
BASE='d146ad5ea598d5dfb216f427479fe6e1f74e92aa'
ROOT='native/world/src/resident/domains/affairs/'
PATHS=[ROOT+x for x in ['affairs_model.hpp','affairs_rules.cpp','affairs_validation.cpp','affairs_supply_suspension.hpp','affairs_views.cpp']]+['native/world/src/resident/domains/tasks/task_dynamic_rules.cpp']
OWNED=[ROOT+'issued_supply/'+x for x in ['affairs_issued_supply.hpp','affairs_issued_supply.cpp','affairs_issued_supply_integration.cpp']]+['native/world/tests/resident/affairs_issued_supply_test.cpp','native/world/tests/resident/affairs_issued_supply_claim_test.cpp','native/world/tests/resident/dynamic_affairs_test.cpp','tools/affairs_issued_supply_registration_candidate.py',ROOT+'affairs_policy_validation.hpp',ROOT+'affairs_policy_validation.cpp','native/world/tests/resident/affairs_policy_validation_test.cpp','native/world/src/resident/domains/tasks/task_public_account_context.hpp','native/world/src/resident/domains/tasks/task_runtime_offer_view.hpp','native/world/src/resident/domains/tasks/task_runtime_offer_view.cpp']


def replace(src,before,after,count=1):
    """原锚点数量不符即停，不用整个旧头覆盖后继功能。"""
    if src.count(before)!=count:raise ValueError((before,src.count(before)))
    return src.replace(before,after)


def transform(path,src):
    """旧调用默认不增暂停，新列表仅能含真实具名发行政策。"""
    if path.endswith('affairs_model.hpp'):
        src=replace(src,'#include "resident/domains/tasks/task_model.hpp"','#include "resident/domains/tasks/task_model.hpp"\n#include <set>')
        src=replace(src,'const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr);','const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr,const std::set<StableId>* issued_suspended_policy_ids=nullptr);',2)
        src=replace(src,'struct EconomicSupplySuspension;','struct EconomicSupplySuspension;class EconomicSupplyPolicyRead;')
        src=replace(src,'const StableId& payee_wallet_id={});','const StableId& payee_wallet_id={},const EconomicSupplyPolicyRead* issued_supply=nullptr);')
        src=replace(src,'const NeedClaimQuote&,const OrganizationRead* organizations=nullptr);','const NeedClaimQuote&,const OrganizationRead* organizations=nullptr,const EconomicSupplyPolicyRead* issued_supply=nullptr);')
        return replace(src,'const StableId& need_id,Integer amount);','const StableId& need_id,Integer amount,const EconomicSupplyPolicyRead* issued_supply=nullptr);')
    if path.endswith('affairs_supply_suspension.hpp'):
        src=replace(src,'const std::vector<EconomicSupplySuspension>* sources){','const std::vector<EconomicSupplySuspension>* sources,const std::set<StableId>* issued_suspended_policy_ids=nullptr){')
        before='    std::set<StableId> result,seen;if(!sources)return {true,std::move(result),{}};'
        after='''    std::set<StableId> result,seen;
    // 新政策暂停只来自同候选S5具名读取；不能以旧来源暂停伪造新issuer或发行身份。
    if(issued_suspended_policy_ids)for(const auto& id:*issued_suspended_policy_ids){const auto* policy=catalog.policies.find(id);const auto* fact=common.state().facts.find(id);if(!policy||policy->id!=id||policy->source_key!=id||policy->semantic_key!=id||id.rfind("economic_supply_policy:",0)!=0||!fact||fact->id!=id||fact->kind!="economic_supply_policy_issued"||fact->domain!="economic_supply_policies")return {false,{},common_failure("InvalidAffairsIssuedSupplySuspension",id).error};result.insert(id);}
    if(!sources)return {true,std::move(result),{}};'''
        return replace(src,before,after)
    if path.endswith('affairs_views.cpp'):
        src=replace(src,'const StableId& need_id,Integer amount){','const StableId& need_id,Integer amount,const EconomicSupplyPolicyRead* issued_supply){')
        return replace(src,'offer.id,need->revision);','offer.id,need->revision,nullptr,nullptr,{},issued_supply);')
    if path.endswith('task_dynamic_rules.cpp'):
        src=replace(src,'const NeedClaimQuote& quote,const OrganizationRead* organizations){','const NeedClaimQuote& quote,const OrganizationRead* organizations,const EconomicSupplyPolicyRead* issued_supply){')
        src=replace(src,'quote.terms.payee_wallet_id);','quote.terms.payee_wallet_id,issued_supply);')
        src=replace(src,'offer->id,*command.need_revision);','offer->id,*command.need_revision,nullptr,nullptr,{},execution?execution->issued_supply:nullptr);')
        return replace(src,'claim_need(common,affairs,state,q);','claim_need(common,affairs,state,q,nullptr,execution?execution->issued_supply:nullptr);')
    if path.endswith('affairs_rules.cpp'):
        lines=[line for line in src.splitlines() if line.startswith('Status policy_valid(const NeedPolicy& p){')];assert len(lines)==1
        src=replace(src,'#include "affairs_model.hpp"','#include "affairs_model.hpp"\n#include "affairs_policy_validation.hpp"')
        src=replace(src,lines[0],'Status policy_valid(const NeedPolicy& p){return validate_need_policy(p);}')
        src=replace(src,'#include "affairs_policy_validation.hpp"','#include "affairs_policy_validation.hpp"\n#include "issued_supply/affairs_issued_supply.hpp"')
        src=replace(src,'const StableId& payee_wallet_id){','const StableId& payee_wallet_id,const EconomicSupplyPolicyRead* issued_supply){')
        anchor='if(affair_source_suspended(state,*n))return {false,{},common_failure("NeedSourceSuspended",need_id).error};'
        src=replace(src,anchor,anchor+'auto issued_valid=validate_affairs_issued_supply_claim(common,*n,*offer,issued_supply);if(!issued_valid.ok)return {false,{},issued_valid.error};')
    if path.endswith('affairs_validation.cpp'):
        src=replace(src,'suspended.value.count(policy->id)&&affair->phase!=AffairPhase::suspended','suspended.value.count(policy->id)&&!(issued_suspended_policy_ids&&issued_suspended_policy_ids->count(policy->id))&&affair->phase!=AffairPhase::suspended')
        src=replace(src,'if(offer.status!=(terminal?OfferStatus::closed:OfferStatus::withdrawn))return common_failure("SuspendedSupplyOfferPublished",row.first);','const bool issued=issued_suspended_policy_ids&&issued_suspended_policy_ids->count(policy->id);if(terminal?offer.status!=OfferStatus::closed:(issued?(offer.status!=OfferStatus::published&&offer.status!=OfferStatus::withdrawn):offer.status!=OfferStatus::withdrawn))return common_failure("SuspendedSupplyOfferPublished",row.first);')
    src=replace(src,'const std::vector<EconomicSupplySuspension>* supply_suspensions){','const std::vector<EconomicSupplySuspension>* supply_suspensions,const std::set<StableId>* issued_suspended_policy_ids){')
    return replace(src,'read_affairs_suspended_policies(common,catalog,supply_suspensions)','read_affairs_suspended_policies(common,catalog,supply_suspensions,issued_suspended_policy_ids)')


def main():
    """冻结每叶基准/结果OID和所有者白名单，实际公共合入由中央串行完成。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip();shared=[];owned=[]
    for name in PATHS:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+name]);after=transform(name,before.decode()).encode();dst=a.output/'candidate'/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(after);shared.append({'path':name,'base_oid':oid(before),'result_oid':oid(after)})
    for name in OWNED:
        data=(a.owned_repo/name).read_bytes();assert data==subprocess.check_output(['git','-C',str(a.owned_repo),'show',head+':'+name]);dst=a.output/'candidate'/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(data);owned.append({'path':name,'result_oid':oid(data)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'source_commit':head,'files':shared,'owned_files':owned,'build_cpp':[ROOT+'affairs_policy_validation.cpp',ROOT+'issued_supply/affairs_issued_supply.cpp',ROOT+'issued_supply/affairs_issued_supply_integration.cpp'],'build_headers':[ROOT+'affairs_policy_validation.hpp',ROOT+'issued_supply/affairs_issued_supply.hpp'],'requires':['81fe3ae original NeedPolicy validation helper','S5 economic_supply_policies/1 typed source reader at least13ed96d'],'status':'SOURCE_ONLY_WORLD_ADAPTER_PENDING','necessary_gates':{'projection_and_hot_source':'35/0 component including current opportunity filtering','affected_TUs':'6/0 initial, final cold4/0; opportunity integration syntax checked separately','original_claim_asset_gate':'DEFERRED: old23 archive pulls old prepare/claim ABI objects; link failures kept. Must run with final same-generation integration.'},'cold_contract':'Only issued-policy unclosed Affair open/active/suspended and nonterminal Offer published/withdrawn may lag permission changes. Source ledgers, exact terms/assets and historical observations remain strict. Cold is read-only. All publish/preview/actual claim and NPC calls use current same-candidate reader. Old SupplySuspension remains strict.'},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
