// 真实已收到的辞任申请才能触发首长本人答复；作者意图不能替代Office/Grant。
#pragma once
#include "political_office_resignation_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_office_resignation_responses_module="political_office_resignation_responses";
inline constexpr const char* political_office_resignation_response_template="office_resignation_response";
// 理由仅来自原任期已满/未满，不扫描私约或将未来继任当成已有人补缺。
struct PoliticalOfficeResignationResponseIntent {
 StableId id,resignation_policy_id,approver_actor_id,director_policy_id;
 Integer minimum_service_ticks=0;
};
struct PoliticalOfficeResignationResponseCatalog {Integer version=1;OrderedTable<PoliticalOfficeResignationResponseIntent> intents;};
struct PoliticalOfficeResignationResponseDecision {bool accept=false;StableId reason;Integer term_started_tick=0,required_ticks=0;};
Outcome<PoliticalOfficeResignationResponseCatalog> decode_political_office_resignation_responses(const BoundaryValue&);
Outcome<std::optional<PoliticalOfficeResignationResponseCatalog>> decode_political_office_resignation_response_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
BoundaryValue encode_political_office_resignation_responses(const PoliticalOfficeResignationResponseCatalog&);
Status validate_political_office_resignation_responses(const PoliticalOfficeResignationResponseCatalog&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalOfficeResignationCatalog&);
// 精确原Office读须已经核到请求人的这个epoch；历史用答复actual fact，当前为空。
Outcome<PoliticalOfficeResignationResponseDecision> read_political_office_resignation_response_decision(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalOfficeResignationRequest&,const PoliticalOfficeResignationResponseIntent&,Integer tick,const StableId& actual_answer_fact={});
}
