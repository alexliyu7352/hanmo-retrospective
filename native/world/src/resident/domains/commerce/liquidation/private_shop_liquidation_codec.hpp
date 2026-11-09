// 新清算段独立同序编码，旧Shop1、Task和继产字节保持；字段缺漏、重复或未知均硬拒。
#pragma once
#include "private_shop_liquidation.hpp"
#include "resident/domains/commerce/private_shop_codec.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PrivateShopExecutorPolicy& v){a("id",v.id);a("premises_id",v.premises_id);a("proprietor_actor_id",v.proprietor_actor_id);a("executor_actor_id",v.executor_actor_id);a("proprietor_role_id",v.proprietor_role_id);a("executor_role_id",v.executor_role_id);a("proprietor_dialogue_id",v.proprietor_dialogue_id);a("proprietor_appointment_node_id",v.proprietor_appointment_node_id);a("executor_dialogue_id",v.executor_dialogue_id);a("executor_acceptance_node_id",v.executor_acceptance_node_id);}
template<class A>void record(A& a,PrivateShopLiquidationCatalog& v){a("version",v.version);a("policies",v.policies);}
template<class A>void record(A& a,PrivateShopExecutorMandate& v){a("id",v.id);a("policy_id",v.policy_id);a("shop_id",v.shop_id);a("proprietor_actor_id",v.proprietor_actor_id);a("executor_actor_id",v.executor_actor_id);a("shop_wallet_id",v.shop_wallet_id);a("shop_inventory_id",v.shop_inventory_id);a("return_wallet_id",v.return_wallet_id);a("return_inventory_id",v.return_inventory_id);a("acceptance_fact_id",v.acceptance_fact_id);a("appointed_tick",v.appointed_tick);a("requested_by_executor",v.requested_by_executor);a("agreement",v.agreement);}
template<class A>void record(A& a,PrivateShopExecutorAcceptance& v){a("id",v.id);a("mandate_fact_id",v.mandate_fact_id);a("executor_actor_id",v.executor_actor_id);a("tick",v.tick);}
template<class A>void record(A& a,PrivateShopExecutorRevocation& v){a("id",v.id);a("mandate_fact_id",v.mandate_fact_id);a("actor_id",v.actor_id);a("decision",v.decision);a("tick",v.tick);}
template<class A>void record(A& a,PrivateShopEstateClosingRecord& v){a("id",v.id);a("closing_fact_id",v.closing_fact_id);a("shop_id",v.shop_id);a("proprietor_actor_id",v.proprietor_actor_id);a("executor_actor_id",v.executor_actor_id);a("mandate_fact_id",v.mandate_fact_id);a("acceptance_fact_id",v.acceptance_fact_id);a("death_fact_id",v.death_fact_id);a("death_cause_fact_id",v.death_cause_fact_id);a("death_notice_binding_fact_id",v.death_notice_binding_fact_id);a("death_notice_handoff_fact_id",v.death_notice_handoff_fact_id);a("tick",v.tick);}
template<class A>void record(A& a,PrivateShopLiquidationResultBinding& v){a("id",v.id);a("actor_id",v.actor_id);a("command_kind",v.command_kind);a("result_id",v.result_id);a("request_seq",v.request_seq);a("revision",v.revision);a("tick",v.tick);}
template<class A>void record(A& a,PrivateShopLiquidationState& v){a("version",v.version);a("mandates",v.mandates);a("acceptances",v.acceptances);a("revocations",v.revocations);a("closings",v.closings);a("result_bindings",v.result_bindings);}
}
namespace hanmo::resident {
Outcome<PrivateShopLiquidationCatalog> decode_private_shop_liquidation_catalog(const BoundaryValue&);
Outcome<PrivateShopLiquidationState> decode_private_shop_liquidations(const BoundaryValue&);
class ProjectionWriter;
Status emit_private_shop_liquidations_fields(ProjectionWriter&,const PrivateShopLiquidationState&);
BoundaryValue encode_private_shop_liquidations(const PrivateShopLiquidationState&);
Status visit_private_shop_liquidations_checkpoint(CodecVisitor&,PrivateShopLiquidationState&);
// 禁用时本段与四类Common孤头同拒；有段的完整来源另由原生命周期validator验证。
Status emit_private_shop_liquidation_fact_payload_fields(ProjectionWriter&,const CommonStore&,const PrivateShopLiquidationState&,const FactRecord&);
Outcome<BoundaryValue> encode_private_shop_liquidation_fact_payload(const CommonStore&,const PrivateShopLiquidationState&,const FactRecord&);
Status validate_private_shop_liquidation_layout(const CommonStore&,const PrivateShopLiquidationState*);
}
