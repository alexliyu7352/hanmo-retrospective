#include "resident/kernel/catalog_section_observation.hpp"
// 目录只支持作者明确的自愿金额上限和原主体先还本金；未知偿付政策不能用默认值代替。
#include "task_personal_advance_catalog.hpp"
#include "task_personal_advance_rules.hpp"
#include <set>
namespace hanmo::resident {
namespace {
void unique_fields(const BoundaryValue& value,const StableId& path){std::set<StableId> seen;for(const auto& field:boundary::object(value,path))if(!seen.insert(field.first).second)boundary::fail("DuplicateTaskAdvanceField",path+"/"+field.first);}
}
Outcome<TaskPersonalAdvanceCatalog> decode_task_personal_advance_catalog(const BoundaryValue& value){try{
    using namespace boundary;unique_fields(value,"task_personal_advances");fields(value,{"enabled","version","policies"},{},"task_personal_advances");
    if(!get_bool(value,"enabled","task_personal_advances"))fail("TaskAdvanceExplicitEnableRequired","task_personal_advances/enabled");
    TaskPersonalAdvanceCatalog result;result.version=get_int(value,"version","task_personal_advances");if(result.version!=1)fail("UnsupportedTaskAdvanceVersion","task_personal_advances/version");
    for(const auto& row:array(field(value,"policies","task_personal_advances"),"task_personal_advances/policies")){
        unique_fields(row,"task_personal_advances/policies");fields(row,{"id","version","task_template_id","maximum_cash","repayment_policy"},{},"task_personal_advances/policies");
        TaskPersonalAdvancePolicy p;p.id=get_string(row,"id","policy");p.version=get_int(row,"version","policy");p.template_id="procurement";p.task_template_id=get_string(row,"task_template_id","policy");p.maximum_cash=get_int(row,"maximum_cash","policy");p.repayment_policy=get_string(row,"repayment_policy","policy");
        auto valid=validate_task_personal_advance_policy(p);if(!valid.ok)return {false,{},valid.error};const auto id=p.id;if(!result.policies.insert(id,std::move(p)))fail("DuplicateTaskAdvancePolicy","task_personal_advances/policies");
    }
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_task_personal_advance_catalog(const TaskPersonalAdvanceCatalog& catalog,const TaskCatalog& tasks){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 125, "TaskPersonalAdvanceCatalog", "validate_task_personal_advance_catalog", &catalog);

    if(catalog.version!=1)return common_failure("UnsupportedTaskAdvanceVersion");std::set<StableId> templates;
    for(const auto& row:catalog.policies.rows()){
        const auto& p=row.second;auto valid=validate_task_personal_advance_policy(p);if(!valid.ok)return valid;
        const auto* source=tasks.templates.find(p.task_template_id);
        if(row.first!=p.id||!source||source->template_id!="procurement"||!templates.insert(p.task_template_id).second)return common_failure("InvalidTaskAdvanceTemplate",row.first);
    }
    return common_success();
}
// 冷恢复也核真实作者规则；改大上限或串用另一采购模板不会成为一份新授权。
Status validate_task_personal_advance_catalog_links(const TaskPersonalAdvanceCatalog& catalog,const TaskCatalog& tasks,const TaskPersonalAdvanceState& state){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 126, "TaskPersonalAdvanceCatalog", "validate_task_personal_advance_catalog_links", &catalog);

    auto valid=validate_task_personal_advance_catalog(catalog,tasks);if(!valid.ok)return valid;
    for(const auto& row:state.terms.rows()){
        const auto& p=row.second.policy;const auto* author=catalog.policies.find(p.id);
        if(!author||p.version!=author->version||p.template_id!=author->template_id||p.task_template_id!=author->task_template_id||p.maximum_cash!=author->maximum_cash||p.repayment_policy!=author->repayment_policy)return common_failure("TaskAdvancePolicySourceMismatch",row.first);
    }
    return common_success();
}
Outcome<std::optional<TaskPersonalAdvancePolicy>> task_personal_advance_policy_for_offer(const TaskPersonalAdvanceCatalog& catalog,const TaskCatalog& tasks,const TaskState& state,const StableId& id){
    auto valid=validate_task_personal_advance_catalog(catalog,tasks);if(!valid.ok)return {false,{},valid.error};const auto* offer=state.offer_instances.find(id);
    if(!offer)return {false,{},common_failure("UnknownRuntimeOffer",id).error};
    for(const auto& row:catalog.policies.rows())if(row.second.task_template_id==offer->template_id)return {true,row.second,{}};
    return {true,std::nullopt,{}};
}
}
