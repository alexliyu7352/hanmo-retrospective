#include "resident/kernel/catalog_section_observation.hpp"
// 群体工资与具名工资、财政映射独立启用，三者共用唯一Need且不重复拥有欠额。
#include "cohort_wage_need_adapter.hpp"
#include "../economic_phase_clock.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 未映射的真实经济来源由其他领域处理，不借该映射扩展工资类型。
const CohortWageNeedPolicy* policy_for(const CohortWageNeedCatalog& catalog,const StableId& obligation){for(const auto& row:catalog.policies.rows())if(row.second.obligation_id==obligation)return &row.second;return nullptr;}
// 只从已有欠据/付款解析义务，不按人物名或钱包余额猜来源。
const ArrearRecord* debt_for(const LocalEconomyState& state,const StableId& id){const auto* payment=state.payments.find(id);return payment?state.arrears.find(payment->arrear_id):state.arrears.find(id);}
// 主体种类参与身份校验，cohort不能被同名Actor替换。
bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
// 稳定来源键保留每期欠据，后一期责任不会覆盖前一期。
bool matches(const Need& need,const WorldAffair& affair,const CohortWageNeedPolicy& policy,const CohortWageObservation& value){return need.source_domain=="cohort_wages"&&need.source_key=="cohort_wage:"+value.obligation_id+":"+value.arrear_id&&need.policy_id==policy.id&&need.obligation_id==value.obligation_id&&need.arrear_id==value.arrear_id&&need.measure==NeedMeasure::outstanding_obligation&&need.unit=="money"&&need.item_id.empty()&&need.target_amount==value.due_amount&&same_subject(need.beneficiary,value.beneficiary)&&same_subject(affair.principal,value.principal)&&affair.semantic_key==policy.semantic_key+":"+value.arrear_id;}
// 冷完整性按共同事实顺序建立要求，付款分表顺序不能改写历史。
Outcome<std::map<StableId,std::vector<CohortWageObservation>>> observations(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& economy,const CohortWageNeedCatalog& catalog,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const CurrentMomentRead* actual_clock){
    std::map<StableId,std::vector<CohortWageObservation>> out;
    std::vector<StableId> ids;for(const auto& row:catalog.policies.rows())ids.push_back(row.second.obligation_id);auto history=observe_cohort_wage_history(common,state,economy,ids,takeover,execution,actual_clock);if(!history.ok)return {false,{},history.error};
    for(auto& value:history.value)if(value.due_amount>0)out[value.arrear_id].push_back(std::move(value));
    for(const auto& row:state.arrears.rows())if(row.second.original_due>0&&policy_for(catalog,row.second.obligation_id)&&(!out.count(row.first)||out[row.first].front().source_fact_id!=row.first))return {false,{},common_failure("MissingCohortWageDueSource",row.first).error};
    return {true,std::move(out),{}};
}
}
// 作者闭集不接受具名工资、口粮、重映射同一义务或空语义键。
Status validate_cohort_wage_need_catalog(const CohortWageNeedCatalog& catalog,const LocalEconomyCatalog& economy){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 77, "CohortWageNeedCatalog", "validate_cohort_wage_need_catalog", &catalog);

    if(catalog.version!=1)return common_failure("UnsupportedCohortWageNeedVersion");
    if(!economy.enabled||catalog.policies.empty()||catalog.policies.size()>20000)return common_failure("InvalidCohortWageNeedPolicyCount");
    std::set<StableId> sources,semantics;
    for(const auto& row:catalog.policies.rows()){const auto& policy=row.second;const auto* obligation=economy.obligations.find(policy.obligation_id);if(row.first!=policy.id||policy.id.empty()||policy.semantic_key.empty()||!obligation||obligation->kind!=ObligationKind::wage||obligation->principal.kind!=SubjectKind::organization||obligation->principal.id!=policy.organization_id||obligation->beneficiary.kind!=SubjectKind::cohort||!economy.cohorts.find(obligation->beneficiary.id)||!sources.insert(policy.obligation_id).second||!semantics.insert(policy.semantic_key).second)return common_failure("InvalidCohortWageNeedPolicy",row.first);}
    return common_success();
}
// 未知版本/多余作者字段必须拒绝，不能隐式打开旧工资或财政模块。
Outcome<CohortWageNeedCatalog> decode_cohort_wage_need_catalog(const BoundaryValue& input){try{using namespace boundary;fields(input,{"version","policies"},{},"cohort_wage_need_policies");CohortWageNeedCatalog out;out.version=get_int(input,"version","cohort_wage_need_policies");if(out.version!=1)fail("UnsupportedCohortWageNeedVersion","cohort_wage_need_policies");for(const auto& row:object(field(input,"policies","cohort_wage_need_policies"),"policies")){fields(row.second,{"id","obligation_id","organization_id","semantic_key"},{},row.first);CohortWageNeedPolicy p;p.id=get_string(row.second,"id",row.first);p.obligation_id=get_string(row.second,"obligation_id",row.first);p.organization_id=get_string(row.second,"organization_id",row.first);p.semantic_key=get_string(row.second,"semantic_key",row.first);if(row.first!=p.id||!out.policies.insert(row.first,std::move(p)))fail("InvalidCohortWageNeedPolicy",row.first);}return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 引用同一状态，访问器自身没有独立持久账。
CohortWageNeedAccess::CohortWageNeedAccess(const LocalEconomyState& economy,const LocalEconomyCatalog& economy_catalog,const CohortWageNeedCatalog& catalog,const CurrentMomentRead* actual_clock):economy_(economy),economy_catalog_(economy_catalog),catalog_(catalog),actual_clock_(actual_clock){}
CohortWageNeedAccess::CohortWageNeedAccess(const LocalEconomyState& economy,const LocalEconomyCatalog& economy_catalog,const CohortWageNeedCatalog& catalog,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const CurrentMomentRead* actual_clock):economy_(economy),economy_catalog_(economy_catalog),catalog_(catalog),takeover_(takeover),execution_(execution),actual_clock_(actual_clock){}
// 每张正应付的唯一Need必须完整覆盖真实due/payment，已结清也不能删除来源。
Status CohortWageNeedAccess::validate_cohort_wage_need_state(const CommonStore& common,const AffairsState& affairs)const{
    auto valid=validate_cohort_wage_need_catalog(catalog_,economy_catalog_);if(!valid.ok)return valid;auto required=observations(common,economy_,economy_catalog_,catalog_,takeover_,execution_,actual_clock_);if(!required.ok)return {false,{},required.error};std::set<StableId> used;
    for(const auto& row:affairs.needs.rows()){
        const auto& need=row.second;if(need.source_domain!="cohort_wages")continue;const auto found=required.value.find(need.arrear_id);const auto* affair=affairs.affairs.find(need.affair_id);
        if(row.first!=need.id||!affair||found==required.value.end()||found->second.empty()||!used.insert(need.arrear_id).second)return common_failure("InvalidCohortWageNeed",row.first);
        const auto& first=found->second.front();const auto& last=found->second.back();const auto* policy=policy_for(catalog_,first.obligation_id);
        if(!policy||!matches(need,*affair,*policy,first)||affair->id!=need.affair_id||affair->episode!=1||affair->need_ids!=std::vector<StableId>{need.id}||affair->cause_fact_ids!=std::vector<StableId>{first.arrear_id}||affair->opened_tick!=first.fact_tick||need.created_tick!=first.fact_tick||need.last_observed_tick!=last.fact_tick||need.source_fact_id!=last.source_fact_id||need.fulfilled_amount!=last.paid_amount||!need.target_inventory_id.empty()||need.physical_amount!=0||need.deadline_tick||!need.claim_ids.empty()||need.source_revision!=static_cast<Integer>(found->second.size())||need.revision!=need.source_revision+1)return common_failure("CohortWageNeedSourceMismatch",row.first);
        std::vector<StableId> ids;for(const auto& value:found->second)ids.push_back(value.source_fact_id);if(ids!=need.receipt_ids)return common_failure("IncompleteCohortWageNeedSources",row.first);
        const auto expected=last.outstanding_amount==0?NeedStatus::fulfilled:(last.paid_amount>0?NeedStatus::partial:NeedStatus::open);if(need.status!=expected||(affair->closed_tick&&(expected!=NeedStatus::fulfilled||affair->phase!=AffairPhase::resolved)))return common_failure("InvalidCohortWageNeedSettlement",row.first);
    }
    for(const auto& row:required.value)if(!used.count(row.first))return common_failure("MissingCohortWageNeed",row.first);
    return common_success();
}
// 本步仅登记派生责任；资源不足仍由原工资周期产生部分支付收据。
Status reconcile_cohort_wage_needs_in_candidate(CommonStore& common,AffairsState& affairs,const LocalEconomyState& state,const LocalEconomyCatalog& economy,const CohortWageNeedCatalog& catalog,const std::vector<StableId>& fact_ids,const CurrentMomentRead* actual_clock){return reconcile_cohort_wage_needs_in_candidate(common,affairs,state,economy,catalog,fact_ids,nullptr,nullptr,actual_clock);}
Status reconcile_cohort_wage_needs_in_candidate(CommonStore& common,AffairsState& affairs,const LocalEconomyState& state,const LocalEconomyCatalog& economy,const CohortWageNeedCatalog& catalog,const std::vector<StableId>& fact_ids,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const CurrentMomentRead* actual_clock){
    auto actual_tick=read_economic_calendar_tick(common,actual_clock);if(!actual_tick.ok)return {false,{},actual_tick.error};
    auto valid=validate_cohort_wage_need_catalog(catalog,economy);if(!valid.ok)return valid;
    std::vector<StableId> ids;for(const auto& row:catalog.policies.rows())ids.push_back(row.second.obligation_id);auto history=observe_cohort_wage_history(common,state,economy,ids,takeover,execution,actual_clock);if(!history.ok)return {false,{},history.error};
    std::map<StableId,const CohortWageObservation*> facts;for(const auto& value:history.value)facts.emplace(value.source_fact_id,&value);
    for(const auto& id:fact_ids){
        if(!common.state().facts.find(id))return common_failure("CohortWageSourceUnknown",id);
        const auto* debt=debt_for(state,id);const auto* policy=debt?policy_for(catalog,debt->obligation_id):nullptr;if(!policy)continue;
        const auto observed=facts.find(id);if(observed==facts.end())return common_failure("CohortWageSourceUnknown",id);const auto& value=*observed->second;if(value.due_amount==0)continue;
        Need* need=nullptr;for(auto& row:affairs.needs.rows())if(row.second.source_domain=="cohort_wages"&&row.second.arrear_id==value.arrear_id){if(need)return common_failure("DuplicateCohortWageNeed",value.arrear_id);need=&row.second;}
        if(need&&std::find(need->receipt_ids.begin(),need->receipt_ids.end(),id)!=need->receipt_ids.end())continue;
        if(value.fact_tick!=actual_tick.value)return common_failure("StaleCohortWageNeedSource",id);
        if(!need){
            if(id!=value.arrear_id)return common_failure("MissingCohortWageNeedDue",id);auto affair_id=common.next_id("affair");if(!affair_id.ok)return {false,{},affair_id.error};auto need_id=common.next_id("need");if(!need_id.ok)return {false,{},need_id.error};
            WorldAffair affair;affair.id=affair_id.value;affair.semantic_key=policy->semantic_key+":"+value.arrear_id;affair.principal=value.principal;affair.opened_tick=value.fact_tick;affair.cause_fact_ids={id};affair.need_ids={need_id.value};affairs.affairs.insert(affair.id,affair);
            Need created;created.id=need_id.value;created.affair_id=affair_id.value;created.source_domain="cohort_wages";created.source_key="cohort_wage:"+value.obligation_id+":"+value.arrear_id;created.beneficiary=value.beneficiary;created.unit="money";created.measure=NeedMeasure::outstanding_obligation;created.created_tick=value.fact_tick;created.target_amount=value.due_amount;created.policy_id=policy->id;created.obligation_id=value.obligation_id;created.arrear_id=value.arrear_id;affairs.needs.insert(created.id,created);need=affairs.needs.find(created.id);
        }
        const auto* affair=affairs.affairs.find(need->affair_id);if(!affair||!matches(*need,*affair,*policy,value)||need->fulfilled_amount>value.paid_amount||need->last_observed_tick>value.fact_tick||need->revision==std::numeric_limits<Integer>::max()||need->source_revision==std::numeric_limits<Integer>::max())return common_failure("CohortWageNeedSourceMismatch",id);
        need->fulfilled_amount=value.paid_amount;need->source_fact_id=id;need->last_observed_tick=value.fact_tick;++need->source_revision;++need->revision;need->receipt_ids.push_back(id);need->status=value.outstanding_amount==0?NeedStatus::fulfilled:(value.paid_amount>0?NeedStatus::partial:NeedStatus::open);
    }
    return common_success();
}
}
