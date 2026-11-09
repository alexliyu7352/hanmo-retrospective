// INJ-C2 content-only selection. This is not an effect application, episode
// creation, capability snapshot, or permission to enable Condition5.
#pragma once
#include "model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/military/military_exact_sources.hpp"
namespace hanmo::resident {
bool injury_capability_map_selected(const ActorFoundationCatalog&) noexcept;
Status validate_injury_capability_map(const ActorFoundationCatalog&);
Status validate_original_military_wound_rule(const ActorFoundationCatalog&);
// Definition/source verification is independent of episode application.
// It does not authorize unresolved recovery gates or final World admission.
Status validate_injury_capability_map_definition(const ActorFoundationCatalog&);
Status validate_original_military_wound_rule_definition(const ActorFoundationCatalog&);
bool same_original_injury_effect(const InjuryEffectItem&,const InjuryEffectItem&) noexcept;
Outcome<InjuryCapabilityMap> decode_injury_capability_map(const BoundaryValue&,const CatalogIdentity&);
// 仅冷作者入口允许省摘要并绑定真实目录锁；存档/运行decoder不变。
Outcome<InjuryCapabilityMap> decode_authored_injury_capability_map(
    const BoundaryValue &, const CatalogIdentity &);
struct SelectedMilitaryInjurySource {
 MilitaryActorConsequence original;
 const InjuryCapabilityMapEntry* selected_mapping=nullptr;
};
// Reads the real S2 consequence and returns the one already-selected locked
// content row. It neither chooses from today's hp nor invents a random result.
Outcome<SelectedMilitaryInjurySource> read_selected_military_injury_source(
 const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,
 const SourceRef&,Integer source_ordinal,const MilitaryCasualtyHistoryContext&);
struct SelectedMilitaryInjuryResultBuildView {
 SelectedInjuryResult::Kind kind;
 const ActorFoundationRuleRef& injury_rule_ref;
 const ActorFoundationRuleRef& result_choice_ref;
 const std::vector<InjuryEffectItem>* effect_items=nullptr;
};
// Actual source-selected immutable value, before episode installation. The
// first-member lease owns only this outgoing copy; target tables retain their
// own true copies. No history/head/body mutation or RNG occurs in this factory.
class ProducedSelectedMilitaryInjuryResult {
 friend Outcome<ProducedSelectedMilitaryInjuryResult> produce_selected_military_injury_result(
  const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,
  const SourceRef&,Integer,const MilitaryCasualtyHistoryContext&);
 CheckpointMemoryCharge copied_fields_;SelectedInjuryResult result_;
 explicit ProducedSelectedMilitaryInjuryResult(const SelectedMilitaryInjuryResultBuildView&);
public:
 ProducedSelectedMilitaryInjuryResult()=default;
 ProducedSelectedMilitaryInjuryResult(const ProducedSelectedMilitaryInjuryResult&)=delete;
 ProducedSelectedMilitaryInjuryResult(ProducedSelectedMilitaryInjuryResult&&)noexcept=default;
 ProducedSelectedMilitaryInjuryResult& operator=(const ProducedSelectedMilitaryInjuryResult&)=delete;
 ProducedSelectedMilitaryInjuryResult& operator=(ProducedSelectedMilitaryInjuryResult&&)=delete;
 const SelectedInjuryResult& result()const noexcept{return result_;}
};
Outcome<ProducedSelectedMilitaryInjuryResult> produce_selected_military_injury_result(
 const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,
 const SourceRef&,Integer source_ordinal,const MilitaryCasualtyHistoryContext&);
struct CharacterConditionSources;
struct OriginalInjuryEffectSource {
 const ConditionReceiptV2* receipt=nullptr;
 const InjuryEffectItem* effect=nullptr;
};
// Borrows the immutable value actually selected by a real original source.
// This never infers whether it is active/reduced/released today. Episode and
// recovery authorization remain separate original lifecycle obligations.
Outcome<OriginalInjuryEffectSource> read_original_injury_effect_source(
 const CharacterConditionSources&,const ConditionReceiptRefV2&,const StableId& effect_key);
}
