#!/usr/bin/env python3
"""仅把真实 Shop1 租/工资接入原 LE 冷门；显式固定基准，不混未注册日租或改旧数据。"""
import argparse, hashlib, json, subprocess
from pathlib import Path
BASE='e69a9ef915f62255f3ef1feed7dbffe2e9f93738'
ROOT='native/world/src/resident/'
PATHS=[ROOT+'domains/local_economy/local_economy_model.hpp',ROOT+'domains/local_economy/local_economy_validation.cpp',ROOT+'kernel/world_bundle.cpp','native/world/resident.cmake']
def once(text,old,new):
    if text.count(old)!=1: raise ValueError('required unique anchor: '+old[:100])
    return text.replace(old,new)
def transform(path,s):
    if path.endswith('local_economy_model.hpp'):
        old='Status validate_local_economy(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const PopulationMilitaryReadContext*,const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const EstateEmploymentContext*);'
        return once(s,old,old+'\n// 私租/私工资必须借同候选原业务来源，早位不依赖经营资格或法律全验。\nstruct PrivateShopResponsibilitySourceContext;\nStatus validate_local_economy(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const PopulationMilitaryReadContext*,const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const EstateEmploymentContext*,const PrivateShopResponsibilitySourceContext*);')
    if path.endswith('local_economy_validation.cpp'):
        s=once(s,'#include <map>','#include <map>\n#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"')
        start='Status validate_obligations('
        pos=s.index(start); end=s.index('{',pos)
        s=s[:pos]+s[pos:end].replace('const EstateEmploymentContext* employment)','const EstateEmploymentContext* employment,const PrivateShopEconomySourceIds& shops)')+s[end:]
        s=once(s,'std::set<std::pair<StableId,Integer>> due_dates,ration_dates;std::map<StableId,Integer> paid;','std::set<std::pair<StableId,Integer>> due_dates,ration_dates;std::map<StableId,Integer> paid;\n    const std::set<StableId> shop_obligations(shops.obligation_ids.begin(),shops.obligation_ids.end()),shop_fractions(shops.fractional_arrear_ids.begin(),shops.fractional_arrear_ids.end());')
        s=once(s,'catalog.obligations.size()+s.membership_wage_bindings.size()+employed','catalog.obligations.size()+s.membership_wage_bindings.size()+employed+shop_obligations.size()')
        s=once(s,'for(const auto& row:s.obligations.rows()){const auto& o=row.second;if(employment','for(const auto& row:s.obligations.rows()){const auto& o=row.second;if(shop_obligations.count(row.first))continue;if(employment')
        s=once(s,'const auto& a=row.second;bool terminal=false;','const auto& a=row.second;bool terminal=shop_fractions.count(row.first);')
        sig='Status validate_local_economy(const CommonStore& c,const LocalEconomyState& s,const LocalEconomyCatalog& catalog,const OrganizationRead& organizations,const PopulationMilitaryReadContext* military,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const EstateEmploymentContext* employment)'
        s=once(s,sig+'{',sig+'{return validate_local_economy(c,s,catalog,organizations,military,takeover,execution,employment,nullptr);}\n'+sig[:-1]+',const PrivateShopResponsibilitySourceContext* shops){\n    PrivateShopEconomySourceIds shop_sources;if(shops){if(&shops->common!=&c||&shops->economy!=&s||&shops->economy_catalog!=&catalog||shops->phase)return common_failure("PrivateShopEconomyValidationContextMismatch");auto read=read_private_shop_economy_source_ids(*shops);if(!read.ok)return {false,{},read.error};shop_sources=std::move(read.value);}')
        return once(s,'validate_obligations(c,s,catalog,organizations,military,takeover,execution,employment)','validate_obligations(c,s,catalog,organizations,military,takeover,execution,employment,shop_sources)')
    if path.endswith('world_bundle.cpp'):
        old='        valid=validate_local_economy(common,local_economy,*catalog->local_economy,authority,population_context?&*population_context:nullptr,takeover_economy?&*takeover_economy:nullptr,takeover_economy?&economic_takeover_execution:nullptr,employment?&employment->context():nullptr);if(!valid.ok)return valid;'
        new='        std::optional<PrivateShopResponsibilitySourceContext> private_obligations;if(private_shops_enabled())private_obligations.emplace(PrivateShopResponsibilitySourceContext{common,local_economy,*catalog->local_economy,private_shops,*catalog->private_shops,nullptr});\n'+old.replace('employment?&employment->context():nullptr);','employment?&employment->context():nullptr,private_obligations?&*private_obligations:nullptr);')
        return once(s,old,new)
    if path.endswith('resident.cmake'):
        old='    src/resident/domains/commerce/private_shop_asset_responsibilities.cpp\n'
        if s.count(old)!=2: raise ValueError('product and identity anchors')
        s=s.replace(old,old+'    src/resident/domains/commerce/private_shop_economy_sources.cpp\n')
        return s+'\n# 私约只沿原 LE 钱账，早来源结构与末经营全验均须保留。\nadd_executable(test_resident_private_shop_economy_sources tests/resident/economic_world/private_shop_economy_sources_test.cpp)\ntarget_link_libraries(test_resident_private_shop_economy_sources PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_private_shop_economy_sources PRIVATE src)\ntarget_compile_options(test_resident_private_shop_economy_sources PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_private_shop_economy_sources COMMAND test_resident_private_shop_economy_sources)\n'
    raise ValueError(path)
def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=False);rows=[]
    for path in PATHS:
        before=subprocess.check_output(['git','-C',a.repo,'show',BASE+':'+path]);after=transform(path,before.decode()).encode();dest=out/'candidate'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(after)
        rows.append(dict(path=path,before_oid=subprocess.check_output(['git','-C',a.repo,'rev-parse',BASE+':'+path],text=True).strip(),after_oid=subprocess.check_output(['git','hash-object','--stdin'],input=after).decode().strip(),sha256=hashlib.sha256(after).hexdigest()))
    (out/'manifest.json').write_text(json.dumps(dict(base=BASE,shared=rows,status='SOURCE_ONLY',scope='原 Shop1 私租/工资来源，独立于未注册日租'),ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__': main()
