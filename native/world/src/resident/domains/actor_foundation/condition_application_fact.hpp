// 已选本人表达能力的真实身体应用次序；沿原 receipt 同 ID，不保存第二身体账。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <string_view>
namespace hanmo::resident {
class CommonStore;
struct ActorFoundationCatalog;
struct AuthoredActorFoundationState;
struct BoundActorConditionWrite;
struct CatalogIdentity;
struct ConditionReceiptV2;
struct FactRecord;
// 只选原家庭/政治及首次 private_estate_services/2 本人表达历史能力；
// source/1 + Condition8 also owns this real history; it never selects Family.
// 同一个选择器核重复/未知版本，旧未选 profile 不补历史。
enum class ConditionApplicationHistoryOwner {
  family_autonomy,
  political_continuity,
  estate_residence,
  injury_sources
};
Outcome<bool> condition_application_facts_selected(const CatalogIdentity &);
// 消费者仍只借自己真实已选能力；任一其它能力已选不授跨域历史准入。
Outcome<bool>
condition_application_facts_selected(const CatalogIdentity &,
                                     ConditionApplicationHistoryOwner);
bool owns_condition_application_fact_kind(std::string_view) noexcept;
// 唯一实际 writer 叶。只认同候选真实 capture，必须在原首写后、通知前调用。
// 未选保旧无头写法；任一失败须由外层丢弃整个 World 候选，不能吞错发布。
Status append_condition_application_fact_in_candidate(
    CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, const BoundActorConditionWrite &);
struct ConditionApplicationFactView {
  const FactRecord *header = nullptr;
  const ConditionReceiptV2 *receipt = nullptr;
  Integer source_schema = 2;
};
class ConditionApplicationFactVisitor {
public:
  virtual ~ConditionApplicationFactVisitor() = default;
  virtual Status
  original_condition_application(const ConditionApplicationFactView &) = 0;
};
// 仅头/正文身份和次序覆盖，不替代各原 cause/能力/生效时间的完整验证。
Status validate_condition_application_fact_headers(
    const CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &);
Status visit_condition_application_fact_headers(
    const CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, ConditionApplicationFactVisitor &);
// 原 Common 头借读，拒同值副本；不分配普通 SourceRef 字符串。
Outcome<ConditionApplicationFactView> read_condition_application_fact_header(
    const CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, const FactRecord &);
// once 分支只反验已存在同头，不重追加；旧未选仍不要求新头。
Status validate_condition_application_fact_for_receipt(
    const CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, const ConditionReceiptV2 &);
} // namespace hanmo::resident
