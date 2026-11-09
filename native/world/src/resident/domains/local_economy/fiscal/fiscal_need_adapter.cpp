#include "resident/kernel/catalog_section_observation.hpp"
// 财政欠款需求是原账的派生责任；本文件不付款、不新增税债或任务资金。
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
#include "occupation_fiscal_debt_source.hpp"
#include "../economic_phase_clock.hpp"
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
using SourceKey=std::tuple<StableId,StableId,FiscalDebtUnit>;
using Observations=std::map<StableId,std::vector<FiscalDebtObservation>>;
// 税源及单位精确匹配作者选项；不能把同名义务或另一辖区的债务合并。
const FiscalNeedPolicy* policy_for(const FiscalNeedCatalog& catalog,const FiscalSettlementState& fiscal,const FiscalDebtObservation& observation,const OccupationFiscalSettlementState* occupation){
    const auto* period=fiscal.periods.find(observation.period_id);const auto* current=occupation?occupation->periods.find(observation.period_id):nullptr;if((period!=nullptr)==(current!=nullptr))return nullptr;
    const auto& district=period?period->district_id:current->district_id;
    for(const auto& row:catalog.policies.rows())if(row.second.district_id==district&&row.second.source_id==observation.source_id&&row.second.unit==observation.unit)return &row.second;
    return nullptr;
}
// 主体类型是来源的一部分；cohort/organization不能因为字符串相等被当成同一人。
bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
// 相同钱粮欠据的派生字段必须逐项相同；金额只从观察来源提取。
bool matches(const Need& need,const WorldAffair& affair,const FiscalNeedPolicy& policy,const FiscalDebtObservation& observation){
    return need.source_domain=="fiscal_settlement"&&need.source_key=="fiscal_obligation:"+observation.obligation_id+":"+observation.arrear_id&&need.measure==NeedMeasure::outstanding_obligation&&need.policy_id==policy.id&&need.obligation_id==observation.obligation_id&&need.arrear_id==observation.arrear_id&&need.target_amount==observation.due_amount&&same_subject(need.beneficiary,observation.beneficiary)&&need.unit==(observation.unit==FiscalDebtUnit::cash?"money":"item")&&need.item_id==observation.item_id&&same_subject(affair.principal,observation.taxpayer)&&affair.semantic_key==policy.semantic_key+":"+observation.arrear_id;
}
// 只按明确新事实种类路由；记录缺失仍由新reader拒绝，未知或旧种类保留原严格入口。
Outcome<std::vector<FiscalDebtObservation>> observe(const CommonStore& common,const LocalEconomyState& economy,const FiscalSettlementState& fiscal,const FiscalSettlementCatalog& taxes,const StableId& id,const OccupationFiscalSettlementState* occupation,const CurrentMomentRead* actual_clock){
 const auto* fact=common.state().facts.find(id);if(occupation&&fact&&fact->domain=="local_economy"&&(fact->kind==occupation_fiscal_period_kind||fact->kind==occupation_fiscal_suspension_kind||fact->kind==occupation_fiscal_assessment_kind||fact->kind==occupation_fiscal_payment_kind))return observe_occupation_fiscal_debt_fact(common,economy,*occupation,taxes,id,actual_clock);
 return observe_fiscal_debt_fact(common,economy,fiscal,taxes,id,actual_clock);
}
// 完整冷验从原assessment和payment收据构建必需集合，不能只认可存档声称的子集。
Outcome<Observations> observations(const CommonStore& common,const LocalEconomyState& economy,const FiscalSettlementState& fiscal,const FiscalSettlementCatalog& taxes,const FiscalNeedCatalog& catalog,const OccupationFiscalSettlementState* occupation,const CurrentMomentRead* actual_clock){
    Observations result;
    auto append=[&](const StableId& id)->Status{auto values=observe(common,economy,fiscal,taxes,id,occupation,actual_clock);if(!values.ok)return {false,{},values.error};for(auto& value:values.value)if(value.due_amount>0&&policy_for(catalog,fiscal,value,occupation))result[value.arrear_id].push_back(std::move(value));return common_success();};
    for(const auto& row:fiscal.assessments.rows()){auto checked=append(row.first);if(!checked.ok)return {false,{},checked.error};}
    for(const auto& row:fiscal.payments.rows()){auto checked=append(row.first);if(!checked.ok)return {false,{},checked.error};}
    if(occupation){for(const auto& row:occupation->assessments.rows()){auto checked=append(row.first);if(!checked.ok)return {false,{},checked.error};}for(const auto& row:occupation->payments.rows()){auto checked=append(row.first);if(!checked.ok)return {false,{},checked.error};}}
    return {true,std::move(result),{}};
}
}
// 作者只映射实际存在的税源单位，不授予支出、征收或家庭控制权。
Status validate_fiscal_need_catalog(const FiscalNeedCatalog& catalog,const FiscalSettlementCatalog& taxes){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 69, "FiscalNeedCatalog", "validate_fiscal_need_catalog", &catalog);

    if(catalog.version!=1||taxes.version!=1)return common_failure("UnsupportedFiscalNeedVersion");
    if(catalog.policies.empty()||catalog.policies.size()>20000)return common_failure("InvalidFiscalNeedPolicyCount");
    std::set<SourceKey> sources;std::set<StableId> semantics;
    for(const auto& row:catalog.policies.rows()){
        const auto& p=row.second;const auto* district=taxes.districts.find(p.district_id);const auto* source=district?district->sources.find(p.source_id):nullptr;
        if(row.first!=p.id||p.id.empty()||p.semantic_key.empty()||!source||(p.unit!=FiscalDebtUnit::cash&&p.unit!=FiscalDebtUnit::grain)||!sources.emplace(p.district_id,p.source_id,p.unit).second||!semantics.insert(p.semantic_key).second)return common_failure("InvalidFiscalNeedPolicy",row.first);
    }
    return common_success();
}
// 冷作者字段严格闭集；未知单位不能落入默认钱款分支。
Outcome<FiscalNeedCatalog> decode_fiscal_need_catalog(const BoundaryValue& input){try{
    using namespace boundary;fields(input,{"version","policies"},{},"fiscal_need_policies");FiscalNeedCatalog catalog;catalog.version=get_int(input,"version","fiscal_need_policies");if(catalog.version!=1)fail("UnsupportedFiscalNeedVersion","fiscal_need_policies");
    for(const auto& row:object(field(input,"policies","fiscal_need_policies"),"policies")){
        fields(row.second,{"id","district_id","source_id","semantic_key","unit"},{},row.first);FiscalNeedPolicy p;p.id=get_string(row.second,"id",row.first);p.district_id=get_string(row.second,"district_id",row.first);p.source_id=get_string(row.second,"source_id",row.first);p.semantic_key=get_string(row.second,"semantic_key",row.first);const auto unit=get_string(row.second,"unit",row.first);if(unit=="cash")p.unit=FiscalDebtUnit::cash;else if(unit=="grain")p.unit=FiscalDebtUnit::grain;else fail("UnknownFiscalNeedUnit",row.first);if(row.first!=p.id||!catalog.policies.insert(row.first,std::move(p)))fail("InvalidFiscalNeedPolicy",row.first);
    }
    return {true,std::move(catalog),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 读访问器仅持不可变引用，不制造财政状态副本或另一份可花余额。
FiscalNeedAccess::FiscalNeedAccess(const LocalEconomyState& economy,const FiscalSettlementState& fiscal,const FiscalSettlementCatalog& taxes,const FiscalNeedCatalog& catalog,const CurrentMomentRead* actual_clock):economy_(economy),fiscal_(fiscal),taxes_(taxes),catalog_(catalog),actual_clock_(actual_clock){}
FiscalNeedAccess::FiscalNeedAccess(const LocalEconomyState& economy,const FiscalSettlementState& fiscal,const FiscalSettlementCatalog& taxes,const FiscalNeedCatalog& catalog,const OccupationFiscalSettlementState& occupation,const CurrentMomentRead* actual_clock):FiscalNeedAccess(economy,fiscal,taxes,catalog,actual_clock){occupation_=&occupation;}
// 真实原账要求每种正应付恰有一条Need，全部付款按原顺序保留，丢记录不能靠恢复补全。
Status FiscalNeedAccess::validate_fiscal_need_state(const CommonStore& common,const AffairsState& affairs)const{
    auto valid=validate_fiscal_need_catalog(catalog_,taxes_);if(!valid.ok)return valid;auto required=observations(common,economy_,fiscal_,taxes_,catalog_,occupation_,actual_clock_);if(!required.ok)return {false,{},required.error};std::set<StableId> used;
    for(const auto& row:affairs.needs.rows()){
        const auto& need=row.second;if(need.source_domain!="fiscal_settlement")continue;const auto found=required.value.find(need.arrear_id);const auto* affair=affairs.affairs.find(need.affair_id);
        if(row.first!=need.id||!affair||found==required.value.end()||found->second.empty()||!used.insert(need.arrear_id).second)return common_failure("InvalidFiscalNeed",row.first);
        const auto& first=found->second.front();const auto& last=found->second.back();const auto* policy=policy_for(catalog_,fiscal_,first,occupation_);
        if(!policy||!matches(need,*affair,*policy,first)||affair->id!=need.affair_id||affair->episode!=1||affair->need_ids!=std::vector<StableId>{need.id}||affair->cause_fact_ids!=std::vector<StableId>{first.assessment_id}||affair->opened_tick!=first.fact_tick||need.created_tick!=first.fact_tick||need.last_observed_tick!=last.fact_tick||need.source_fact_id!=last.source_fact_id||need.fulfilled_amount!=last.paid_amount||!need.target_inventory_id.empty()||need.physical_amount!=0||need.deadline_tick||!need.claim_ids.empty()||need.source_revision!=static_cast<Integer>(found->second.size())||need.revision!=need.source_revision+1)return common_failure("FiscalNeedSourceMismatch",row.first);
        std::vector<StableId> ids;for(const auto& source:found->second)ids.push_back(source.source_fact_id);if(ids!=need.receipt_ids)return common_failure("IncompleteFiscalNeedSources",row.first);
        const auto expected=last.outstanding_amount==0?NeedStatus::fulfilled:(last.paid_amount>0?NeedStatus::partial:NeedStatus::open);if(need.status!=expected||(affair->closed_tick&&(expected!=NeedStatus::fulfilled||affair->phase!=AffairPhase::resolved)))return common_failure("InvalidFiscalNeedSettlement",row.first);
    }
    for(const auto& row:required.value)if(!used.count(row.first))return common_failure("MissingFiscalNeed",row.first);
    return common_success();
}
// 只处理实际本步事实；已记录的重放无效应，未知旧付款不能绕过原评税建立需求。
static Status reconcile(CommonStore& common,AffairsState& affairs,const LocalEconomyState& economy,const FiscalSettlementState& fiscal,const FiscalSettlementCatalog& taxes,const FiscalNeedCatalog& catalog,const std::vector<StableId>& fact_ids,const OccupationFiscalSettlementState* occupation,const CurrentMomentRead* actual_clock){
    auto actual_tick=read_economic_calendar_tick(common,actual_clock);if(!actual_tick.ok)return {false,{},actual_tick.error};
    auto valid=validate_fiscal_need_catalog(catalog,taxes);if(!valid.ok)return valid;
    for(const auto& id:fact_ids){
        auto values=observe(common,economy,fiscal,taxes,id,occupation,actual_clock);if(!values.ok)return {false,{},values.error};
        for(const auto& value:values.value){
            const auto* policy=policy_for(catalog,fiscal,value,occupation);if(!policy||value.due_amount==0)continue;Need* need=nullptr;
            for(auto& row:affairs.needs.rows())if(row.second.source_domain=="fiscal_settlement"&&row.second.arrear_id==value.arrear_id){if(need)return common_failure("DuplicateFiscalNeed",value.arrear_id);need=&row.second;}
            if(need&&std::find(need->receipt_ids.begin(),need->receipt_ids.end(),id)!=need->receipt_ids.end())continue;
            if(value.fact_tick!=actual_tick.value)return common_failure("StaleFiscalNeedSource",id);
            if(!need){
                if(id!=value.assessment_id)return common_failure("MissingFiscalNeedAssessment",id);auto affair_id=common.next_id("affair");if(!affair_id.ok)return {false,{},affair_id.error};auto need_id=common.next_id("need");if(!need_id.ok)return {false,{},need_id.error};
                WorldAffair affair;affair.id=affair_id.value;affair.semantic_key=policy->semantic_key+":"+value.arrear_id;affair.principal=value.taxpayer;affair.opened_tick=value.fact_tick;affair.cause_fact_ids={id};affair.need_ids={need_id.value};affairs.affairs.insert(affair.id,affair);
                Need created;created.id=need_id.value;created.affair_id=affair_id.value;created.source_domain="fiscal_settlement";created.source_key="fiscal_obligation:"+value.obligation_id+":"+value.arrear_id;created.beneficiary=value.beneficiary;created.item_id=value.item_id;created.unit=value.unit==FiscalDebtUnit::cash?"money":"item";created.measure=NeedMeasure::outstanding_obligation;created.created_tick=value.fact_tick;created.target_amount=value.due_amount;created.policy_id=policy->id;created.obligation_id=value.obligation_id;created.arrear_id=value.arrear_id;affairs.needs.insert(created.id,created);need=affairs.needs.find(created.id);
            }
            const auto* affair=affairs.affairs.find(need->affair_id);if(!affair||!matches(*need,*affair,*policy,value)||need->fulfilled_amount>value.paid_amount||need->last_observed_tick>value.fact_tick||need->revision==std::numeric_limits<Integer>::max()||need->source_revision==std::numeric_limits<Integer>::max())return common_failure("FiscalNeedSourceMismatch",id);
            need->fulfilled_amount=value.paid_amount;need->source_fact_id=id;need->last_observed_tick=value.fact_tick;++need->source_revision;++need->revision;need->receipt_ids.push_back(id);need->status=value.outstanding_amount==0?NeedStatus::fulfilled:(value.paid_amount>0?NeedStatus::partial:NeedStatus::open);
        }
    }
    return common_success();
}
// 两个业务入口共用唯一Need更新器，空上下文不推断新段已启用。
Status reconcile_fiscal_needs_in_candidate(CommonStore& c,AffairsState& a,const LocalEconomyState& e,const FiscalSettlementState& f,const FiscalSettlementCatalog& t,const FiscalNeedCatalog& n,const std::vector<StableId>& ids,const CurrentMomentRead* actual_clock){return reconcile(c,a,e,f,t,n,ids,nullptr,actual_clock);}
Status reconcile_fiscal_needs_in_candidate(CommonStore& c,AffairsState& a,const LocalEconomyState& e,const FiscalSettlementState& f,const FiscalSettlementCatalog& t,const FiscalNeedCatalog& n,const std::vector<StableId>& ids,const OccupationFiscalSettlementState& occupation,const CurrentMomentRead* actual_clock){return reconcile(c,a,e,f,t,n,ids,&occupation,actual_clock);}

}
