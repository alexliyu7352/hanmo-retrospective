// Original native catalog identity-lock copies. No wire or admission policy.
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct FieldBattleCatalog;
struct GovernanceWorkFeeCatalog;struct EconomicFacilityCatalog;struct OrganizationCatalog;struct OrganizationLifecycleCatalog;struct PreparedCatalog;struct SocialCatalog;struct ActorFoundationCatalog;struct ActorSocialOccupancyInitialCatalog;struct GrowthPermanentCatalog;struct CatalogIdentity;
// This is a real shared_ptr deleter, not an alias or a caller-supplied paid flag.
// 原 job 生命周期由 shared ownership 持有；不再保复制字段的内存账。
namespace catalog_copy_detail {
template<class T>struct Delete {
    std::shared_ptr<const CheckpointMemoryCharge> memory;
    CheckpointAllocator<T> allocator;
    void operator()(T* value)noexcept{if(value){value->~T();allocator.deallocate(value,1);}}
};
template<class T>const Delete<std::remove_const_t<T>>* owner(const std::shared_ptr<T>& value)noexcept{
    return std::get_deleter<Delete<std::remove_const_t<T>>>(value);
}
}
// The original shared pointees stay shared. Only the explicitly declared copied values
// and the replacement identity are planned; actual node/control allocations use
// the same job allocator. The returned shared owner keeps ordinary-field fees.
// Social requires the actual original_relations author projection; the lock
// replaces its existing nested identity and never creates an absent projection.
std::shared_ptr<SocialCatalog> copy_locked_native_catalog(const SocialCatalog&,const CatalogIdentity&);
std::shared_ptr<PreparedCatalog> copy_locked_native_catalog(const PreparedCatalog&,const CatalogIdentity&);
std::shared_ptr<FieldBattleCatalog> copy_locked_native_catalog(const FieldBattleCatalog&,const CatalogIdentity&);
std::shared_ptr<ActorFoundationCatalog> copy_locked_native_catalog(const ActorFoundationCatalog&,const CatalogIdentity&);
std::shared_ptr<ActorSocialOccupancyInitialCatalog> copy_locked_native_catalog(const ActorSocialOccupancyInitialCatalog&,const CatalogIdentity&);
std::shared_ptr<GrowthPermanentCatalog> copy_locked_native_catalog(const GrowthPermanentCatalog&,const CatalogIdentity&);
std::shared_ptr<OrganizationCatalog> copy_locked_native_catalog(const OrganizationCatalog&,const CatalogIdentity&);
std::shared_ptr<OrganizationLifecycleCatalog> copy_locked_native_catalog(const OrganizationLifecycleCatalog&,const CatalogIdentity&);
struct FieldGroupPassageCatalog;
std::shared_ptr<FieldGroupPassageCatalog> copy_locked_native_catalog(
    const FieldGroupPassageCatalog &, const CatalogIdentity &);
struct PoliticalRelationsCatalog;
// P3 原目录沿同一真实 shared owner 复制和锁身份。
std::shared_ptr<PoliticalRelationsCatalog> copy_locked_native_catalog(
    const PoliticalRelationsCatalog &, const CatalogIdentity &);
struct GovernanceProgramCatalog;
std::shared_ptr<GovernanceProgramCatalog> copy_locked_native_catalog(const GovernanceProgramCatalog&,const CatalogIdentity&);
struct DependentProvisionCatalog;
std::shared_ptr<DependentProvisionCatalog> copy_locked_native_catalog(const DependentProvisionCatalog&,const CatalogIdentity&);
std::shared_ptr<GovernanceWorkFeeCatalog> copy_locked_native_catalog(const GovernanceWorkFeeCatalog&,const CatalogIdentity&);
// Explicit authored Facility compile ingress only. Never relock a physical restored catalog.
// The original full physical identity validator remains mandatory after this lock.
std::shared_ptr<EconomicFacilityCatalog> copy_locked_native_catalog(const EconomicFacilityCatalog&,const CatalogIdentity&);
}
