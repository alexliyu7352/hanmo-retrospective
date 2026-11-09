// 后继任期只消费原首职及已完成的正式继位/生前交接，不把临时代办或普通换任当成新提名权。
#pragma once
#include "resident/domains/politics/head_handover/political_head_handover_rules.hpp"
namespace hanmo::resident {
// 独立下一轮政策尚未注册；原两个1版业务与字节不由此纯读放宽。
inline constexpr const char* political_successor_rounds_domain="political_successor_rounds";
enum class PoliticalHeadOrigin {authored,death_accession,living_handover,successor_round,term_handover};
struct PoliticalHeadSource {
 WorldIdentity source_identity;
 StableId policy_id,organization_id,office_id,actor_id,predecessor_actor_id;
 StableId source_fact_id,office_change_fact_id,before_fact_id;
 PoliticalHeadOrigin origin=PoliticalHeadOrigin::authored;
 Integer office_epoch=0,installed_tick=0,observed_tick=0;
};
// 后任交接的纯来源适配只接受原Office已核截面，不回调任何完整领域验；未启用保持空。
using PoliticalTermHandoverHeadRead=std::function<Outcome<std::optional<PoliticalHeadSource>>(const PoliticalSuccessionPolicy&,const PoliticalSuccessionOfficeSource&)>;
// 原组织、正式继位与可选生前交接须先完成各自全验；这里只对具名任职来源交叉核验，无递归全验。
struct PoliticalHeadSourceContext {
 const CommonStore& common;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const PoliticalSuccessionCatalog& succession_catalog;const PoliticalSuccessionNominationState& nominations;const PoliticalSuccessionAccessionState& accessions;
 const PoliticalHeadHandoverCatalog* handover_catalog=nullptr;const PoliticalHeadHandoverState* handovers=nullptr;
 PoliticalTermHandoverHeadRead term_handover_head;
};
// 仅新轮真实头可读取原Office截面，仍先验完整原Office尾与两种事实先后。
Outcome<PoliticalSuccessionOfficeSource> read_political_successor_round_office_before(const PoliticalHeadSourceContext&,const StableId& succession_policy_id,Integer tick,const StableId& actual_fact);
// 真空职返回空；持有人缺正式来源则明确报错，不退回作者首任或今日Owner。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source(const PoliticalHeadSourceContext&,const StableId& succession_policy_id);
// 边界只接未来新轮的实际具名事实；调用方仍须先核本记录payload.tick与传入tick精确相等。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source_before(const PoliticalHeadSourceContext&,const StableId& succession_policy_id,Integer tick,const StableId& actual_fact);
}
