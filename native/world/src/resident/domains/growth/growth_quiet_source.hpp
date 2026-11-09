// The original long-rest quiet predicate: this actor's dialogue, party, Army.
#pragma once
#include "growth_fixed_service_model.hpp"
#include "resident/domains/military/military_active_army_read.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
namespace hanmo::resident {
struct GrowthState;
struct GrowthQuietView {
 bool quiet=false;
 // Original owners only. The Social initial head is required even when empty;
 // Army contributes its actual complete formation/member/closure chain.
 std::vector<SourceRef> original_sources;
};
struct GrowthQuietHeadChange {
 SourceRef actual_source;Integer source_schema=0;WorldMoment occurred_at;
 GrowthQuietView before,after;
};
class GrowthQuietRead {
public:virtual ~GrowthQuietRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<GrowthQuietView> read_actual(const StableId& actor,const WorldMoment&)const=0;
 virtual Outcome<std::optional<GrowthQuietHeadChange>> read_next_change(const StableId& actor,const WorldMoment& from,const std::vector<SourceRef>& from_head,const WorldMoment& to,const std::vector<SourceRef>& to_head,const WorldMoment& actual_now)const=0;
 virtual Outcome<GrowthQuietView> read_at_source_head(const StableId& actor,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment& actual_now)const=0;
};
class NativeGrowthQuietRead final:public GrowthQuietRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryExactState* exact_;
 std::optional<MilitarySelectedHistoryRead> selected_;
 const NavalState* naval_;const NativeActorSocialOccupancyRead& social_;const CurrentMomentRead& clock_;
 const MilitaryFormationSourceRead* formations_;const MilitaryReturnSourceRead* returns_;
public:
 NativeGrowthQuietRead(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& e,const NavalState* n,const NativeActorSocialOccupancyRead& s,const CurrentMomentRead& clock,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr):common_(c),military_(m),exact_(&e),naval_(n),social_(s),clock_(clock),formations_(formations),returns_(returns){}
 // 同World owner持有的选型上下文；Field实际M由真实段应用clock提供。
 NativeGrowthQuietRead(const CommonStore& c,const MilitaryState& m,const MilitarySelectedHistoryRead& selected,const NavalState* n,const NativeActorSocialOccupancyRead& s,const CurrentMomentRead& clock,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr):common_(c),military_(m),exact_(selected.exact),selected_(selected),naval_(n),social_(s),clock_(clock),formations_(formations),returns_(returns){}
 bool bound_to(const CommonStore& c)const noexcept override{return &c==&common_&&social_.bound_to(c);}
 Outcome<GrowthQuietView> read_actual(const StableId&,const WorldMoment&)const override;
 Outcome<std::optional<GrowthQuietHeadChange>> read_next_change(const StableId&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment&)const override;
 Outcome<GrowthQuietView> read_at_source_head(const StableId&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment&)const override;
};
struct CharacterConditionSources;struct GrowthState;
// Captures only the original quiet predicate. Site/title input is supplied by
// its own actual owner and is never inferred from this quiet result.
Outcome<LifeQuietServiceInputs> capture_growth_long_rest_quiet(const CharacterConditionSources&,const StableId& actor,const StableId& original_place);
Status refresh_growth_long_rest_quiet_projection(const CharacterConditionSources&,const StableId& actor,LifeQuietServiceInputs&);
Status validate_growth_long_rest_quiet(const CharacterConditionSources&,const StableId& actor,const LifeQuietServiceInputs&);
bool growth_quiet_input_source_shape(const SourceRef&,Integer schema)noexcept;
Outcome<std::optional<GrowthQuietHeadChange>> read_growth_long_rest_next_quiet_change(const CharacterConditionSources&,const StableId&,const LifeQuietServiceInputs& from,const LifeQuietServiceInputs* terminal=nullptr);
Status validate_growth_long_rest_quiet_input_change(const CharacterConditionSources&,const GrowthState&,const LifeFixedServiceBoundary&,const LifeOccupationInputChange&);
Status validate_growth_long_rest_quiet_input_coverage(const CharacterConditionSources&,const GrowthState&);
Status validate_growth_long_rest_quiet_span(const CharacterConditionSources&,const StableId& actor,const LifeQuietServiceInputs&,const MomentSpan& actual_service);
}
