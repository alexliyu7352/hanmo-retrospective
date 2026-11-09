// 接敌冷读从原journey、军籍和历史玩家重建首轮，不消费战斗自称的before快照。
#pragma once
#include "naval_battle_state.hpp"
#include "naval_gunnery_skill_contract.hpp"
#include "military_roster_history.hpp"
#include "military_authority_time.hpp"
#include "military_exact_sources.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
// Same-candidate original source leaves. No whole World, Office Original or
// second Naval ledger. The owner keeps every borrowed root alive synchronously.
struct NavalOriginalSourceContext {
 const CommonStore& common;const MilitaryState& military;const MilitaryExactState& exact;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 const MilitaryAuthoritySourceContext& authority;
 const MilitaryFormationSourceRead& formations;const MilitaryReturnSourceRead& returns;
 const ExactTrainingPeriodRead* periods=nullptr;
 const MilitaryActualSourceWindow* window=nullptr;
};
// Stack-only window adapters retain the original provider and its binding.
// They never repair a missing source or retain a caller callback.
class NavalWindowedFormationSourceRead final:public MilitaryFormationSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryFormationSourceRead& formations_;const MilitaryReturnSourceRead& returns_;const MilitaryActionTimeState& actions_;const MilitaryCatalog& catalog_;const MilitaryAccess& access_;ExactArithmeticBudget budget_;const MilitaryActualSourceWindow& window_;const WorldMoment& now_;
public:
 NavalWindowedFormationSourceRead(const CommonStore& c,const MilitaryState& m,const MilitaryFormationSourceRead& formations,const MilitaryReturnSourceRead& returns,const MilitaryActionTimeState& actions,const MilitaryCatalog& catalog,const MilitaryAccess& access,ExactArithmeticBudget budget,const MilitaryActualSourceWindow& w,const WorldMoment& now):common_(c),military_(m),formations_(formations),returns_(returns),actions_(actions),catalog_(catalog),access_(access),budget_(budget),window_(w),now_(now){}
 NavalWindowedFormationSourceRead(const NavalOriginalSourceContext& x,const MilitaryActualSourceWindow& w,const WorldMoment& now):NavalWindowedFormationSourceRead(x.common,x.military,x.formations,x.returns,x.actions,x.authority.office.catalog,x.authority.office.access,x.authority.office.budget,w,now){}
 bool bound_to(const CommonState& c,const MilitaryState& m)const noexcept override{return &c==&common_.state()&&&m==&military_&&formations_.bound_to(c,m);}
 Outcome<std::optional<MilitaryFormationSourceView>> read_exact_army_formation(const StableId&)const override;
};
class NavalWindowedReturnSourceRead final:public MilitaryReturnSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryFormationSourceRead& formations_;const MilitaryReturnSourceRead& returns_;const MilitaryActionTimeState& actions_;const MilitaryCatalog& catalog_;const MilitaryAccess& access_;ExactArithmeticBudget budget_;const MilitaryActualSourceWindow& window_;const WorldMoment& now_;
public:
 NavalWindowedReturnSourceRead(const CommonStore& c,const MilitaryState& m,const MilitaryFormationSourceRead& formations,const MilitaryReturnSourceRead& returns,const MilitaryActionTimeState& actions,const MilitaryCatalog& catalog,const MilitaryAccess& access,ExactArithmeticBudget budget,const MilitaryActualSourceWindow& w,const WorldMoment& now):common_(c),military_(m),formations_(formations),returns_(returns),actions_(actions),catalog_(catalog),access_(access),budget_(budget),window_(w),now_(now){}
 NavalWindowedReturnSourceRead(const NavalOriginalSourceContext& x,const MilitaryActualSourceWindow& w,const WorldMoment& now):NavalWindowedReturnSourceRead(x.common,x.military,x.formations,x.returns,x.actions,x.authority.office.catalog,x.authority.office.access,x.authority.office.budget,w,now){}
 bool bound_to(const CommonStore& c,const MilitaryState& m)const noexcept override{return &c==&common_&&&m==&military_&&returns_.bound_to(c,m);}
 Outcome<std::optional<MilitaryCompletedReturnSource>> read_exact_army_return(const StableId&)const override;
};
Status validate_naval_original_source_context(const NavalOriginalSourceContext&,const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
// Reservation outlives the locally copied anchor; no borrowed Common header
// is retained past this synchronous frame and no uncharged success strings.
struct NavalActualPrefixScratch {
 CheckpointMemoryCharge copied_fields;CommonActualFactPrefix prefix;
 NavalActualPrefixScratch()=default;
 explicit NavalActualPrefixScratch(const CurrentCommonFactPrefix&);
 NavalActualPrefixScratch(const NavalActualPrefixScratch&)=delete;
 NavalActualPrefixScratch(NavalActualPrefixScratch&&)noexcept=default;
 NavalActualPrefixScratch& operator=(const NavalActualPrefixScratch&)=delete;
 NavalActualPrefixScratch& operator=(NavalActualPrefixScratch&&)=delete;
};
Outcome<NavalActualPrefixScratch> read_naval_original_prefix_before(const CommonStore&,const StableId&);
Outcome<std::vector<MilitaryUnitRecord>> read_naval_original_rosters_before(const NavalOriginalSourceContext&,const MilitaryCatalog&,const std::vector<StableId>&,const StableId&,const PopulationMilitaryState*,const PopulationMilitaryCatalog*);
Outcome<MilitaryArmyCommandBinding> read_naval_original_command_before(const NavalOriginalSourceContext&,const StableId& army_id,const StableId& boundary);
Outcome<OfficeRecord> read_naval_original_office_before(const NavalOriginalSourceContext&,const StableId& office_id,const StableId& boundary);
// Original casualty sources at a real ordinal cut. This returns the number of
// matching permanent changes, not current actor availability or a new ledger.
Outcome<Integer> read_naval_original_actor_changes_before(const NavalOriginalSourceContext&,const StableId& actor_id,const StableId& army_id,const StableId& boundary,bool alive_only);
// Source proof only: original prior-death validity of the retained Naval
// receipt. It cannot supply actor availability or any state-at-prefix result.
Outcome<Integer> read_naval_receipt_prior_death_source(const NavalOriginalSourceContext&,const StableId& actor_id,const StableId& receipt_id);
// Original fatal receipt uniqueness, including a later legacy/exact death;
// proof only, never a state-at-prefix result.
Outcome<Integer> read_naval_receipt_other_deaths_source(const NavalOriginalSourceContext&,const StableId& actor_id,const StableId& receipt_id);
// 适配器只能绑定同一World的Family历史玩家与原Travel纯几何正文，缺失直接拒绝。
struct NavalHistoryAccess {
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
 std::function<Outcome<TravelMotion>(const PointI&,const TerrainTravelParams&,bool,const StableId&,const StableId&)> terrain_geometry;
 const PopulationMilitaryState* population=nullptr;
 const PopulationMilitaryCatalog* population_catalog=nullptr;
 const NavalGunnerySkillRead* gunnery_skills=nullptr;
 const NavalOriginalSourceContext* original_sources=nullptr;
};
Outcome<NavalBattleRecord> naval_contact_history_source(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalAssetState& before_assets,const NavalCatalog&,const NavalContactFact&,const NavalHistoryAccess&,const NavalGunneryPreparation* =nullptr);
}
