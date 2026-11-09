// 共享候选：原辞任自持读之上薄接真实Director18，不改外部三命令的玩家边界。
#pragma once
#include "political_office_resignation_world.hpp"
#include "resident/domains/politics/office_resignations/political_office_resignation_response_validation.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_political_office_resignation_response_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Outcome<PoliticalOfficeResignationQuote> prepare_authored_office_resignation_world(const WorldBundle&,const StableId&,const AuthoredOfficeResignationCommand&);
Outcome<PoliticsEffects> commit_authored_office_resignation_world(WorldBundle&,const StableId&,const AuthoredOfficeResignationCommand&,const PoliticalOfficeResignationQuote&);
Status validate_political_office_resignation_response_world(const WorldBundle&);
}
