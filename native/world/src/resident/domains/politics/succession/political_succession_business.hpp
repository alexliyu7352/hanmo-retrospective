// 四个本人办理入口共用一份完整新账；原Office仅正式就任一项可写。
#pragma once
#include "political_succession_accession.hpp"
#include "political_succession_receipts.hpp"
namespace hanmo::resident {
struct PoliticalSuccessionContext {const PoliticalSuccessionAccessionSourceContext& sources;const PoliticalSuccessionState& state;PoliticalSuccessionResultMatch result_match;};
struct PoliticalSuccessionQuote {WorldIdentity identity;StableId actor_id;PoliticalSuccessionCommand command;std::variant<PoliticalSuccessionNominationQuote,PoliticalSuccessionAccessionQuote> details;};
struct PoliticalSuccessionEffects {StableId source_fact_id,office_change_fact_id;};
Status validate_political_succession_sources(const PoliticalSuccessionContext&);
Status validate_political_successions(const PoliticalSuccessionContext&);
Outcome<PoliticalSuccessionQuote> preview_political_succession(const PoliticalSuccessionContext&,const StableId& actor,const PoliticalSuccessionCommand&);
// 调用者保存同候选Common/Organization/本账，结果永久绑定完成后才可发布。
Outcome<PoliticalSuccessionEffects> apply_political_succession_in_candidate(CommonStore&,OrganizationState&,PoliticalSuccessionState&,const PoliticalSuccessionContext&,const StableId& actor,const PoliticalSuccessionCommand&,const PoliticalSuccessionQuote&);
}
