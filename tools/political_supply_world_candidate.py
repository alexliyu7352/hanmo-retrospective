#!/usr/bin/env python3
"""Prepare only the two shared political access leaves; central remains their writer."""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
BASE='9effd89a84873a7cec0e30bb3a5761011c62151e'
def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--base',default=BASE);p.add_argument('--source',type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 if a.base!=BASE:raise SystemExit('This candidate requires the fixed 9eff base')
 if a.output.exists():raise SystemExit('Fresh output required')
 a.output.mkdir(parents=True);records=[];patch=[]
 def load(path):return (a.source/path).read_text() if a.source and (a.source/path).exists() else subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo,text=True)
 def replace(text,old,new):
  if text.count(old)!=1:raise SystemExit('Anchor mismatch: '+old[:120])
  return text.replace(old,new)
 for suffix in ['hpp','cpp']:
  path='native/world/src/resident/kernel/political_takeover_integration.'+suffix;before=load(path);after=before
  if suffix=='hpp':
   after=replace(after,'#include "estate_employment_integration.hpp"','#include "estate_employment_integration.hpp"\n#include "resident/domains/local_economy/supply/supply_policy.hpp"\n#include "resident/domains/politics/supply/politics_supply_adoptions.hpp"')
   after=replace(after,'    PoliticalTakeoverContext context_;','    std::unique_ptr<EconomicSupplyPendingAccess> supply_pending_;\n    PoliticalTakeoverContext context_;')
  else:
   after=replace(after,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "economic_supply_policy_integration.hpp"')
   after=replace(after,'    return common_success();\n}\nPoliticalTakeoverOriginalContext original', '    const bool supply=w.catalog->economic_supply_policies_enabled;if(supply!=bool(w.catalog->economic_supply_policies)||(supply&&(w.political_supply_adoptions.version!=1||w.economic_supply_policies.version!=1)))return common_failure("MissingSupplyPolicyPair");\n    return common_success();\n}\nPoliticalTakeoverOriginalContext original')
   after=replace(after,'    context_{w.common,*w.catalog->common,w.organization,*w.catalog->organization,*w.catalog->politics,catalog,state,w.military,*w.catalog->military,military_access_,originals_,phase},','    supply_pending_(w.economic_supply_policies_enabled()?std::make_unique<EconomicSupplyPendingAccess>(EconomicSupplyPendingContext{w.common,*w.catalog->common,w.local_economy,*w.catalog->local_economy,*w.catalog->economic_supply_policies,w.economic_supply_policies,phase}):nullptr),\n    context_{w.common,*w.catalog->common,w.organization,*w.catalog->organization,*w.catalog->politics,catalog,state,w.military,*w.catalog->military,military_access_,originals_,phase,w.economic_supply_policies_enabled()?&w.political_supply_adoptions:nullptr,supply_pending_.get()},')
   after=replace(after,'commit_political_takeover_command_in_transaction(w.common,state,read.value->context(),actor,quote)','commit_political_takeover_command_in_transaction(w.common,state,read.value->context(),actor,quote,w.economic_supply_policies_enabled()?&w.political_supply_adoptions:nullptr)')
   after=replace(after,'    return committed;','    if(w.economic_supply_policies_enabled()){auto projection=project_economic_supply_world_in_candidate(w);if(!projection.ok)return {false,{},projection.error};}\n    return committed;')
  target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(after)
  oid=lambda value:subprocess.check_output(['git','hash-object','--stdin'],cwd=a.repo,input=value,text=True).strip()
  records.append({'path':path,'before_oid':oid(before),'after_oid':oid(after),'sha256':hashlib.sha256(after.encode()).hexdigest()});patch.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),'a/'+path,'b/'+path))
 (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'status':'SOURCE_ONLY_PENDING_COMPOSITE_SYNTAX','base':a.base,'shared':records,'dependencies':['S6 cca21eb owned adoption/authority source','S5 paired World state and EconomicSupplyPendingAccess','S5 economic_supply_policy_integration.hpp::project_economic_supply_world_in_candidate'],'required_caller_write_sets':['PoliticalTakeoverDecisionCommand saves political_supply_adoptions','both NPC paths save political_supply_adoptions and economic_supply_policies as appropriate','military pending callback projects newly paused economic issued policies'],'cmake':'central unique writer'},indent=2)+'\n')
if __name__=='__main__':main()
