#include "resident/domains/military/military_model.hpp"
// 原义务/任务来源不含可写余额；多受益人和私人寄存货权逐项保留。
#include "politics_takeover_source_conversion.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
namespace {
const Owner* selected_owner(const OccupationAssetManifest& manifest,const StableId& id,bool inventory){if(inventory){for(const auto& row:manifest.inventories)if(row.first==id)return &row.second.from_owner;}else for(const auto& row:manifest.wallets)if(row.first==id)return &row.second.from_owner;return nullptr;}
void container(PoliticalOriginalResponsibility& out,const OccupationAssetManifest& manifest,const StableId& id,bool inventory,const StableId& role,const StableId& item={}){PoliticalResponsibilityContainer c;c.id=id;c.inventory=inventory;c.role=role;if(!item.empty())c.item_id=item;const auto* owner=selected_owner(manifest,id,inventory);c.selected=owner!=nullptr;if(owner)c.original_owner=*owner;out.containers.push_back(std::move(c));}
StableId economic_role(EconomicContainerRole role){switch(role){case EconomicContainerRole::payment:return "payment";case EconomicContainerRole::receipt:return "receipt";case EconomicContainerRole::production_input:return "production_input";case EconomicContainerRole::production_output:return "production_output";}return {};}
StableId task_role(TaskAccountPurpose role){switch(role){case TaskAccountPurpose::funding:return "funding";case TaskAccountPurpose::reward:return "reward";case TaskAccountPurpose::payee:return "payee";case TaskAccountPurpose::escrow:return "escrow";case TaskAccountPurpose::delivery:return "delivery";case TaskAccountPurpose::return_custody:return "return_custody";case TaskAccountPurpose::shipment_source:return "shipment_source";case TaskAccountPurpose::shipment_cargo:return "shipment_cargo";case TaskAccountPurpose::shipment_destination:return "shipment_destination";}return {};}
}
PoliticalResponsibilityRef political_economic_responsibility_ref(const EconomicResponsibilityRef& ref){
    PoliticalResponsibilityRef out;out.responsibility_id=ref.id;
    switch(ref.kind){
    case EconomicResponsibilityKind::production_unit:out.kind=PoliticalResponsibilityKind::production_unit;break;
    case EconomicResponsibilityKind::production_asset_use:out.kind=PoliticalResponsibilityKind::production_asset_use;out.unit_id=ref.unit_id;out.asset_id=ref.id;break;
    case EconomicResponsibilityKind::pending_configuration:out.kind=PoliticalResponsibilityKind::pending_configuration;break;
    case EconomicResponsibilityKind::obligation:out.kind=PoliticalResponsibilityKind::obligation;break;
    case EconomicResponsibilityKind::arrear:out.kind=PoliticalResponsibilityKind::arrear;break;
    case EconomicResponsibilityKind::fiscal_assessment:out.kind=PoliticalResponsibilityKind::fiscal_assessment;break;
    case EconomicResponsibilityKind::relief_contract:out.kind=PoliticalResponsibilityKind::relief_contract;break;
    case EconomicResponsibilityKind::supply_source:out.kind=PoliticalResponsibilityKind::supply_source;break;
    }return out;
}
Outcome<EconomicResponsibilityRef> economic_political_responsibility_ref(const PoliticalResponsibilityRef& ref){
    if(!takeover_detail::valid_ref(ref)||ref.kind==PoliticalResponsibilityKind::task_contract)return {false,{},common_failure("InvalidPoliticalEconomicResponsibility").error};
    EconomicResponsibilityRef out;out.id=ref.responsibility_id;
    switch(ref.kind){
    case PoliticalResponsibilityKind::production_unit:out.kind=EconomicResponsibilityKind::production_unit;break;
    case PoliticalResponsibilityKind::production_asset_use:out.kind=EconomicResponsibilityKind::production_asset_use;out.unit_id=*ref.unit_id;break;
    case PoliticalResponsibilityKind::pending_configuration:out.kind=EconomicResponsibilityKind::pending_configuration;break;
    case PoliticalResponsibilityKind::obligation:out.kind=EconomicResponsibilityKind::obligation;break;
    case PoliticalResponsibilityKind::arrear:out.kind=EconomicResponsibilityKind::arrear;break;
    case PoliticalResponsibilityKind::fiscal_assessment:out.kind=EconomicResponsibilityKind::fiscal_assessment;break;
    case PoliticalResponsibilityKind::relief_contract:out.kind=EconomicResponsibilityKind::relief_contract;break;
    case PoliticalResponsibilityKind::supply_source:out.kind=EconomicResponsibilityKind::supply_source;break;
    default:return {false,{},common_failure("InvalidPoliticalEconomicResponsibility").error};
    }return {true,std::move(out),{}};
}
void canonicalize_political_original_responsibility(PoliticalOriginalResponsibility& out){
    std::sort(out.containers.begin(),out.containers.end(),[](const auto& a,const auto& b){return std::tie(a.inventory,a.id,a.role,a.item_id)<std::tie(b.inventory,b.id,b.role,b.item_id);});
    std::sort(out.assets.begin(),out.assets.end(),[](const auto& a,const auto& b){return std::tie(a.kind,a.id)<std::tie(b.kind,b.id);});
    std::sort(out.protected_custody.begin(),out.protected_custody.end(),[](const auto& a,const auto& b){return std::tie(a.inventory_id,a.lot_id,a.task_id)<std::tie(b.inventory_id,b.lot_id,b.task_id);});
    std::sort(out.authored_sources.begin(),out.authored_sources.end(),[](const auto& a,const auto& b){return std::tie(a.table,a.id,a.parent_id)<std::tie(b.table,b.id,b.parent_id);});
    std::sort(out.record_sources.begin(),out.record_sources.end(),[](const auto& a,const auto& b){return std::tie(a.kind,a.id)<std::tie(b.kind,b.id);});
    std::sort(out.source_fact_ids.begin(),out.source_fact_ids.end());
}
Outcome<PoliticalOriginalResponsibility> political_original_economic_source(const EconomicResponsibilitySource& source,const OccupationAssetManifest& manifest){
    PoliticalOriginalResponsibility out;out.responsibility=political_economic_responsibility_ref(source.reference);out.principal=source.original_principal;out.beneficiaries=source.original_beneficiaries;out.source_fact_ids=source.source_fact_ids;
    StableId unit;for(const auto& a:source.authored_sources){out.authored_sources.push_back({a.table,a.id,a.parent_id});if(a.table=="production_units")unit=a.id;}
    if(source.original_operating_owner){if(unit.empty())return {false,{},common_failure("MissingPoliticalOperatingSource").error};out.assets.push_back({PoliticalResponsibilityAssetSource::Kind::production_unit,unit,*source.original_operating_owner});}
    for(const auto& a:source.production_assets)out.assets.push_back({PoliticalResponsibilityAssetSource::Kind::production_asset,a.id,a.original_owner});
    for(const auto& c:source.containers){const auto role=economic_role(c.role);if(role.empty())return {false,{},common_failure("UnknownEconomicContainerRole").error};container(out,manifest,c.id,c.inventory,role,c.item_id);}
    canonicalize_political_original_responsibility(out);return {true,std::move(out),{}};
}
Outcome<PoliticalOriginalResponsibility> political_original_task_source(const TaskPublicAccountDependency& source,const OccupationAssetManifest& manifest){
    PoliticalOriginalResponsibility out;out.responsibility={PoliticalResponsibilityKind::task_contract,source.responsibility_id,{},{}};
    if(source.original_issuer.kind!=Owner::Kind::actor&&source.original_issuer.kind!=Owner::Kind::organization)return {false,{},common_failure("UnsupportedPoliticalTaskPrincipal").error};
    out.principal=SubjectRef{source.original_issuer.kind==Owner::Kind::actor?SubjectKind::actor:SubjectKind::organization,source.original_issuer.id};out.assignee=source.assignee;if(source.delivery_beneficiary)out.delivery_beneficiaries.push_back(*source.delivery_beneficiary);
    if(source.contract_id)out.record_sources.push_back({"contract",*source.contract_id});for(const auto& id:source.task_ids)out.record_sources.push_back({"task",id});
    if(source.static_cash_execution_supported){if(source.contract_id||source.task_ids.size()!=1||source.task_ids.front()!=source.responsibility_id)return {false,{},common_failure("InvalidStaticTaskCashExecutionSource").error};out.record_sources.push_back({"static_task_cash_v2",source.responsibility_id});}
    for(const auto& c:source.wallets){const auto role=task_role(c.purpose);if(role.empty())return {false,{},common_failure("UnknownTaskAccountPurpose").error};container(out,manifest,c.container_id,false,role);}
    for(const auto& c:source.inventories){const auto role=task_role(c.purpose);if(role.empty())return {false,{},common_failure("UnknownTaskAccountPurpose").error};container(out,manifest,c.container_id,true,role);}
    for(const auto& c:source.cargo)if(c.selected_container)out.protected_custody.push_back({c.inventory_id,c.lot_id,c.task_id});
    canonicalize_political_original_responsibility(out);return {true,std::move(out),{}};
}
}
