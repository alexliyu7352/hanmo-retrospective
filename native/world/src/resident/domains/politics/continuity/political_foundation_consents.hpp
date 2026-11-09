// 成立条款只消费真实同版支持史；不把旧接受盖过后来本人拒绝。
#pragma once
#include "political_foundation_decisions.hpp"
namespace hanmo::resident {
// 返回同一意向/真人的最后一项实际本人决定，顺序来自Common，不取ID或M大小。
// ok+nullptr仅表示该真人没有实际决定，不表示同意；借值随同候选生命周期。
Outcome<const FoundationSupportSource *>
read_effective_foundation_actor_support(const FoundationSupportContext &,
                                        const SourceRef &actual_intent,
                                        const StableId &actor_id);
// 原回应实际送达founder才可读；只借现有S3真实receipt，不新增回应账。
// 此口核当前候选，不能拿后到的receipt证明历史FoundingDecision先前已知。
Outcome<const FoundationSupportSource *>
read_foundation_support_received_by_founder(
    const FoundationSupportContext &, const SourceRef &actual_support,
    const MomentOwnerSource &actual_response_delivery);
// 当前准备的最终Terms每个接受ref都须仍是本人当前同版接受且founder实际获知；
// 不要求所有受邀者接受，不因全局support表存在自动让founder读取。
// 此有限叶只处理已实现的真人支持源。cohort/当前可用人数/劳力、载体、供养、
// 资产、旧责任与初始公权仍由各实际owner另核，不能据此产生FoundingDecision。
Status
validate_foundation_actor_acceptance_refs(const FoundationSupportContext &,
                                          const SourceRef &actual_intent,
                                          const FoundationTerms &);
// 历史FoundingDecision的必要个人同意子条件：严格只看实际决定Common头之前
// 的本人选择和已到回应，同M后来拒绝/送达既不倒改旧决定也不能补旧缺口。
// 调用方必须先完整反读该决定的payload/参与者/真实M；本口不证明资源、
// 成立权限或登记成功，不可作为独立完整FoundingDecision source发布。
Status validate_foundation_actor_acceptance_refs_before_source(
    const FoundationSupportContext &, const SourceRef &actual_intent,
    const FoundationTerms &, const MomentOwnerSource &actual_decision,
    const WorldMoment &actual_decided_at);
} // namespace hanmo::resident
