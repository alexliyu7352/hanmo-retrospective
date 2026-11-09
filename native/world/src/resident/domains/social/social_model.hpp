// 社会关系、事件与每人的记忆仅在此typed域拥有，公共事实ID不另复制payload。
#pragma once
#include "../../model/dynamic_contract.hpp"
#include "../../storage/common_codec.hpp"
#include "social_relation_result.hpp"
#include "social_time2_model.hpp"
#include <functional>
namespace hanmo::resident {
struct DirectorCatalog;
struct DirectorState;
struct RelationRecord {
  std::optional<StableId> id;
  StableId from_actor, to_actor;
  Integer trust = 0, affinity = 0;
  FieldOrder field_order{};
};
struct SocialMemory {
  Integer learned_tick = 0;
  StableId source_actor_id;
  FieldOrder field_order{};
};
struct SocialEventOriginalApplication {
  MomentOwnerSource original_period_opening;
  WorldMoment occurred_at;
};
struct SocialEvent {
  StableId id, kind;
  Integer tick = 0;
  StableId place_id, actor_id, target_actor_id, item_id;
  Integer quantity = 0;
  StableId reason, fact_id;
  FieldOrder field_order{};
  // Only selected social/2: original Calendar source and the actual event M.
  std::optional<SocialEventOriginalApplication> original_application;
};
// 报告只存来源事实的有界观察；收报永不再次执行钱粮、交付或组织结果。
enum class ReportVisibility { participants, local_public, organization };
enum class ReportReliability { witnessed, official, hearsay };
enum class MessageStatus { pending, delivered, cancelled };
struct Report {
  StableId id, fact_id, payload_kind;
  SubjectRef origin_subject;
  Integer observation_tick = 0;
  ReportVisibility visibility_scope = ReportVisibility::participants;
  std::optional<Integer> valid_until_tick;
  ReportReliability reliability_kind = ReportReliability::witnessed;
  DynamicFactView observation;
};
struct PendingMessage {
  StableId id, report_id;
  SubjectRef sender, receiver;
  StableId channel;
  Integer created_tick = 0, due_tick = 0;
  MessageStatus status = MessageStatus::pending;
  // version2当面分享冻结当时场所；普通延时渠道不能借此绕过下一半日限制。
  std::optional<StableId> local_place_id;
  // 新3/4在原receiver槽编码tag；legacy接收者仍保原强类型，未选地址不得静默使用。
  std::optional<SocialS3AddressBinding> address_binding;
};
struct KnownReport {
  SubjectRef receiver;
  StableId report_id;
  std::optional<StableId> source_message_id;
  Integer learned_tick = 0;
  SubjectRef source_subject;
};
struct DynamicSocialState {
  Integer version = 1, last_collected_tick = -1;
  OrderedTable<Report> reports;
  OrderedTable<PendingMessage> pending_messages;
  OrderedTable<OrderedTable<KnownReport>> known_reports;
  std::optional<SocialTime2State> time2;
};
struct SocialState {
  Integer last_tick = 0;
  OrderedTable<RelationRecord> relations;
  OrderedTable<OrderedTable<SocialMemory>> memories;
  OrderedTable<SocialEvent> events;
  FieldOrder field_order{};
  std::optional<DynamicSocialState> dynamic;
  // Selected by the actual social physical section, never inferred from rows.
  Integer original_relation_version = 1;
  OrderedTable<SocialRelationResult> relation_results;
};
struct SocialPolicy {
  StableId kind, target_actor_id, food_item_id;
  Integer interval_ticks = 0;
  bool share_news = false;
};
struct SocialCatalog {
  OrderedTable<SocialPolicy> policies;
  // Only explicitly prepared social/2 retains the immutable authored seed.
  std::optional<OriginalSocialRelationCatalog> original_relations;
};
// 只读公共历史事实的明确社会投影；未知事实保持不可见，已知类型缺实现不得伪造亲历。
struct SocialObservedFact {
  StableId id, kind, actor_id;
  Integer tick = 0;
  std::vector<StableId> participants;
};
class SocialFactRead {
public:
  virtual ~SocialFactRead() = default;
  virtual std::optional<SocialObservedFact>
  social_fact(const StableId &) const = 0;
  virtual bool permits_named_membership_facts() const { return false; }
};
struct SocialEffects {
  bool applied = false;
  std::vector<StableId> events;
};
// 问讯冻结一条原知识来源；none表示本次无新消息，不以另一条消息替换旧报价。
struct SocialNewsQuote {
  enum class Kind { none, legacy_fact, life_report };
  Kind kind = Kind::none;
  StableId source_id;
};
// 新profile初始化显式启用，旧profile检查点保持原字段布局。
Status initialize_dynamic_social(SocialState &, bool regional_life = false);
Status collect_dynamic_reports(CommonStore &, SocialState &,
                               const DynamicResultRead &, Integer tick,
                               const DirectorCatalog *routes = nullptr,
                               const DirectorState *registry = nullptr);
Status deliver_dynamic_reports(CommonStore &, SocialState &,
                               const DynamicStepInput &);
Outcome<StableId> queue_dynamic_report(CommonStore &, SocialState &,
                                       const StableId &report_id,
                                       const SubjectRef &sender,
                                       const SubjectRef &receiver,
                                       const StableId &channel,
                                       Integer created_tick, Integer due_tick);
Status observe_dynamic_report(const CommonStore &, SocialState &,
                              const StableId &report_id,
                              const SubjectRef &receiver, Integer tick);
Status validate_dynamic_social(const CommonStore &, const SocialState &,
                               const DynamicResultRead *results = nullptr,
                               bool named_membership = false);
Outcome<DynamicSocialState> decode_dynamic_social(const BoundaryValue &);
BoundaryValue encode_dynamic_social(const DynamicSocialState &);
Status visit_dynamic_social_checkpoint(CodecVisitor &, DynamicSocialState &);
// 作者准许的work/trade类别逐次传入；只分享真实参与者已知的终态，报价不改变知识。
// 看管例外只由同世界真实来源读取；默认空回调保留原不可用拒绝。
Status
preview_share_life_report(const CommonStore &, const SocialState &,
                          const StableId &report_id, const StableId &speaker,
                          const StableId &listener,
                          const std::vector<StableId> &allowed_categories,
                          const std::function<Outcome<bool>(const StableId &)>
                              &custody_dialogue_available = {});
// 看管例外只由同世界真实来源读取；默认空回调保留原不可用拒绝。
Status share_life_report(CommonStore &, SocialState &,
                         const StableId &report_id, const StableId &speaker,
                         const StableId &listener,
                         const std::vector<StableId> &allowed_categories,
                         const std::function<Outcome<bool>(const StableId &)>
                             &custody_dialogue_available = {});
// 中央先计算pin与保留来源；这里只清理明确过期的生活报告及其已完成传播，不处理其他消息。
Status prune_life_reports(SocialState &,
                          const std::vector<StableId> &removed_fact_ids);
// 原ask_news先分享一条较早社会记忆，再考虑作者准许的本人生活报告；交互owner另核真实会话。
// 看管例外只由同世界真实来源读取；默认空回调保留原不可用拒绝。
Outcome<SocialNewsQuote>
preview_social_news(const CommonStore &, const SocialState &,
                    const CommonCatalog &, const StableId &listener,
                    const StableId &speaker,
                    const std::vector<StableId> &allowed_categories,
                    const SocialFactRead *facts = nullptr,
                    const std::function<Outcome<bool>(const StableId &)>
                        &custody_dialogue_available = {});
// 看管例外只由同世界真实来源读取；默认空回调保留原不可用拒绝。
Outcome<SocialEffects>
apply_social_news(CommonStore &, SocialState &, const CommonCatalog &,
                  const StableId &listener, const StableId &speaker,
                  const SocialNewsQuote &,
                  const std::vector<StableId> &allowed_categories,
                  const SocialFactRead *facts = nullptr,
                  const std::function<Outcome<bool>(const StableId &)>
                      &custody_dialogue_available = {});
Integer social_trust(const SocialState &, const StableId &from,
                     const StableId &to);
Status social_change_relation(CommonWrite &, SocialState &,
                              const StableId &from, const StableId &to,
                              Integer trust_delta, Integer affinity_delta);
Status validate_social(const CommonStore &, const SocialState &,
                       const CommonCatalog &,
                       const SocialFactRead *facts = nullptr,
                       std::optional<Integer> unselected_clock_origin = {},
                       bool exact_supply_source_scope = false);
Outcome<SocialEffects> step_social(CommonStore &, SocialState &,
                                   const SocialCatalog &, const CommonCatalog &,
                                   const ActivityRead &, Integer finish_tick,
                                   const SocialFactRead *facts = nullptr);
Outcome<SocialEffects> hear_social_news(CommonStore &, SocialState &,
                                        const CommonCatalog &,
                                        const StableId &listener,
                                        const StableId &speaker, Integer tick,
                                        const SocialFactRead *facts = nullptr);
Outcome<SocialCatalog> decode_social_catalog(const BoundaryValue &data);
Outcome<SocialState> decode_social_seed(const BoundaryValue &state);
BoundaryValue encode_social_state(const SocialState &);
Status visit_social_checkpoint(CodecVisitor &, SocialState &);
} // namespace hanmo::resident
