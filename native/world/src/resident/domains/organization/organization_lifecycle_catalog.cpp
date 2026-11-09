#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_lifecycle_catalog.hpp"
#include "organization_lifecycle_codec_detail.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
bool same_identity(const CatalogIdentity&a,const CatalogIdentity&b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.artifact_sha256==b.artifact_sha256&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions;}
template<class T,class Equal>bool duplicate(const std::vector<T>& values,Equal equal){for(std::size_t i=0;i<values.size();++i)for(std::size_t j=0;j<i;++j)if(equal(values[i],values[j]))return true;return false;}
bool has(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
bool sources_valid(const std::vector<SourceRef>& values){for(const auto& v:values)if(!valid_source_ref(v))return false;return !duplicate(values,[](const auto&a,const auto&b){return same_source_ref(a,b);});}
template<class C>Status fields(C& c,OrganizationLifecycleCatalog& value){
 // 原目录物理1不认识作者定位声明；不得静默丢弃后写出不可重验的目录。
 if(!c.reading())for(const auto& row:value.seed_policies.rows())
  if(row.second.initial_facility_seed_ref)
   return c.reject("OrganizationAuthoredFacilitySeedHasNoLegacyCatalogWire");
 Integer version=1;c.field("schema_version",version);if(!c.status().ok)return c.status();if(version!=1)return c.reject("UnsupportedOrganizationLifecycleCatalogSchema");
 organization_lifecycle_fields::table(c,"seed_policies",value.seed_policies,[](auto& a,auto& r){return organization_lifecycle_fields::seed(a,r);});
 return c.sequence("authored_seeds",value.authored_seeds,[](auto& a,auto& r){a.field("organization_id",r.first);return a.object("lifecycle_seed_policy_ref",[&](auto& b){return organization_lifecycle_fields::rule(b,r.second);});});
}
}
Status validate_organization_lifecycle_catalog(const OrganizationLifecycleCatalog& c,const OrganizationCatalog& base){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 86, "OrganizationLifecycleCatalog", "validate_organization_lifecycle_catalog", &c);

 if(!base.enabled||!base.authority_lifecycle_enabled||!same_identity(c.identity,base.authority_catalog_identity))return common_failure("OrganizationLifecycleCatalogIdentityMismatch");
 for(const auto& row:c.seed_policies.rows()){
  const auto& p=row.second;
  if(row.first!=p.id||p.id.empty()||p.version<1||p.organization_kind.empty()||!organization_lifecycle_tag(p.initial_lifecycle)||bool(p.terminal_reason)!=(p.initial_lifecycle==OrganizationStatus::terminal)||p.terminal_reason==OrganizationTerminalReason::legacy_dissolved)
   return common_failure("InvalidOrganizationLifecycleSeedPolicy",row.first);
  if(p.terminal_reason&&(static_cast<Integer>(*p.terminal_reason)<0||static_cast<Integer>(*p.terminal_reason)>3))return common_failure("InvalidOrganizationTerminalSeedReason",row.first);
  if(p.initial_facility_seed_ref &&
     (p.initial_facility_seed_ref->seed_row_key.empty() ||
      p.initial_facility_seed_ref->original_asset_ref.empty() ||
      p.base_right_ref || !p.mobile_carrier_refs.empty() ||
      p.initial_lifecycle!=OrganizationStatus::active))
   return common_failure("InvalidOrganizationInitialFacilitySeedBinding",row.first);
  if(duplicate(p.required_office_ids,[](const auto&a,const auto&b){return a==b;})||!sources_valid(p.mobile_carrier_refs)||!sources_valid(p.initial_responsibility_refs)||(p.base_right_ref&&!valid_source_ref(*p.base_right_ref)))return common_failure("InvalidOrganizationLifecycleSeedReferences",row.first);
  for(const auto& id:p.required_office_ids)if(id.empty()||!base.offices.find(id))return common_failure("OrganizationLifecycleRequiredOfficeMissing",id);
 }
 if(c.authored_seeds.size()!=base.organizations.rows().size())return common_failure("OrganizationLifecycleSeedCoverage");
 for(std::size_t i=0;i<c.authored_seeds.size();++i){
  const auto& binding=c.authored_seeds[i];const auto* org=base.organizations.find(binding.first);const auto* policy=c.seed_policies.find(binding.second.id);
  for(std::size_t j=0;j<i;++j)if(c.authored_seeds[j].first==binding.first)return common_failure("DuplicateOrganizationLifecycleSeed",binding.first);
  if(!org||org->id!=binding.first||!policy||policy->version!=binding.second.version||binding.second.catalog_digest!=c.identity.content_digest||policy->organization_kind!=org->kind||policy->initial_lifecycle!=org->status)return common_failure("OrganizationLifecycleSeedBindingMismatch",binding.first);
  if(duplicate(org->office_ids,[](const auto&a,const auto&b){return a==b;}))return common_failure("DuplicateOrganizationInitialOffice",org->id);
  for(const auto& id:org->office_ids){const auto* office=base.offices.find(id);if(!office||office->id!=id||office->organization_id!=org->id)return common_failure("OrganizationInitialOfficeOwnerMismatch",id);}
  for(const auto& id:policy->required_office_ids)if(!has(org->office_ids,id))return common_failure("OrganizationLifecycleRequiredOfficeOwnerMismatch",id);
  // 有据点记录不是使用权；缺原权利/载体必须拒，不能凭status=active跳过。
  if(org->status!=OrganizationStatus::terminal&&!policy->base_right_ref&&!policy->initial_facility_seed_ref&&policy->mobile_carrier_refs.empty())return common_failure("OrganizationLifecycleInitialCarrierSourceRequired",org->id);
 }
 for(const auto& row:base.offices.rows()){const auto* org=base.organizations.find(row.second.organization_id);if(!org||!has(org->office_ids,row.first))return common_failure("OrganizationInitialOfficeUnowned",row.first);}
 return common_success();
}
Status visit_organization_lifecycle_catalog(CodecVisitor& c,OrganizationLifecycleCatalog& value){return fields(c,value);}
Status emit_organization_lifecycle_catalog_projection(ProjectionWriter& out,std::string_view name,const OrganizationLifecycleCatalog& value,const OrganizationCatalog& base){auto valid=validate_organization_lifecycle_catalog(value,base);if(!valid.ok)return valid;return emit_projection_record<ActorFoundationProjectionArchive>(out,name,value,[](auto& c,auto& v){return fields(c,v);});}
Outcome<BoundaryValue> encode_organization_lifecycle_catalog(const OrganizationLifecycleCatalog& value,const OrganizationCatalog& base){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_organization_lifecycle_catalog_projection(writer,"",value,base);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationLifecycleCatalog> decode_organization_lifecycle_catalog(const BoundaryValue& value,const CatalogIdentity& identity,const OrganizationCatalog& base){ActorFoundationJsonVisitor reader(value);auto valid=reader.prepare_default_values<OrganizationLifecycleCatalog>();if(!valid.ok)return {false,{},valid.error};OrganizationLifecycleCatalog out;valid=fields(reader,out);if(valid.ok)valid=reader.finish();if(valid.ok){if(auto* owner=ScopedCheckpointJsonDecode::current())owner->ordinary(std::size_t{0});else if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->ordinary(std::size_t{0});out.identity=CatalogIdentity(identity);valid=validate_organization_lifecycle_catalog(out,base);}return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationLifecycleCatalog> decode_authored_organization_lifecycle_catalog(
    const BoundaryValue &value, const CatalogIdentity &identity,
    const OrganizationCatalog &base) {
  if (identity.content_digest.empty())
    return {false, {}, common_failure("OrganizationLifecycleCatalogIdentityRequired").error};
  ActorFoundationJsonVisitor reader(value);
  auto valid = reader.prepare_default_values<OrganizationLifecycleCatalog>();
  if (!valid.ok)
    return {false, {}, valid.error};
  OrganizationLifecycleCatalog out;
  Integer version = 1;
  reader.field("schema_version", version);
  if (reader.status().ok && version != 1)
    reader.reject("UnsupportedOrganizationLifecycleCatalogSchema");
  const auto *policies = value.find("seed_policies");
  const auto *policy_rows = policies ? policies->as<BoundaryValue::Dict>() : nullptr;
  std::size_t policy_index = 0;
  organization_lifecycle_fields::table(
      reader, "seed_policies", out.seed_policies, [&](auto &a, auto &r) {
        if (!policy_rows || policy_index >= policy_rows->size())
          return a.reject("OrganizationLifecycleAuthorRowMismatch");
        const auto &authored = (*policy_rows)[policy_index++].second;
        auto status = organization_lifecycle_fields::seed(a, r);
        if (!status.ok || !authored.find("initial_facility_seed_ref"))
          return status;
        // 实际成员只有两个空字符串；先经原默认成员计划再构造可选声明。
        status = a.template prepare_default_values<StableId>(2);
        if (!status.ok)
          return status;
        r.initial_facility_seed_ref.emplace();
        return a.object("initial_facility_seed_ref", [&](auto &b) {
          b.field("seed_row_key", r.initial_facility_seed_ref->seed_row_key);
          return b.field("original_asset_ref", r.initial_facility_seed_ref->original_asset_ref);
        });
      });
  // 只借同一次 sequence 的真实作者行，不复制或改写输入树。
  const auto *seeds = value.find("authored_seeds");
  const auto *rows = seeds ? seeds->as<BoundaryValue::Array>() : nullptr;
  std::size_t index = 0;
  reader.sequence("authored_seeds", out.authored_seeds, [&](auto &a, auto &r) {
    if (!rows || index >= rows->size())
      return a.reject("OrganizationLifecycleAuthorRowMismatch");
    const auto *rule = (*rows)[index++].find("lifecycle_seed_policy_ref");
    a.field("organization_id", r.first);
    return a.object("lifecycle_seed_policy_ref", [&](auto &b) {
      b.field("id", r.second.id);
      b.field("version", r.second.version);
      if (rule && rule->find("catalog_digest"))
        b.field("catalog_digest", r.second.catalog_digest);
      else if (b.status().ok)
        r.second.catalog_digest =
            checkpoint_decode_boundary_string(identity.content_digest);
      if (b.status().ok &&
          (r.second.id.empty() || r.second.version < 1 ||
           r.second.catalog_digest.empty()))
        return b.reject("InvalidOrganizationLifecycleRule");
      return b.status();
    });
  });
  valid = reader.finish();
  if (valid.ok) {


    if (auto *owner = ScopedCheckpointJsonDecode::current())
      owner->ordinary(std::size_t{0});
    else if (auto *owner = ScopedCheckpointBoundaryDecode::current())
      owner->ordinary(std::size_t{0});
    out.identity = CatalogIdentity(identity);
    // 显式摘要仍必须与真实锁定身份强等；原覆盖、来源与生命周期门全部保留。
    valid = validate_organization_lifecycle_catalog(out, base);
  }
  return {valid.ok, std::move(out), valid.error};
}
}
