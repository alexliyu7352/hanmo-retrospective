// 精确头门之后仍调用唯一原终止query；不把有头、死亡或今日位置单独当行动结束证明。
#include "private_shop_travel_termination_source.hpp"
#include "resident/domains/military/military_journey_termination.hpp"
#include "resident/domains/military/naval_journey_termination.hpp"
#include "resident/domains/military/naval_navigation_history.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
// 原全局实例只认规范正整数和已分配上界，不允许别种前缀、前导零或pending未分配键。
bool canonical(const StableId& id,const StableId& prefix,Integer next){
    if(id.rfind(prefix,0)!=0)return false;Integer n=0;const auto* end=id.data()+id.size();auto parsed=std::from_chars(id.data()+prefix.size(),end,n);
    return parsed.ec==std::errc{}&&parsed.ptr==end&&n>0&&n<next&&id==prefix+std::to_string(n);
}
}
Outcome<PrivateShopTravelMilitaryEndSource> read_private_shop_travel_military_end(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const NavalState* naval,const PrivateShopTravelMilitaryEndRequest& request){
    const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelMilitaryEndSource>{return {false,{},common_failure(code,request.source_fact_id).error};};
    StableId kind,domain,prefix;
    switch(request.kind){
        case PrivateShopTravelExpenseEndKind::military_casualties:kind="military_casualties";domain="military";prefix="military:";break;
        case PrivateShopTravelExpenseEndKind::naval_round:kind="naval_round";domain="military_naval";prefix="naval_round:";break;
        case PrivateShopTravelExpenseEndKind::naval_capture:kind="naval_capture";domain="military_naval";prefix="naval_capture:";break;
        case PrivateShopTravelExpenseEndKind::naval_continue:kind="naval_continue";domain="military_naval";prefix="naval_continue:";break;
        case PrivateShopTravelExpenseEndKind::naval_navigation:kind="naval_navigation_step";domain="military_naval";prefix="naval_navigation:";break;
        default:return fail("PrivateShopTravelMilitaryEndKindRequired");
    }
    const auto* head=common.state().facts.find(request.source_fact_id);
    if(!head||head->id!=request.source_fact_id||head->kind!=kind||head->domain!=domain||!canonical(request.source_fact_id,prefix,common.state().next_instance_id)||!canonical(request.action_id,"action:",common.state().next_instance_id)||request.actor_id.empty()||!common.actor(request.actor_id))return fail("PrivateShopTravelMilitaryEndHeaderMismatch");
    if(common.action(request.action_id))return fail("PrivateShopTravelMilitaryEndReleaseRequired");
    PrivateShopTravelMilitaryEndSource out;out.source_identity=common.state().identity;out.request=request;
    if(request.kind==PrivateShopTravelExpenseEndKind::military_casualties){
        auto source=query_military_journey_termination(common,military,catalog,request.source_fact_id,request.action_id,request.actor_id);if(!source.ok)return {false,{},source.error};if(!source.value)return fail("PrivateShopTravelMilitaryEndSourceRequired");
        const auto& s=*source.value;if(s.source_fact_id!=request.source_fact_id||s.action_id!=request.action_id||s.actor_id!=request.actor_id)return fail("PrivateShopTravelMilitaryEndSourceMismatch");
        out.battle_id=s.battle_id;out.casualty_fact_id=s.source_fact_id;out.original_reason=s.reason;out.tick=s.tick;
    }else {
        if(!naval)return fail("PrivateShopTravelNavalEndHistoryRequired");
        if(request.kind==PrivateShopTravelExpenseEndKind::naval_navigation){
            auto source=query_naval_navigation_termination(common,naval->navigation,request.source_fact_id,request.action_id);if(!source.ok)return {false,{},source.error};if(!source.value)return fail("PrivateShopTravelMilitaryEndSourceRequired");
            const auto& s=*source.value;if(s.source_id!=request.source_fact_id||s.action_id!=request.action_id||s.actor_id!=request.actor_id)return fail("PrivateShopTravelMilitaryEndSourceMismatch");
            out.original_reason=s.reason;out.tick=s.tick;
        }else {
            auto source=query_naval_journey_termination(common,military,*naval,request.source_fact_id,request.action_id,request.actor_id);if(!source.ok)return {false,{},source.error};if(!source.value)return fail("PrivateShopTravelMilitaryEndSourceRequired");
            const auto& s=*source.value;if(s.source_fact_id!=request.source_fact_id||s.action_id!=request.action_id||s.actor_id!=request.actor_id)return fail("PrivateShopTravelMilitaryEndSourceMismatch");
            out.battle_id=s.battle_id;out.casualty_fact_id=s.casualty_fact_id;out.original_reason=s.reason;out.tick=s.tick;
        }
    }
    if(out.tick<0||out.tick>common.tick()||out.original_reason.empty())return fail("PrivateShopTravelMilitaryEndBoundaryMismatch");
    return {true,std::move(out),{}};
}
}
