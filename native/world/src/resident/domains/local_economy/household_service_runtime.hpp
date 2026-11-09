#pragma once
#include "cohort_household_service.hpp"
#include "market_service_reconciliation.hpp"
#include "household_facility_sources.hpp"
namespace hanmo::resident {
// 工厂仅组合当前WorldBundle借用，provider/holder本体不捕获旧候选。
struct HouseholdServiceReadBinding {const HouseholdPurchaseContext& household;const MarketServiceCapacityContext& market;};
struct HouseholdServiceWriteBinding {CommonStore& common;HouseholdPurchaseState& state;ActionElapsedStore& elapsed;const HouseholdPurchaseContext& household;const MarketServiceCapacityContext& market;};
class HouseholdServiceReadLease {public:virtual ~HouseholdServiceReadLease()=default;virtual HouseholdServiceReadBinding bindings()const=0;};
class HouseholdServiceWriteLease {public:virtual ~HouseholdServiceWriteLease()=default;virtual HouseholdServiceWriteBinding bindings()=0;};
struct HouseholdServiceWorldFactory {
 Outcome<std::shared_ptr<const HouseholdServiceReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<HouseholdServiceWriteLease>> (*write)(WorldBundle&,const WorldMoment&)=nullptr;
};
HouseholdServiceWorldFactory native_household_service_world_factory() noexcept;
FacilityHolderRegistration make_household_service_holder_registration(HouseholdServiceWorldFactory);
FacilityCapacityReconciliationRegistration make_market_service_reconciliation_registration(const StableId& provider_domain,HouseholdServiceWorldFactory);
// 由S5完整economic phase合入；正常满工的until由S0唯一facility runtime派给上方真实holder。
Status provide_household_service_interval(HouseholdServiceWriteBinding&,const MomentSpan&,ExactPhaseWriteContext&);
Outcome<std::vector<ExactEventPrediction>> predict_household_failed_period_closures(const HouseholdServiceReadBinding&,const WorldMoment&,const WorldMoment&);
Outcome<ExactEventApplied> apply_household_failed_period_closure(HouseholdServiceWriteBinding&,const ExactEventPrediction&);
Status validate_household_service_runtime_sources(const HouseholdServiceReadBinding&,const WorldMoment&);
}
