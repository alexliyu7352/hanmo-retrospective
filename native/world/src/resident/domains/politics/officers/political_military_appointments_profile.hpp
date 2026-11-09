// 新政治任命只与Military6具名调任成对启用；旧任命段和旧政治回执不借此升级。
#pragma once
#include "political_military_appointments.hpp"
#include "resident/domains/military/military_officer_profile.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
inline bool political_military_appointment_command_kind(const StableId& kind){return kind=="political_military_officer_issue"||kind=="political_military_officer_answer"||kind=="political_military_officer_withdraw";}
inline Status validate_political_military_appointment_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const PoliticalMilitaryAppointmentState* state){
    auto pair=validate_military_officer_module_pair(modules);if(!pair.ok)return pair;bool found=false,politics=false;for(const auto& v:modules){if(v.first==military_officer_political_module)found=true;if(v.first=="politics"&&v.second=="1")politics=true;}
    if(found!=enabled||bool(state)!=enabled||(enabled&&(!politics||state->version!=1)))return common_failure("PoliticalMilitaryAppointmentProfileMismatch");return common_success();
}
// 普通旧档不能仅通过写一个Done回执或孤立事实偷偷启动调任业务。
inline Status validate_political_military_appointment_command_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();for(const auto& row:common.state().facts.rows())if(row.second.kind==political_officer_issue_kind||row.second.kind==political_officer_accept_kind||row.second.kind==political_officer_decline_kind||row.second.kind==political_officer_withdraw_kind)return common_failure("PoliticalMilitaryAppointmentProfileRequired",row.first);
    for(const auto& v:common.state().history)if(political_military_appointment_command_kind(v.kind))return common_failure("PoliticalMilitaryAppointmentProfileRequired",v.result_id);
    for(const auto& v:common.state().results.rows())if(v.second&&political_military_appointment_command_kind(v.second->kind))return common_failure("PoliticalMilitaryAppointmentProfileRequired",v.first);return common_success();
}
Status validate_political_military_appointment_receipts(const PoliticalMilitaryAppointmentContext&,const std::function<bool(const CommitResult&)>& is_done);
Outcome<BoundaryValue> merge_political_military_appointment_page(const PoliticalMilitaryAppointmentContext&,const StableId& actor,const StableId& site,BoundaryValue page);
BoundaryValue encode_political_military_appointment_quote(const PoliticalMilitaryAppointmentQuote&);
}
