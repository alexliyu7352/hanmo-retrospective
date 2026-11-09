// 同tick原控制头须在全局实例与Common序同侧；不把今日玩家倒填旧许可。
#include "voluntary_path_control.hpp"
#include "voluntary_succession.hpp"
#include "historical_player.hpp"
#include "voluntary_path_state.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
Integer serial(const CommonStore& c,const StableId& id){auto colon=id.find(':');if(colon==StableId::npos)return -1;auto text=id.substr(colon+1);Integer n=0;auto r=std::from_chars(text.data(),text.data()+text.size(),n);return r.ec==std::errc{}&&r.ptr==text.data()+text.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==text?n:-1;}
}
Outcome<StableId> read_voluntary_player_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
 const auto* h=c.state().facts.find(fact);const auto n=serial(c,fact);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
 const bool original=h&&h->domain=="family"&&fact.rfind("family:",0)==0&&(h->kind=="family_succession_consent"||h->kind=="family_voluntary_succession");
 const bool cancelled=h&&h->domain==voluntary_path_module&&h->kind==voluntary_path_cancelled_kind&&fact.rfind("voluntary_path:",0)==0;
 if(!h||h->id!=fact||(!original&&!cancelled)||n<1||at==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("VoluntaryPlayerBoundaryRequired",fact).error};
 auto side=[&](const StableId& id,Integer changed)->Status{const auto number=serial(c,id);const auto where=std::find(order.begin(),order.end(),id);
  // 换控完成头本身是合法开区间边界；它不能被算入提交前玩家。
  if(id==fact)return original&&h->kind=="family_voluntary_succession"&&changed==tick?common_success():common_failure("VoluntaryPlayerOrderMismatch",id);
  return number>0&&where!=order.end()&&((number<n)==(where<at))&&(where<at?changed<=tick:changed>=tick)?common_success():common_failure("VoluntaryPlayerOrderMismatch",id);};
 for(const auto& id:family.successions){auto s=family_succession_step(family,id);if(!s.ok)return {false,{},s.error};auto v=side(id,s.value.tick);if(!v.ok)return {false,{},v.error};}
 if(legacy)for(const auto& r:legacy->activations.rows()){auto v=side(r.first,r.second.tick);if(!v.ok)return {false,{},v.error};}
 return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,fact):family_player_before_fact(c,family,tick,fact);
}
}
