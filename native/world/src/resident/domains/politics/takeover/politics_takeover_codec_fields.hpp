// 原接管字段唯一表；旧/1及显式/2均复用，/2尾字段由受信访问器补入。
#pragma once
#include "politics_takeover_state.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<PoliticalResponsibilityKind> { static std::vector<std::pair<PoliticalResponsibilityKind,StableId>> rows() { return {{PoliticalResponsibilityKind::production_unit,"production_unit"},{PoliticalResponsibilityKind::production_asset_use,"production_asset_use"},{PoliticalResponsibilityKind::pending_configuration,"pending_configuration"},{PoliticalResponsibilityKind::obligation,"obligation"},{PoliticalResponsibilityKind::arrear,"arrear"},{PoliticalResponsibilityKind::fiscal_assessment,"fiscal_assessment"},{PoliticalResponsibilityKind::relief_contract,"relief_contract"},{PoliticalResponsibilityKind::supply_source,"supply_source"},{PoliticalResponsibilityKind::task_contract,"task_contract"}}; } };
template<> struct EnumNames<PoliticalResponsibilityDisposition> { static std::vector<std::pair<PoliticalResponsibilityDisposition,StableId>> rows() { return {{PoliticalResponsibilityDisposition::continue_under_original,"continue_under_original"},{PoliticalResponsibilityDisposition::suspend,"suspend"},{PoliticalResponsibilityDisposition::assume_by_successor,"assume_by_successor"},{PoliticalResponsibilityDisposition::settlement_required,"settlement_required"}}; } };
template<> struct EnumNames<PoliticalResponsibilityAssetSource::Kind> { static std::vector<std::pair<PoliticalResponsibilityAssetSource::Kind,StableId>> rows() { return {{PoliticalResponsibilityAssetSource::Kind::wallet,"wallet"},{PoliticalResponsibilityAssetSource::Kind::inventory,"inventory"},{PoliticalResponsibilityAssetSource::Kind::production_unit,"production_unit"},{PoliticalResponsibilityAssetSource::Kind::production_asset,"production_asset"}}; } };
// 世界身份按原字节严格十六进制保存，不把新Session代次倒写历史。
template<class A> void record(A& a,WorldIdentity& v){
    StableId hex;const char* digits="0123456789abcdef";for(auto b:v.world_id){hex+=digits[b>>4];hex+=digits[b&15];}
    a("world_id",hex);if(hex.size()!=32)boundary::fail("InvalidTakeoverWorldId","world_id");
    auto nibble=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
    for(std::size_t i=0;i<16;++i){const auto h=nibble(hex[2*i]),l=nibble(hex[2*i+1]);if(h<0||l<0)boundary::fail("InvalidTakeoverWorldId","world_id");if(politics_projection_detail::reading(a))v.world_id[i]=static_cast<std::uint8_t>(h*16+l);}
    a("execution_epoch",v.execution_epoch);a("revision",v.revision);a("tick",v.tick);
}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalOccupationRuleSource& v){a("rule_id",v.rule_id);a("rule_version",v.rule_version);a("integration_ticks",v.integration_ticks);a("income_basis_points",v.income_basis_points);}
template<class A> void record(A& a,PoliticalTakeoverRule& v){a("id",v.id);a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("stronghold_id",v.stronghold_id);a("garrison_id",v.garrison_id);a("successor_office_id",v.successor_office_id);a("occupation_rule",v.occupation_rule);}
template<class A> void record(A& a,PoliticalTakeoverCatalog& v){a("version",v.version);a("enabled",v.enabled);a("rules",v.rules);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalResponsibilityRef& v){a("kind",v.kind);a("responsibility_id",v.responsibility_id);a("unit_id",v.unit_id);a("asset_id",v.asset_id);}
template<class A> void record(A& a,PoliticalResponsibilityRequest& v){a("responsibility",v.responsibility);a("disposition",v.disposition);a("payment_wallet_id",v.payment_wallet_id);a("payment_inventory_id",v.payment_inventory_id);a("production_asset_ids",v.production_asset_ids);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalResponsibilityAssetSource& v){a("kind",v.kind);a("id",v.id);a("frozen_owner",v.frozen_owner);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalResponsibilityContainer& v){a("id",v.id);a("role",v.role);a("inventory",v.inventory);a("selected",v.selected);a("item_id",v.item_id);a("original_owner",v.original_owner);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalProtectedCustody& v){a("inventory_id",v.inventory_id);a("lot_id",v.lot_id);a("task_id",v.task_id);}
// 作者键与运行记录保结构，避免用分隔符拼接后产生身份歧义。
template<class A> void record(A& a,PoliticalAuthoredResponsibilitySource& v){a("table",v.table);a("id",v.id);a("parent_id",v.parent_id);}
template<class A> void record(A& a,PoliticalResponsibilityRecordSource& v){a("kind",v.kind);a("id",v.id);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalOriginalResponsibility& v){a("responsibility",v.responsibility);a("principal",v.principal);a("assignee",v.assignee);a("beneficiaries",v.beneficiaries);a("delivery_beneficiaries",v.delivery_beneficiaries);a("assets",v.assets);a("containers",v.containers);a("protected_custody",v.protected_custody);a("source_fact_ids",v.source_fact_ids);a("authored_sources",v.authored_sources);a("record_sources",v.record_sources);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalTakeoverRegistration& v){a("id",v.id);a("rule_id",v.rule_id);a("occupation_fact_id",v.occupation_fact_id);a("source_identity",v.source_identity);a("tick",v.tick);a("captured_next_instance_id",v.captured_next_instance_id);a("former_organization_id",v.former_organization_id);a("successor_organization_id",v.successor_organization_id);a("public_wallet_ids",v.public_wallet_ids);a("public_inventory_ids",v.public_inventory_ids);a("responsibilities",v.responsibilities);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalTakeoverApprovalRecord& v){a("id",v.id);a("registration_fact_id",v.registration_fact_id);a("previous_approval_fact_id",v.previous_approval_fact_id);a("authority",v.authority);a("grant_id",v.grant_id);a("tick",v.tick);a("decision_fact_ids",v.decision_fact_ids);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalExecutionAccount& v){a("role",v.role);a("original_container_id",v.original_container_id);a("execution_container_id",v.execution_container_id);a("inventory",v.inventory);}
template<class A> void record(A& a,PoliticalResponsibilityDecision& v){a("decision_fact_id",v.decision_fact_id);a("takeover_fact_id",v.takeover_fact_id);a("responsibility",v.responsibility);a("disposition",v.disposition);a("original_principal",v.original_principal);a("original_beneficiaries",v.original_beneficiaries);a("original_assignee",v.original_assignee);a("original_delivery_beneficiaries",v.original_delivery_beneficiaries);a("execution_principal",v.execution_principal);a("collection_beneficiaries",v.collection_beneficiaries);a("original_assets",v.original_assets);a("payment_wallet_id",v.payment_wallet_id);a("receipt_wallet_id",v.receipt_wallet_id);a("payment_inventory_id",v.payment_inventory_id);a("receipt_inventory_id",v.receipt_inventory_id);a("production_asset_ids",v.production_asset_ids);a("execution_accounts",v.execution_accounts);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalTakeoverDecisionRecord& v){a("id",v.id);a("approval_fact_id",v.approval_fact_id);a("decision",v.decision);}
// 完整保存本记录的不可变来源和具名引用。
template<class A> void record(A& a,PoliticalTakeoverState& v){a("version",v.version);a("registrations",v.registrations);a("approvals",v.approvals);a("decisions",v.decisions);}
}
