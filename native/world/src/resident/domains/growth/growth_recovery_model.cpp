// 精确来源记录的形状门；拒越界、重复终止、未关联工单与不符合原恢复点数学的前后值。
#include "growth_recovery_model.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
bool canonical(const StableId& id,const StableId& prefix){const auto key=prefix+":";if(id.rfind(key,0)!=0)return false;const auto digits=id.substr(key.size());Integer n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);return r.ec==std::errc{}&&r.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits;}
bool ordered(const FieldOrder& order,std::initializer_list<const char*> expected){if(order.empty())return true;std::set<StableId> want;for(const auto* key:expected)want.insert(key);const std::set<StableId> seen(order.begin(),order.end());return seen==want&&seen.size()==order.size();}
}
Integer growth_recovery_service_points(GrowthRecoveryMethod method){return method==GrowthRecoveryMethod::safe_lodging?1:method==GrowthRecoveryMethod::qualified_clinic?2:-1;}
Status validate_growth_recovery_shape(const GrowthRecoveryState& s){
    if(!ordered(s.field_order,{"version","services","credits","closures"})||s.services.size()>1000000||s.credits.size()>1000000||s.closures.size()>1000000)return common_failure("InvalidGrowthRecoveryState");
    for(const auto& row:s.services.rows()){const auto& v=row.second;if(!canonical(row.first,"action")||v.actor_id.empty()||v.provider_actor_id.empty()||v.actor_id==v.provider_actor_id||v.role_id.empty()||v.place_id.empty()||v.start_tick<0||v.duration<1||v.duration>720||growth_recovery_service_points(v.method)<1||!ordered(v.field_order,{"actor_id","provider_actor_id","role_id","place_id","method","start_tick","duration"}))return common_failure("InvalidGrowthRecoveryService",row.first);}
    std::set<std::pair<StableId,Integer>> ordinals;
    for(const auto& row:s.credits.rows()){const auto& v=row.second;const auto* service=s.services.find(v.action_id);if(!canonical(row.first,"recovery")||v.kind!="personal_recovery_credit"||!service||v.tick<0||v.ordinal<1||v.ordinal>service->duration||v.tick<service->start_tick||v.tick-service->start_tick!=v.ordinal||v.offered_points!=growth_recovery_service_points(service->method)||!ordinals.emplace(v.action_id,v.ordinal).second||!ordered(v.field_order,{"kind","action_id","tick","ordinal","injury_before","points_before","injury_after","points_after","offered_points","used_points"}))return common_failure("InvalidGrowthRecoveryCredit",row.first);auto calculated=apply_growth_recovery_points(v.before,v.offered_points);if(!calculated.ok||calculated.value.after.injury!=v.after.injury||calculated.value.after.points!=v.after.points||calculated.value.used_points!=v.used_points)return common_failure("GrowthRecoveryCreditMathMismatch",row.first);}
    std::set<StableId> closed;
    for(const auto& row:s.closures.rows()){const auto& v=row.second;const auto* service=s.services.find(v.action_id);const bool dead=v.reason=="patient_dead"||v.reason=="provider_dead",complete=v.reason=="completed";const bool reason=dead||complete||v.reason=="cancelled";
        if(!canonical(row.first,"recovery")||s.credits.find(row.first)||v.kind!="personal_recovery_closed"||!service||!reason||v.tick<service->start_tick||v.provided_half_days<0||v.provided_half_days>service->duration||v.tick-service->start_tick<v.provided_half_days||(complete&&(v.provided_half_days!=service->duration||v.tick-service->start_tick!=service->duration))||(!complete&&v.provided_half_days==service->duration)||dead!=!v.cause_fact_id.empty()||!closed.insert(v.action_id).second||!ordered(v.field_order,{"kind","action_id","tick","provided_half_days","reason","cause_fact_id","commit_result_id","commit_actor_id","commit_kind","commit_request_seq","commit_revision","commit_advance"}))return common_failure("InvalidGrowthRecoveryClosure",row.first);
        const bool unbound=v.commit_result_id.empty();
        if(unbound?(!v.commit_actor_id.empty()||!v.commit_kind.empty()||v.commit_request_seq!=0||v.commit_revision!=0||v.commit_advance):(!canonical(v.commit_result_id,"result")||v.commit_actor_id.empty()||v.commit_kind.empty()||v.commit_request_seq<1||v.commit_request_seq>2147483647||v.commit_revision<1))return common_failure("InvalidGrowthRecoveryCommitBinding",row.first);
    }
    return common_success();
}
}
