// 唯一锁内作者资格的有限解析；人物实际技能/历史由S3调用原K3，不由S6倒填。
#include "governance_program.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
namespace hanmo::resident {namespace {
Status bad(const StableId& code,const StableId& id={}){return common_failure(code,id);}
Outcome<const GovernanceExecutorQualification*> resolve(const GovernanceProgramCatalog& c,
 const GovernanceProgramSpec& spec,const std::optional<SourceRef>& ref){
 const auto* terms=spec.approved_work_terms?std::get_if<GovernanceSupervisionWorkTerms>(&*spec.approved_work_terms):nullptr;
 if((c.schema_version!=2&&c.schema_version!=3&&c.schema_version!=4)||c.specs.find(spec.spec_ref.id)!=&spec||!terms)
  return {false,nullptr,bad("GovernanceExecutorQualificationCatalog2Required",spec.spec_ref.id).error};
 if(terms->required_skill_ref.has_value()!=ref.has_value()||
    (ref&&!same_source_ref(*ref,*terms->required_skill_ref)))
  return {false,nullptr,bad("GovernanceAcceptedSkillReferenceChanged",spec.spec_ref.id).error};
 if(!ref){if(terms->kind==WorkKind::agronomy_supervision)
   return {false,nullptr,bad("AgronomyExecutorQualificationRequired",spec.spec_ref.id).error};
  if(terms->kind!=WorkKind::industry_supervision&&terms->kind!=WorkKind::construction_supervision)
   return {false,nullptr,bad("UnsupportedGovernanceSupervisionQualification",spec.spec_ref.id).error};
  return {true,nullptr,{}};
 }
 const auto* q=c.executor_qualifications.find(ref->id);
 if(ref->domain!="politics"||ref->kind!="governance_executor_qualification"||ref->id!=spec.executor_qualification_id||
    !q||q->id!=ref->id||q->id.empty()||!growth_skill_defined(q->skill_id,true)||q->minimum_level<0||q->minimum_level>4)
  return {false,nullptr,bad("GovernanceExecutorQualificationMissing",ref->id).error};
 if(terms->kind==WorkKind::agronomy_supervision&&(q->skill_id!="agriculture"||q->minimum_level<1))
  return {false,nullptr,bad("AgronomyExecutorQualificationMismatch",ref->id).error};
 return {true,q,{}};
}
}
Status validate_governance_executor_qualifications(const GovernanceProgramCatalog& c){
 if(c.schema_version==1)return (c.executor_qualifications.empty()&&c.continuity_definitions.empty())?Status{true,{},{}}:bad("UnselectedGovernanceExecutorQualifications");
 if((c.schema_version!=2&&c.schema_version!=3&&c.schema_version!=4)||c.identity.content_digest.empty())return bad("UnsupportedGovernanceProgramCatalogSchema");
 for(const auto& row:c.executor_qualifications.rows()){
  const auto& q=row.second;
  if(row.first!=q.id||q.id.empty()||!growth_skill_defined(q.skill_id,true)||q.minimum_level<0||q.minimum_level>4)
   return bad("InvalidGovernanceExecutorQualification",row.first);
 }
 for(const auto& row:c.continuity_definitions.rows())if(row.first!=row.second.id||row.first.empty()||!governance_original_continuity_id(row.second.original_grant_policy))return bad("InvalidGovernanceContinuityDefinition",row.first);
 for(const auto& row:c.specs.rows()){auto definition=read_governance_continuity_definition(c,row.second);if(!definition.ok)return {false,{},definition.error};}
 for(const auto& row:c.specs.rows())if(row.second.approved_work_terms)
  if(const auto* terms=std::get_if<GovernanceSupervisionWorkTerms>(&*row.second.approved_work_terms)){
   auto q=resolve(c,row.second,terms->required_skill_ref);if(!q.ok)return {false,{},q.error};
  }
 for(const auto& row:c.patrol_gain_rules.rows())if(!growth_skill_defined(row.second.skill_id,true))
  return bad("UnknownPatrolGainSkill",row.first);
 return {true,{},{}};
}
Status validate_governance_executor_qualification_skills(const GovernanceProgramCatalog& c,const CommonCatalog& shared){
 auto valid=validate_governance_executor_qualifications(c);if(!valid.ok)return valid;
 for(const auto& row:c.executor_qualifications.rows())if(!growth_skill_defined(row.second.skill_id,shared.extended_skills))
  return bad("UnselectedGovernanceQualificationSkill",row.first);
 for(const auto& row:c.patrol_gain_rules.rows())if(!growth_skill_defined(row.second.skill_id,shared.extended_skills))
  return bad("UnselectedPatrolGainSkill",row.first);
 return {true,{},{}};
}
Outcome<const GovernanceExecutorQualification*> read_governance_executor_qualification(
 const GovernanceProgramCatalog& c,const GovernanceProgramSpec& spec,const std::optional<SourceRef>& ref){
 auto valid=validate_governance_executor_qualifications(c);if(!valid.ok)return {false,nullptr,valid.error};
 return resolve(c,spec,ref);
}
const char* governance_original_continuity_id(GovernanceOriginalContinuity value)noexcept{
 switch(value){case GovernanceOriginalContinuity::continue_under_organization:return "continue_under_organization";case GovernanceOriginalContinuity::stop_when_vacant:return "stop_when_vacant";}return nullptr;
}
Outcome<const GovernanceContinuityDefinition*> read_governance_continuity_definition(const GovernanceProgramCatalog& c,const GovernanceProgramSpec& spec){
 const auto& ref=spec.continuity_policy_ref;const auto* row=c.continuity_definitions.find(ref.id);
 if((c.schema_version!=2&&c.schema_version!=3&&c.schema_version!=4)||c.specs.find(spec.spec_ref.id)!=&spec||spec.spec_ref.catalog_digest!=c.identity.content_digest||ref.domain!="politics"||ref.kind!="governance_continuity_definition"||!row||row->id!=ref.id||!governance_original_continuity_id(row->original_grant_policy))return {false,nullptr,bad("GovernanceOriginalContinuityDefinitionRequired",ref.id).error};
 return {true,row,{}};
}
Status validate_governance_continuity_policy_binding(const GovernanceProgramCatalog& c,const GovernanceProgramSpec& spec,const StableId& actual){
 auto row=read_governance_continuity_definition(c,spec);if(!row.ok)return {false,{},row.error};
 return actual==governance_original_continuity_id(row.value->original_grant_policy)?common_success():bad("GovernanceContinuityGrantMismatch",spec.spec_ref.id);
}

}
