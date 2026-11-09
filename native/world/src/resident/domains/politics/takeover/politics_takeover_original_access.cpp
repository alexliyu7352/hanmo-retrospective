#include "resident/domains/military/military_model.hpp"
// 历史全集来自原发生序与军事manifest，今日Owner或货物位置不替代原权源。
#include "politics_takeover_original_access.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_responsibility.hpp"
#include "politics_takeover_detail.hpp"
#include "resident/domains/military/occupation_codec.hpp"
#include "resident/domains/local_economy/takeover/responsibility_history.hpp"
namespace hanmo::resident {
Status PoliticalTakeoverOriginalAccess::validate_source(const MilitaryOccupationFact& source) const{
    const auto& x=context_;auto evaluation=political_takeover_evaluation_tick(x.common,x.phase);if(!evaluation.ok)return {false,{},evaluation.error};const auto* actual=takeover_detail::occupation(x.military,source.id);const auto* header=x.common.state().facts.find(source.id);
    if(!actual||!header||header->kind!="military_occupation"||header->domain!="military"||!source.assets_applied.value_or(false)||!source.asset_manifest||source.tick>evaluation.value||!compare_boundary(occupation_codec::encode(*actual),occupation_codec::encode(source)).exact())return common_failure("InvalidPoliticalOriginalOccupation",source.id);
    auto source_valid=validate_occupation_receipt(x.common,x.military,x.military_catalog,x.military_access,source.id,evaluation.value);if(!source_valid.ok)return source_valid;
    return x.phase?validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access,*x.phase):validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access);
}
Outcome<std::vector<PoliticalResponsibilityRef>> PoliticalTakeoverOriginalAccess::responsibility_keys(const MilitaryOccupationFact& source) const{
    auto valid=validate_source(source);if(!valid.ok)return {false,{},valid.error};const auto& x=context_;std::vector<PoliticalResponsibilityRef> out;
    if(x.economy_catalog.enabled){auto economic=read_economic_public_account_responsibility_keys(x.common,x.economy,x.economy_catalog,x.fiscal,x.fiscal_catalog,x.relief,source,x.phase,x.employment,x.private_shops);if(!economic.ok)return {false,{},economic.error};for(const auto& ref:economic.value)out.push_back(political_economic_responsibility_ref(ref));}
    if(x.occupation_fiscal){if(!x.fiscal_catalog)return {false,{},common_failure("OccupationFiscalDependenciesRequired").error};auto keys=read_occupation_fiscal_public_account_keys(x.common,x.economy,*x.fiscal_catalog,*x.occupation_fiscal,source,x.phase);if(!keys.ok)return {false,{},keys.error};for(const auto& ref:keys.value)out.push_back(political_economic_responsibility_ref(ref));}
    auto tasks=read_task_public_account_responsibility_keys(x.common,x.tasks,x.task_catalog,x.governance,x.envoy,source,x.phase);if(!tasks.ok)return {false,{},tasks.error};for(const auto& id:tasks.value)out.push_back({PoliticalResponsibilityKind::task_contract,id,{},{}});
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return takeover_detail::key(a)<takeover_detail::key(b);});for(std::size_t i=0;i<out.size();++i)if(!takeover_detail::valid_ref(out[i])||(i&&takeover_detail::key(out[i-1])==takeover_detail::key(out[i])))return {false,{},common_failure("DuplicatePoliticalOriginalResponsibility").error};
    return {true,std::move(out),{}};
}
Outcome<PoliticalOriginalResponsibility> PoliticalTakeoverOriginalAccess::original_responsibility(const MilitaryOccupationFact& source,const PoliticalResponsibilityRef& ref) const{
    auto valid=validate_source(source);if(!valid.ok)return {false,{},valid.error};const auto& x=context_;
    if(!takeover_detail::valid_ref(ref))return {false,{},common_failure("InvalidPoliticalResponsibilityReference").error};
    if(ref.kind==PoliticalResponsibilityKind::task_contract){auto task=read_task_public_account_responsibility_history(x.common,x.tasks,x.task_catalog,source,ref.responsibility_id,x.phase);if(!task.ok)return {false,{},task.error};return political_original_task_source(task.value,*source.asset_manifest);}
    if(ref.kind==PoliticalResponsibilityKind::fiscal_assessment&&x.occupation_fiscal&&x.occupation_fiscal->assessments.find(ref.responsibility_id)){if(!x.fiscal_catalog)return {false,{},common_failure("OccupationFiscalDependenciesRequired").error};auto read=read_occupation_fiscal_public_account_history(x.common,x.economy,*x.fiscal_catalog,*x.occupation_fiscal,source,{EconomicResponsibilityKind::fiscal_assessment,ref.responsibility_id,{}},x.phase);if(!read.ok)return {false,{},read.error};return political_original_economic_source(read.value,*source.asset_manifest);}
    if(!x.economy_catalog.enabled)return {false,{},common_failure("EconomicResponsibilityDisabled",ref.responsibility_id).error};
    auto economic_ref=economic_political_responsibility_ref(ref);if(!economic_ref.ok)return {false,{},economic_ref.error};auto economic=read_economic_public_account_responsibility_history(x.common,x.economy,x.economy_catalog,x.fiscal,x.fiscal_catalog,x.relief,source,economic_ref.value,x.phase);if(!economic.ok)return {false,{},economic.error};return political_original_economic_source(economic.value,*source.asset_manifest);
}
}
