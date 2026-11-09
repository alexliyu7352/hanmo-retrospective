// 围城提议与逐将命令只读/写入口；中央负责同候选、调用者身份及全局事实登记。
#pragma once
#include "siege_integration_state.hpp"
#include "military_integration_asset_state.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
// 上下文只借唯一军事实体；不复制库存/人口或持有整个世界的动态字典。
struct SiegeIntegrationReadContext {
    const CommonRead& common;const MilitaryState& military;const MilitaryCatalog& military_catalog;const MilitaryAccess& access;
    const SiegeIntegrationState& state;const SiegeIntegrationCatalog& catalog;
};
struct SiegeIntegrationWriteContext {
    CommonWrite& common;MilitaryState& military;const MilitaryCatalog& military_catalog;const MilitaryAccess& access;
    SiegeIntegrationState& state;const SiegeIntegrationCatalog& catalog;
    SiegeIntegrationReadContext read()const{return {common,military,military_catalog,access,state,catalog};}
};
// 预览不分配ID；proposal仅在提出时返回，现存决定则精确指向永久提议。
struct SiegeIntegrationQuote { StableId proposal_id,proposal_key,status;std::optional<SiegeIntegrationProposal> proposal; };
const SiegeIntegrationProposal* siege_integration_proposal(const SiegeIntegrationState&,const StableId& id);
const SiegeIntegrationAttempt* siege_integration_attempt(const SiegeIntegrationState&,const StableId& proposal_id);
const SiegeIntegrationAttempt* current_siege_integration_attempt(const SiegeIntegrationState&,const StableId& siege_id);
// 语义key由稳定typed字段生成，不包含来源ID、时刻或原字段顺序。
StableId siege_integration_proposal_key(const SiegeIntegrationProposal&);
Outcome<SiegeIntegrationProposal> project_siege_integration(const SiegeIntegrationReadContext&,const StableId& actor,const SiegeIntegrationPropose&);
Status validate_siege_integration_proposal(const SiegeIntegrationReadContext&,const SiegeIntegrationProposal&);
Status validate_siege_integration_acceptances(const SiegeIntegrationReadContext&,const SiegeIntegrationProposal&);
Outcome<SiegeIntegrationQuote> preview_siege_integration(const SiegeIntegrationReadContext&,const StableId& actor,const SiegeIntegrationCommand&);
Outcome<SiegeIntegrationEffect> apply_siege_integration(SiegeIntegrationWriteContext&,const StableId& actor,const SiegeIntegrationCommand&);
// 只读NPC裁定不是同意；执行方必须把accept/refuse重新送入apply入口，不得自己写决定。
Outcome<SiegeIntegrationNpcDecision> preview_siege_integration_npc(const SiegeIntegrationReadContext&,const StableId& proposal_id,const StableId& actor);
// NPC执行先重新取得有限政策，再调用同一actor命令；等待不写事实，理由只由内部政策生成。
Outcome<SiegeIntegrationEffect> apply_siege_integration_npc(SiegeIntegrationWriteContext&,const StableId& proposal_id,const StableId& actor);
Status validate_siege_integration_state(const SiegeIntegrationReadContext&);
Status validate_siege_integration_catalog(const SiegeIntegrationCatalog&);
// 阶段失效只结束语义来源已变的尝试；忙碌、供给波动和查询不会代签或清除历史。
Status refresh_siege_integration_attempts(const SiegeIntegrationReadContext&,SiegeIntegrationState&);
// 冷边界明确版本且拒绝多余字段；本域不更改中央域序、命令登记或场景启用。
Outcome<SiegeIntegrationState> decode_siege_integration_state(const BoundaryValue&);
BoundaryValue encode_siege_integration_state(const SiegeIntegrationState&);
Status visit_siege_integration_checkpoint(CodecVisitor&,SiegeIntegrationState&);
Outcome<SiegeIntegrationCatalog> decode_siege_integration_catalog(const BoundaryValue&);
BoundaryValue encode_siege_integration_proposal(const SiegeIntegrationProposal&);
BoundaryValue encode_siege_integration_decision(const SiegeOfficerDecision&);
BoundaryValue encode_siege_integration_actor_action(const SiegeIntegrationActorAction&);
BoundaryValue encode_siege_integration_terms(const SiegeIntegrationTerms&);
Outcome<SiegeIntegrationTerms> decode_siege_integration_terms(const BoundaryValue&);
// 交接事实复用本域快照codec，中央只拥有section登记与全局顺序。
BoundaryValue encode_military_integration_facts(const std::vector<MilitaryIntegrationFact>&);
Outcome<std::vector<MilitaryIntegrationFact>> decode_military_integration_facts(const BoundaryValue&);
Status visit_military_integration_facts(CodecVisitor&,std::vector<MilitaryIntegrationFact>&);
} // 任何失败由中央丢弃候选；已发生的围城粮耗不在此退款。
