// 首长仅办理亲自收到的原申请；原承诺、会话、Action和公平预算先于此有限审批。
#pragma once
#include "political_office_resignation_rules.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct PoliticalOfficeResignationResponseKnown {
 WorldIdentity identity;StableId observer_actor_id,intent_id,request_fact_id,own_answer_fact_id;
 Integer request_tick=0,response_until=0;
};
class PoliticalOfficeResignationResponseRead {
public:
 virtual ~PoliticalOfficeResignationResponseRead()=default;
 virtual Outcome<PoliticalOfficeResignationResponseKnown> known_response(const StableId&,const PoliticalOfficeResignationResponseIntent&)const=0;
 virtual Outcome<std::shared_ptr<const CommandPayload>> preview_response_local(const StableId&,const PoliticalOfficeResignationResponseIntent&,const PoliticalOfficeResignationResponseKnown&)const=0;
};
struct PoliticalOfficeResignationResponseDirectorContext {
 const CommonStore& common;const TravelCatalog& travel;const PoliticalSuccessionCatalog& heads;const PoliticalOfficeResignationCatalog& resignations;
 const PoliticalOfficeResignationResponseRead& read;StableId selected_policy_id;
};
Outcome<std::optional<DecisionProposal>> propose_director_office_resignation_response(const PoliticalOfficeResignationResponseDirectorContext&,const KnownSubjectView&,const DirectorPolicy&,const PoliticalOfficeResignationResponseIntent&,const DecisionProposal&,Integer candidate_limit);
// 纯有限知识只沿该本人收到的原request及亲自答复，不以远方今日职位或撤回推导新消息。
Outcome<PoliticalOfficeResignationResponseKnown> read_political_office_resignation_response_known(const PoliticalOfficeResignationContext&,const StableId&,const PoliticalOfficeResignationResponseIntent&);
Outcome<AuthoredOfficeResignationCommand> preview_political_office_resignation_response_local(const PoliticalOfficeResignationContext&,const StableId&,const PoliticalOfficeResignationResponseIntent&,const PoliticalOfficeResignationResponseKnown&);
}
