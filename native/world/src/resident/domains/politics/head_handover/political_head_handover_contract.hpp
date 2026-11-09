// 生前首职交接是独立双方业务；原家族换控、免职或死亡继位意愿均不构成让职许可。
#pragma once
#include "resident/domains/politics/succession/political_succession_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_head_handover_domain="political_head_handovers";
inline constexpr const char* political_head_handover_offer_command="political_offer_head_handover";
inline constexpr const char* political_head_handover_answer_command="political_answer_head_handover";
inline constexpr const char* political_head_handover_withdraw_command="political_withdraw_head_handover";
// 此政策只扩原首任的生前交接出口；专项零金额Grant与原死亡继位提名Grant各自明确。
struct PoliticalHeadHandoverPolicy {StableId id,succession_policy_id,handover_grant_id;Integer response_ticks=0;};
struct PoliticalHeadHandoverCatalog {Integer version=1;OrderedTable<PoliticalHeadHandoverPolicy> policies;};
// 意愿签发时不腾空Office；候选本人接受才在同一候选内写一次原Office变更。
struct PoliticalOfferHeadHandover {ActingAuthority authority;StableId policy_id,candidate_actor_id;Integer expected_office_epoch=0;};
struct PoliticalAnswerHeadHandover {StableId offer_fact_id;bool accept=false;Integer expected_office_epoch=0;};
struct PoliticalWithdrawHeadHandover {ActingAuthority authority;StableId offer_fact_id;};
// 只接受这四种真实本业务头；payload、双方身份及实际结果绑定仍由本账完整冷验负责。
std::optional<Integer> political_head_handover_fact_sequence(const CommonStore&,const StableId&);
// 历史只读原Office在具名交接事实前的截面，不能以今天持有人或副手倒填生前签署。
Outcome<PoliticalSuccessionOfficeSource> read_political_head_handover_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalSuccessionPolicy&,Integer tick,const StableId& actual_fact);
Status validate_political_head_handover_catalog(const CommonStore&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalHeadHandoverCatalog&);
Outcome<PoliticalHeadHandoverCatalog> decode_political_head_handover_catalog(const BoundaryValue&);
BoundaryValue encode_political_head_handover_catalog(const PoliticalHeadHandoverCatalog&);
} // 未登记独立段前不可启用；原死亡继位1和其事实布局保持原语义。
