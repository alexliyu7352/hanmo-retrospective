#include "director_command_acceptance.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <charconv>
#include <string_view>
#include <utility>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId&code,const StableId&id={}){return {false,{},common_failure(code,id).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const CatalogIdentity&a,const CatalogIdentity&b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
// 原actor:sequence的唯一字节key借用比较，避免在全表校验时重复构造深串。
bool command_key_equals(const StableId& key,const StableId& actor,Integer sequence){char number[32];const auto end=std::to_chars(number,number+sizeof(number),sequence);std::string_view rest(key);if(rest.size()<actor.size()||rest.substr(0,actor.size())!=actor)return false;rest.remove_prefix(actor.size());if(rest.empty()||rest.front()!=':')return false;rest.remove_prefix(1);return rest==std::string_view(number,std::size_t(end.ptr-number));}
// CatalogIdentity复制来自当前真实完整目录，含每个module key/value；不按wire长度收费。
CheckpointMemoryCharge reserve_command_copy(const StableId& actor,const StableId& kind,const StableId& result,const StableId& policy,const CatalogIdentity& identity,const MomentOwnerSource& effect){
 return CheckpointMemoryCharge(0);
}
Outcome<const CommitResult*> original(const CommonStore&common,const DirectorAcceptedCommand&command){
 if(command.actor_id.empty()||command.command_kind.empty()||command.request_seq<1||command.result_id.empty()||command.policy_id.empty()||command.actual_effect.source_schema<1||command.actual_effect.source_ref.domain.empty()||command.actual_effect.source_ref.kind.empty()||command.actual_effect.source_ref.id.empty())return fail<const CommitResult*>("InvalidDirectorAcceptedCommand",command.result_id);
 auto moment=compare_world_moments(command.accepted_at,command.accepted_at);if(!moment.ok)return {false,nullptr,moment.error};
 const std::shared_ptr<const CommitResult>*cached=nullptr;for(const auto&row:common.state().results.rows())if(command_key_equals(row.first,command.actor_id,command.request_seq)){cached=&row.second;break;}const auto*sequence=common.state().request_sequences.find(command.actor_id);
 if(!cached||!*cached||!sequence||*sequence<command.request_seq)return fail<const CommitResult*>("DirectorActualCommandResultRequired",command.result_id);
 const auto&result=**cached;if(!result.ok||result.result_id!=command.result_id||result.kind!=command.command_kind||result.tick!=command.accepted_at.half_day_tick)return fail<const CommitResult*>("DirectorCommandResultMismatch",command.result_id);
 const HistoryRecord*history=nullptr;for(const auto&row:common.state().history)if(row.result_id==command.result_id){if(history)return fail<const CommitResult*>("DuplicateDirectorCommandHistory",command.result_id);history=&row;}
 if(!history||history->actor_id!=command.actor_id||history->kind!=command.command_kind||history->tick!=result.tick)return fail<const CommitResult*>("DirectorCommandHistoryMismatch",command.result_id);
 for(const auto&row:common.state().results.rows())if(row.second&&row.second->result_id==command.result_id&&!command_key_equals(row.first,command.actor_id,command.request_seq))return fail<const CommitResult*>("DirectorCommandIdentityAliased",command.result_id);
 return {true,&result,{}};
}
Status shape(const DirectorCatalog&catalog,const DirectorAdoptionResult2&decision,const DirectorAcceptedCommand&command){
 if(decision.subject.kind!=SubjectKind::actor||decision.subject.id!=command.actor_id||decision.typed_proposal_kind!=static_cast<Integer>(DirectorDecisionRoute2::actor)||decision.status!=DirectorAdoptionStatus2::adopted||decision.registration_epoch<1||decision.decision_seq<1||decision.reviewed_tick!=command.accepted_at.half_day_tick)return common_failure("DirectorCommandDecisionMismatch",command.result_id);
 const auto&source=decision.source_policy_ref;const auto*admission=source.domain=="director"&&source.kind=="actor_admission"?catalog.admissions.find(source.id):nullptr;
 if(!admission||admission->policy_id!=command.policy_id||!catalog.policies.find(command.policy_id))return common_failure("DirectorCommandOriginalPolicyMismatch",command.result_id);
 bool effect=false;for(const auto&source:decision.actual_outcome_refs)if(same(source,command.actual_effect.source_ref))effect=true;
 return effect?common_success():common_failure("DirectorCommandAppliedSourceMissing",command.result_id);
}
}
// fee先于所有拥有字段构造；WorldMoment的limbs沿原实际分配器，不在普通字符串账重复。
DirectorAcceptedCommand::DirectorAcceptedCommand(const StableId& actor,const StableId& kind,Integer seq,const StableId& result,const StableId& policy,const CatalogIdentity& catalog,const WorldMoment& at,const MomentOwnerSource& effect)
 :copy_memory(reserve_command_copy(actor,kind,result,policy,catalog,effect)),actor_id(actor),command_kind(kind),request_seq(seq),result_id(result),policy_id(policy),policy_catalog(catalog),accepted_at(at),actual_effect(effect){}
DirectorAcceptedCommand::DirectorAcceptedCommand(const DirectorAcceptedCommand& value)
 :DirectorAcceptedCommand(value.actor_id,value.command_kind,value.request_seq,value.result_id,value.policy_id,value.policy_catalog,value.accepted_at,value.actual_effect){}
// CanonicalFraction has a copying rvalue constructor. Construct the valid empty
// destination first, then transfer its actual limbs with the owner's no-throw
// swap. A budget failure before that swap leaves the source intact.
DirectorAcceptedCommand::DirectorAcceptedCommand(DirectorAcceptedCommand&& value)
 :DirectorAcceptedCommand(){swap(value);}
void DirectorAcceptedCommand::swap(DirectorAcceptedCommand& value)noexcept{
 copy_memory.swap(value.copy_memory);actor_id.swap(value.actor_id);command_kind.swap(value.command_kind);std::swap(request_seq,value.request_seq);result_id.swap(value.result_id);policy_id.swap(value.policy_id);
 policy_catalog.content_id.swap(value.policy_catalog.content_id);policy_catalog.content_version.swap(value.policy_catalog.content_version);policy_catalog.content_digest.swap(value.policy_catalog.content_digest);policy_catalog.ruleset_id.swap(value.policy_catalog.ruleset_id);policy_catalog.schema_sha256.swap(value.policy_catalog.schema_sha256);policy_catalog.pack_set_digest.swap(value.policy_catalog.pack_set_digest);policy_catalog.module_versions.swap(value.policy_catalog.module_versions);policy_catalog.artifact_sha256.swap(value.policy_catalog.artifact_sha256);
 std::swap(accepted_at.half_day_tick,value.accepted_at.half_day_tick);accepted_at.sub_half_day.swap(value.accepted_at.sub_half_day);
 actual_effect.source_ref.domain.swap(value.actual_effect.source_ref.domain);actual_effect.source_ref.kind.swap(value.actual_effect.source_ref.kind);actual_effect.source_ref.id.swap(value.actual_effect.source_ref.id);std::swap(actual_effect.source_schema,value.actual_effect.source_schema);
}
// Charges always travel with the fields they own. No destroyed object is left
// behind on an allocating move, and replaced storage stays charged to its owner.
DirectorAcceptedCommand& DirectorAcceptedCommand::operator=(DirectorAcceptedCommand&& value)noexcept{if(this!=&value)swap(value);return *this;}
DirectorAcceptedCommand& DirectorAcceptedCommand::operator=(const DirectorAcceptedCommand& value){if(this!=&value){DirectorAcceptedCommand fresh(value);swap(fresh);}return *this;}
Outcome<DirectorAcceptedCommand> capture_director_accepted_command(const CommonStore&common,const DirectorCatalog&catalog,const CatalogIdentity&identity,const StableId&policy,const StableId&actor,Integer seq,const CommitResult&result,const MomentOwnerSource&effect,const WorldMoment&at){
 if(!catalog.subjects2||!catalog.command_acceptance_bindings||!catalog.policies.find(policy))return fail<DirectorAcceptedCommand>("DirectorCommandPhysical6Required",policy);
 DirectorAcceptedCommand value{actor,result.kind,seq,result.result_id,policy,identity,at,effect};auto checked=original(common,value);if(!checked.ok)return {false,{},checked.error};if(checked.value!=&result)return fail<DirectorAcceptedCommand>("DirectorOriginalResultBorrowRequired",result.result_id);return {true,std::move(value),{}};
}
Outcome<DirectorAcceptedCommandView> read_director_accepted_command(const CommonStore&common,const DirectorState&state,const DirectorCatalog&catalog,const CatalogIdentity&identity,const DirectorAcceptedCommandKey&key){
 if(!state.subject2||state.subject2->command_binding_version!=1||!catalog.subjects2||!catalog.command_acceptance_bindings)return fail<DirectorAcceptedCommandView>("DirectorCommandPhysical6Required");
 const auto id=director_adoption_key2({SubjectKind::actor,key.actor_id},key.registration_epoch,key.decision_seq);const auto*decision=state.subject2->adoption_results.find(id);
 if(!decision||!decision->accepted_command)return fail<DirectorAcceptedCommandView>("DirectorActualCommandAdoptionRequired",id);const auto&command=*decision->accepted_command;
 if(command.actor_id!=key.actor_id||command.command_kind!=key.command_kind||command.request_seq!=key.request_seq||command.result_id!=key.result_id||!same(command.policy_catalog,identity))return fail<DirectorAcceptedCommandView>("DirectorActualCommandIdentityMismatch",id);
 auto valid=shape(catalog,*decision,command);if(!valid.ok)return {false,{},valid.error};auto result=original(common,command);if(!result.ok)return {false,{},result.error};return {true,{decision,&command,result.value},{}};
}
Status validate_director_accepted_commands(const CommonStore&common,const DirectorState&state,const DirectorCatalog&catalog,const CatalogIdentity&identity){
 if(!state.subject2)return catalog.command_acceptance_bindings?common_failure("DirectorCommandPhysical6Required"):common_success();const auto&subjects=*state.subject2;
 if(subjects.command_binding_version!=(catalog.command_acceptance_bindings?1:0))return common_failure("DirectorCommandPhysicalSelectionMismatch");
 for(const auto&row:subjects.adoption_results.rows()){const auto&decision=row.second;const bool needs=catalog.command_acceptance_bindings&&decision.subject.kind==SubjectKind::actor&&decision.status==DirectorAdoptionStatus2::adopted;
  if(decision.accepted_command.has_value()!=needs)return common_failure("DirectorCommandAdoptionCoverageMismatch",row.first);if(!needs)continue;const auto&command=*decision.accepted_command;
  auto result=read_director_accepted_command(common,state,catalog,identity,{decision.subject.id,decision.registration_epoch,decision.decision_seq,command.command_kind,command.request_seq,command.result_id});if(!result.ok)return {false,{},result.error};
  for(const auto&other:subjects.adoption_results.rows())if(other.first!=row.first&&other.second.accepted_command&&other.second.accepted_command->result_id==command.result_id)return common_failure("DirectorCommandAdoptedTwice",command.result_id);
 }
 return common_success();
}
Status validate_director_command_application(const CommonStore&common,const DirectorCatalog&catalog,const SubjectDecisionProposal2&proposal,const SubjectDecisionApplied2&applied,const WorldMoment&at){
 const bool required=catalog.command_acceptance_bindings&&proposal.route==DirectorDecisionRoute2::actor&&applied.outcome.kind==DecisionOutcome::Kind::adopted;
 if(required!=applied.accepted_command.has_value())return common_failure("DirectorActualCommandBindingRequired",proposal.plan_id);if(!required)return common_success();const auto&command=*applied.accepted_command;
 if(!proposal.actor_proposal||!proposal.actor_proposal->command||command.actor_id!=proposal.subject.id||command.command_kind!=proposal.actor_proposal->command->kind||command.policy_id!=proposal.policy_id||!applied.applied_effect_ref||!same(command.actual_effect.source_ref,*applied.applied_effect_ref))return common_failure("DirectorCommandProposalMismatch",proposal.plan_id);
 auto now=compare_world_moments(command.accepted_at,at);if(!now.ok)return {false,{},now.error};if(now.value)return common_failure("DirectorCommandAcceptanceMomentMismatch",proposal.plan_id);auto result=original(common,command);return result.ok?common_success():Status{false,{},result.error};
}
}
