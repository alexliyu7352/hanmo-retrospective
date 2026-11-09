#include "resident/kernel/catalog_section_observation.hpp"
#include "governance_work_fee_policy.hpp"
namespace hanmo::resident {
SourceRef governance_work_fee_policy_ref(const GovernanceWorkFeePolicy&p){return {"local_economy","governance_work_expense_policy",p.id};}
Status validate_governance_work_fee_catalog(const GovernanceWorkFeeCatalog&c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 70, "GovernanceWorkFeeCatalog", "validate_governance_work_fee_catalog", &c);

 if((c.version!=1&&c.version!=2)||c.identity.content_id.empty()||c.identity.content_version.empty()||c.identity.content_digest.empty()||c.identity.ruleset_id.empty())return common_failure("GovernanceWorkFeeCatalogRequired");
 for(const auto&row:c.policies.rows()){const auto&p=row.second;if(row.first!=p.id||p.id.empty()||p.rule_version.empty()||(p.work_kind!=WorkKind::patrol&&p.work_kind!=WorkKind::agronomy_supervision&&p.work_kind!=WorkKind::industry_supervision&&p.work_kind!=WorkKind::construction_supervision))return common_failure("InvalidWorkFeePolicy",row.first);auto shape=validate_governance_work_fee_values(p.fee_lines,p.cost_coverage,p.advance);if(!shape.ok)return shape;if(c.version==2&&!p.advance.eligible_fee_line_ids.empty())return common_failure("ServicePrepaymentRemoved",row.first);
 }
 return common_success();
}
Outcome<const GovernanceWorkFeePolicy*> read_governance_work_fee_policy(const GovernanceWorkFeeCatalog&c,const SourceRef&r){auto valid=validate_governance_work_fee_catalog(c);if(!valid.ok)return {false,{},valid.error};const auto*p=c.policies.find(r.id);if(!p||!same_source_ref(r,governance_work_fee_policy_ref(*p)))return {false,{},common_failure("WorkFeePolicySourceMissing",r.id).error};return {true,p,{}};}
Outcome<GovernanceWorkExpenseState> make_initial_governance_work_expenses(const CommonStore&common,const GovernanceWorkFeeCatalog&catalog,Integer selected_version){
 auto rejected=[](const Status&status)->Outcome<GovernanceWorkExpenseState>{return {false,{},status.error};};
 if(selected_version!=1&&selected_version!=2)return rejected(common_failure("UnsupportedGovernanceWorkExpenseVersion",{},ErrorKind::unsupported_capability));
 auto valid=validate_governance_work_fee_catalog(catalog);if(!valid.ok)return rejected(valid);
 const auto& metadata=common.state().metadata;
 if(catalog.identity.content_id!=metadata.content_id||catalog.identity.content_version!=metadata.content_version||catalog.identity.content_digest!=metadata.content_digest||catalog.identity.ruleset_id!=metadata.ruleset_id)return rejected(common_failure("GovernanceWorkFeeCatalogIdentityMismatch"));
 if(catalog.version!=selected_version)return rejected(common_failure("GovernanceWorkFeeSelectedVersionMismatch"));
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="local_economy"){
  const auto&kind=row.second.kind;
  if(kind=="governance_work_advance_paid"||kind=="service_advance_applied"||kind=="work_advance_refund_assessed")return rejected(common_failure("RetiredServicePrepaymentSource",row.first));
  if(kind=="governance_work_expense_terms"||kind=="governance_work_charge"||kind=="work_service_settled")return rejected(common_failure("GovernanceWorkExpenseOriginalRestoreRequired",row.first));
 }
 for(const auto&row:common.state().wallets.rows())for(const auto&reserved:row.second.reserved){const auto&ref=reserved.action_id;if(ref.kind==ReservationRef::Kind::domain_activity&&ref.domain_id=="local_economy"&&ref.activity_kind=="governance_work_expense")return rejected(common_failure("GovernanceWorkExpenseOriginalRestoreRequired",ref.id));}
 GovernanceWorkExpenseState out;out.version=selected_version;return {true,std::move(out),{}};
}

}
