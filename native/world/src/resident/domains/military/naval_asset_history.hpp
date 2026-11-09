// 实船标题/船损/编队位置从作者和共同事实顺序重放；临时投影不是第二份权威资产账。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
#include "naval_repair_source_local.hpp"
namespace hanmo::resident {
// 完整战术冷史可在同一事件前读取原实船状态，不能用事实自称的 before 回填起点。
struct NavalHistoryAccess;struct NavalOriginalSourceContext;struct CommonActualFactPrefix;
using NavalHistoryObserver=std::function<Status(const StableId& kind,const StableId& source_id,const NavalAssetState& before)>;
Status validate_naval_asset_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const NavalHistoryObserver& observer={},const NavalHistoryAccess* navigation_geometry=nullptr);
struct ExactNavalAssetHistoryContext {
 const MilitaryActionTimeState& military_actions;const ActionElapsedStore& elapsed;
 ExactNavalRepairSourceContext repair_sources;
 const NavalOriginalSourceContext* original_sources=nullptr;
};
// Same original asset-event fold with exact repair source leaves. The original
// legacy signature retains its tick and Action checks. No legacy tick payload
// is manufactured for a new repair.
Status validate_naval_asset_history_exact(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const ExactNavalAssetHistoryContext&,const NavalHistoryObserver& observer={},const NavalHistoryAccess* navigation_geometry=nullptr);
// Synchronous borrowed actual predecessor, stopping before the named real
// repair start/result Common head. No caller-provided coordinate or prefix.
// The adopting owner separately validates its actual terms and service rights.
Status visit_naval_assets_before_exact_repair_source(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const ExactNavalAssetHistoryContext&,const SourceRef& actual_boundary,Integer source_schema,const NavalHistoryObserver&,const NavalHistoryAccess* navigation_geometry=nullptr);

// Same validated prefix, observing every original asset event in causal order,
// including the requested boundary before its own mutation. Used by the single
// original battle replay; it does not emit or retain a second asset history.
Status visit_naval_asset_events_through_exact_repair_boundary(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const ExactNavalAssetHistoryContext&,const SourceRef& actual_boundary,Integer source_schema,const NavalHistoryObserver&,const NavalHistoryAccess* navigation_geometry=nullptr);

// Same original source replay at the trusted current M, before an immediate
// interruption consumer has restored the stable projections. Every actual
// source is checked; this does not certify the final current asset/load state.
Status visit_naval_asset_source_history_exact(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const ExactNavalAssetHistoryContext&,const NavalHistoryObserver&,const NavalHistoryAccess* navigation_geometry=nullptr);

using NavalAssetPrefixObserver=std::function<Status(const NavalAssetState&)>;
// Actual Common cut, not a repair event. Exactly one snapshot, also for an
// empty stream or a cut after its final Naval event. The endpoint M is real.
Status visit_naval_asset_events_at_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const ExactNavalAssetHistoryContext&,const CommonActualFactPrefix&,const WorldMoment& at,const NavalHistoryObserver&,const NavalAssetPrefixObserver&,const NavalHistoryAccess* navigation_geometry=nullptr);
}
