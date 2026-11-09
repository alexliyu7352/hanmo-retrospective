// 原财政唯一字段表；/2只在调用者显式追加真Moment，/1原前缀不变。
#pragma once
#include "politics_fiscal.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<FiscalTaxMode>{static std::vector<std::pair<FiscalTaxMode,StableId>> rows(){return {{FiscalTaxMode::lenient,"lenient"},{FiscalTaxMode::normal,"normal"},{FiscalTaxMode::surcharge,"surcharge"}};}};
// 同一快照完整冻结来源、公库和生效时刻，数值税基留在经济域。
template<class A> void record(A& a,FiscalPolicySnapshot& v){a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("organization_id",v.organization_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("source_fact_id",v.source_fact_id);a("policy_revision",v.policy_revision);a("issued_tick",v.issued_tick);a("effective_from_tick",v.effective_from_tick);a("mode",v.mode);}
// 旧任期的签发人、Grant与epoch随历史保存，不使用今日职位改写旧记录。
template<class A> void record(A& a,FiscalPolicyRecord& v){a("id",v.id);a("issuer_actor_id",v.issuer_actor_id);a("issuer_office_id",v.issuer_office_id);a("grant_id",v.grant_id);a("authority_epoch",v.authority_epoch);a("snapshot",v.snapshot);}
// 该子状态只存真实政治决定，不含任何可花余额或预期收税。
template<class A> void record(A& a,PoliticalFiscalState& v){a("policies",v.policies);}
// 能力开关不含初始税政，作者不能夹带来源未成立的征收决定。
template<class A> void record(A& a,PoliticalFiscalCatalog& v){a("enabled",v.enabled);}
// 仅codec临时适配共同签发身份，避免与新S5的ActingAuthority访问器重定义；不增加持久权威表。
struct FiscalAuthorityWire { ActingAuthority& value; };
template<class A> void record(A& a,FiscalAuthorityWire& wire){auto& v=wire.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
// 所有期望revision和实际公粮容器必填，旧按钮不能自动补零继续执行。
template<class A> void record(A& a,PoliticalSetTaxCommand& v){FiscalAuthorityWire authority{v.authority};a("authority",authority);a("jurisdiction_id",v.jurisdiction_id);a("public_inventory_id",v.public_inventory_id);a("expected_jurisdiction_revision",v.expected_jurisdiction_revision);a("expected_policy_revision",v.expected_policy_revision);a("mode",v.mode);}
// 回执仅证明政策成立，数额和支付仍由S5的实际结算回执说明。
template<class A> void record(A& a,PoliticalFiscalEffects& v){a("policy_fact_id",v.policy_fact_id);a("policy_revision",v.policy_revision);a("effective_from_tick",v.effective_from_tick);}
}
