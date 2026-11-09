// P §15.4 / M-CONTINUITY 的实际意向与本人支持；不产生组织、成员或公权。
#pragma once
#include "political_foundation_terms.hpp"
#include "resident/domains/interaction/communication_delivery_read.hpp"
#include "resident/kernel/world_timeline.hpp"

namespace hanmo::resident {
class CommonStore;
struct FoundationInstitutionCatalog;
struct CharacterConditionSources;
struct ProfileControlEligibilitySources;
class CurrentMomentRead;
class ReadStamp;
class ReadStampAuthority;
class DomainReadVersionSource;
struct SocialState;
struct CommunicationLinksState;
class CommunicationSourceRead;
class ExactResultTimeRead;
struct ExactResultReadStamp;

inline constexpr const char *political_continuity_module =
    "political_continuity";
inline constexpr const char *foundation_intent_kind =
    "foundation_intent_created";
inline constexpr const char *foundation_support_kind =
    "foundation_support_decided";

// 当前候选只实现真实玩家控制路径，不能把它注册成 NPC/完整 Lifecycle 能力。
// 意向不可变。新输入另建真实意向，已发生本人决定不自动迁至另一版本。
struct FoundationIntentSource {
  StableId id;
  Integer terms_revision = 1;
  FoundationIntentTerms terms;
  WorldMoment proposed_at;
};
enum class FoundationSupportDecision { accept, refuse };
struct FoundationSupportSource {
  StableId id;
  SourceRef intent_source;
  Integer terms_revision = 1;
  StableId actor_id;
  FoundationSupportDecision decision = FoundationSupportDecision::refuse;
  MomentOwnerSource actual_content_receipt;
  WorldMoment decided_at;
};

// 原唯一 lifecycle_decisions 的有限叶。尚未拥有整个 M-CONTINUITY 根，
// 后续 FoundingDecision/RegistrationResult 仍必须在这一个 union 内接入。
using PoliticalLifecycleDecisionRecord =
    std::variant<FoundationIntentSource, FoundationSupportSource>;

// 所有引用只借同一个未发布 World 候选；外层 retaining owner 校验并保活。
// 不允许独立空表/模拟身体/另一世界控制来源拼成可发布来源组。
struct FoundationPersonalSourceContext {
  const CommonStore &common;
  const OrderedTable<PoliticalLifecycleDecisionRecord> &decisions;
  const PoliticalFoundationRequirementCatalog &requirements;
  const FoundationInstitutionCatalog &institutions;
  const CharacterConditionSources &body;
  const ProfileControlEligibilitySources &control;
  const CurrentMomentRead &clock;
};
struct FoundationSupportContext {
  const FoundationPersonalSourceContext &personal;
  const SocialState &social;
  const CommunicationLinksState &communications;
  const CommunicationSourceRead &communication_sources;
  const ExactResultTimeRead &times;
  const ExactResultReadStamp &result_stamp;
};

// 下列来源读必须先解实际 payload/M，再由原应用前缀查身体；不用当前head。
Status validate_foundation_intent_source_shape(const FoundationIntentSource &);
Status
validate_foundation_support_source_shape(const FoundationSupportSource &);
SourceRef foundation_intent_source(const StableId &);
SourceRef foundation_support_source(const StableId &);
Outcome<const FoundationIntentSource *>
read_foundation_intent(const FoundationPersonalSourceContext &,
                       const SourceRef &, Integer schema);
// P10捕获只读真实源身份/时刻/本人来源，不回调需要同一ExactResult的获知门。
// 不能据此授支持有效性；稳定出口仍必须调用下面完整read/validate核实际获知。
Outcome<const FoundationSupportSource *>
read_foundation_support_result_source(const FoundationPersonalSourceContext &,
                                      const SourceRef &, Integer schema);
Outcome<const FoundationSupportSource *>
read_foundation_support(const FoundationSupportContext &, const SourceRef &,
                        Integer schema);
Status validate_foundation_personal_sources(const FoundationSupportContext &);
// 真实 S3 披露只借原意向正文；不从 Report 字符串、旁人署名或当前条款猜版本。
Outcome<OfficialContentRef>
read_foundation_intent_official_content(const FoundationPersonalSourceContext &,
                                        const SourceRef &, Integer schema);
Outcome<const FoundationIntentSource *>
read_foundation_intent_official_body(const FoundationPersonalSourceContext &,
                                     const OfficialContentRef &);

// 玩家本人显式输入；NPC 原 Director 采用尚未实现，绝不借玩家路径代签。
struct CreateFoundationIntentCommand {
  FoundationIntentTerms terms;
};
struct DecideFoundationSupportCommand {
  SourceRef intent_source;
  Integer expected_terms_revision = 1;
  FoundationSupportDecision decision = FoundationSupportDecision::refuse;
  MomentOwnerSource actual_content_receipt;
};
// 同一实际 ResidentWorld 候选调用，失败必须整候选丢弃；不自行 publish。
// 外层原 seq/result 缓存负责命令重放，此处不把不同真实表达合成同一选择。
Outcome<SourceRef> create_player_foundation_intent_in_candidate(
    CommonStore &, OrderedTable<PoliticalLifecycleDecisionRecord> &,
    const FoundationPersonalSourceContext &, const StableId &actual_actor,
    const CreateFoundationIntentCommand &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);
Outcome<SourceRef> decide_player_foundation_support_in_candidate(
    CommonStore &, OrderedTable<PoliticalLifecycleDecisionRecord> &,
    const FoundationSupportContext &, const StableId &actual_actor,
    const DecideFoundationSupportCommand &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);
} // namespace hanmo::resident
