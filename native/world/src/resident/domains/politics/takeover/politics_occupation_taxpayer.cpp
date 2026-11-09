#include "resident/domains/military/military_model.hpp"
// 历史边界使用原暂停事实前的军事实例顺序；今日Owner或后来的同tick占领不能改写旧期。
#include "politics_occupation_taxpayer.hpp"
#include "politics_takeover_detail.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include <map>
namespace hanmo::resident {
Outcome<OccupationFiscalTaxpayerSource> PoliticalOccupationFiscalTaxpayerAccess::occupation_fiscal_taxpayer_source(const Owner& original,const StableId& wallet,const StableId& inventory,Integer due,const std::optional<StableId>& pause)const{
    const auto& x=context_;const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<OccupationFiscalTaxpayerSource>{return {false,{},common_failure(code,id).error};};
    auto evaluation=political_takeover_evaluation_tick(x.common,x.phase);if(!evaluation.ok)return {false,{},evaluation.error};
    if(original.kind!=Owner::Kind::organization||original.id.empty()||!x.organization_catalog.organizations.find(original.id)||wallet.empty()||inventory.empty()||due<0||due>evaluation.value||(!pause&&due!=evaluation.value))return fail("InvalidOccupationFiscalTaxpayerRequest");
    Integer boundary=x.common.state().next_instance_id;
    if(pause){const auto* fact=x.common.state().facts.find(*pause);if(!fact||fact->id!=*pause||fact->domain!="local_economy"||(fact->kind!="occupation_fiscal_period_suspended"&&fact->kind!="occupation_fiscal_source_assessed"&&fact->kind!="occupation_fiscal_payment"&&fact->kind!="fiscal_period_assessed"&&fact->kind!="fiscal_source_assessed"&&fact->kind!="fiscal_payment"))return fail("InvalidOccupationFiscalPauseBoundary",*pause);auto id=organization_instance_sequence(*pause,fact->kind);if(!id||*id>=boundary)return fail("InvalidOccupationFiscalPauseBoundary",*pause);boundary=*id;}
    auto valid=x.phase?validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access,*x.phase):validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access);if(!valid.ok)return {false,{},valid.error};
    OccupationFiscalTaxpayerSource result;result.source_identity=x.common.state().identity;result.original_taxpayer=original;result.due_tick=due;
    auto resolve=[&](const StableId& id,bool is_inventory,OccupationFiscalTaxpayerContainer& out)->Status{
        const auto* authored=is_inventory?x.common_catalog.required_inventories.find(id):x.common_catalog.required_wallets.find(id);
        const auto* actual_wallet=is_inventory?nullptr:x.common.wallet(id);const auto* actual_inventory=is_inventory?x.common.inventory(id):nullptr;
        if(!authored||!owner_equal(authored->owner,original)||(!actual_wallet&&!actual_inventory))return common_failure("OccupationFiscalTaxpayerAuthoredOwnerMismatch",id);
        out.id=id;out.owner=authored->owner;Owner current=authored->owner;Integer prior=0,prior_tick=-1;StableId prior_fact;
        auto before=[&](const StableId& first,const StableId& last){bool seen=false;for(const auto& row:x.common.state().facts.rows()){if(row.first==last)return seen;if(row.first==first)seen=true;}return false;};
        auto transfer_for=[&](const MilitaryOccupationFact& fact)->std::optional<std::pair<Owner,Owner>>{if(is_inventory){for(const auto& item:fact.asset_manifest->inventories)if(item.first==id)return std::make_pair(item.second.from_owner,item.second.to_owner);}else{for(const auto& item:fact.asset_manifest->wallets)if(item.first==id)return std::make_pair(item.second.from_owner,item.second.to_owner);}return {};};
        std::map<Integer,const MilitaryOccupationFact*> ordered;
        for(const auto& fact:x.military.occupation_facts){if(!fact.assets_applied.value_or(false)||!fact.asset_manifest)return common_failure("OccupationAssetsNotApplied",fact.id);if(!transfer_for(fact))continue;auto sequence=organization_instance_sequence(fact.id,"military");if(!sequence||!ordered.emplace(*sequence,&fact).second)return common_failure("InvalidOccupationFiscalTaxpayerChain",fact.id);}
        for(const auto& row:ordered){const auto& fact=*row.second;const auto transfer=*transfer_for(fact);
            const auto* header=x.common.state().facts.find(fact.id);if(!header||header->id!=fact.id||header->domain!="military"||header->kind!="military_occupation"||row.first<=prior||fact.tick<prior_tick||(!prior_fact.empty()&&!before(prior_fact,fact.id))||(pause&&((row.first<boundary)!=before(fact.id,*pause)))||!owner_equal(transfer.first,current))return common_failure("InvalidOccupationFiscalTaxpayerChain",fact.id);
            current=transfer.second;prior=row.first;prior_tick=fact.tick;prior_fact=fact.id;
            if(row.first<boundary&&fact.tick<=due){out.owner=current;out.occupation_fact_ids.push_back(fact.id);}
        }
        const auto& actual=is_inventory?actual_inventory->owner:actual_wallet->owner;if(!owner_equal(actual,current))return common_failure("OccupationFiscalTaxpayerCurrentOwnerMismatch",id);
        return common_success();
    };
    auto first=resolve(wallet,false,result.wallet);if(!first.ok)return {false,{},first.error};auto second=resolve(inventory,true,result.inventory);if(!second.ok)return {false,{},second.error};return {true,std::move(result),{}};
}
}
