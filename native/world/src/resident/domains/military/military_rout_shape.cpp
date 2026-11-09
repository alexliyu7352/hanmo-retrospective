// 形状门不授予历史真实性；入口和轮次分别按实际永久序列保留，内容完整回放在history门。
#include "military_rout_codec.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
Integer sequence(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return -1;Integer n=0;const auto tail=id.substr(prefix.size());const auto p=std::from_chars(tail.data(),tail.data()+tail.size(),n);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&n>0&&std::to_string(n)==tail?n:-1;}
bool fields(FieldOrder a,FieldOrder b){if(a.empty())return true;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;}
bool clean(const MilitaryBattleRecord& b){return !b.id.empty()&&b.round>=1&&b.round<=10000&&(b.phase=="planning"||b.phase=="result")&&b.round_receipt_ids.empty()&&b.settlement_receipt_id.empty()&&!b.applied&&b.completed_tick==-1;}
bool progress(const std::vector<MilitaryRoutProgress>& rows,Integer round){StableId prior;for(const auto& p:rows){if(p.unit_id.empty()||(!prior.empty()&&prior>=p.unit_id)||p.started_round<1||p.started_round>round||p.blocked_rounds<0||p.blocked_rounds>2||!fields(p.field_order,{"unit_id","started_round","blocked_rounds"}))return false;prior=p.unit_id;}return true;}
}
Status validate_military_rout_shape(const MilitaryRoutState& s){
 if(!fields(s.field_order,{"entries","rounds"}))return common_failure("MilitaryRoutShapeMismatch");Integer last=0,tick=-1;std::set<StableId> entries,rounds,casualties;
 for(const auto& e:s.entries){const auto n=sequence(e.id,"military_rout_entry:");if(n<=last||e.tick<0||e.tick<tick||!entries.insert(e.initial.id).second||!clean(e.initial)||e.initial.round!=1||e.initial.phase!="planning"||e.initial.created_tick!=e.tick||e.initial.last_tick!=e.tick||!fields(e.field_order,{"id","tick","initial"}))return common_failure("MilitaryRoutEntryShapeMismatch",e.id);last=n;tick=e.tick;}
 last=0;tick=-1;for(const auto& f:s.rounds){const auto n=sequence(f.id,"military_rout:");const bool normal=f.resolution_kind=="round";const std::set<StableId> reasons{"authority_lost","responsible_actor_lost","truce","cancelled","timeout"};if(n<=last||f.tick<0||f.tick<tick||f.round<1||f.round>10000||f.battle_id.empty()||f.battle_round_fact_id.empty()||f.casualty_fact_id.empty()||!rounds.insert(f.battle_round_fact_id).second||!casualties.insert(f.casualty_fact_id).second||!f.damage_sources.empty()||(!normal&&f.resolution_kind!="termination")||(normal?!f.termination_reason.empty():!reasons.count(f.termination_reason))||!clean(f.before)||!clean(f.after)||f.before.id!=f.battle_id||f.after.id!=f.battle_id||f.before.round!=f.round||f.before.phase!="planning"||!progress(f.progress_before,f.before.round)||!progress(f.progress_after,f.after.round)||!fields(f.field_order,{"id","battle_id","battle_round_fact_id","casualty_fact_id","tick","round","resolution_kind","termination_reason","before","after","damage_sources","progress_before","progress_after"}))return common_failure("MilitaryRoutRoundShapeMismatch",f.id);last=n;tick=f.tick;}
 return common_success();
}
}
