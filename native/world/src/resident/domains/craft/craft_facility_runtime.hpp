#pragma once
#include "craft_facility_boundary_write.hpp"
namespace hanmo::resident {
// 编译回调仅保两个函数指针；每次借原实际 World/M 的整组来源。
struct CraftFacilityReadBinding {const CraftFacilityBoundaryReadContext& original;};
struct CraftFacilityWriteBinding {
 CraftFacilityBoundaryWriteContext& original;
 EconomicFacilitySourcesState& facility_sources;
 EconomicFacilityReconciliationWriteContext& physical;
 const std::vector<FacilityHolderRegistration>& complete_holder_registry;
};
class CraftFacilityReadLease {public:virtual ~CraftFacilityReadLease()=default;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual CraftFacilityReadBinding bindings()const=0;
};
class CraftFacilityWriteLease {public:virtual ~CraftFacilityWriteLease()=default;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual CraftFacilityWriteBinding bindings()=0;
};
struct CraftFacilityWorldFactory {
 Outcome<std::shared_ptr<const CraftFacilityReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<CraftFacilityWriteLease>> (*write)(WorldBundle&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
};
CraftFacilityWorldFactory native_craft_facility_world_factory() noexcept;
enum class OriginalCraftFacilityHolder:unsigned char {craft_job,repair};
enum class OriginalEconomicFacilityProvider:unsigned char {local_economy,estate,commerce};
FacilityHolderRegistration make_craft_facility_holder_registration(
 ReservationRef::Kind,OriginalCraftFacilityHolder,CraftFacilityWorldFactory);
FacilityCapacityReconciliationRegistration make_craft_facility_capacity_registration(
 OriginalEconomicFacilityProvider,CraftFacilityWorldFactory);
FacilityClaimUntilReconciliationRegistration make_craft_facility_until_registration(
 OriginalEconomicFacilityProvider,CraftFacilityWorldFactory);
}
