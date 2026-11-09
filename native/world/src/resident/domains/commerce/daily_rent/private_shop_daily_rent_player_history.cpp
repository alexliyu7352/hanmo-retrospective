// 不把当前玩家或可重排的Common序单独当历史；两类原控制切换都须与原日租头交叉证明。
#include "private_shop_daily_rent_player_history.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
Outcome<StableId> read_private_shop_rent_player_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& boundary){
    auto fail=[&](const StableId& code,const StableId& id)->Outcome<StableId>{return {false,{},common_failure(code,id).error};};
    const auto* rent=c.state().facts.find(boundary);const auto& order=c.state().facts.order();const auto end=std::find(order.begin(),order.end(),boundary);
    if(!rent||rent->id!=boundary||rent->domain!=private_shop_daily_rent_module||(rent->kind!=private_shop_rent_paid_kind&&rent->kind!=private_shop_rent_resumed_kind)||end==order.end()||tick<0||tick>c.tick())return fail("PrivateShopRentPlayerBoundaryMismatch",boundary);
    const auto sequence=organization_instance_sequence(boundary,rent->kind);if(!sequence||*sequence>=c.state().next_instance_id)return fail("PrivateShopRentPlayerBoundaryMismatch",boundary);std::set<Integer> used{*sequence};
    // 原控制头即使同tick，也不得被单独移过付款/复业而改写控制者。
    auto crossing=[&](const StableId& id,const StableId& prefix,const StableId& kind,const StableId& domain,Integer changed)->Status{
        const auto number=organization_instance_sequence(id,prefix);const auto* head=c.state().facts.find(id);const auto at=std::find(order.begin(),order.end(),id);
        if(!number||*number>=c.state().next_instance_id||!used.insert(*number).second||!head||head->id!=id||head->kind!=kind||head->domain!=domain||at==order.end()||changed<0||changed>c.tick())return common_failure("PrivateShopRentControlSourceMismatch",id);
        const bool earlier=*number<*sequence;if(earlier!=(at<end)||(earlier&&changed>tick)||(!earlier&&changed<tick))return common_failure("PrivateShopRentControlOrderMismatch",id);return common_success();
    };
    for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};const auto* head=c.state().facts.find(id);if(!head||(head->kind!="family_succession"&&head->kind!="family_voluntary_succession"))return fail("PrivateShopRentControlSourceMismatch",id);auto valid=crossing(id,"family",head->kind,"family",step.value.tick);if(!valid.ok)return {false,{},valid.error};}
    if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=crossing(row.first,"family_legacy","family_legacy_control_activated","family_legacy",row.second.tick);if(!valid.ok)return {false,{},valid.error};}
    return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,boundary):family_player_before_fact(c,family,tick,boundary);
}
}
