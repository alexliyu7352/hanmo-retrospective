// 三个新Done命令分别绑定其真实单一写入，不改变旧回执结构或已淘汰缓存规则。
#include "political_military_appointments_profile.hpp"
#include "political_military_appointments_detail.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_military_appointment_receipts(const PoliticalMilitaryAppointmentContext& c,const std::function<bool(const CommitResult&)>& is_done){
 const auto& common=c.common.state();std::set<StableId> matched;
 // 缓存键是actor:request_sequence，不是result_id；已按原64项规则淘汰的回执不再要求存在。
 for(const auto& cached:common.results.rows()){
  if(!cached.second||!political_military_appointment_command_kind(cached.second->kind))continue;const auto& result=*cached.second;
  const auto split=cached.first.rfind(':');if(split==StableId::npos||!is_done||!is_done(result))return common_failure("InvalidPoliticalMilitaryAppointmentReceipt",cached.first);
  const auto actor=cached.first.substr(0,split);auto request=organization_instance_sequence(cached.first,actor);const auto* last=common.request_sequences.find(actor);if(!request||!last||*request>*last)return common_failure("InvalidPoliticalMilitaryAppointmentReceipt",cached.first);
  const HistoryRecord* history=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* previous=nullptr;
  for(const auto& entry:common.history){if(entry.result_id==result.result_id){if(history)return common_failure("DuplicatePoliticalMilitaryAppointmentReceiptHistory",cached.first);history=&entry;lower=previous;}previous=&entry;}
  if(!history||history->kind!=result.kind||history->tick!=result.tick||history->actor_id!=actor)return common_failure("PoliticalMilitaryAppointmentReceiptHistoryMismatch",cached.first);
  auto upper_sequence=organization_instance_sequence(result.result_id,"result"),lower_sequence=lower?organization_instance_sequence(lower->result_id,"result"):std::optional<Integer>{0};if(!upper_sequence||!lower_sequence||*upper_sequence<=*lower_sequence)return common_failure("InvalidPoliticalMilitaryAppointmentReceiptOrder",cached.first);
  StableId source;unsigned count=0;const auto take=[&](const auto& record,const char* prefix,const StableId& signer){auto id=organization_instance_sequence(record.id,prefix);if(id&&*id>*lower_sequence&&*id<*upper_sequence&&record.tick==result.tick&&signer==actor){source=record.id;++count;}};
  if(result.kind=="political_military_officer_issue")for(const auto& record:c.state.authorizations.rows())take(record.second,"political_military_authorization",record.second.authority.actor_id);
  else if(result.kind=="political_military_officer_answer")for(const auto& record:c.state.answers.rows())take(record.second,"political_military_answer",record.second.actor_id);
  else for(const auto& record:c.state.withdrawals.rows())take(record.second,"political_military_withdrawal",record.second.authority.actor_id);
  if(count!=1||!matched.insert(source).second)return common_failure("PoliticalMilitaryAppointmentReceiptSourceMismatch",cached.first);
 }
 return common_success();
}

}
