// 原首职真实死亡空缺后的本人就任；所有公权仍只由原Office/epoch提供。
#pragma once
#include "political_succession_accession_sources.hpp"
namespace hanmo::resident {
struct PoliticalSuccessionAccessionContext {const PoliticalSuccessionAccessionSourceContext& sources;const PoliticalSuccessionAccessionState& state;};
struct PoliticalSuccessionAccessionQuote {WorldIdentity identity;StableId actor_id;PoliticalAccedeSuccession command;PoliticalSuccessionAccessionBasis basis;};
Status validate_political_succession_accessions(const PoliticalSuccessionAccessionContext&);
Outcome<PoliticalSuccessionAccessionQuote> preview_political_succession_accession(const PoliticalSuccessionAccessionContext&,const StableId& actor,const PoliticalAccedeSuccession&);
// 同一World候选覆盖Common、原Organization与本就任账；任何晚失败撤回三者。
Outcome<StableId> apply_political_succession_accession_in_candidate(CommonStore&,OrganizationState&,PoliticalSuccessionAccessionState&,const PoliticalSuccessionAccessionContext&,const StableId&,const PoliticalAccedeSuccession&,const PoliticalSuccessionAccessionQuote&);
}
