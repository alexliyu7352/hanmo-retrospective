// 野外专项只能以32768新配对开启；原驻营pair和旧政治Done字节保持各自准入。
#pragma once
#include "political_field_appointments.hpp"
#include "resident/domains/military/military_field_command_profile.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
inline bool political_field_appointment_command_kind(const StableId& kind){return kind=="political_field_appointment_issue"||kind=="political_field_appointment_answer"||kind=="political_field_appointment_withdraw";}
inline Status validate_political_field_appointment_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const PoliticalFieldAppointmentState* state){auto pair=validate_military_field_command_module_pair(modules);if(!pair.ok)return pair;bool found=false,politics=false;for(const auto& v:modules){if(v.first==military_field_command_political_module)found=true;if(v.first=="politics"&&v.second=="1")politics=true;}if(found!=enabled||bool(state)!=enabled||(enabled&&(!politics||state->version!=1)))return common_failure("PoliticalFieldProfileMismatch");return common_success();}
// 孤头和孤立缓存不能给未启用模块授予野外换帅能力。
inline Status validate_political_field_appointment_command_history(const CommonStore& common,bool enabled){if(enabled)return common_success();for(const auto& row:common.state().facts.rows())if(row.second.kind==political_field_issue_kind||row.second.kind==political_field_accept_kind||row.second.kind==political_field_decline_kind||row.second.kind==political_field_withdraw_kind)return common_failure("PoliticalFieldProfileRequired",row.first);for(const auto& v:common.state().history)if(political_field_appointment_command_kind(v.kind))return common_failure("PoliticalFieldProfileRequired",v.result_id);for(const auto& v:common.state().results.rows())if(v.second&&political_field_appointment_command_kind(v.second->kind))return common_failure("PoliticalFieldProfileRequired",v.first);return common_success();}
Status validate_political_field_appointment_receipts(const PoliticalFieldAppointmentContext&,const std::function<bool(const CommitResult&)>&);
BoundaryValue encode_political_field_appointment_quote(const PoliticalFieldAppointmentQuote&);
Outcome<BoundaryValue> query_political_field_appointments(const PoliticalFieldAppointmentContext&,Integer cursor=0);
}
