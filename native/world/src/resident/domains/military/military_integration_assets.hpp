// 军事整编的有限资产与来源接缝；人口和经济写入仍由各自owner验证。
#pragma once
#include <functional>
#include "resident/domains/military/military_refs.hpp"
#include "military_integration_asset_state.hpp"
namespace hanmo::resident {
// 接缝由中央从同一候选构造；空人口来源无需适配器，有来源则必须真实续接。
struct MilitaryIntegrationAccess {
    std::function<Status(const SiegeIntegrationProposal&)> preview_population;
    std::function<Outcome<std::vector<StableId>>(const MilitaryIntegrationFact&)> apply_population;
    // 中央逐人检查任务、合同与组织实际参与；缺少检查器不能解释为空承诺。
    std::function<Status(const std::vector<StableId>&)> preview_actor_commitments;
};
Outcome<MilitaryIntegrationQuote> preview_military_integration(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const SiegeIntegrationState&,const SiegeIntegrationProposal&,const MilitaryIntegrationAccess&);
Outcome<StableId> apply_military_integration(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const SiegeIntegrationState&,const SiegeIntegrationProposal&,const MilitaryIntegrationQuote&,const MilitaryIntegrationAccess&);
Status validate_military_integrations(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
struct SiegeIntegrationReadContext;struct MilitaryAuthorityTimeContext;struct MilitaryAuthoritySourceContext;
// Original proposal snapshot source. Legacy keeps its original proof; exact
// requires the explicitly selected same-candidate authority/Office context.
// The actual retained proposal supplies the strict predecessor boundary.
Status validate_siege_integration_appointment_source(
 const SiegeIntegrationReadContext&,const SiegeIntegrationAppointment&,
 const StableId& expected_actor,const SiegeIntegrationProposal&);
// Finite retained transfer/proposal/appointment association and actual heads.
// No whole integrations/authority callback. This does not prove that every
// ended office was still enabled at transfer: the chronological caller does.
Status validate_military_integration_appointment_source(
 const MilitaryAuthorityTimeContext&,const MilitaryIntegrationFact&);
Status validate_siege_integration_appointment_source(
 const MilitaryAuthoritySourceContext&,const SiegeIntegrationAppointment&,
 const StableId& expected_actor,const SiegeIntegrationProposal&);
Status validate_military_integration_appointment_source(
 const MilitaryAuthoritySourceContext&,const MilitaryIntegrationFact&);
// 原编组加有序交接解释指定时刻归属；返回不抹除已服役军团的历史名册。
std::vector<StableId> military_army_units_at(const MilitaryState&,const StableId& army_id,Integer tick,const StableId& before_fact_id={});
std::vector<StableId> military_army_members_at(const MilitaryState&,const StableId& army_id,Integer tick,const StableId& before_fact_id={});
StableId military_unit_organization_at(const MilitaryState&,const MilitaryCatalog&,const StableId& unit_id,Integer tick,const StableId& before_fact_id={});
const MilitaryIntegrationFact* military_army_handover(const MilitaryState&,const StableId& army_id);
} // 失败返回交由中央丢弃整个候选，不能单独发布军事或人口副本。
