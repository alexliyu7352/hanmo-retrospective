// Original catalog actual owner; raw payload planning delegates to the sole typed engine.
#include "catalog_copy_plan.hpp"
#include "resident/kernel/source_owned_fields/dependencies.hpp"
#include "resident/domains/politics/treaty/field_group_passage_catalog.hpp"
#include "resident/domains/politics/relations/political_relations.hpp"
#include <utility>
#include <stdexcept>
namespace hanmo::resident { namespace catalog_copy_detail {
// Select only the original lock slot; copy construction, reservation and
// actual shared deleter remain the same algorithm for every admitted type.
template<class T>CatalogIdentity& identity_slot(T& value){return value.identity;}
CatalogIdentity& identity_slot(SocialCatalog& value){return value.original_relations->identity;}
CatalogIdentity& identity_slot(OrganizationCatalog& value){return value.authority_catalog_identity;}
template<class T>const CatalogIdentity& identity_slot(const T& value){return value.identity;}
const CatalogIdentity& identity_slot(const SocialCatalog& value){return value.original_relations->identity;}
const CatalogIdentity& identity_slot(const OrganizationCatalog& value){return value.authority_catalog_identity;}
template<class T>std::shared_ptr<T> copy_locked(const T& source,const CatalogIdentity& identity){
    // 真实复制／identity swap 和 shared deleter 不变；只保同 job 生命周期。
    auto held=checkpoint_make_shared<CheckpointMemoryCharge>(0);
    std::shared_ptr<T> result;
    {
        CatalogIdentity replacement(identity);
        CheckpointAllocator<T> allocator;
        T* raw=allocator.allocate(1);
        try{::new(static_cast<void*>(raw))T(source);}catch(...){allocator.deallocate(raw,1);throw;}
        result=std::shared_ptr<T>(raw,Delete<T>{held,allocator},allocator);
        static_assert(std::is_nothrow_swappable_v<CatalogIdentity>,"catalog identity handoff must not allocate");
        using std::swap;swap(identity_slot(*result),replacement);
        // replacement now owns exactly the fresh copy of source's old identity.
    }
    return result;
}

} // catalog_copy_detail
std::shared_ptr<SocialCatalog> copy_locked_native_catalog(const SocialCatalog& source,const CatalogIdentity& identity){
    if(!source.original_relations)boundary::unsupported("OriginalSocialRelationCatalogRequired","social/catalog");
    return catalog_copy_detail::copy_locked(source,identity);
}
std::shared_ptr<PreparedCatalog> copy_locked_native_catalog(const PreparedCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
// P3 只复用原计费复制算法，不产生替代目录。
std::shared_ptr<PoliticalRelationsCatalog> copy_locked_native_catalog(
    const PoliticalRelationsCatalog &source, const CatalogIdentity &identity) {
  return catalog_copy_detail::copy_locked(source, identity);
}
std::shared_ptr<FieldBattleCatalog> copy_locked_native_catalog(const FieldBattleCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<ActorFoundationCatalog> copy_locked_native_catalog(const ActorFoundationCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<ActorSocialOccupancyInitialCatalog> copy_locked_native_catalog(const ActorSocialOccupancyInitialCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<GrowthPermanentCatalog> copy_locked_native_catalog(const GrowthPermanentCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<OrganizationCatalog> copy_locked_native_catalog(const OrganizationCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<OrganizationLifecycleCatalog> copy_locked_native_catalog(const OrganizationLifecycleCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<GovernanceProgramCatalog> copy_locked_native_catalog(const GovernanceProgramCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<DependentProvisionCatalog> copy_locked_native_catalog(const DependentProvisionCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<GovernanceWorkFeeCatalog> copy_locked_native_catalog(const GovernanceWorkFeeCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
std::shared_ptr<EconomicFacilityCatalog> copy_locked_native_catalog(const EconomicFacilityCatalog& source,const CatalogIdentity& identity){return catalog_copy_detail::copy_locked(source,identity);}
}

namespace hanmo::resident {
// 唯一 fixture 权源目录仍由原同 job shared deleter 持有。
std::shared_ptr<FieldGroupPassageCatalog> copy_locked_native_catalog(
    const FieldGroupPassageCatalog &source, const CatalogIdentity &identity) {
  return catalog_copy_detail::copy_locked(source, identity);
}
}
