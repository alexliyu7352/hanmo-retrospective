#pragma once
#include "dialogue_command.hpp"
#include "household_purchase_report_model.hpp"
#include "interaction_response_sources.hpp"
#include "occupancy/actor_social_occupancy.hpp"
#include "visit_messages/visit_message_receipt_state.hpp"
#include "visit_messages/visit_reception_state.hpp"
// 会面/对白/事件的唯一类型化状态；所有vector保旧字典插入序，查找不改变该序。
#include "appointments/personal_appointment_state.hpp"
#include "born_dialogues/born_personal_dialogue_state.hpp"
#include "communication_links.hpp"
#include "contact_inquiries/contact_inquiry_state.hpp"
#include "personal_dialogues/known_personal_dialogue_state.hpp"
#include "report_disclosure_types.hpp"
#include "resident/domains/conditions/condition_model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/common_model.hpp"
#include "resident/model/regional_life_contract.hpp"
#include "runtime_offer_knowledge.hpp"
#include "visit_appointments/visit_appointment_meeting_state.hpp"
#include "visit_appointments/visit_appointment_state.hpp"
#include "visit_messages/visit_message_state.hpp"
#include "visit_replies/visit_reply_state.hpp"
#include <functional>
namespace hanmo::resident {
struct SocialNewsQuote;
struct PartyInvitationCatalog;
struct PartyRecruitmentConsentCatalog;
struct VisitMessageCatalog;
struct PartyRecruitmentConsentQuote;
// 场所上下文没有kind字段；路遇上下文显式携带encounter身份，禁止继承岗位服务。

struct ExactEncounterOrigin {
  MomentOwnerSource contact_source;
  WorldMoment created_at;
};
// Immutable outcome of a real zero-time dialogue opening at a say/choice node.
// Its identity is the original session ID; it does not record consent or a
// message delivery.
struct FaceToFaceContactResult {
  StableId session_id, session_actor_id, provider_actor_id, dialogue_id,
      first_contact_node_id, first_contact_phase;
  InteractionContext context;
  WorldMoment occurred_at;
};
// The real zero-time telling result belongs to the original interaction source.
// Message delivery/knowledge remain only in Social and communication_links.
struct FaceToFaceReportDisclosure {
  StableId id, contact_source_id, sender_actor_id, recipient_actor_id,
      report_id;
  StableId dialogue_node_id, dialogue_phase;
  SourceRef sender_knowledge_source;
  ReportDisclosurePermission permission;
  WorldMoment disclosed_at;
};
struct FaceToFaceContactSources {
  Integer version = 1;
  OrderedTable<FaceToFaceContactResult> results;
  OrderedTable<FaceToFaceReportDisclosure> report_disclosures;
  // /3 only: plan-scoped actual telling, never a cohort known_reports table.
  OrderedTable<HouseholdPurchaseReportDisclosure> purchase_plan_disclosures;
};
struct EncounterRecord {
  StableId id;
  std::array<StableId, 2> leaders;
  std::vector<StableId> participants;
  std::array<Location, 2> anchors;
  std::array<std::vector<StableId>, 2> anchor_field_orders;
  std::vector<StableId> anchors_order, action_ids_order, resume_paused_order;
  std::array<StableId, 2> action_ids;
  std::array<bool, 2> resume_paused{};
  Integer created_tick = 0, resolved_tick = -1;
  std::optional<ExactEncounterOrigin> exact_origin;
  std::optional<WorldMoment> resolved_at;
  StableId status = "pending", result_id;
  bool talked = false;
  std::vector<StableId> field_order;
};
struct FlagRecord {
  StableId id;
  bool value = false;
};
struct KnowledgeSource {
  StableId kind, id;
  std::vector<StableId> field_order;
};
struct KnowledgeRecord {
  StableId offer_id;
  Integer discovered_tick = 0;
  KnowledgeSource source;
  std::vector<StableId> field_order;
};
struct ActorKnowledge {
  StableId actor_id;
  std::vector<KnowledgeRecord> offers;
};
// 当前旅程不会执行业务action节点；类型可表示作者命令，触及未支持命令必须能力失败。
// DialogueCommand has one unchanged definition in dialogue_command.hpp.
struct ActionCompletion;
struct DialogueOutcome {
  bool present = false, ok = false;
  std::optional<bool> success;
  StableId code;
  std::vector<StableId> field_order;
  std::shared_ptr<const ActionCompletion> completion;
};
struct InteractionSession {
  StableId actor_id, id, dialogue_id, node_id, phase, pending_action_id;
  InteractionContext context;
  std::optional<DialogueCommand> pending_command;
  DialogueOutcome last_outcome;
  bool trade_completed = false;
  std::vector<StableId> field_order;
  // 来源只属于这次已打开的会话；正式动作完成后仍保留历史绑定但不重新索取旧份额。
  std::optional<RegionalOpportunityBinding> regional_binding;
  bool regional_action_committed = false;
};
// 曝光只在成功打开会话后登记，读取和失败报价不消费来源轮次。
struct RegionalExposure {
  StableId observer_actor_id, binding_id, episode;
  Integer last_tick = 0;
  RegionalSourceRef source;
};
struct RegionalInteractionState {
  Integer version = 1;
  std::vector<RegionalExposure> exposures;
};
struct EventRecord {
  StableId key, event_id, actor_id, place_id, status;
  Integer last_tick = 0;
  std::vector<StableId> field_order;
};
struct InteractionState {
  std::optional<InteractionResponseSources> response_sources;
  std::optional<ActorSocialOccupancySources> social_occupancy;
  std::optional<CommunicationLinksState> communication_links;
  std::optional<RuntimeOfferKnowledgeState> runtime_offer_knowledge;
  std::vector<FlagRecord> flags;
  std::vector<ActorKnowledge> knowledge;
  std::vector<EventRecord> events;
  std::vector<InteractionSession> interactions;
  std::vector<EncounterRecord> encounters;
  std::optional<FaceToFaceContactSources> face_to_face_contacts;
  std::optional<RegionalInteractionState> regional;
  std::optional<VisitMessageState> visit_messages;
  std::optional<VisitMessageReceiptState> visit_message_receipts;
  std::optional<VisitReceptionState> visit_reception_claims;
  std::optional<PersonalAppointmentState> personal_appointments;
  std::optional<ContactInquiryState> contact_inquiries;
  std::optional<VisitReplyState> visit_message_replies;
  std::optional<VisitAppointmentState> visit_appointment_invitations;
  std::optional<VisitAppointmentMeetingState> visit_appointment_meetings;
  std::optional<BornPersonalDialogueState> born_personal_dialogues;
  std::optional<KnownPersonalDialogueState> known_personal_dialogues;
};
struct DialogueOption {
  StableId id, next, text_key;
  std::vector<Condition> conditions;
};
struct DialogueNode {
  StableId id, kind, next, speaker, text_key, if_true, if_false, on_success,
      on_failure;
  std::optional<Condition> condition;
  std::optional<DialogueCommand> command;
  std::vector<DialogueOption> options;
};
struct DialogueDefinition {
  StableId id, context_role, entry;
  std::vector<DialogueNode> nodes;
};
struct EventDefinition {
  StableId id, place_id, trigger, on_unavailable, dialogue_id;
  bool once_per_actor = false;
  Integer cooldown_ticks = 0;
  std::vector<Condition> conditions;
};
struct ActorDialogueDefinition {
  StableId actor_id;
  std::optional<StableId> dialogue_id, road_dialogue_id;
};
struct EncounterRouteDefinition {
  StableId id, origin, destination, line;
  Integer duration_ticks = 0;
};
struct PlaceAdmission {
  StableId id;
  std::vector<Condition> conditions;
};
// 发现来源只编译作者原allowlist，已获知记录仍唯一保存在InteractionState。
struct PropDefinition {
  StableId id, place_id, name_key, kind;
  std::vector<StableId> offer_ids;
};
struct OfferDiscoveryDefinition {
  StableId offer_id;
  std::vector<KnowledgeSource> sources;
};
struct CompanionRoleDefinition {
  StableId role_id;
  Integer required_trust = 0;
};
struct SceneCatalog;
// 作者只声明愿意分享的本人结果类别或真实岗位需求；没有任意执行脚本。
struct RegionalActorTopic {
  StableId id, actor_id, dialogue_id, activity;
  Integer cooldown_ticks = 0;
};
struct RegionalNeedTopic {
  StableId id, role_id, dialogue_id, item_id;
  Integer cooldown_ticks = 0;
};
// 作者结果主题只绑定本人既有任务模板与原办理岗位，不提供执行命令。
struct RegionalOutcomeTopic {
  StableId id, role_id, task_template_id, dialogue_id;
  Integer cooldown_ticks = 0;
};
struct InteractionCatalog {
  Integer response_sources_version =
      0; // Explicit physical 5..13; no state inference.
  std::vector<PlaceAdmission> admissions;
  bool road_encounters = false;
  bool npc_society =
      false; // 冷目录原能力声明，只控制已有近事询问入口，不推断人物能力。
  std::vector<StableId> flags, place_ids, settlement_ids, map_ids, offer_ids;
  std::shared_ptr<const CommonCatalog> common;
  std::vector<ActorDialogueDefinition> actor_dialogues;
  std::vector<DialogueDefinition> dialogues;
  std::vector<EventDefinition> events;
  std::vector<EncounterRouteDefinition> routes;
  std::vector<PropDefinition> props;
  std::vector<OfferDiscoveryDefinition> discoveries;
  std::vector<CompanionRoleDefinition> companions;
  std::shared_ptr<const SceneCatalog> scene;
  bool regional_enabled = false;
  std::vector<RegionalActorTopic> regional_actor_topics;
  std::vector<RegionalNeedTopic> regional_need_topics;
  std::vector<RegionalOutcomeTopic> regional_outcome_topics;
  // 仅显式邀请profile安装此只读目录；空值维持旧岗位的招募语义。
  std::shared_ptr<const PartyInvitationCatalog> party_invitations;
  std::shared_ptr<const PartyRecruitmentConsentCatalog>
      party_recruitment_consents;
  std::shared_ptr<const VisitMessageCatalog> visit_messages;
};
struct RoleAssignmentView {
  StableId actor_id;
  bool enabled = false;
};
// 只引用A的岗位与B的行旅，不复制岗位任命或旅程参数。
struct InteractionAccess : ConditionAccess {
  // Selected owner history writes through the same candidate only.
  bool social_occupancy_selected = false;
  ActorSocialOccupancyWrite *social_occupancy = nullptr;
  std::function<std::optional<RoleAssignmentView>(const StableId &)> role;
  std::function<bool(const StableId &)> battle_occupied;
  std::function<bool(const StableId &, const PointI &)> terrain_point_valid;
  std::function<Status(const StableId &, const StableId &, bool)>
      set_journey_encounter;
  // S1 proves this exact contact is still the current spatial/action binding;
  // true only for its real released arrival terminal.
  std::function<Outcome<bool>(const StableId &, const MomentOwnerSource &)>
      exact_contact_side;
  std::function<Outcome<std::pair<StableId, bool>>(const StableId &)>
      journey_encounter;
  std::function<StableId(const StableId &)> army_id;
  std::function<Outcome<InteractionContext>(const StableId &, const StableId &,
                                            const StableId &)>
      returned_retainer_context;
  std::function<bool(const StableId &)> shop_exists;
  std::function<bool(const StableId &, const InteractionContext &)>
      shop_representative;
  std::function<Status(const StableId &, const std::optional<PartyRecord> &)>
      write_party;
  std::function<Status(const StableId &)> automatic_discovery_guard;
  // 新服务准入与已接受行动续行分开；旧profile未绑定时保持原检查。
  std::function<Status(const StableId &, const StableId &, const StableId &,
                       const StableId &)>
      service_admission;
  // 只证明该实际会话绑定新出生模板打开表；不按provider/prototype泛放私人会面。
  std::function<Outcome<bool>(const InteractionSession &)>
      born_personal_session;
  std::function<Outcome<bool>(const InteractionSession &)>
      known_personal_session;
  // 中央绑定同一候选的有限来源读取与Social分享，交互域不读取远方私账。
  std::function<Outcome<std::vector<RegionalOpportunityBinding>>(
      const StableId &, const StableId &)>
      regional_candidates;
  std::function<Status(const RegionalOpportunityBinding &)> regional_share;
  // 本人公托回访的补句只读取原合同结果；无回调时不能伪造成功文字。
  std::function<Outcome<StableId>(const RegionalOpportunityBinding &)>
      regional_outcome_text;
  // 当面问讯只借当前会话的真实说话者，Social仍拥有来源与知情变更。
  std::function<Outcome<std::shared_ptr<const SocialNewsQuote>>(
      const StableId &, const StableId &)>
      news_preview;
  std::function<Outcome<std::vector<StableId>>(
      const StableId &, const StableId &, const SocialNewsQuote &)>
      news_apply;
  // 仅借同世界真实看管来源豁免普通对白的
  // unavailable；不授予岗位服务或离开许可。
  std::function<Outcome<bool>(const StableId &)> custody_dialogue_available;
  // 同锁公开供货告示的真实 Inspect 来源；预览不写，执行沿同候选唯一 writer。
  bool supply_inspection_selected = false;
  std::function<Status(const StableId &, const StableId &)>
      inspect_supply_preview;
  std::function<Status(const StableId &, const StableId &)>
      inspect_supply_apply;
};
struct EncounterTalk {
  StableId encounter_id;
};
struct EncounterLeave {
  StableId encounter_id;
};
struct Talk {
  StableId role_id;
  StableId topic_id{};
};
struct TalkActor {
  StableId actor_id, topic_id;
};
struct AskNews {};
struct OpenEvent {
  StableId event_id;
};
struct DialogueNext {};
struct DialogueChoose {
  StableId option_id;
};
struct EndInteraction {};
struct Inspect {
  StableId prop_id;
};
struct DiscoverOffer {
  StableId offer_id, prop_id;
  std::optional<InteractionContext> context;
  bool offer_id_valid = true, has_prop_id = false;
};
struct JoinParty {
  StableId actor_id;
  std::optional<InteractionContext> context;
  std::optional<StableId> return_receipt_id;
};
struct DismissCompanion {
  StableId actor_id;
};
struct DialogueConfirm {};
struct DialogueCancel {};
struct CloseTrade {};
struct OpenTrade {
  StableId shop_id;
  std::optional<InteractionContext> context;
};
using InteractionCommand =
    std::variant<EncounterTalk, EncounterLeave, Talk, OpenEvent, DialogueNext,
                 DialogueChoose, EndInteraction, Inspect, DiscoverOffer,
                 JoinParty, DismissCompanion, DialogueConfirm, DialogueCancel,
                 CloseTrade, OpenTrade, TalkActor, AskNews>;
struct InteractionQuote {
  Integer duration = 0;
  StableId encounter_id, dialogue_id, next, event_id;
  std::optional<InteractionContext> context;
  std::optional<KnowledgeSource> source;
  std::optional<StableId> party_id, target;
  std::optional<Integer> cost_per_tick;
  std::optional<RegionalOpportunityBinding> regional_binding;
  std::shared_ptr<const SocialNewsQuote> news_quote;
  std::shared_ptr<const PartyRecruitmentConsentQuote> party_recruitment_consent;
};
struct InteractionEffect {
  StableId encounter_id, settlement_result;
  std::optional<StableId> party_id{}, person_id{};
  std::optional<std::vector<StableId>> news_ids;
};
const EncounterRecord *pending_encounter(const InteractionState &,
                                         const StableId &);
bool interaction_occupied(const InteractionState &, const StableId &);
std::optional<bool> flag_value(const InteractionState &, const StableId &);
bool anchors_meet(const InteractionCatalog &, const Location &,
                  const Location &);
Outcome<InteractionContext> validate_interaction(
    const CommonRead &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &, const InteractionContext &,
    const StableId &service = {}, const StableId &own_action = {});
Outcome<InteractionQuote>
preview_interaction(const CommonRead &, const InteractionState &,
                    const InteractionCatalog &, const InteractionAccess &,
                    const StableId &, const InteractionCommand &);
Outcome<InteractionEffect> apply_interaction(
    CommonWrite &, InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &, const InteractionCommand &,
    const InteractionQuote &, const WorldMoment *exact_at = nullptr,
    InteractionSelectedResponsePending *response_pending = nullptr);
// 单一安装入口的请求只借用B已经按稳定pair顺序选定的命中；apply_motion调用B原截断/暂停规则。
struct EncounterInstall {
  std::array<StableId, 2> leaders, action_ids;
  std::array<Location, 2> anchors;
  std::array<std::vector<StableId>, 2> anchor_field_orders;
  std::vector<StableId> anchors_order, action_ids_order, resume_paused_order;
  std::array<bool, 2> resume_paused{};
  std::function<Status(const StableId &, bool)> apply_motion;
};
Outcome<StableId>
install_encounter(CommonWrite &, InteractionState &,
                  const StableId &player_actor_id, const EncounterInstall &,
                  const std::vector<StableId> &retained_encounter_ids = {},
                  const ExactEncounterOrigin *exact_origin = nullptr);
Status pump_dialogue(CommonWrite &, InteractionState &,
                     const InteractionCatalog &, const InteractionAccess &,
                     const StableId &);
Status enter_place_events(const CommonRead &, InteractionState &,
                          const InteractionCatalog &, const InteractionAccess &,
                          const StableId &);
Status reconcile_events(const CommonRead &, InteractionState &,
                        const InteractionCatalog &, const InteractionAccess &);
Status validate_interaction_state(const CommonRead &, const InteractionState &,
                                  const InteractionCatalog &,
                                  const InteractionAccess &);
Status visit_interaction_checkpoint(CodecVisitor &, InteractionState &,
                                    bool regional = false);
Status visit_interaction_selected_checkpoint(CodecVisitor &, InteractionState &,
                                             Integer selected_physical_version);
} // namespace hanmo::resident
