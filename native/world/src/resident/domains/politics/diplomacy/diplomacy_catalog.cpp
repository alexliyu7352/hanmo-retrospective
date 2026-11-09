#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 通行协商目录严核实际组织/Office/地点与作者期限，不生成提议、消息或授权事实。
#include "diplomacy_catalog.hpp"
#include "diplomacy_delegation.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 工程有界ID仅用于引用，不能借空串或超长值回退到默认对象。
bool identifier(const StableId& id){return !id.empty()&&id.size()<=4096;}
// 作者列表不得重复；所有条款和地点都须显式出现。
bool unique_ids(const std::vector<StableId>& ids){if(ids.empty()||ids.size()>4096)return false;std::set<StableId> seen;for(const auto& id:ids)if(!identifier(id)||!seen.insert(id).second)return false;return true;}
}
// 无效作者输入先拒绝，运行层不能在接触时临时补造对方官职或通行范围。
Status validate_diplomacy_catalog(const CommonCatalog& common,const OrganizationCatalog& organizations,const DiplomacyCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 91, "DiplomacyCatalog", "validate_diplomacy_catalog", &catalog);

    if((catalog.version!=1&&catalog.version!=2)||(catalog.version==2)!=catalog.v2.has_value())return common_failure("DiplomacyVersionMismatch");
    if(!catalog.enabled)return catalog.passage_terms.empty()&&catalog.mission_policies.empty()?common_success():common_failure("UnexpectedDiplomacyCatalog");
    if(!organizations.enabled||catalog.passage_terms.empty()||catalog.mission_policies.empty()||catalog.passage_terms.size()>4096||catalog.mission_policies.size()>4096)return common_failure("InvalidDiplomacyCatalog");
    for(const auto& row:catalog.passage_terms.rows()){
        const auto& terms=row.second;
        if(row.first!=terms.id||!identifier(terms.id)||!common.texts.find(terms.name_key)||terms.grantor_organization_id==terms.beneficiary_organization_id||!organizations.organizations.find(terms.grantor_organization_id)||!organizations.organizations.find(terms.beneficiary_organization_id)||!unique_ids(terms.place_ids)||terms.duration_ticks<=0||terms.expiry_notice_ticks<=0||terms.expiry_notice_ticks>=terms.duration_ticks)return common_failure("InvalidDiplomaticPassageTerms",row.first);
        for(const auto& place:terms.place_ids)if(!common.places.find(place))return common_failure("UnknownDiplomaticPlace",place);
    }
    std::set<StableId> referenced;
    for(const auto& row:catalog.mission_policies.rows()){
        const auto& policy=row.second;const auto* target=organizations.offices.find(policy.target_office_id);
        if(row.first!=policy.id||!identifier(policy.id)||!common.texts.find(policy.name_key)||policy.issuer_organization_id==policy.target_organization_id||!organizations.organizations.find(policy.issuer_organization_id)||!organizations.organizations.find(policy.target_organization_id)||!target||target->organization_id!=policy.target_organization_id||!common.places.find(policy.issuer_site_id)||!common.places.find(policy.target_site_id)||policy.offer_valid_ticks<=0||!unique_ids(policy.passage_terms_ids))return common_failure("InvalidDiplomaticMissionPolicy",row.first);
        if(policy.task_terms){const auto& t=*policy.task_terms;if(t.acceptance_valid_ticks<=0||t.acceptance_valid_ticks>policy.offer_valid_ticks||t.delivery_work_ticks<=0||t.delivery_work_ticks>=policy.offer_valid_ticks||t.continuity_policy_id!="continue_under_organization"||(t.return_report_required?t.report_work_ticks<=0:t.report_work_ticks!=0))return common_failure("InvalidDiplomaticTaskTerms",row.first);}
        for(const auto& id:policy.passage_terms_ids){const auto* terms=catalog.passage_terms.find(id);if(!terms||terms->beneficiary_organization_id!=policy.issuer_organization_id||terms->grantor_organization_id!=policy.target_organization_id)return common_failure("DiplomaticTermsPartyMismatch",id);referenced.insert(id);}
    }
    if(referenced.size()!=catalog.passage_terms.size())return common_failure("UnreferencedDiplomaticTerms");
    if(catalog.v2)return validate_diplomacy_v2_catalog(common,organizations,catalog,*catalog.v2);
    return common_success();
}
// 严格逐字段读目录；不接受任意JSON条款或缺省永久许可。
Outcome<DiplomacyCatalog> decode_diplomacy_catalog(const BoundaryValue& value){try{
    using namespace boundary;DiplomacyCatalog catalog;catalog.version=get_int(value,"version","/diplomacy");if(catalog.version==1)fields(value,{"version","enabled","passage_terms","mission_policies"},{},"/diplomacy");else if(catalog.version==2)fields(value,{"version","enabled","passage_terms","mission_policies","v2"},{},"/diplomacy");else fail("DiplomacyVersionMismatch","/diplomacy/version");catalog.enabled=get_bool(value,"enabled","/diplomacy");if(catalog.version==2){auto extension=decode_diplomacy_v2_catalog(field(value,"v2","/diplomacy"));if(!extension.ok)return {false,{},extension.error};catalog.v2=std::move(extension.value);}
    for(const auto& row:object(field(value,"passage_terms","/diplomacy"),"/diplomacy/passage_terms")){const auto& v=row.second;fields(v,{"id","name_key","grantor_organization_id","beneficiary_organization_id","place_ids","duration_ticks","expiry_notice_ticks"},{},row.first);DiplomaticPassageTerms t;t.id=get_string(v,"id",row.first);t.name_key=get_string(v,"name_key",row.first);t.grantor_organization_id=get_string(v,"grantor_organization_id",row.first);t.beneficiary_organization_id=get_string(v,"beneficiary_organization_id",row.first);t.place_ids=strings(field(v,"place_ids",row.first),row.first);t.duration_ticks=get_int(v,"duration_ticks",row.first);t.expiry_notice_ticks=get_int(v,"expiry_notice_ticks",row.first);if(row.first!=t.id||!catalog.passage_terms.insert(row.first,std::move(t)))fail("InvalidDiplomaticPassageTerms",row.first);}
    for(const auto& row:object(field(value,"mission_policies","/diplomacy"),"/diplomacy/mission_policies")){const auto& v=row.second;fields(v,{"id","name_key","issuer_organization_id","target_organization_id","issuer_site_id","target_site_id","target_office_id","passage_terms_ids","offer_valid_ticks"},{"task_terms"},row.first);DiplomaticMissionPolicy p;p.id=get_string(v,"id",row.first);p.name_key=get_string(v,"name_key",row.first);p.issuer_organization_id=get_string(v,"issuer_organization_id",row.first);p.target_organization_id=get_string(v,"target_organization_id",row.first);p.issuer_site_id=get_string(v,"issuer_site_id",row.first);p.target_site_id=get_string(v,"target_site_id",row.first);p.target_office_id=get_string(v,"target_office_id",row.first);p.passage_terms_ids=strings(field(v,"passage_terms_ids",row.first),row.first);p.offer_valid_ticks=get_int(v,"offer_valid_ticks",row.first);if(const auto* terms=v.find("task_terms")){fields(*terms,{"acceptance_valid_ticks","delivery_work_ticks","report_work_ticks","return_report_required","continuity_policy_id"},{},row.first);DiplomaticMissionTaskTerms t;t.acceptance_valid_ticks=get_int(*terms,"acceptance_valid_ticks",row.first);t.delivery_work_ticks=get_int(*terms,"delivery_work_ticks",row.first);t.report_work_ticks=get_int(*terms,"report_work_ticks",row.first);t.return_report_required=get_bool(*terms,"return_report_required",row.first);t.continuity_policy_id=get_string(*terms,"continuity_policy_id",row.first);p.task_terms=std::move(t);}if(row.first!=p.id||!catalog.mission_policies.insert(row.first,std::move(p)))fail("InvalidDiplomaticMissionPolicy",row.first);}
    return {true,std::move(catalog),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 输出保留全部作者字段，初始目录不夹带任何已生效状态或接受评分。
// 同一原字段次序供值与shape输出，全部字符串和列表只借读。
Status emit_diplomacy_catalog_fields(ProjectionWriter& writer,const DiplomacyCatalog& catalog){
 writer.field("version",catalog.version);writer.field("enabled",catalog.enabled);
 writer.object("passage_terms",[&](auto& entries){
  for(const auto& row:catalog.passage_terms.rows()){
   auto status=entries.object(row.first,[&](auto& fields){const auto& t=row.second;
    fields.field("id",t.id);fields.field("name_key",t.name_key);
    fields.field("grantor_organization_id",t.grantor_organization_id);fields.field("beneficiary_organization_id",t.beneficiary_organization_id);
    fields.sequence("place_ids",t.place_ids,[](auto& element,const auto& id){return element.field("",id);});
    fields.field("duration_ticks",t.duration_ticks);fields.field("expiry_notice_ticks",t.expiry_notice_ticks);return fields.status();});
   if(!status.ok)return status;
  }return entries.status();});
 writer.object("mission_policies",[&](auto& entries){
  for(const auto& row:catalog.mission_policies.rows()){
   auto status=entries.object(row.first,[&](auto& fields){const auto& p=row.second;
    fields.field("id",p.id);fields.field("name_key",p.name_key);
    fields.field("issuer_organization_id",p.issuer_organization_id);fields.field("target_organization_id",p.target_organization_id);
    fields.field("issuer_site_id",p.issuer_site_id);fields.field("target_site_id",p.target_site_id);fields.field("target_office_id",p.target_office_id);
    fields.sequence("passage_terms_ids",p.passage_terms_ids,[](auto& element,const auto& id){return element.field("",id);});fields.field("offer_valid_ticks",p.offer_valid_ticks);
    if(p.task_terms)fields.object("task_terms",[&](auto& task){const auto& t=*p.task_terms;
     task.field("acceptance_valid_ticks",t.acceptance_valid_ticks);task.field("delivery_work_ticks",t.delivery_work_ticks);task.field("report_work_ticks",t.report_work_ticks);
     task.field("return_report_required",t.return_report_required);task.field("continuity_policy_id",t.continuity_policy_id);return task.status();});
    return fields.status();});
   if(!status.ok)return status;
  }return entries.status();});
 if(catalog.version==2&&catalog.v2)writer.object("v2",[&](auto& fields){return emit_diplomacy_v2_catalog_fields(fields,*catalog.v2);});return writer.status();
}
BoundaryValue encode_diplomacy_catalog(const DiplomacyCatalog& catalog){return politics_projection_detail::encode_fields(catalog,emit_diplomacy_catalog_fields);}
}
