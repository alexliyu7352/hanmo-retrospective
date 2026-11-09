// Family A 的本人已知对象借原真实亲历，不生成消息/接触/知识，也不调用整 P10。
#pragma once
#include "resident/domains/family/care_education.hpp"
#include "resident/domains/interaction/known_contacts/known_contact_location.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct FamilyKnownContactReadContext1 {
  const KnownContactLocationAccess &contacts;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
};
// 当前仍用原 latest contact 投影；caller 的真实角色/Family 模板由 A owner
// 先核。
Outcome<bool> read_family_known_contact_target1(
    const FamilyKnownContactReadContext1 &, const StableId &observer,
    const StableId &target, const KnowledgeSourceRef &, const ReadStamp &);
// actual_a/expression 必须由 A 唯一表 owner 借出并核对应关系、固定
// target；本叶只核 该真实头前本人亲历。不会反调 A 完整 validator 或接受 inquiry
// 的假边界。
Status validate_family_known_contact_target_before_fact1(
    const FamilyKnownContactReadContext1 &, const FactRecord &actual_a,
    const FamilyPersonalExpression2 &actual_expression, const StableId &target,
    const KnowledgeSourceRef &, const ReadStamp &);
} // namespace hanmo::resident
