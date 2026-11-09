// 接册保存全院真实源的截面，不以今日有效通缉列表代替已解除案件或旧许可处分。
#include "political_shop_registry_sources.hpp"
#include "political_shop_licensing_detail.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Outcome<PoliticalShopRegistryLegalSources> read_political_shop_registry_legal_sources(const CommonStore& common,const PoliticalShopLicensingCatalog& catalog,const PoliticalShopLicensingState& state,const StableId& court_id,const StableId& boundary,std::optional<Integer> accepted_tick){
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PoliticalShopRegistryLegalSources>{return {false,{},common_failure(code,id).error};};
    if(catalog.version!=1||state.version!=1||!catalog.courts.find(court_id)||common.tick()<0)return fail("PoliticalShopRegistryOriginalBookRequired",court_id);
    const auto total=state.registries.size()+state.warrants.size()+state.releases.size()+state.clearances.size()+state.permits.size()+state.revocations.size();if(total>65536)return fail("PoliticalShopHistoryLimit");
    Integer tick=common.tick(),cutoff=common.state().next_instance_id;std::size_t limit=common.state().facts.size();
    std::map<StableId,std::size_t> positions;for(std::size_t i=0;i<common.state().facts.size();++i)positions.emplace(common.state().facts.rows()[i].first,i);
    if(!boundary.empty()){
        const auto* head=common.state().facts.find(boundary);auto number=organization_instance_sequence(boundary,political_shop_registry_accept_prefix);
        if(!accepted_tick||*accepted_tick<0||*accepted_tick>common.tick()||!head||head->id!=boundary||head->domain!=political_shop_registry_succession_module||head->kind!=political_shop_registry_accept_kind||!number||*number>=cutoff||!positions.count(boundary))return fail("PoliticalShopRegistryAcceptanceBoundaryRequired",boundary);
        tick=*accepted_tick;cutoff=*number;limit=positions.at(boundary);
    }else if(accepted_tick&&*accepted_tick!=tick)return fail("PoliticalShopRegistryAcceptanceBoundaryRequired");
    PoliticalShopRegistryLegalSources result;std::size_t seen=0;Integer previous=0,last_tick=-1;
    auto prior=[&](const StableId& source,const StableId& dependent){const auto a=positions.find(source),b=positions.find(dependent);return a!=positions.end()&&b!=positions.end()&&a->second<b->second;};
    for(std::size_t i=0;i<common.state().facts.size();++i){
        const auto& row=common.state().facts.rows()[i];const auto& head=row.second;const auto prefix=shop_law_detail::prefix_for(head.kind);
        if(head.domain!=political_shop_licensing_module&&(prefix.empty()||head.kind=="private_shop_opened"))continue;
        auto checked=validate_political_shop_licensing_fact_header(common,state,head);if(!checked.ok)return {false,{},checked.error};
        auto number=shop_law_detail::fact_sequence(common,row.first,head.kind);if(!number.ok)return {false,{},number.error};
        StableId owner;Integer when=-1;
        if(const auto* x=state.registries.find(row.first)){owner=x->court_id;when=x->tick;}
        else if(const auto* x=state.warrants.find(row.first)){owner=x->court_id;when=x->tick;const auto* root=state.registries.find(x->registry_fact_id);if(!root||root->court_id!=owner||!prior(root->id,x->id))return fail("PoliticalShopRegistryOriginalSourceMismatch",row.first);}
        else if(const auto* x=state.releases.find(row.first)){const auto* source=state.warrants.find(x->warrant_fact_id);if(!source||!prior(source->id,x->id))return fail("PoliticalShopRegistryOriginalSourceMismatch",row.first);owner=source->court_id;when=x->tick;}
        else if(const auto* x=state.clearances.find(row.first)){owner=x->court_id;when=x->tick;const auto* root=state.registries.find(x->registry_fact_id);if(!root||root->court_id!=owner||!prior(root->id,x->id))return fail("PoliticalShopRegistryOriginalSourceMismatch",row.first);}
        else if(const auto* x=state.permits.find(row.first)){const auto* source=state.clearances.find(x->clearance_fact_id);if(!source||!prior(source->id,x->id))return fail("PoliticalShopRegistryOriginalSourceMismatch",row.first);owner=source->court_id;when=x->tick;}
        else if(const auto* x=state.revocations.find(row.first)){const auto* source=state.permits.find(x->permit_fact_id);const auto* clear=source?state.clearances.find(source->clearance_fact_id):nullptr;if(!source||!clear||!prior(source->id,x->id))return fail("PoliticalShopRegistryOriginalSourceMismatch",row.first);owner=clear->court_id;when=x->tick;}
        if(owner.empty()||!catalog.courts.find(owner)||number.value<=previous||when<last_tick||when<0||when>common.tick())return fail("PoliticalShopRegistryOriginalOrderMismatch",row.first);
        if((number.value<cutoff)!=(i<limit)||(i<limit&&when>tick)||(i>=limit&&when<tick))return fail("PoliticalShopRegistryOriginalBoundaryMismatch",row.first);
        previous=number.value;last_tick=when;++seen;
        if(owner!=court_id||i>=limit)continue;
        if(state.registries.find(row.first)){if(!result.initial_registry_fact_id.empty())return fail("PoliticalShopRegistryOriginalBookAmbiguous",court_id);result.initial_registry_fact_id=row.first;}
        result.fact_ids.push_back(row.first);
    }
    if(seen!=total||result.initial_registry_fact_id.empty()||result.fact_ids.empty()||result.fact_ids.front()!=result.initial_registry_fact_id)return fail("PoliticalShopRegistryOriginalBookRequired",court_id);
    return {true,std::move(result),{}};
}
}
