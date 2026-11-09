// M-GOV完整九tag物理布局；JSON、checkpoint和冻结投影复用一张字段表。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "governance_program.hpp"
#include "resident/domains/tasks/work_execution/work_result_source_codec.hpp"
#include "resident/domains/tasks/work_execution/work_follow_through_codec.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {namespace {
using namespace organization_lifecycle_fields;
// These seven original state rows contain exact values whose rvalue transfer
// may allocate through the real numeric allocator. A failed private decode is
// discarded; partially moved rows are never retried or published.
template<class T>inline constexpr bool governance_decoded_state_row=
 std::is_same_v<T,GovernanceProgramApproval>||std::is_same_v<T,GovernanceExecutionBinding>||
 std::is_same_v<T,GovernanceProgramOutcome>||std::is_same_v<T,GovernanceAppliedEffectSource>||
 std::is_same_v<T,GovernanceSupervisionExecutionApproval>||
 std::is_same_v<T,GovernanceWorkReassignmentApproval>||std::is_same_v<T,GovernanceWorkDeadlineTermination>;
struct GovernanceDecodedRowTransfer {
 template<class Destination,class T>Status operator()(Destination& destination,std::pair<StableId,T>& row)const{
  static_assert(governance_decoded_state_row<T>,"Only original Governance state rows may use this transfer");
  destination.emplace_back(std::move(row));return common_success();
 }
};
template<class T,class F>Status governance_state_table(CodecVisitor& c,const StableId& name,OrderedTable<T>& rows,F visit){
 static_assert(governance_decoded_state_row<T>);
 return common_checkpoint::table(c,name,rows,visit,GovernanceDecodedRowTransfer{});
}
template<class T,class F>Status governance_state_table(ActorFoundationJsonVisitor& c,const StableId& name,OrderedTable<T>& rows,F visit){
 static_assert(governance_decoded_state_row<T>);
 return c.table(name,rows,visit,GovernanceDecodedRowTransfer{});
}
template<class T,class F>Status governance_state_table(ActorFoundationProjectionArchive& c,const StableId& name,OrderedTable<T>& rows,F visit){
 static_assert(governance_decoded_state_row<T>);
 return organization_lifecycle_fields::table(c,name,rows,visit);
}
constexpr const char* target_tags[]={"pacification","agronomy","patrol","repair_fortification","build_facility","relief","industry_support","recruit","train"};
template<class C>Status period(C& c,BudgetPeriod& p){c.field("period_from_tick",p.period_from_tick);return c.field("period_until_tick",p.period_until_tick);}
template<class C>Status budget(C& c,BudgetExecutionRef& r){c.field("budget_id",r.budget_id);c.field("budget_revision",r.budget_revision);c.field("policy_revision",r.policy_revision);return c.field("approval_fact_id",r.approval_fact_id);}
template<class C>Status subject(C& c,SubjectRef& r){static constexpr const char* tags[]={"actor","organization","cohort"};enum_field(c,"kind",r.kind,tags);return c.field("id",r.id);}
template<class C>Status scope(C& c,ScopeRef& r){static constexpr const char* tags[]={"site","region","force","account","activity"};enum_field(c,"kind",r.kind,tags);return c.field("id",r.id);}
template<class C>Status acting(C& c,ActingAuthority& a){c.field("organization_id",a.organization_id);c.field("office_id",a.office_id);c.field("authority_epoch",a.authority_epoch);return c.field("actor_id",a.actor_id);}
template<class C>Status basis_ref(C& c,AuthorityBasisRef& r){c.object("issued_source",[&](auto& v){return source(v,r.issued_source);});return c.field("source_schema",r.source_schema);}
template<class C>Status deadline(C& c,StateNeedDeadline& d){c.field("tick",d.tick);return c.field("inclusive",d.inclusive);}
template<class C>Status work_kind(C& c,WorkKind& value){static constexpr const char* tags[]={"patrol","agronomy_supervision","industry_supervision","construction_supervision"};return enum_field(c,"kind",value,tags);}
template<class C>Status work_ref(C& c,WorkRef& r){c.field("owner_domain",r.owner_domain);static constexpr const char* tags[]={"agronomy_operation","industrial_operation","facility_construction"};enum_field(c,"kind",r.kind,tags);return c.field("work_id",r.work_id);}
// 未知tag在分支取值前拒绝；写侧不替换原variant或生成删字段子集。
template<class C,class V,std::size_t N>Status tag(C& c,V& value,const char* const (&names)[N],std::size_t& index){StableId name;if(!c.reading()){index=value.index();if(index>=N)return c.reject("InvalidGovernanceVariant");name=names[index];}c.field("kind",name);if(!c.status().ok)return c.status();index=0;for(;index<N&&name!=names[index];++index){}return index<N?c.status():c.reject("UnknownGovernanceVariant");}
template<class C>Status patrol_gain(C& c,PatrolGainBasis& value){std::size_t index=0;static constexpr const char* tags[]={"fixed_approved_delta","completed_skill_rule"};auto valid=tag(c,value,tags,index);if(!valid.ok)return valid;if(index==0){if(c.reading())value=FixedApprovedDelta{};return c.field("security_delta_points",std::get<FixedApprovedDelta>(value).security_delta_points);}if(c.reading())value=CompletedSkillRule{};return c.object("rule_ref",[&](auto& v){return rule(v,std::get<CompletedSkillRule>(value).rule_ref);});}
template<class C>Status patrol_rule(C& c,PatrolGainRule& x){c.object("rule_ref",[&](auto& v){return rule(v,x.rule_ref);});c.field("base_gain",x.base_gain);c.field("skill_id",x.skill_id);return c.field("cap",x.cap);}
template<class C>Status target(C& c,GovernanceTarget& value){std::size_t index=0;auto valid=tag(c,value,target_tags,index);if(!valid.ok)return valid;switch(index){
case 0:{if(c.reading())value=GovernancePacification{};auto& x=std::get<GovernancePacification>(value);c.field("jurisdiction_id",x.jurisdiction_id);c.field("relief_terms_id",x.relief_terms_id);c.field("support_delta_points",x.support_delta_points);return c.status();}
case 1:{if(c.reading())value=GovernanceAgronomy{};auto& x=std::get<GovernanceAgronomy>(value);c.field("production_unit_id",x.production_unit_id);c.field("improvement_policy_id",x.improvement_policy_id);return c.status();}
case 2:{if(c.reading())value=GovernancePatrol{};auto& x=std::get<GovernancePatrol>(value);Integer target_version=2;c.field("target_version",target_version);if(!c.status().ok)return c.status();if(target_version!=2)return c.reject("UnsupportedPatrolTargetVersion");c.field("jurisdiction_id",x.jurisdiction_id);strings(c,"unit_ids",x.unit_ids);c.object("gain",[&](auto& v){return patrol_gain(v,x.gain);});return c.status();}
case 3:{if(c.reading())value=GovernanceFortificationRepair{};auto& x=std::get<GovernanceFortificationRepair>(value);c.field("stronghold_id",x.stronghold_id);c.field("topology_id",x.topology_id);c.field("node_id",x.node_id);c.field("target_hp",x.target_hp);return c.status();}
case 4:{if(c.reading())value=GovernanceFacilityBuild{};auto& x=std::get<GovernanceFacilityBuild>(value);c.field("project_spec_id",x.project_spec_id);c.object("site_use_ref",[&](auto& v){return source(v,x.site_use_ref);});c.object("asset_owner",[&](auto& v){return subject(v,x.asset_owner);});return c.status();}
case 5:{if(c.reading())value=GovernanceRelief{};auto& x=std::get<GovernanceRelief>(value);strings(c,"relief_terms_ids",x.relief_terms_ids);c.object("effect_period",[&](auto& v){return period(v,x.effect_period);});c.object("effect_measure",[&](auto& v){std::size_t n=0;static constexpr const char* names[]={"cash","grain"};auto ok=tag(v,x.effect_measure,names,n);if(!ok.ok)return ok;if(n==0){if(v.reading())x.effect_measure=GovernanceCashRelief{};auto& m=std::get<GovernanceCashRelief>(x.effect_measure);v.field("currency_id",m.currency_id);v.field("target_amount",m.target_amount);return v.field("max_support_delta",m.max_support_delta);}if(v.reading())x.effect_measure=GovernanceGrainRelief{};auto& m=std::get<GovernanceGrainRelief>(x.effect_measure);v.field("item_id",m.item_id);v.field("target_quantity",m.target_quantity);return v.field("max_support_delta",m.max_support_delta);});return c.status();}
case 6:{if(c.reading())value=GovernanceIndustrySupport{};auto& x=std::get<GovernanceIndustrySupport>(value);c.field("production_unit_id",x.production_unit_id);c.field("investment_policy_id",x.investment_policy_id);c.object("investment_contract_ref",[&](auto& v){return source(v,x.investment_contract_ref);});return c.status();}
case 7:{if(c.reading())value=GovernanceRecruit{};auto& x=std::get<GovernanceRecruit>(value);c.field("recruitment_order_id",x.recruitment_order_id);return c.status();}
case 8:{if(c.reading())value=GovernanceTrain{};auto& x=std::get<GovernanceTrain>(value);c.field("training_order_id",x.training_order_id);return c.status();}
}return c.reject("UnknownGovernanceTarget");}
template<class C>Status route(C& c,std::variant<PatrolCircuit,StationWatch>& value){std::size_t index=0;static constexpr const char* tags[]={"patrol_circuit","station_watch"};auto valid=tag(c,value,tags,index);if(!valid.ok)return valid;if(index==0){if(c.reading())value=PatrolCircuit{};auto& x=std::get<PatrolCircuit>(value);sources(c,"checkpoint_refs",x.checkpoint_refs);return sources(c,"allowed_passage_refs",x.allowed_passage_refs);}if(c.reading())value=StationWatch{};return c.object("station_ref",[&](auto& v){return source(v,std::get<StationWatch>(value).station_ref);});}
template<class C>Status work_terms(C& c,GovernanceApprovedWorkTerms& value,Integer schema){std::size_t index=0;static constexpr const char* tags[]={"patrol","supervised_work"};auto valid=tag(c,value,tags,index);if(!valid.ok)return valid;if(index==0){if(c.reading())value=GovernancePatrolWorkTerms{};auto& x=std::get<GovernancePatrolWorkTerms>(value);strings(c,"permitted_leader_actor_ids",x.permitted_leader_actor_ids);c.object("route",[&](auto& v){return route(v,x.route);});c.field("minimum_available_people",x.minimum_available_people);c.object("completion_deadline",[&](auto& v){return deadline(v,x.completion_deadline);});c.object("supply_order_ref",[&](auto& v){return source(v,x.supply_order_ref);});c.field("rule_id",x.rule_id);return c.field("rule_version",x.rule_version);}if(c.reading())value=GovernanceSupervisionWorkTerms{};auto& x=std::get<GovernanceSupervisionWorkTerms>(value);c.object("work_kind",[&](auto& v){return work_kind(v,x.kind);});strings(c,"permitted_supervisor_actor_ids",x.permitted_supervisor_actor_ids);c.object("economic_work_ref",[&](auto& v){return work_ref(v,x.economic_work_ref);});c.object("completion_deadline",[&](auto& v){return deadline(v,x.completion_deadline);});c.object("target_site_or_anchor_ref",[&](auto& v){return source(v,x.target_site_or_anchor_ref);});c.optional("required_skill_ref",x.required_skill_ref,[](auto& v,auto& x){return source(v,x);});c.field("rule_id",x.rule_id);c.field("rule_version",x.rule_version);
 if(schema==3||schema==4){
  if(!c.reading()&&!x.initial_execution_time_use)return c.reject("GovernanceInitialSupervisionDurationRequired");
  if(c.reading())x.initial_execution_time_use.emplace();
  return c.object("initial_execution_time_use",[&](auto& v){
   StableId kind="action_duration";v.field("kind",kind);
   if(!v.status().ok)return v.status();if(kind!="action_duration")return v.reject("SupervisionRequiresActionDuration");
   v.field("half_days",x.initial_execution_time_use->half_days);
   return v.status().ok&&x.initial_execution_time_use->half_days<=0?v.reject("InvalidSupervisionActionDuration"):v.status();
  });
 }
 if(x.initial_execution_time_use)return c.reject("GovernanceSupervisionDurationRequiresCatalog3");
 return c.status();}
template<class C>Status spec(C& c,GovernanceProgramSpec& x,Integer schema){c.object("spec_ref",[&](auto& v){return rule(v,x.spec_ref);});c.object("target",[&](auto& v){return target(v,x.target);});c.field("resource_recipe_id",x.resource_recipe_id);c.field("executor_qualification_id",x.executor_qualification_id);c.field("required_work_half_days",x.required_work_half_days);c.object("scope",[&](auto& v){return scope(v,x.scope);});static constexpr const char* tags[]={"whole_cycle","incremental_receipts","external_terminal"};enum_field(c,"completion_policy",x.completion_policy,tags);c.object("continuity_policy_ref",[&](auto& v){return source(v,x.continuity_policy_ref);});c.optional("expense_policy_ref",x.expense_policy_ref,[](auto& v,auto& x){return source(v,x);});c.field("offer_valid_half_days",x.offer_valid_half_days);return c.optional("approved_work_terms",x.approved_work_terms,[schema](auto& v,auto& x){return work_terms(v,x,schema);});}
template<class C>Status executor(C& c,GovernanceExecutor& value){std::size_t index=0;static constexpr const char* tags[]={"named_actor","agency"};auto valid=tag(c,value,tags,index);if(!valid.ok)return valid;if(index==0){if(c.reading())value=GovernanceNamedExecutor{};return c.field("actor_id",std::get<GovernanceNamedExecutor>(value).actor_id);}if(c.reading())value=GovernanceAgencyExecutor{};return c.object("agency_ref",[&](auto& v){return source(v,std::get<GovernanceAgencyExecutor>(value).agency_ref);});}
template<class C>Status approval(C& c,GovernanceProgramApproval& x){c.field("id",x.id);c.object("spec_ref",[&](auto& v){return rule(v,x.spec_ref);});c.object("target",[&](auto& v){return target(v,x.target);});c.object("executor",[&](auto& v){return executor(v,x.executor);});c.object("budget_execution",[&](auto& v){return budget(v,x.budget_execution);});c.object("budget_period",[&](auto& v){return period(v,x.budget_period);});c.field("payer_wallet_id",x.payer_wallet_id);c.object("contract_debtor",[&](auto& v){return subject(v,x.contract_debtor);});c.object("issuer",[&](auto& v){return acting(v,x.issuer);});c.object("authority_basis_ref",[&](auto& v){return basis_ref(v,x.authority_basis_ref);});c.field("expected_target_revision",x.expected_target_revision);c.object("issued_at",[&](auto& v){return exact_fields::moment(v,x.issued_at);});return c.object("valid_until",[&](auto& v){return exact_fields::moment(v,x.valid_until);});}
template<class C>Status binding(C& c,GovernanceExecutionBinding& x){c.field("id",x.id);c.field("program_id",x.program_id);c.object("offered_task_ref",[&](auto& v){return source(v,x.offered_task_ref);});c.optional("task_acceptance_ref",x.task_acceptance_ref,[](auto& v,auto& s){return source(v,s);});c.optional("execution_ref",x.execution_ref,[](auto& v,auto& s){return source(v,s);});return c.object("occurred_at",[&](auto& v){return exact_fields::moment(v,x.occurred_at);});}
template<class C>Status application(C& c,GovernanceApplication& value){std::size_t index=0;auto valid=tag(c,value,target_tags,index);if(!valid.ok)return valid;switch(index){
case 0:{if(c.reading())value=GovernancePacificationApplication{};auto& x=std::get<GovernancePacificationApplication>(value);c.object("actual_work_result",[&](auto& v){return source(v,x.actual_work_result);});c.object("public_relief_receipt",[&](auto& v){return source(v,x.public_relief_receipt);});c.object("civil_consequence",[&](auto& v){return source(v,x.civil_consequence);});return c.status();}
case 1:{if(c.reading())value=GovernanceAgronomyApplication{};auto& x=std::get<GovernanceAgronomyApplication>(value);c.object("production_improvement_receipt",[&](auto& v){return source(v,x.production_improvement_receipt);});return c.status();}
case 2:{if(c.reading())value=GovernancePatrolApplication{};auto& x=std::get<GovernancePatrolApplication>(value);c.object("actual_work_result",[&](auto& v){return source(v,x.actual_work_result);});c.object("security_consequence",[&](auto& v){return source(v,x.security_consequence);});return c.status();}
case 3:{if(c.reading())value=GovernanceFortificationApplication{};auto& x=std::get<GovernanceFortificationApplication>(value);c.object("fortification_repair_result",[&](auto& v){return source(v,x.fortification_repair_result);});return c.status();}
case 4:{if(c.reading())value=GovernanceFacilityApplication{};auto& x=std::get<GovernanceFacilityApplication>(value);c.object("facility_activation_receipt",[&](auto& v){return source(v,x.facility_activation_receipt);});return c.status();}
case 5:{if(c.reading())value=GovernanceReliefApplication{};auto& x=std::get<GovernanceReliefApplication>(value);sources(c,"actual_receipts",x.actual_receipts);c.object("civil_consequence",[&](auto& v){return source(v,x.civil_consequence);});return c.status();}
case 6:{if(c.reading())value=GovernanceIndustryApplication{};auto& x=std::get<GovernanceIndustryApplication>(value);c.object("production_improvement_receipt",[&](auto& v){return source(v,x.production_improvement_receipt);});return c.status();}
case 7:{if(c.reading())value=GovernanceRecruitApplication{};auto& x=std::get<GovernanceRecruitApplication>(value);c.object("population_military_result",[&](auto& v){return source(v,x.population_military_result);});return c.status();}
case 8:{if(c.reading())value=GovernanceTrainApplication{};auto& x=std::get<GovernanceTrainApplication>(value);c.object("military_training_receipt",[&](auto& v){return source(v,x.military_training_receipt);});return c.status();}
}return c.reject("UnknownGovernanceApplication");}
template<class C>Status outcome(C& c,GovernanceProgramOutcome& x){c.field("id",x.id);c.field("program_id",x.program_id);enum_field(c,"target_kind",x.target_kind,target_tags);c.object("execution_ref",[&](auto& v){return source(v,x.execution_ref);});c.object("actual_result_ref",[&](auto& v){return source(v,x.actual_result_ref);});c.sequence("application_refs",x.application_refs,[](auto& v,auto& a){return application(v,a);});static constexpr const char* tags[]={"awaiting_acceptance","working","paused","partial","completed","cancelled","failed"};enum_field(c,"status",x.status,tags);sources(c,"remaining_requirement",x.remaining_requirement);c.object("continuity",[&](auto& v){return source(v,x.continuity);});c.object("occurred_at",[&](auto& v){return exact_fields::moment(v,x.occurred_at);});return c.optional("patrol_unavailable",x.patrol_unavailable,[](auto& v,auto& terminal){return work_follow_through_codec::terminal(v,terminal);});}
template<class C>Status effect(C& c,GovernanceAppliedEffectSource& x){c.field("id",x.id);c.field("program_id",x.program_id);c.field("outcome_source_id",x.outcome_source_id);c.object("application",[&](auto& v){return application(v,x.application);});return c.object("applied_at",[&](auto& v){return exact_fields::moment(v,x.applied_at);});}
// 新签发只保存原引用及批准H；无Work进度、价格或当前权限副本。
template<class C>Status execution_approval(C& c,GovernanceSupervisionExecutionApproval& x,Integer state_schema){
 c.field("id",x.id);c.field("work_id",x.work_id);
 c.object("authorization",[&](auto& v){return work_source_codec::authorization(v,x.authorization);});
 c.field("execution_ordinal",x.execution_ordinal);
 c.object("preceding_terminal_ref",[&](auto& v){v.object("source_ref",[&](auto& z){return source(z,x.preceding_terminal_ref.source_ref);});return v.field("source_schema",x.preceding_terminal_ref.source_schema);});
 c.object("quote_ref",[&](auto& v){return rule(v,x.quote_ref);});c.object("spec_ref",[&](auto& v){return rule(v,x.spec_ref);});
 c.field("service_limit_half_days",x.service_limit_half_days);c.object("issuer",[&](auto& v){return acting(v,x.issuer);});
 c.object("authority_basis_ref",[&](auto& v){return basis_ref(v,x.authority_basis_ref);});
 c.object("budget_execution",[&](auto& v){return budget(v,x.budget_execution);});c.object("budget_period",[&](auto& v){return period(v,x.budget_period);});
 c.field("expected_target_revision",x.expected_target_revision);c.object("issued_at",[&](auto& v){return exact_fields::moment(v,x.issued_at);});
 if(state_schema==3){c.field("source_schema",x.source_schema);c.optional("successor_acceptance_ref",x.successor_acceptance_ref,[](auto& v,auto& s){return source(v,s);});}
 else if(x.source_schema!=1||x.successor_acceptance_ref)return c.reject("GovernanceSuccessorAcceptanceRequiresState3");
 if(x.source_schema!=1&&x.source_schema!=2)return c.reject("GovernanceSupervisionSourceSchemaRequired");
 if(x.source_schema==1&&x.successor_acceptance_ref)return c.reject("GovernanceLegacySupervisionAcceptanceMismatch");return c.status();
}
// 新表与既有 rows 共用同一 codec，不借默认空表推断 schema。
template<class C>Status owner_source(C& c,MomentOwnerSource& x){c.object("source_ref",[&](auto& v){return source(v,x.source_ref);});return c.field("source_schema",x.source_schema);}
template<class C>Status reassignment(C& c,GovernanceWorkReassignmentApproval& x){
 c.field("id",x.id);c.field("work_id",x.work_id);c.object("authorization",[&](auto& v){return work_source_codec::authorization(v,x.authorization);});c.object("work_acceptance_ref",[&](auto& v){return source(v,x.work_acceptance_ref);});
 c.object("executor_predecessor_ref",[&](auto& v){return owner_source(v,x.executor_predecessor_ref);});c.optional("preceding_terminal_ref",x.preceding_terminal_ref,[](auto& v,auto& s){return owner_source(v,s);});
 c.object("work_kind",[&](auto& v){return work_kind(v,x.kind);});c.field("old_actor_id",x.old_actor_id);c.field("new_actor_id",x.new_actor_id);c.object("spec_ref",[&](auto& v){return rule(v,x.spec_ref);});
 c.object("issuer",[&](auto& v){return acting(v,x.issuer);});c.object("authority_basis_ref",[&](auto& v){return basis_ref(v,x.authority_basis_ref);});c.object("budget_execution",[&](auto& v){return budget(v,x.budget_execution);});c.object("budget_period",[&](auto& v){return period(v,x.budget_period);});c.field("expected_target_revision",x.expected_target_revision);return c.object("issued_at",[&](auto& v){return exact_fields::moment(v,x.issued_at);});
}
template<class C>Status deadline_termination(C& c,GovernanceWorkDeadlineTermination& x){c.field("id",x.id);c.field("work_id",x.work_id);c.object("authorization",[&](auto& v){return work_source_codec::authorization(v,x.authorization);});c.object("work_acceptance_ref",[&](auto& v){return source(v,x.work_acceptance_ref);});c.object("spec_ref",[&](auto& v){return rule(v,x.spec_ref);});c.object("absolute_deadline",[&](auto& v){return exact_fields::moment(v,x.absolute_deadline);});return c.object("effective_at",[&](auto& v){return exact_fields::moment(v,x.effective_at);});}
template<class C>Status state_fields(C& c,GovernanceProgramState& s,Integer expected=1){
 if((!c.reading()&&s.schema_version!=expected)||(expected==1&&!s.supervision_execution_approvals.empty())||(expected!=3&&(!s.work_reassignment_approvals.empty()||!s.work_deadline_terminations.empty())))return c.reject("UnsupportedGovernanceProgramsSchema");
 Integer schema=expected;c.field("schema_version",schema);if(!c.status().ok)return c.status();
 if(schema!=expected||(schema!=1&&schema!=2&&schema!=3))return c.reject("UnsupportedGovernanceProgramsSchema");if(c.reading())s.schema_version=schema;
 governance_state_table(c,"approvals",s.approvals,[](auto& v,auto& r){return approval(v,r);});governance_state_table(c,"execution_bindings",s.execution_bindings,[](auto& v,auto& r){return binding(v,r);});
 governance_state_table(c,"outcome_sources",s.outcome_sources,[](auto& v,auto& r){return outcome(v,r);});governance_state_table(c,"applied_effect_sources",s.applied_effect_sources,[](auto& v,auto& r){return effect(v,r);});
 if(schema>=2)governance_state_table(c,"supervision_execution_approvals",s.supervision_execution_approvals,[&](auto& v,auto& r){return execution_approval(v,r,schema);});
 if(schema==3){governance_state_table(c,"work_reassignment_approvals",s.work_reassignment_approvals,[](auto& v,auto& r){return reassignment(v,r);});governance_state_table(c,"work_deadline_terminations",s.work_deadline_terminations,[](auto& v,auto& r){return deadline_termination(v,r);});}return c.status();
}
template<class C>Status executor_qualification(C& c,GovernanceExecutorQualification& q){c.field("id",q.id);c.field("skill_id",q.skill_id);return c.field("minimum_level",q.minimum_level);}
template<class C>Status continuity_definition(C& c,GovernanceContinuityDefinition& d){c.field("id",d.id);static constexpr const char* policies[]={"continue_under_organization","stop_when_vacant"};return enum_field(c,"original_grant_policy",d.original_grant_policy,policies);}
template<class C>Status execution_quote(C& c,GovernanceSupervisionExecutionQuote& q){
 c.object("quote_ref",[&](auto& v){return rule(v,q.quote_ref);});c.object("program_spec_ref",[&](auto& v){return rule(v,q.program_spec_ref);});
 return c.object("time_use",[&](auto& v){StableId kind="action_duration";v.field("kind",kind);if(!v.status().ok)return v.status();
  if(kind!="action_duration")return v.reject("SupervisionRequiresActionDuration");v.field("half_days",q.time_use.half_days);
  return v.status().ok&&q.time_use.half_days<=0?v.reject("InvalidSupervisionActionDuration"):v.status();});
}
template<class C>Status catalog_fields(C& c,GovernanceProgramCatalog& s,Integer expected=1){
 if((!c.reading()&&s.schema_version!=expected)||(expected==1&&(!s.executor_qualifications.empty()||!s.continuity_definitions.empty()))||
    (expected!=4&&!s.supervision_execution_quotes.empty()))return c.reject("UnsupportedGovernanceProgramCatalogSchema");
 Integer schema=expected;c.field("schema_version",schema);if(!c.status().ok)return c.status();
 if(schema!=expected||(schema!=1&&schema!=2&&schema!=3&&schema!=4))return c.reject("UnsupportedGovernanceProgramCatalogSchema");if(c.reading())s.schema_version=schema;
 table(c,"specs",s.specs,[schema](auto& v,auto& r){return spec(v,r,schema);});table(c,"patrol_gain_rules",s.patrol_gain_rules,[](auto& v,auto& r){return patrol_rule(v,r);});
 if(schema>=2){table(c,"executor_qualifications",s.executor_qualifications,[](auto& v,auto& r){return executor_qualification(v,r);});table(c,"continuity_definitions",s.continuity_definitions,[](auto& v,auto& r){return continuity_definition(v,r);});}
 if(schema==4)table(c,"supervision_execution_quotes",s.supervision_execution_quotes,[](auto& v,auto& r){return execution_quote(v,r);});return c.status();
}
}
Status visit_governance_programs(CodecVisitor& c,GovernanceProgramState& s){return state_fields(c,s);}
Status emit_governance_programs_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return state_fields(c,v);});}
Outcome<BoundaryValue> encode_governance_programs(const GovernanceProgramState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_programs_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramState> decode_governance_programs(const BoundaryValue& value){GovernanceProgramState out;ActorFoundationJsonVisitor reader(value);auto status=state_fields(reader,out);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}
// 所选目录沿同一原字段表借读发射；外层shape不构造目录值树。
Status emit_governance_program_catalog_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramCatalog& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return catalog_fields(c,v);});}
Outcome<BoundaryValue> encode_governance_program_catalog(const GovernanceProgramCatalog& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_program_catalog_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog(const BoundaryValue& value,const CatalogIdentity& identity){GovernanceProgramCatalog out{identity};ActorFoundationJsonVisitor reader(value);auto status=catalog_fields(reader,out);if(status.ok)status=reader.finish();if(status.ok)status=validate_governance_program_catalog(out);return {status.ok,std::move(out),status.error};}
// /2保留原语义检查与显式schema门，不凭同形目录启用新选择。
Status emit_governance_program_catalog2_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramCatalog& s){auto status=validate_governance_program_catalog(s);if(!status.ok)return status;return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return catalog_fields(c,v,2);});}
Outcome<BoundaryValue> encode_governance_program_catalog2(const GovernanceProgramCatalog& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_program_catalog2_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog2(const BoundaryValue& value,const CatalogIdentity& identity,const CommonCatalog& shared){GovernanceProgramCatalog out{identity};ActorFoundationJsonVisitor reader(value);auto status=catalog_fields(reader,out,2);if(status.ok)status=reader.finish();if(status.ok)status=validate_governance_program_catalog(out);if(status.ok)status=validate_governance_executor_qualification_skills(out,shared);return {status.ok,std::move(out),status.error};}

// 同一字段表的显式 /3 出口；旧 /1 /2 不接收或丢弃新增 H。
Status visit_governance_program_catalog3(CodecVisitor& c,GovernanceProgramCatalog& s){return catalog_fields(c,s,3);}
Status emit_governance_program_catalog3_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramCatalog& s){auto status=validate_governance_program_catalog(s);if(!status.ok)return status;return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return catalog_fields(c,v,3);});}
Outcome<BoundaryValue> encode_governance_program_catalog3(const GovernanceProgramCatalog& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_program_catalog3_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog3(const BoundaryValue& value,const CatalogIdentity& identity,const CommonCatalog& shared){GovernanceProgramCatalog out{identity};ActorFoundationJsonVisitor reader(value);auto status=catalog_fields(reader,out,3);if(status.ok)status=reader.finish();if(status.ok)status=validate_governance_program_catalog(out);if(status.ok)status=validate_governance_executor_qualification_skills(out,shared);return {status.ok,std::move(out),status.error};}

// 显式state/2与catalog/4读写、checkpoint、投影共用上列唯一字段表。
Status visit_governance_programs2(CodecVisitor& c,GovernanceProgramState& s){return state_fields(c,s,2);}
Status emit_governance_programs2_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return state_fields(c,v,2);});}
Outcome<BoundaryValue> encode_governance_programs2(const GovernanceProgramState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_programs2_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramState> decode_governance_programs2(const BoundaryValue& value){GovernanceProgramState out;ActorFoundationJsonVisitor reader(value);auto status=state_fields(reader,out,2);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}
Status visit_governance_program_catalog4(CodecVisitor& c,GovernanceProgramCatalog& s){return catalog_fields(c,s,4);}
Status emit_governance_program_catalog4_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramCatalog& s){auto status=validate_governance_program_catalog(s);if(!status.ok)return status;return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return catalog_fields(c,v,4);});}
Outcome<BoundaryValue> encode_governance_program_catalog4(const GovernanceProgramCatalog& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_program_catalog4_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog4(const BoundaryValue& value,const CatalogIdentity& identity,const CommonCatalog& shared){GovernanceProgramCatalog out{identity};ActorFoundationJsonVisitor reader(value);auto status=catalog_fields(reader,out,4);if(status.ok)status=reader.finish();if(status.ok)status=validate_governance_program_catalog(out);if(status.ok)status=validate_governance_executor_qualification_skills(out,shared);return {status.ok,std::move(out),status.error};}

// 明确 state/3 的唯一物理出口；/1 /2 原入口仍按其原字段读写。
Status visit_governance_programs3(CodecVisitor& c,GovernanceProgramState& s){return state_fields(c,s,3);}
Status emit_governance_programs3_projection(ProjectionWriter& sink,std::string_view name,const GovernanceProgramState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return state_fields(c,v,3);});}
Outcome<BoundaryValue> encode_governance_programs3(const GovernanceProgramState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto status=emit_governance_programs3_projection(sink,"",s);if(status.ok)status=sink.finish();return {status.ok,std::move(out),status.error};}
Outcome<GovernanceProgramState> decode_governance_programs3(const BoundaryValue& value){GovernanceProgramState out;ActorFoundationJsonVisitor reader(value);auto status=state_fields(reader,out,3);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}

}
