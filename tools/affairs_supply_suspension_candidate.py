"""具名四叶供给暂停候选；保旧Affairs字节，只使用既有phase/status及当前S5只读投影。"""
from pathlib import Path
import argparse
import difflib
import json
import subprocess
from task_public_account_rules_candidate import BASE, PREFIX, replace, signature, oid, transform as task_transform
SHARED=[PREFIX+'domains/affairs/'+name for name in ('affairs_model.hpp','affairs_rules.cpp','affairs_validation.cpp','affairs_views.cpp')]

def transform(path,text):
    if path.endswith('affairs_model.hpp'):
        text=replace(text,'struct CohortWageNeedCatalog;','struct EconomicSupplySuspension;\nstruct CohortWageNeedCatalog;')
        for name in ('reconcile_affairs','validate_affairs'):
            text=signature(text,name,'const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr',True)
    elif path.endswith('affairs_rules.cpp'):
        text=replace(text,'#include "affairs_model.hpp"','#include "affairs_model.hpp"\n#include "affairs_supply_suspension.hpp"')
        text=signature(text,'reconcile_affairs','const std::vector<EconomicSupplySuspension>* supply_suspensions')
        text=replace(text,'if(tick!=common.tick())return common_failure("AffairsTickMismatch");','if(tick!=common.tick())return common_failure("AffairsTickMismatch");auto suspended=read_affairs_suspended_policies(common,catalog,supply_suspensions);if(!suspended.ok)return {false,{},suspended.error};')
        text=replace(text,'if(!policy)continue;auto valid=policy_valid(*policy);','if(!policy)continue;if(suspended.value.count(policy->id))return common_failure("SuspendedSupplyObservation",observation.source_key);auto valid=policy_valid(*policy);')
        text=replace(text,'if(n->revision!=revision)return','if(affair_source_suspended(state,*n))return {false,{},common_failure("NeedSourceSuspended",need_id).error};if(n->revision!=revision)return')
        text=replace(text,'bool closed=true,claimed=false;','bool closed=true,claimed=false,source_suspended=false;')
        text=replace(text,'claimed=claimed||!need->claim_ids.empty();','claimed=claimed||!need->claim_ids.empty();source_suspended=source_suspended||suspended.value.count(need->policy_id);')
        text=replace(text,'else affair.phase=claimed?AffairPhase::active:AffairPhase::open;','else affair.phase=source_suspended?AffairPhase::suspended:(claimed?AffairPhase::active:AffairPhase::open);')
        text=replace(text,'if(current){current->status=need_gap(need)==0?','if(affair_source_suspended(state,need)){if(current)current->status=OfferStatus::withdrawn;continue;}if(current){current->status=need_gap(need)==0?')
    elif path.endswith('affairs_validation.cpp'):
        text=replace(text,'#include "affairs_model.hpp"','#include "affairs_model.hpp"\n#include "affairs_supply_suspension.hpp"')
        text=signature(text,'validate_affairs','const std::vector<EconomicSupplySuspension>* supply_suspensions')
        text=replace(text,'auto initial=make_initial_affairs(catalog);','auto suspended=read_affairs_suspended_policies(common,catalog,supply_suspensions);if(!suspended.ok)return {false,{},suspended.error};auto initial=make_initial_affairs(catalog);')
        text=replace(text,'||inventory->owner.kind!=Owner::Kind::organization||inventory->owner.id!=policy->organization_id||','||inventory->owner.kind!=Owner::Kind::organization||(!suspended.value.count(policy->id)&&inventory->owner.id!=policy->organization_id)||')
        text=replace(text,'if(need.deadline_tick&&*need.deadline_tick<need.created_tick)','if(supply_suspensions&&!affair->closed_tick&&suspended.value.count(policy->id)&&affair->phase!=AffairPhase::suspended)return common_failure("AffairsSupplySuspensionMismatch",need.id);if(need.deadline_tick&&*need.deadline_tick<need.created_tick)')
        text=replace(text,'return common_failure("RuntimeOfferPolicyMismatch",row.first);}','return common_failure("RuntimeOfferPolicyMismatch",row.first);if(suspended.value.count(policy->id)){const bool terminal=need->status==NeedStatus::fulfilled||need->status==NeedStatus::expired||need->status==NeedStatus::cancelled;if(offer.status!=(terminal?OfferStatus::closed:OfferStatus::withdrawn))return common_failure("SuspendedSupplyOfferPublished",row.first);}}')
    elif path.endswith('affairs_views.cpp'):
        text=replace(text,'#include "affairs_model.hpp"','#include "affairs_model.hpp"\n#include "affairs_supply_suspension.hpp"')
        text=replace(text,'if(!need||known.place_id','if(!need||affair_source_suspended(affairs,*need)||known.place_id')
    else:raise ValueError(path)
    return text

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--base',default=BASE);parser.add_argument('--source',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);rows=[];patch=[]
    for path in SHARED:
        before=(args.source/path).read_text() if args.source else subprocess.check_output(['git','show',args.base+':'+path],cwd=root).decode();prepared=before
        if not args.source and path.endswith(('affairs_model.hpp','affairs_rules.cpp')):prepared=task_transform(path,prepared)
        after=transform(path,prepared)
        for folder,data in [('base',before),('candidate',after)]:
            target=out/folder/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(data)
        rows.append({'path':path,'base_git_oid':oid(before.encode()),'candidate_git_oid':oid(after.encode())});patch.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),'a/'+path,'b/'+path))
    (out/'registration.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps({'base':str(args.source or args.base),'status':'SOURCE_ONLY; requires same-world S5 supply read at bootstrap/finish and cold validation','shared':rows},indent=2)+'\n');print(out/'manifest.json')
if __name__=='__main__':main()
