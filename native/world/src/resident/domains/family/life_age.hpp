// 原70岁年界一次寿终规则；声明整岁与真实出生分型，只有原PCG权威采用候选会抽样。
#pragma once
#include "family_life_exact.hpp"
#include "life_age_basis.hpp"
#include "family_birth.hpp"
#include "profile_control_eligibility.hpp"
#include "resident/kernel/original_calendar.hpp"
#include <string_view>
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct AgeCheckFact {
    StableId id,actor_id;WorldId world_id{};StableId scenario_id;LifeAgeBasisRef age_basis;
    CalendarRuleRef calendar_profile_ref;Integer game_year_ordinal=0,age_years=0;
    ActorFoundationRuleRef life_policy_ref,settings_ref;bool was_controlled_at_check=false;Integer threshold_bp=0;std::optional<Integer> draw;
    enum class Outcome{survived,died,disabled,below_age};Outcome outcome=Outcome::below_age;Integer from_tick=0,to_tick=0;
};
struct LifeAgeLastCheck {StableId actor_id;CalendarRuleRef calendar_profile_ref;Integer game_year_ordinal=0;StableId check_fact_id;};
struct LifeAgeSettingsDefinition {ActorFoundationRuleRef ref;bool controlled_natural_death_enabled=true;};
struct LifeAgeSettingsChange {StableId id,actor_id;ActorFoundationRuleRef before,after;WorldMoment effective_at;};
struct LifeAgeCatalog {ActorFoundationRuleRef policy_ref,initial_settings_ref;OrderedTable<LifeAgeSettingsDefinition> settings;CalendarRuleRef calendar_profile_ref;};
struct LifeAgeState {OrderedTable<AgeCheckFact> checks;OrderedTable<LifeAgeLastCheck> last_age_check_year;OrderedTable<LifeAgeSettingsChange> settings_changes;};
class BornCharacterSourceRead;
struct LifeAgeSources {const CommonStore& common;const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;const FamilyState& family;const FamilyBirthState& births;const ProfileControlEligibilitySources& control;const OriginalCalendarRead& calendar;const BornCharacterSourceRead* born_sources=nullptr;};
struct LifeAgeBasisSnapshot {LifeAgeBasisRef ref;Integer age_years=0;};
Outcome<LifeAgeBasisSnapshot> read_life_age_basis(const LifeAgeSources&,const StableId& actor,const WorldMoment& actual_at,const StableId& before_fact={});
// The caller has verified its original payload's integer projection. This
// evaluates only calendar age at that projection and uses the real Common
// append cut for existence. It does not assert that the event occurred at
// fractional offset zero or manufacture an exact event timestamp.
Outcome<LifeAgeBasisSnapshot> read_life_age_basis_at_original_fact(const LifeAgeSources&,const StableId& actor,Integer projected_tick,const SourceRef& original_fact,const CurrentMomentRead&);
Outcome<LifeAgeBasisSnapshot> read_life_age_basis_at_original_prefix(const LifeAgeSources&,const StableId& actor,Integer projected_tick,const CommonActualFactPrefix&,const CurrentMomentRead&);
Status validate_life_age_catalog(const LifeAgeCatalog&,const CatalogIdentity&,const OriginalCalendarRead&);
Status validate_age_check_fact(const LifeAgeSources&,const LifeAgeCatalog&,const LifeAgeState&,const AgeCheckFact&);
Status validate_life_age_state(const LifeAgeSources&,const LifeAgeCatalog&,const LifeAgeState&,const FamilyDeathState&,const WorldMoment& current);
// S0 PIN-1先核跨域真实pin；本口只给owner资格，绝不自行删Common头。
Outcome<std::vector<SourceRef>> releasable_age_check_sources(const LifeAgeState&,const FamilyDeathState&);
Status validate_age_check_owner_release(const LifeAgeState&,const FamilyDeathState&,const SourceRef&);
struct AgeCheckApplication {std::optional<StableId> check_fact_id;std::optional<NaturalAgeDeathCause4> death_cause;bool already_checked=false;};
// 由life_age真年界调用；死者直接无检查/无draw，died结果必须同候选立即交唯一生命writer。
Outcome<AgeCheckApplication> apply_life_age_check(CommonStore&,LifeAgeState&,const LifeAgeCatalog&,const LifeAgeSources&,const StableId& actor,Integer from_tick,Integer to_tick,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<StableId> change_controlled_natural_age_setting(CommonStore&,LifeAgeState&,const LifeAgeCatalog&,const ProfileControlEligibilitySources&,const StableId& actor,const ActorFoundationRuleRef& expected,const ActorFoundationRuleRef& selected,const WorldMoment& actual_at,const CurrentMomentRead* actual_clock=nullptr);
class LifeAgeDeathSourceRead final:public NaturalAgeDeathSourceRead {
public:LifeAgeDeathSourceRead(const LifeAgeSources&,const LifeAgeCatalog&,const LifeAgeState&);
    Outcome<LifeCauseView4> read_age_death_source(const NaturalAgeDeathCause4&)const override;
private:const LifeAgeSources& sources_;const LifeAgeCatalog& catalog_;const LifeAgeState& state_;
};
Status visit_life_age_checks(CodecVisitor&,LifeAgeState&);
Outcome<BoundaryValue> encode_life_age_checks(const LifeAgeState&);
Outcome<LifeAgeState> decode_life_age_checks(const BoundaryValue&);
class ProjectionWriter;Status emit_life_age_checks_projection(ProjectionWriter&,std::string_view,const LifeAgeState&);
}
