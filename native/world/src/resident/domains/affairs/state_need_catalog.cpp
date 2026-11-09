#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "state_need_catalog.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {namespace {
void unique_fields(const BoundaryValue&v,const StableId&path){const auto&rows=boundary::object(v,path);for(std::size_t i=0;i<rows.size();++i)for(std::size_t j=0;j<i;++j)if(rows[i].first==rows[j].first)boundary::fail("DuplicateStateNeedAuthorField",path+"/"+rows[i].first);}
Status policy_fields(ProjectionWriter&w,const StateNeedPolicy&p){
 w.field("id",p.id);if(p.goal_kind==0)w.text("goal_kind","care_coverage");else if(p.goal_kind==1)w.text("goal_kind","medical_resolution");else return w.reject("UnknownStateNeedGoalKind");
 w.field("owner_rule",p.owner_rule);w.field("task_template",p.task_template);w.field("claim_window",p.claim_window);
 w.optional("deadline_policy",p.deadline,[](auto&c,const auto&d){c.field("tick",d.tick);return c.field("inclusive",d.inclusive);});
 w.field("disclosure",p.disclosure);w.field("fee_reward_terms",p.fee_reward_terms);return w.field("cancellation_rule",p.cancellation_rule);
}
}
Status validate_state_need_catalog(const StateNeedCatalog&c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 13, "StateNeedCatalog", "validate_state_need_catalog", &c);

 if(c.version!=1)return common_failure("UnsupportedStateNeedCatalogVersion",{},ErrorKind::unsupported_capability);
 if(c.policies.empty())return common_failure("StateNeedPoliciesRequired",{},ErrorKind::unsupported_capability);
 for(const auto&row:c.policies.rows()){
  const auto&p=row.second;if(row.first!=p.id||p.id.empty()||p.goal_kind<0||p.goal_kind>1||p.owner_rule.empty()||p.task_template.empty()||p.claim_window<1||p.disclosure.empty()||p.fee_reward_terms.empty()||p.cancellation_rule.empty()||(p.deadline&&p.deadline->tick<0))return common_failure("InvalidStateNeedPolicy",row.first);
 }
 return common_success();
}
Outcome<StateNeedCatalog> decode_state_need_catalog(const BoundaryValue&value){try{
 using namespace boundary;const StableId path="state_need_policies";unique_fields(value,path);fields(value,{"version","policies"},{},path);StateNeedCatalog out;out.version=get_int(value,"version",path);if(out.version!=1)unsupported("UnsupportedStateNeedCatalogVersion",path);
 for(const auto&row:object(field(value,"policies",path),path+"/policies")){
  const auto&v=row.second;unique_fields(v,row.first);fields(v,{"id","goal_kind","owner_rule","task_template","claim_window","deadline_policy","disclosure","fee_reward_terms","cancellation_rule"},{},row.first);StateNeedPolicy p;
  p.id=get_string(v,"id",row.first);const auto kind=get_string(v,"goal_kind",row.first);if(kind=="care_coverage")p.goal_kind=0;else if(kind=="medical_resolution")p.goal_kind=1;else unsupported("UnknownStateNeedGoalKind",row.first);
  p.owner_rule=get_string(v,"owner_rule",row.first);p.task_template=get_string(v,"task_template",row.first);p.claim_window=get_int(v,"claim_window",row.first);
  const auto&deadline=field(v,"deadline_policy",row.first);if(!std::holds_alternative<std::monostate>(deadline.data)){unique_fields(deadline,row.first+"/deadline_policy");fields(deadline,{"tick","inclusive"},{},row.first+"/deadline_policy");p.deadline=StateNeedDeadline{get_int(deadline,"tick",row.first),get_bool(deadline,"inclusive",row.first)};}
  p.disclosure=get_string(v,"disclosure",row.first);p.fee_reward_terms=get_string(v,"fee_reward_terms",row.first);p.cancellation_rule=get_string(v,"cancellation_rule",row.first);
  if(row.first!=p.id||!out.policies.insert(row.first,std::move(p)))fail("DuplicateStateNeedPolicy",row.first);
 }
 auto valid=validate_state_need_catalog(out);return valid.ok?Outcome<StateNeedCatalog>{true,std::move(out),{}}:Outcome<StateNeedCatalog>{false,{},valid.error};
 }catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status emit_state_need_catalog(ProjectionWriter&w,std::string_view name,const StateNeedCatalog&value){
 auto valid=validate_state_need_catalog(value);if(!valid.ok)return valid;return w.object(name,[&](auto&out){out.field("version",value.version);return out.object("policies",[&](auto&rows){for(const auto&row:value.policies.rows()){auto s=rows.object(row.first,[&](auto&record){return policy_fields(record,row.second);});if(!s.ok)return s;}return rows.status();});});
}
Outcome<BoundaryValue> encode_state_need_catalog(const StateNeedCatalog&value){BoundaryValue output;BoundaryProjectionWriter writer(output);auto valid=emit_state_need_catalog(writer,"",value);if(valid.ok)valid=writer.finish();return valid.ok?Outcome<BoundaryValue>{true,std::move(output),{}}:Outcome<BoundaryValue>{false,{},valid.error};}
}
