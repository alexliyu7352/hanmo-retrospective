// Explicit original Organization body/runtime composition. The sole current
// Grant table remains OrganizationState.current_grants.
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/organization/office_authority.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;
// 组织命令只借本次世界的真实阶段钟，不接受请求时间或缓存的半日时刻。
class NativeOrganizationAuthorityClock final : public OrganizationAuthorityClock {
 const WorldBundle& world_;
public:
 // 仅持调用方拥有的世界；此钟与使用它的同步命令共用生命期。
 explicit NativeOrganizationAuthorityClock(const WorldBundle& world):world_(world){}
 // 每次转交原实际时刻入口，保留缺少有效阶段时的原拒绝。
 Outcome<WorldMoment> read_organization_authority_now()const override;
};
struct NativeOrganizationRuntimeSelection {Integer body_version=0;bool selected=false;};
Outcome<NativeOrganizationRuntimeSelection> read_native_organization_runtime_selection(const BoundaryValue& scenario,bool named_membership);
Outcome<Integer> selected_native_organization_runtime(const CatalogIdentity&);
Outcome<Integer> selected_native_organization_service_effects(const CatalogIdentity&);
Status append_native_organization_runtime_module(const BoundaryValue&,CatalogIdentity&);
Outcome<OrganizationCatalog> decode_native_organization_catalog(const BoundaryValue& scenario,const BoundaryValue& body,const CatalogIdentity&,bool named_membership);
Status prepare_native_organization_runtime_catalog(const BoundaryValue& scenario,PreparedCatalog&);
Status validate_native_organization_runtime_catalog(const PreparedCatalog&);
Status validate_native_organization_runtime_layout(const WorldBundle&);
}
