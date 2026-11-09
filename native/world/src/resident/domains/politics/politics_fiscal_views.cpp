// 治所公文只对实际当地有权者开放；不能借财政读口查看远方钱粮或民间税源。
#include "politics_fiscal.hpp"
#include "politics_internal.hpp"
#include "politics_view_index.hpp"
#include "resident/domains/social/social_model.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 可读名称只表达三档正式政策，不把将来税额冒作已收钱粮。
StableId mode_text(FiscalTaxMode mode){return mode==FiscalTaxMode::lenient?"宽免":mode==FiscalTaxMode::normal?"常赋":"加征";}
}
Outcome<BoundaryValue> query_political_fiscal_entries(const CommonStore& c,const CommonCatalog& cc,const PoliticsState& politics,const PoliticalFiscalState& state,const PoliticalFiscalCatalog& catalog,const OrganizationState& org,const OrganizationCatalog& oc,const ActivityRead& activity,const StableId& actor,const StableId& site,const PoliticsViewIndex* provided_index){
    if(catalog.version!=1||state.version!=1)return {false,{},common_failure("PoliticalFiscalLegacyVersionRequired").error};
    if(!catalog.enabled)return {false,{},common_failure("PoliticalFiscalNotEnabled",{},ErrorKind::unsupported_capability).error};
    const auto here=politics_detail::place(c,actor);if(!here||here->place_id!=site)return {false,{},common_failure("PlaceNotObservable",site).error};
    std::optional<PoliticsViewIndex> rebuilt;if(!provided_index)rebuilt=make_politics_view_index(c,politics,org,oc,SocialState{},&state);const auto& index=provided_index?*provided_index:*rebuilt;if(!politics_view_index_matches(index,c,politics)||index.policy_count!=state.policies.size())return {false,{},common_failure("StalePoliticsViewIndex").error};
    V::Array rows;Integer scanned=0;bool truncated=false;
    for(const auto& jurisdiction_id:politics_index_rows(index.jurisdiction_ids_by_site,site)){
        if(++scanned>256||rows.size()>=64){truncated=true;break;}const auto* selected=politics.jurisdictions.find(jurisdiction_id);if(!selected)return {false,{},common_failure("StalePoliticsViewIndex").error};const auto& j=*selected;if(j.seat_place_id!=site)continue;
        for(const auto& office_id:politics_index_rows(index.office_ids_by_actor,actor)){
            if(++scanned>256){truncated=true;break;}const auto* held=org.offices.find(office_id);if(!held)continue;const auto& office=*held;
            ActingAuthority authority{office.organization_id,office.id,office.epoch,actor};
            if(office.holder_actor_id!=std::optional<StableId>{actor}||!check_authority(c,org,oc,authority,"political_set_tax",{ScopeRef::Kind::site,site},0).ok)continue;
            const auto policy=index.latest_policy_by_jurisdiction.find(j.id);const FiscalPolicyRecord* latest=policy==index.latest_policy_by_jurisdiction.end()?nullptr:state.policies.find(policy->second);if(policy!=index.latest_policy_by_jurisdiction.end()&&(!latest||latest->snapshot.jurisdiction_id!=j.id))return {false,{},common_failure("StalePoliticsViewIndex").error};
            V::Array actions;for(const auto& inventory:j.public_inventory_ids){if(truncated)break;for(const auto mode:{FiscalTaxMode::lenient,FiscalTaxMode::normal,FiscalTaxMode::surcharge}){
                if(++scanned>256){truncated=true;break;}PoliticalSetTaxCommand command{authority,j.id,inventory,j.revision,latest?latest->snapshot.policy_revision:0,mode};
                const auto allowed=preview_political_set_tax(c,politics,state,catalog,org,oc,activity,actor,command);
                actions.push_back(boundary::obj({{"text",V{"改为"+mode_text(mode)}},{"command",V{"political_set_tax"}},{"params",encode_political_set_tax(command)},{"available",V{allowed.ok}},{"code",V{allowed.ok?StableId{}:allowed.error.original.code}}}));
            }}
            const auto* name=cc.texts.find(j.name_key);StableId text=latest?"已登记"+mode_text(latest->snapshot.mode)+"，半日"+std::to_string(latest->snapshot.effective_from_tick)+"起生效":"尚未登记税政";
            rows.push_back(boundary::obj({{"id",V{j.id+":"+office.id}},{"kind",V{"fiscal_policy"}},{"display_name",V{name?*name:j.id}},{"text",V{std::move(text)}},{"source_fact_id",V{latest?latest->id:StableId{}}},{"terms_text",V{"治所税政公文。变更从下一半日起用于后续到期责任，已到期税额和旧欠税保留原依据；此操作不会直接增加公库或扣取私人钱包。"}},{"actions",V{std::move(actions)}}}));break;
        }
    }
    return {true,boundary::obj({{"ok",V{true}},{"rows",V{std::move(rows)}},{"truncated",V{truncated}}}),{}};
}
}
