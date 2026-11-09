#include "resident/kernel/catalog_section_observation.hpp"
#include "native_economic_facility_roots.hpp"
#include "world_bundle.hpp"
#include "native_economic_obligation_sources.hpp"
#include "native_task_work_catalog.hpp"
#include "original_catalog_identity.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_fee_policy.hpp"
namespace hanmo::resident {namespace {
const BoundaryValue* block(const BoundaryValue& scenario,const char* name){
 const BoundaryValue* found=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first==name){
  if(found)boundary::fail("DuplicateEconomicFacilitySelection",name);found=&row.second;
 }
 return found;
}
Integer author(const BoundaryValue& value,const char* name,const char* catalog_field,const char* profile1,const char* profile2=nullptr){
 boundary::fields(value,{"profile",catalog_field},{},name);
 if(boundary::object(value,name).size()!=2)boundary::fail("EconomicFacilitySelectionFieldMismatch",name);
 const auto& profile=boundary::get_string_ref(value,"profile",name);
 if(profile==profile1)return 1;if(profile2&&profile==profile2)return 2;
 boundary::unsupported("UnsupportedEconomicFacilitySelection",name);
}
NativeEconomicFacilityVersions author(const BoundaryValue& scenario){
 NativeEconomicFacilityVersions result;
 if(const auto* value=block(scenario,"economic_facility_capacity_sources"))result.facilities=author(*value,"economic_facility_capacity_sources","economic_facility_catalog","original_economic_facility_capacity_sources_v1");
 if(const auto* value=block(scenario,"production_facility_conditions"))result.conditions=author(*value,"production_facility_conditions","production_facility_conditions_catalog","original_production_facility_conditions_v1");
 if(const auto* value=block(scenario,"governance_work_expense"))result.work_expense=author(*value,"governance_work_expense","governance_work_expense_policies","original_governance_work_expense_v1","original_governance_work_expense_v2");
 if(bool(result.facilities)!=bool(result.conditions))boundary::fail("EconomicFacilityOriginalConditionPairRequired","economic_facility_capacity_sources");
 return result;
}
bool same_versions(const NativeEconomicFacilityVersions& a,const NativeEconomicFacilityVersions& b){return a.facilities==b.facilities&&a.conditions==b.conditions&&a.work_expense==b.work_expense;}
}
Outcome<NativeEconomicFacilityVersions> selected_native_economic_facility_versions(const CatalogIdentity& identity){
 NativeEconomicFacilityVersions result;
 for(const auto& row:identity.module_versions){
  Integer* target=row.first=="economic_facility_capacity_sources"?&result.facilities:row.first=="production_facility_conditions"?&result.conditions:row.first=="governance_work_expense"?&result.work_expense:nullptr;
  if(target){if(*target||(row.second!="1"&&(target!=&result.work_expense||row.second!="2")))return {false,{},common_failure("EconomicFacilityModuleMismatch",row.first).error};*target=row.second=="2"?2:1;}
 }
 if(bool(result.facilities)!=bool(result.conditions))return {false,{},common_failure("EconomicFacilityOriginalConditionPairRequired").error};
 return {true,result,{}};
}
Status append_native_economic_facility_modules(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    for (const auto &row : identity.module_versions)
      if (row.first == "economic_facility_capacity_sources" ||
          row.first == "production_facility_conditions" ||
          row.first == "governance_work_expense")
        return common_failure("DuplicateEconomicFacilityModule", row.first);
    const auto selected = author(scenario);
    if (selected.facilities) {
      identity.module_versions.emplace_back(
          "economic_facility_capacity_sources", "1");
      identity.module_versions.emplace_back("production_facility_conditions",
                                            "1");
    }
    if (selected.work_expense)
      identity.module_versions.emplace_back(
          "governance_work_expense", std::to_string(selected.work_expense));
    return common_success();
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
Status
validate_native_economic_facility_catalogs(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 131, "PreparedCatalog", "validate_native_economic_facility_catalogs", &catalog);

  auto selected = selected_native_economic_facility_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto &v = selected.value;
  if (bool(v.facilities) != bool(catalog.economic_facilities) ||
      bool(v.conditions) != bool(catalog.production_facility_conditions) ||
      bool(v.work_expense) != bool(catalog.governance_work_fees))
    return common_failure("EconomicFacilitySelectedCatalogMismatch");
  if (!v.facilities && !v.work_expense)
    return common_success();
  if (!catalog.common || !catalog.exact_time ||
      !exact_complete_selected(*catalog.exact_time) || !catalog.local_economy)
    return common_failure("EconomicFacilityCompleteOriginalCatalogsRequired",
                          {}, ErrorKind::unsupported_capability);
  if (v.facilities) {
    if (!catalog.local_economy->production_management || !catalog.estate)
      return common_failure("EconomicFacilityOriginalAssetCatalogsRequired", {},
                            ErrorKind::unsupported_capability);
    // 未选私铺只接受无私铺资产来源的原目录，不能造空业务目录充当已选。
    auto shops = validate_economic_facility_original_shop_catalog(
        *catalog.economic_facilities, catalog.private_shops.get());
    if (!shops.ok)
      return shops;
    auto identity = validate_economic_facility_catalog_identity(
        *catalog.economic_facilities, catalog.identity);
    if (!identity.ok)
      return identity;
  }
  if (v.work_expense) {
    if (!catalog.organization || !catalog.tasks ||
        catalog.governance_work_fees->version != v.work_expense ||
        !same_original_catalog_identity(catalog.governance_work_fees->identity,
                                        catalog.identity))
      return common_failure("GovernanceWorkExpenseOriginalCatalogPairRequired");
    auto valid =
        validate_governance_work_fee_catalog(*catalog.governance_work_fees);
    if (!valid.ok)
      return valid;
    auto work = selected_native_task_work_version(catalog.identity);
    if (!work.ok)
      return {false, {}, std::move(work.error)};
    if (work.value != 5)
      return common_failure("GovernanceWorkExpenseOriginalWorkRequired", {},
                            ErrorKind::unsupported_capability);
    if (v.work_expense == 2) {
      auto obligation =
          selected_native_economic_obligation_versions(catalog.identity);
      if (!obligation.ok)
        return {false, {}, std::move(obligation.error)};
      if (obligation.value.sources != 1)
        return common_failure(
            "GovernanceWorkExpenseOriginalObligationRootRequired", {},
            ErrorKind::unsupported_capability);
    }
  }
  return common_success();
}
Status prepare_native_economic_facility_catalogs(const BoundaryValue &scenario,
                                                 PreparedCatalog &catalog) {
  try {
    if (catalog.economic_facilities || catalog.production_facility_conditions ||
        catalog.governance_work_fees)
      return common_failure("EconomicFacilityCatalogAlreadyPrepared", {},
                            ErrorKind::internal);
    auto selected =
        selected_native_economic_facility_versions(catalog.identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (!same_versions(author(scenario), selected.value))
      return common_failure("EconomicFacilitySelectedCatalogMismatch");
    if (selected.value.facilities) {
      {
        ScopedCheckpointJsonDecode original(
            CheckpointCatalogJsonKind::economic_facilities);
        const auto &value =
            *block(scenario, "economic_facility_capacity_sources");
        auto parsed = decode_authored_economic_facility_catalog(
            boundary::field(value, "economic_facility_catalog",
                            "economic_facility_capacity_sources"),
            catalog.identity);
        if (!parsed.ok)
          return {false, {}, std::move(parsed.error)};
        catalog.economic_facilities =
            checkpoint_make_shared<const EconomicFacilityCatalog>(
                std::move(parsed.value));
      }
      {
        ScopedCheckpointJsonDecode original(
            CheckpointCatalogJsonKind::production_facility_conditions);
        const auto &value = *block(scenario, "production_facility_conditions");
        auto parsed = decode_authored_production_facility_conditions_catalog(
            boundary::field(value, "production_facility_conditions_catalog",
                            "production_facility_conditions"),
            catalog.identity, *catalog.economic_facilities);
        if (!parsed.ok)
          return {false, {}, std::move(parsed.error)};
        catalog.production_facility_conditions =
            checkpoint_make_shared<const ProductionFacilityConditionsCatalog>(
                std::move(parsed.value));
      }
    }
    if (selected.value.work_expense) {
      ScopedCheckpointJsonDecode original(
          CheckpointCatalogJsonKind::governance_work_fees);
      const auto &value = *block(scenario, "governance_work_expense");
      auto parsed = decode_governance_work_fee_catalog(
          boundary::field(value, "governance_work_expense_policies",
                          "governance_work_expense"),
          catalog.identity);
      if (!parsed.ok)
        return {false, {}, std::move(parsed.error)};
      catalog.governance_work_fees =
          checkpoint_make_shared<const GovernanceWorkFeeCatalog>(
              std::move(parsed.value));
    }
    return validate_native_economic_facility_catalogs(catalog);
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
Status initialize_native_economic_facility_roots(
    CommonStore &common, const PreparedCatalog &catalog, InitialWorldSeed &seed,
    const FacilityClaimsState *claims, const WorldMoment &now) {
  auto valid = validate_native_economic_facility_catalogs(catalog);
  if (!valid.ok)
    return valid;
  auto selected = selected_native_economic_facility_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (seed.economic_facility_capacity_sources ||
      seed.production_facility_conditions || seed.governance_work_expense)
    return common_failure("EconomicFacilityFreshRootsRequired", {},
                          ErrorKind::internal);
  if (selected.value.facilities) {
    if (!claims || seed.facility_claims.get() != claims ||
        !seed.local_economy || !seed.estate ||
        bool(catalog.private_shops) != bool(seed.private_shops))
      return common_failure("EconomicFacilityOriginalInitialRootsRequired", {},
                            ErrorKind::unsupported_capability);
    auto facilities = checkpoint_make_shared<EconomicFacilitySourcesState>();
    auto conditions =
        checkpoint_make_shared<ProductionFacilityConditionsState>();
    EconomicFacilityInstallationReadContext original{
        common,
        *catalog.economic_facilities,
        *facilities,
        *catalog.local_economy->production_management,
        *catalog.estate,
        catalog.private_shops.get(),
        now};
    valid = initialize_economic_facilities_in_candidate(
        common, *facilities, *conditions, original,
        *catalog.production_facility_conditions, *claims,
        selected.value.facilities);
    if (!valid.ok)
      return valid;
    seed.economic_facility_capacity_sources = std::move(facilities);
    seed.production_facility_conditions = std::move(conditions);
  }
  if (selected.value.work_expense) {
    if (selected.value.work_expense == 2 && !seed.economic_obligation_sources)
      return common_failure(
          "GovernanceWorkExpenseOriginalObligationRootRequired", {},
          ErrorKind::unsupported_capability);
    auto fresh = make_initial_governance_work_expenses(
        common, *catalog.governance_work_fees, selected.value.work_expense);
    if (!fresh.ok)
      return {false, {}, std::move(fresh.error)};
    seed.governance_work_expense =
        checkpoint_make_shared<const GovernanceWorkExpenseState>(
            std::move(fresh.value));
  }
  return common_success();
}
Status validate_native_economic_facility_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("EconomicFacilityCatalogRequired", {},
                          ErrorKind::internal);
  auto valid = validate_native_economic_facility_catalogs(*world.catalog);
  if (!valid.ok)
    return valid;
  auto selected =
      selected_native_economic_facility_versions(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto &v = selected.value;
  if (bool(v.facilities) != bool(world.economic_facility_capacity_sources) ||
      bool(v.conditions) != bool(world.production_facility_conditions) ||
      bool(v.work_expense) != bool(world.governance_work_expense))
    return common_failure("EconomicFacilitySelectedRootMismatch");
  if ((world.economic_facility_capacity_sources &&
       world.economic_facility_capacity_sources->schema_version !=
           v.facilities) ||
      (world.governance_work_expense &&
       world.governance_work_expense->version != v.work_expense))
    return common_failure("EconomicFacilitySelectedStateVersionMismatch");
  // Physical ownership only. Original dynamic business, holder, legal right,
  // financial approval and whole-source reverse coverage remain mandatory.
  return common_success();
}

namespace {
class CheckedEconomicHeaders final:public ProductionFacilityConditionFactHeaderSink,public GovernanceWorkOriginalFactHeaderSink {
public:Status append(const FactRecord&,Integer schema)override{return schema==1?common_success():common_failure("EconomicFacilityOriginalSchemaMismatch");}
};
}
bool is_native_economic_facility_fact_kind(std::string_view kind){return kind=="facility_condition_change"||is_governance_work_original_fact_kind(kind);}
Outcome<Integer> read_native_economic_facility_fact_schema(const CommonStore& common,const PreparedCatalog& catalog,const EconomicFacilitySourcesState* facilities,const ProductionFacilityConditionsState* conditions,const GovernanceWorkExpenseState* work,const FactRecord& head){
 auto selected=selected_native_economic_facility_versions(catalog.identity);if(!selected.ok)return {false,0,std::move(selected.error)};
 if(head.kind=="facility_condition_change"){
  if(!selected.value.conditions||!facilities||!conditions)return {false,0,common_failure("FacilityConditionOriginalSelectedOwnerRequired",head.id).error};
  return read_production_facility_condition_fact_payload_schema(common,*conditions,*facilities,head);
 }
 if(is_governance_work_original_fact_kind(head.kind)){
  if(!selected.value.work_expense||!work)return {false,0,common_failure("GovernanceWorkOriginalSelectedOwnerRequired",head.id).error};
  return read_governance_work_original_fact_payload_schema(common,*work,head);
 }
 if((head.domain=="local_economy"||head.domain=="estate"||head.domain=="commerce")&&(head.kind=="economic_facility_installed"||head.kind=="economic_facility_capacity"||head.kind=="facility_retention"))return {false,0,common_failure("EconomicFacilityOwnedSourceCannotBeCommonHeader",head.id).error};
 return {true,0,{}};
}
Status validate_native_economic_facility_headers(const CommonStore& common,const PreparedCatalog& catalog,const LocalEconomyState* economy,const EconomicFacilitySourcesState* facilities,const ProductionFacilityConditionsState* conditions,const GovernanceWorkExpenseState* work){
 auto selected=selected_native_economic_facility_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(bool(selected.value.facilities)!=bool(facilities)||bool(selected.value.conditions)!=bool(conditions)||bool(selected.value.work_expense)!=bool(work))return common_failure("EconomicFacilitySelectedRootMismatch");
 for(const auto& row:common.state().facts.rows()){auto schema=read_native_economic_facility_fact_schema(common,catalog,facilities,conditions,work,row.second);if(!schema.ok)return {false,{},std::move(schema.error)};}
 CheckedEconomicHeaders sink;
 if(conditions){auto checked=visit_production_facility_condition_fact_headers(common,*conditions,*facilities,sink);if(!checked.ok)return checked;}
 if(work){if(!economy)return common_failure("GovernanceWorkOriginalEconomyRequired");auto checked=visit_governance_work_original_fact_headers(common,*economy,*work,sink);if(!checked.ok)return checked;}
 return common_success();
}
Status emit_native_economic_facility_fact_payloads_fields(ProjectionWriter& writer,const CommonStore& common,const PreparedCatalog& catalog,const LocalEconomyState* economy,const EconomicFacilitySourcesState* facilities,const ProductionFacilityConditionsState* conditions,const GovernanceWorkExpenseState* work,NativeEconomicFacilityFactProjection part){
 if(part!=NativeEconomicFacilityFactProjection::all&&part!=NativeEconomicFacilityFactProjection::conditions&&part!=NativeEconomicFacilityFactProjection::work)return common_failure("UnknownEconomicFacilityFactProjection");
 auto valid=validate_native_economic_facility_headers(common,catalog,economy,facilities,conditions,work);if(!valid.ok)return valid;
 for(const auto& row:common.state().facts.rows()){
  const auto& head=row.second;const bool condition=head.kind=="facility_condition_change",financial=is_governance_work_original_fact_kind(head.kind);
  if((!condition&&!financial)||(condition&&part==NativeEconomicFacilityFactProjection::work)||(financial&&part==NativeEconomicFacilityFactProjection::conditions))continue;
  auto emitted=writer.object(row.first,head.field_order,[&](auto& fields)->Status{
   auto payload=condition?emit_production_facility_condition_fact_payload_fields(fields,common,*conditions,*facilities,head):emit_governance_work_original_fact_payload_fields(fields,common,*work,head);if(!payload.ok)return payload;
   fields.field("kind",head.kind);return fields.field("domain",head.domain);
  });if(!emitted.ok)return emitted;
 }
 return writer.status();
}
}
