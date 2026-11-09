// Explicit original Facility/P08/Work-fee ownership. No dynamic provider is fabricated.
#pragma once
#include <string_view>
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct WorldBundle;struct FacilityClaimsState;class CommonStore;struct WorldMoment;
struct NativeEconomicFacilityVersions {Integer facilities=0,conditions=0,work_expense=0;};
Outcome<NativeEconomicFacilityVersions> selected_native_economic_facility_versions(const CatalogIdentity&);
Status append_native_economic_facility_modules(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_economic_facility_catalogs(const BoundaryValue&,PreparedCatalog&);
Status validate_native_economic_facility_catalogs(const PreparedCatalog&);
Status initialize_native_economic_facility_roots(CommonStore&,const PreparedCatalog&,InitialWorldSeed&,const FacilityClaimsState*,const WorldMoment&);
Status validate_native_economic_facility_layout(const WorldBundle&);
struct LocalEconomyState;struct EconomicFacilitySourcesState;struct ProductionFacilityConditionsState;struct GovernanceWorkExpenseState;struct FactRecord;class ProjectionWriter;
bool is_native_economic_facility_fact_kind(std::string_view);
Outcome<Integer> read_native_economic_facility_fact_schema(const CommonStore&,const PreparedCatalog&,const EconomicFacilitySourcesState*,const ProductionFacilityConditionsState*,const GovernanceWorkExpenseState*,const FactRecord&);
Status validate_native_economic_facility_headers(const CommonStore&,const PreparedCatalog&,const LocalEconomyState*,const EconomicFacilitySourcesState*,const ProductionFacilityConditionsState*,const GovernanceWorkExpenseState*);
enum class NativeEconomicFacilityFactProjection {all,conditions,work};
Status emit_native_economic_facility_fact_payloads_fields(ProjectionWriter&,const CommonStore&,const PreparedCatalog&,const LocalEconomyState*,const EconomicFacilitySourcesState*,const ProductionFacilityConditionsState*,const GovernanceWorkExpenseState*,NativeEconomicFacilityFactProjection);

}
