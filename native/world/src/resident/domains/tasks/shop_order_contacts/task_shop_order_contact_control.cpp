// 仅原controlled-player模式的真实零时开工头；同tick控制与开工实例/Common必须同侧。
#include "task_shop_order_contact_control.hpp"
#include "task_shop_order_contact_detail.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
// 完整原域负责事实授权；这里只检查两个真实已分配实例与Common顺序的同一截面。
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return {};return shop_contact_detail::serial(c,id,id.substr(0,at));}
}
Outcome<StableId> task_shop_order_contact_player_before(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
    const auto* head=c.state().facts.find(fact);const auto boundary=instance(c,fact);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
    if(!head||head->id!=fact||head->domain!=task_shop_order_contact_module||head->kind!=task_shop_order_contact_started_kind||!boundary||at==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("TaskShopOrderContactControlBoundaryRequired",fact).error};
    auto same_side=[&](const StableId& id,Integer changed_tick)->Status{const auto n=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!n||where==order.end()||*n==*boundary||((*n<*boundary)!=(where<at))||(where<at?changed_tick>tick:changed_tick<tick))return common_failure("TaskShopOrderContactControlOrderMismatch",id);return common_success();};
    for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}
    if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
    return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,fact):family_player_before_fact(c,family,tick,fact);
}
}
