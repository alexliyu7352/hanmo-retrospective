// PoliticsEffects精确对应本业务事实与实际原Office回执；不是任意字段更新或新签字授权。
#include "political_office_resignation_business.hpp"
namespace hanmo::resident {
Outcome<PoliticsEffects> apply_political_office_resignation_business_in_candidate(CommonStore& common,OrganizationState& organizations,PoliticalOfficeResignationState& state,const PoliticalOfficeResignationContext& x,const StableId& actor,const PoliticalOfficeResignationCommand& command,const PoliticalOfficeResignationQuote& quote){auto id=apply_political_office_resignation_in_candidate(common,organizations,state,x,actor,command,quote);if(!id.ok)return {false,{},id.error};PoliticsEffects out;out.fact_ids={id.value};out.receipt_ids=out.fact_ids;if(const auto* answer=state.answers.find(id.value);answer&&!answer->office_change_fact_id.empty()){out.fact_ids.push_back(answer->office_change_fact_id);out.receipt_ids.push_back(answer->office_change_fact_id);}return {true,std::move(out),{}};}
}
