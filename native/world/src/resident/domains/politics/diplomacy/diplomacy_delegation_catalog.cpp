#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 委任对象和职责来自明确作者策略；不存在无名随机使者或默认工时。
#include "diplomacy_delegation.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_diplomacy_v2_catalog(const CommonCatalog& common, const OrganizationCatalog& organizations,
    const DiplomacyCatalog& diplomacy, const DiplomacyV2Catalog& catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 92, "DiplomacyCatalog", "validate_diplomacy_v2_catalog", &diplomacy);

    if (diplomacy.version != 2 || catalog.version != 2 || !diplomacy.enabled || catalog.delegation_policies.empty() || catalog.delegation_policies.size() > 4096)
        return common_failure("InvalidDiplomacyV2Catalog");
    for (const auto& row : catalog.delegation_policies.rows()) {
        const auto& p = row.second; const auto* mission = diplomacy.mission_policies.find(p.mission_policy_id);
        const auto* office = organizations.offices.find(p.issuer_office_id);
        if (row.first != p.id || p.id.empty() || p.id.size() > 4096 || !mission || !office ||
            office->organization_id != mission->issuer_organization_id || std::find(common.actor_ids.begin(), common.actor_ids.end(), p.envoy_actor_id)==common.actor_ids.end() ||
            !mission->task_terms || !mission->task_terms->return_report_required || mission->task_terms->report_work_ticks <= 0 ||
            std::find(mission->passage_terms_ids.begin(), mission->passage_terms_ids.end(), p.terms_id) == mission->passage_terms_ids.end())
            return common_failure("InvalidDiplomaticDelegationPolicy", row.first);
    }
    return common_success();
}
Outcome<DiplomacyV2Catalog> decode_diplomacy_v2_catalog(const BoundaryValue& value) {
    try {
        using namespace boundary;
        fields(value, {"version", "delegation_policies"}, {}, "/diplomacy/v2");
        DiplomacyV2Catalog out; out.version = get_int(value, "version", "/diplomacy/v2");
        if (out.version != 2) fail("DiplomacyVersionMismatch", "/diplomacy/v2/version");
        for (const auto& row : object(field(value,"delegation_policies","/diplomacy/v2"),"/diplomacy/v2/delegation_policies")) {
            fields(row.second, {"id","mission_policy_id","terms_id","issuer_office_id","envoy_actor_id"}, {}, row.first);
            DiplomaticDelegationPolicy p{get_string(row.second,"id",row.first),get_string(row.second,"mission_policy_id",row.first),
                get_string(row.second,"terms_id",row.first),get_string(row.second,"issuer_office_id",row.first),get_string(row.second,"envoy_actor_id",row.first)};
            if (row.first != p.id || !out.delegation_policies.insert(row.first,std::move(p))) fail("InvalidDiplomaticDelegationPolicy",row.first);
        }
        return {true,std::move(out),{}};
    } catch (const boundary::DecodeFailure& e) { return {false,{},e.error}; }
}
// 原委任目录仍只有实际作者字段；输出不签发或接受委任。
Status emit_diplomacy_v2_catalog_fields(ProjectionWriter& writer,const DiplomacyV2Catalog& catalog){
 writer.field("version",catalog.version);
 return writer.object("delegation_policies",[&](auto& entries){
  for(const auto& row:catalog.delegation_policies.rows()){
   auto status=entries.object(row.first,[&](auto& fields){const auto& p=row.second;
    fields.field("id",p.id);fields.field("mission_policy_id",p.mission_policy_id);fields.field("terms_id",p.terms_id);
    fields.field("issuer_office_id",p.issuer_office_id);fields.field("envoy_actor_id",p.envoy_actor_id);return fields.status();});
   if(!status.ok)return status;
  }return entries.status();});
}
BoundaryValue encode_diplomacy_v2_catalog(const DiplomacyV2Catalog& catalog){return politics_projection_detail::encode_fields(catalog,emit_diplomacy_v2_catalog_fields);}
}
