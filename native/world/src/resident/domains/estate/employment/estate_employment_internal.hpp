// 内部纯计算与精确事实边界；不得把余额、字符串前缀或当前空账当历史免责。
#pragma once
#include "estate_employment.hpp"
#include "resident/domains/family/actor_age_birth_read.hpp"
#include "../../local_economy/local_economy_internal.hpp"
#include <map>
namespace hanmo::resident::estate_employment_detail {
using local_economy_detail::add;using local_economy_detail::multiply;using local_economy_detail::ratio;
using local_economy_detail::has;using local_economy_detail::PhaseSavepoint;using local_economy_detail::CandidateStateGuard;
// 原作者及创建者保旧读口；选中Birth时必须借同候选出生原件，不接受空指针降级。
Outcome<ActorAgeRead> read_employment_actor_age(const EstateEmploymentContext&,const StableId& actor,Integer tick,const StableId& before_fact={});
struct WageTotals {Integer served=0,earned=0,paid=0,remainder=0;};
Outcome<Integer> observed_tick(const EstateEmploymentContext&);
Outcome<Integer> reservation_cap(Integer numerator,Integer denominator,Integer ticks);
Outcome<WageTotals> wage_totals(const EstateEmploymentContext&,const EstateEmploymentContract&);
Outcome<Integer> actual_reservation(const CommonStore&,const EstateEmploymentContract&);
const EstateEmploymentContract* obligation_contract(const EstateEmploymentState&,const StableId&);
Status validate_catalog(const EstateEmploymentContext&);
Outcome<EstateEmploymentTerms> freeze_terms(const EstateEmploymentContext&,const EstateEmploymentPolicy&,const StableId& worker);
bool same_terms(const EstateEmploymentTerms&,const EstateEmploymentTerms&);
bool service_performed(const EstateProductionFact&,const EstateEmploymentTerms&);
Outcome<StableId> append_fact(CommonStore&,const StableId& prefix,const StableId& kind,const StableId& domain=estate_employment_module);
Status settle_wages(CommonStore&,LocalEconomyState&,EstateEmploymentState&,const StableId& contract,Integer tick,LocalEconomyEffects&,const EstateAssetTitleRead* =nullptr);
}
