// Genuine locked authored origins only. No Foundation, site-use or carrier
// authority is inferred from Common existence or today's title/location.
#pragma once
#include "organization_runtime.hpp"
namespace hanmo::resident {
struct EconomicObligationSourcesState;
struct OrganizationAuthoredOriginContext {
 const CommonStore& common;
 const OrganizationCatalog& organization_catalog;
 const OrganizationLifecycleCatalog& lifecycle_catalog;
 const LocalEconomyCatalog& economy_catalog;
 const LocalEconomyState& economy;
 const EconomicObligationSourcesState& economic_sources;
};
// The currently complete original positive branch is an authored terminal seed
// with no external base/carrier/responsibility source. Its real catalog Office
// and Grant roots are still checked. Unsupported references fail specifically.
class NativeOrganizationAuthoredOriginRead final : public OrganizationAuthoredOriginSourceRead {
 const OrganizationAuthoredOriginContext& context_;
public:
 explicit NativeOrganizationAuthoredOriginRead(const OrganizationAuthoredOriginContext& context):context_(context){}
 Status validate_authored_origin(const OrganizationRecord&,const LifecycleSeedPolicy&,const CatalogIdentity&)const override;
};
// Fresh unpublished world only. Calls the sole original make_initial_organization
// and lifecycle origin writer, never another Office/Grant installer. Failure
// requires discarding Common, OrganizationState and OrganizationRuntime together.
Status initialize_authored_organization_runtime_in_candidate(CommonStore&,OrganizationState&,OrganizationRuntime&,const OrganizationCatalog&,const OrganizationLifecycleCatalog&,const LocalEconomyCatalog&,const LocalEconomyState&,const EconomicObligationSourcesState&);
// Existing authored Office origin identity, not today's term/holder authority.
// The original Office lifecycle validator remains required for full cold state.
Status validate_authored_organization_office_origin(const OrganizationAuthoredOriginContext&,const OrganizationRecord&,const OfficeState&);
}
