#include "resident/domains/growth/growth_skill_api.hpp"
#include "resident/domains/growth/rest_estate_view.hpp"
#include "growth_fixed_service_model.hpp"
#include "growth_life_definition_ref.hpp"
#include "growth_terminal_publication.hpp"
#include "growth_trade_grant.hpp"
#include "long_rest_period.hpp"
#include "study_adjustment_time2_model.hpp"
// 成长与既有课程、服务、交易的类型化条款；人物经验、装备和钱货只由共同存储拥有。
#pragma once
#include "growth_actor_start.hpp"
#include "growth_cards.hpp"
#include "growth_insight_cards.hpp"
#include "growth_study_teaching_model.hpp"
#include "growth_teaching_requests.hpp"
#include "resident/domains/family/innate_study_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/model/regional_life_contract.hpp"
#include "resident/storage/common_codec.hpp"
#include "study_service_model.hpp"
#include <functional>
namespace hanmo::resident {

struct GrowthPersonalSkill {
  StableId kind;
  Integer practice_ticks = 1;
};
// 短寿命访问器借用同一候选的接触、战斗与武学目录，不形成第二份领域状态。
struct StudyTerms;
class MarketBusinessCaptureServices;
class GrowthPersonalRecoveryServices;
class PreparedTerminalRecoveryV1;
class GrowthConditionWriteServices;
class OriginalGrowthServiceRelationWrite;
struct GrowthAccess {
  std::function<Outcome<InteractionContext>(const StableId &,
                                            const InteractionContext &,
                                            const StableId &, const StableId &)>
      contact;
  std::function<bool(const StableId &)> battle_occupied;
  std::function<std::optional<GrowthPersonalSkill>(const StableId &)>
      personal_skill;
  std::function<Status(const ActorRecord &, const StableId &)> skill_eligible;
  std::function<std::optional<Integer>(const StableId &)> armor_defense;
  std::function<bool(const StableId &, const StableId &, const StableId &)>
      procurement_authorized;
  std::function<std::vector<std::pair<StableId, StableId>>(const StableId &)>
      procurement_payers;
  std::function<StableId(const StableId &)> task_name;
  std::function<std::optional<RestEstateView>(const StableId &)> rest_estate;
  std::function<std::vector<StableId>(const StableId &)> rest_children;
  std::function<bool(const StableId &)> rest_quiet;
  // 来源由当前同一World的交互叶证明，本域只保存具体教学约定。
  std::function<Outcome<GrowthTeachingRequestQuote>(
      const StableId &, const StableId &, const StableId &)>
      teaching_request;
  std::function<Status(const GrowthTeachingRequestQuote &)>
      revalidate_teaching_request;
  std::function<Status(const GrowthTeachingAgreement &)>
      validate_teaching_agreement;
  std::function<Outcome<GrowthStudyTeachingRequestSource>(
      const StableId &, const StableId &, const StableId &, Integer)>
      study_teaching_request;
  std::function<Status(const GrowthStudyTeachingRequestSource &)>
      revalidate_study_teaching_request;
  std::function<Status(const GrowthStudyTeachingAgreement &,
                       const StudyTerms &)>
      validate_study_teaching_agreement;
  // 只借同候选原生先天来源；历史读取按原StudyFact/action，不重入整World。
  std::function<Outcome<InnateStudyRead>(const StableId &, const StableId &)>
      innate_study_source;
  std::function<Status(const InnateStudySource &, const StableId &)>
      validate_innate_study_source;
  // 显式永久市场profile的同候选原交易scope；缺服务不能绕开Common静态保护。
  MarketBusinessCaptureServices *market_capture = nullptr;
  // 仅显式personal_injury_recovery模块绑定；无新Growth编码旗标或可持久借用。
  GrowthPersonalRecoveryServices *personal_recovery = nullptr;
  // source1终局满工同栈准备值；不进存档、不替代旧逐期服务owner。
  const PreparedTerminalRecoveryV1 *terminal_recovery = nullptr;
  // 返回访问器自持同栈构造的服务桥，复制访问器不留下悬空回调；clone必须重建访问器。
  std::shared_ptr<GrowthPersonalRecoveryServices> personal_recovery_owner;
  // 未选新条件profile保持空；原同栈scope只观察真实写入，不创建另一份人物现值。
  GrowthConditionWriteServices *condition_writer = nullptr;
  std::shared_ptr<GrowthConditionWriteServices> condition_writer_owner;
  // Same owning lease as the original completion; selected Social2 requires it.
  OriginalGrowthServiceRelationWrite *original_service_relation_writer =
      nullptr;
};
struct GrowthLesson {
  StableId skill_id, wallet_id;
  Integer required_trust = 0, study_ticks = 0, fee = 0;
};
struct BargainQuoteContext {
  StableId quote_id, quote_sha256;
  Integer market_revision = 0, valid_tick = 0;
  FieldOrder field_order{};
};
struct StudyChallengeContext {
  Integer budget = 0, quantity = 0, relation_discount_bp = 0;
  FieldOrder field_order{};
  std::optional<BargainQuoteContext> quote;
};
struct StudyChallengeSource {
  StableId field, kind, id;
  std::optional<StudyChallengeContext> context;
};
struct GrowthCourse {
  StableId skill_id, mode, wallet_id;
  std::optional<StudyChallengeSource> challenge;
  // 作者仅对这门新课程显式选政策；旧课程和能力描述字节不被全局升级。
  std::optional<StableId> settlement_policy;
  // 只属于原作者课程行；不复制到运行StudyTerms或从技能名推版本。
  std::optional<GrowthLifeDefinitionRef1> definition_ref;
};
struct ServiceTerms {
  Integer duration = 0, cost = 0, wage = 0, hp_gain = 0, fatigue_delta = 0;
  StableId wallet_id;
  FieldOrder field_order{};
};
// 原作者活动行持有版本地址，运行 ServiceTerms 继续只含原七成员。
struct GrowthActivityDefinition : ServiceTerms {
  std::optional<GrowthLifeDefinitionRef1> definition_ref;
  // 旧作者和原测试原样转换，不补新地址或改变原服务公式。
  GrowthActivityDefinition() = default;
  GrowthActivityDefinition(ServiceTerms terms)
      : ServiceTerms(std::move(terms)) {
  }
  GrowthActivityDefinition(Integer duration, Integer cost, Integer wage,
                           Integer hp_gain, Integer fatigue_delta,
                           StableId wallet_id, FieldOrder field_order = {})
      : ServiceTerms{duration,
                     cost,
                     wage,
                     hp_gain,
                     fatigue_delta,
                     std::move(wallet_id),
                     std::move(field_order)} {
  }
};
struct GrowthRole {
  std::vector<GrowthLesson> lessons;
  std::vector<GrowthCourse> courses;
  OrderedTable<GrowthActivityDefinition> activities;
};
struct BargainConditionSource {
  StableId condition_id, source;
  std::optional<Integer> value;
  FieldOrder field_order{};
};
struct BargainPolicy {
  StableId challenge_id, source_sha256;
  std::vector<BargainConditionSource> condition_sources;
  FieldOrder field_order{};
};
struct GrowthShop {
  StableId id, place_id, inventory_id, wallet_id;
  std::vector<StableId> representative_roles;
  std::optional<BargainPolicy> bargaining;
};
struct GrowthChallengeCondition {
  StableId id, kind, text_key;
  bool matched = false;
  FieldOrder field_order{};
};
struct GrowthChallengeRequirement {
  StableId node_id, option_id;
  FieldOrder field_order{};
};
struct GrowthChallengeOption {
  StableId id, text_key, kind, method;
  Integer points = 0;
  std::vector<GrowthChallengeRequirement> requires_choices;
  std::optional<StableId> supply_condition_id, terms_condition_id;
  FieldOrder field_order{};
};
struct GrowthChallengeNode {
  StableId id, topic_key, stance_key, prompt_key;
  std::vector<GrowthChallengeOption> options;
  FieldOrder field_order{};
};
struct GrowthChallengeDefinition {
  StableId id, kind, source_sha256;
  Integer difficulty = 0;
  std::vector<GrowthChallengeCondition> conditions;
  std::vector<GrowthChallengeNode> nodes;
  FieldOrder field_order{};
  // 仅显式Family自治2读取作者实际版本；旧目录为0，不补默认版本或改旧存档。
  Integer definition_version = 0;
};
struct GrowthCatalog {
  bool cultivation = false, economy = false, family = false,
       regional_life = false, study_segments = false;
  OrderedTable<GrowthRole> roles;
  OrderedTable<GrowthShop> shops;
  OrderedTable<GrowthChallengeDefinition> etiquette, bargain;
  std::vector<StableId> tradeable_items;
  OrderedTable<StableId> place_names, organization_names;
  std::vector<StableId> estate_ids;
  GrowthCardCatalog cards;
  GrowthActorStartPolicy actor_start;
  GrowthTeachingRequestCatalog teaching_requests;
  GrowthInsightCardCatalog insights;
  GrowthStudyTeachingRequestCatalog study_teaching_requests;
  bool innate_learning = false;
  // 只能由完整Catalog模块选择设置；旧来源绝不按字段存在自动升级。
  bool complete_study_sources = false;
  // Trusted physical G+64 selection only; old G/G+32 stay byte-identical.
  bool complete_exact_period_sources = false;
  // Compiled physical G+96 only; no authored toggle and no inference from
  // fields.
  bool complete_exact_command_cuts = false;
  bool complete_exact_rest_mouth_sources = false;
  // Compiled phase4 / G+160, paired with Condition/3 and permanent/2.
  bool complete_capability_application_chain = false;
};
struct GrowthCommand {
  StableId kind, skill_id, item_id, weapon_id, armor_id, activity, shop_id,
      side, payer_wallet_id, action_id, step_id, option_id, estate_id,
      challenge_mode = "automatic";
  Integer ticks = 0, quantity = 0, duration = 0;
  std::optional<Integer> quoted_total, market_revision;
  std::optional<InteractionContext> context;
  std::optional<StableId> bargain_quote_id;
  FieldOrder field_order{}, trade_field_order{};
};
struct ChallengeFrozen {
  StableId field, kind, challenge_id, mode, source_sha256;
  Integer skill_level = 0, auxiliary_attribute = 0;
  std::optional<StudyChallengeContext> context;
  FieldOrder field_order{}, snapshot_order{};
};
struct GrowthChoice {
  StableId step_id, option_id;
  Integer points = 0;
  FieldOrder field_order{};
};
struct GrowthChallengeResult {
  StableId completion;
  Integer score = 0, auto_score = 0, performance_bp = 10000, request_delta = 0,
          discount_bp = 0, relation_discount_bp = 0, challenge_discount_bp = 0;
  std::optional<Integer> raw_score;
  std::vector<GrowthChoice> choices;
  FieldOrder field_order{};
};
struct GrowthChallengeInstance {
  RngStream seed_origin;
  StableId seed, actor_id, source_id, phase = "active";
  ChallengeFrozen frozen;
  std::vector<GrowthChoice> choices;
  std::optional<GrowthChallengeResult> result;
  bool consumed = false;
  FieldOrder field_order{}, state_order{}, binding_order{};
};
struct TrainingTerms {
  StableId skill_id, student_id, teacher_id, place_id, payer_wallet_id,
      recipient_wallet_id;
  Integer duration = 0, mastery_before = 0, mastery_after = 0,
          student_fatigue = 0, teacher_fatigue = 0, fee = 0;
  FieldOrder field_order{};
  std::optional<GrowthTeachingAgreement> teaching_agreement;
};
struct StudyTerms {
  StableId skill_id, mode, student_id, teacher_id, payer_wallet_id,
      recipient_wallet_id;
  InteractionContext context;
  Integer duration = 0, xp_before = 0, teacher_xp = 0, student_fatigue = 0,
          teacher_fatigue = 0, xp_limit = 0, fee = 0;
  std::optional<ChallengeFrozen> challenge;
  // 缺省保留旧末尾结算；明确标记的新课程才逐课时入账。
  std::optional<StableId> settlement_policy;
  FieldOrder field_order{};
  std::optional<GrowthStudyTeachingAgreement> study_teaching_agreement;
  std::optional<InnateStudySource> innate_source;
};
struct TradeTerms {
  StableId shop_id, item_id, side, payer_wallet_id;
  Integer quantity = 0, total = 0, market_revision = 0, valid_tick = 0;
  Owner owner;
  InteractionContext context;
  FieldOrder field_order{};
  std::optional<StableId> bargain_quote_id, bargain_basis_sha256;
  std::optional<Integer> discount_bp;
};
struct EquipmentChoice {
  StableId id;
  Integer defense = 0;
};
struct StudyCancellation {
  StableId action_id;
  Integer completed_lessons = 0, fee_paid = 0, unprovided_released = 0;
};
// 暂停与续学只描述原课程占用变化，不生成新课程或费用。
struct StudyPauseChange {
  StableId action_id;
  bool paused = false;
  Integer paused_ticks = 0, completed_lessons = 0, reserved = 0;
};
struct RestTerms {
  StableId estate_id, food_item_id;
  Integer duration = 0, ration_per_person = 0;
  std::vector<StableId> mouths;
};
struct RestCancellation {
  StableId action_id;
};
struct BargainMarketBasis {
  Integer base_price = 0, stock = 0, elasticity_bp = 5000, target_stock = 100,
          event_bp = 0, available_quantity = 0;
  StableId shop_wallet_id, shop_inventory_id, actor_inventory_id;
  FieldOrder field_order{};
};
struct BargainTerms {
  GrowthCommand request;
  TradeTerms base_quote;
  BargainMarketBasis market_basis;
  StableId source_sha256, challenge_id;
  Integer skill_level = 0, auxiliary_attribute = 0;
  StudyChallengeContext quote_context;
  FieldOrder field_order{}, snapshot_order{};
};
struct BargainPrepared {
  StableId mode, quote_id, basis_sha256;
  BargainTerms terms;
  GrowthChallengeDefinition definition;
  std::optional<GrowthChallengeInstance> before, next;
};
struct GrowthQuote {
  GrowthCommand command;
  Integer duration = 0, cost = 0, wage = 0, expected_xp = 0, target_level = 0;
  std::optional<InteractionContext> context;
  std::variant<EquipmentChoice, TrainingTerms, StudyTerms, ServiceTerms,
               TradeTerms, StudyCancellation, GrowthChallengeInstance,
               RestTerms, RestCancellation, BargainPrepared, StudyPauseChange>
      data;
  std::optional<GrowthChallengeInstance> challenge_before;
};
struct GrowthAction {
  GrowthCommand request;
  GrowthQuote quote;
  FieldOrder field_order{};
};
// 仅共同开工栈使用，不进GrowthAction/Fact或存档；必须在双方占用前取得。
struct GrowthTeachingStartAdmission {
  WorldIdentity source_identity;
  GrowthTeachingAgreement agreement;
  std::optional<GrowthStudyTeachingAgreement> study_agreement;
};
struct TrainingFact {
  StableId phase = "started", action_kind, actor_id;
  Integer start_tick = 0, completed_tick = -1;
  bool success = false;
  TrainingTerms training;
  std::optional<InteractionContext> context;
  FieldOrder field_order{};
  std::optional<GrowthCapabilityApplication> capability_application;
};
// 迟答只补原课时的已确认表现差额；独立收据禁止改写或重新发放基础教学。
struct StudyPerformanceAdjustment {
  Integer settled_tick = 0, xp_before = 0, performance_bp = 10000,
          xp_gained = 0;
  FieldOrder field_order{};
  std::optional<StudyAdjustmentExactBinding> exact_binding;
};
// 分段收据绑定原行动中的唯一课时和真实结算边界，记录已进入人物的净贡献。
struct StudySegment {
  Integer ordinal = 0, settled_tick = 0, xp_before = 0, performance_bp = 10000,
          xp_gained = 0;
  bool performance_pending = false;
  std::optional<StudyPerformanceAdjustment> adjustment;
  FieldOrder field_order{};
  std::optional<GrowthCapabilityApplication> capability_application;
};
struct StudyLesson {
  Integer fatigue_before = 0, injury = 0, xp = 0;
  FieldOrder field_order{};
  std::optional<StudySegment> segment;
};
// 开放暂停以负一哨兵记录；闭合时保存续学疲劳基线和原课时位置。
struct StudyPause {
  Integer start_tick = 0, end_tick = -1, after_lessons = 0,
          student_fatigue = -1, teacher_fatigue = -1, student_xp = -1;
  FieldOrder field_order{};
  // 恢复时冻结已获课程贡献，区分同tick迟答加成和暂停中的外部成长。
  std::optional<Integer> credited_xp;
};
struct StudyFact {
  StableId phase = "started", actor_id, code;
  Integer start_tick = 0, completed_tick = -1;
  bool success = false;
  StudyTerms terms;
  std::vector<StudyLesson> lessons;
  std::optional<std::vector<StudyPause>> pauses;
  std::optional<Integer> xp_settlement_before;
  std::optional<GrowthChallengeInstance> challenge;
  FieldOrder field_order{};
  std::optional<StudyServiceBindingV2> study_service_binding;
  std::optional<StudyServiceTerminalV2> study_service_terminal;
};
struct RestFact {
  StableId actor_id, estate_id, phase = "active", food_item_id, stop_reason;
  Integer start_tick = 0, duration = 0, elapsed = 0, completed_tick = -1,
          ration_per_person = 0, consumed = 0;
  std::vector<StableId> last_mouths;
  FieldOrder field_order{};
  std::vector<LongRestActualPeriod> actual_periods;
};
struct BargainFact {
  StableId actor_id, basis_sha256, consumed_action_id;
  BargainTerms terms;
  GrowthChallengeInstance instance;
  std::optional<TradeTerms> receipt;
  FieldOrder field_order{};
};
using GrowthFact = std::variant<TrainingFact, StudyFact, RestFact, BargainFact>;
// 原交易终局发生点；来源版本固定2，不从Tick或服务累计反推时刻。
struct TradeTerminalTime {
  Integer source_schema = 2;
  WorldMoment ended_at;
};
// 生活结果冻结原条款及实际结算；不从动作消失、计划文字或钱包差额补造收入。
struct LifeActivityRecord {
  LifeActivityView view;
  InteractionContext context;
  std::variant<ServiceTerms, TradeTerms> terms;
  Integer cost_paid = 0, wage_paid = 0;
  FieldOrder field_order{}, view_field_order{};
  // G+32十槽末尾固定保留；service严格null，不借trade时刻伪造rest终态。
  std::optional<UnimplementedLifeFixedValue> selected_trade_settlement;
  std::optional<TradeTerminalTime> trade_terminal_time;
};
struct GrowthState {
  OrderedTable<GrowthAction> actions;
  StudyCapabilityState lesson_capabilities;
  OrderedTable<GrowthFact> facts;
  OrderedTable<LifeActivityRecord> life_activities;
  std::optional<GrowthCardsState> cards;
  OrderedTable<LifeFixedServiceBoundary> fixed_service_boundaries;
  OrderedTable<GrowthTerminalPublication> terminal_publications;
  // Owned here; serialized only in the independent trade-grant/1 section.
  OrderedTable<GrowthTradeGrant> trade_grants;
};
struct GrowthPublicOption {
  StableId id, text, kind, method;
  std::optional<StableId> supply_condition_id, terms_condition_id;
};
struct GrowthPublicCondition {
  StableId id, kind, text;
  bool matched = false;
};
struct GrowthPublicChallenge {
  StableId kind, phase, step_id, topic, stance, prompt;
  Integer round = 0, budget = 0, quantity = 0;
  std::vector<GrowthPublicOption> options;
  std::vector<GrowthPublicCondition> conditions;
  std::optional<GrowthChallengeResult> result;
};
struct GrowthEffect {
  StableId kind, code, skill_id, action_id, quote_id;
  bool success = true, already_completed = false, reopened = false,
       paused = false;
  Integer mastery = 0, xp_gained = 0, fee_paid = 0, unprovided_released = 0,
          completed_lessons = 0, elapsed = 0, paused_ticks = 0, reserved = 0;
  std::optional<GrowthChallengeInstance> challenge;
  std::optional<GrowthPublicChallenge> public_challenge;
  FieldOrder field_order{};
};
struct GrowthStep {
  bool completed = false;
  GrowthEffect effect;
};
// 技艺目录按显式profile开放原十项或完整十六项，仍共用原五个阈值和唯一人物经验。


// 读取不补技能块；写入新六项必须已有明确扩展，不将旧人物静默升级。



Integer growth_lesson_xp(Integer fatigue, Integer injury);
bool growth_command_kind(const StableId &);
bool growth_action_kind(const StableId &);
bool growth_busy_command(const StableId &);
Outcome<GrowthCatalog> decode_growth_catalog(const BoundaryValue &,
                                             const CommonCatalog &,
                                             bool regional_life = false,
                                             bool innate_learning = false);
Outcome<GrowthState> decode_growth_seed(const BoundaryValue &,
                                        bool regional_life = false,
                                        bool cards = false,
                                        bool teaching_requests = false,
                                        bool study_teaching_requests = false,
                                        bool innate_learning = false);
BoundaryValue encode_growth_state(const GrowthState &,
                                  bool regional_life = false,
                                  bool cards = false,
                                  bool teaching_requests = false,
                                  bool study_teaching_requests = false,
                                  bool innate_learning = false);
// 只在新世界初态登记明确来源；冷恢复必须读取原账，不重新初始化。
Status initialize_growth_card_state(const CommonState &, GrowthState &,
                                    const GrowthCatalog &);
Outcome<GrowthCommand> decode_growth_command(const StableId &,
                                             const BoundaryValue &);
BoundaryValue encode_growth_command(const GrowthCommand &);
Outcome<GrowthQuote> preview_growth(const CommonStore &, const GrowthState &,
                                    const GrowthCatalog &,
                                    const CommonCatalog &, const SocialState &,
                                    const GrowthAccess &, const StableId &,
                                    const GrowthCommand &,
                                    const StableId &own_action = {});
Outcome<std::optional<GrowthTeachingStartAdmission>>
prepare_growth_start(const CommonStore &, const GrowthCatalog &,
                     const GrowthAccess &, const StableId &,
                     const GrowthQuote &);
Status start_growth(CommonStore &, GrowthState &, const GrowthCatalog &,
                    const CommonCatalog &, const SocialState &,
                    const GrowthAccess &, const ActionCoreView &,
                    const GrowthQuote &,
                    const GrowthTeachingStartAdmission *admission = nullptr);
Outcome<GrowthStep> step_growth(CommonStore &, GrowthState &,
                                const GrowthCatalog &, const CommonCatalog &,
                                SocialState &, InteractionState &,
                                const GrowthAccess &, const ActionCoreView &);
Outcome<GrowthEffect>
apply_growth_instant(CommonStore &, GrowthState &, const GrowthCatalog &,
                     const CommonCatalog &, const SocialState &,
                     const GrowthAccess &, const StableId &,
                     const GrowthQuote &);
Status validate_growth(const CommonStore &, const GrowthState &,
                       const GrowthCatalog &, const CommonCatalog &,
                       const GrowthAccess &);
Outcome<BoundaryValue> encode_growth_quote(const GrowthQuote &);
BoundaryValue encode_growth_effect(const GrowthEffect &);
Outcome<GrowthEffect> decode_growth_effect(const StableId &,
                                           const BoundaryValue &);
Status visit_growth_checkpoint(CodecVisitor &, GrowthState &,
                               bool regional_life = false, bool cards = false);
Status visit_growth_effect(CodecVisitor &, GrowthEffect &);
// 有限查询只返回本人的进度、当前可接触课程与公开题面，不暴露教师账本或全世界。
Outcome<BoundaryValue>
growth_courses_view(const CommonStore &, const GrowthState &,
                    const GrowthCatalog &, const CommonCatalog &,
                    const SocialState &, const GrowthAccess &, const StableId &,
                    const InteractionContext &, Integer ticks = 2);
Outcome<BoundaryValue>
growth_skills_view(const CommonStore &, const GrowthState &,
                   const GrowthCatalog &, const CommonCatalog &,
                   const StableId &, const StableId &selected_skill = {});
Outcome<BoundaryValue> growth_challenge_view(const CommonStore &,
                                             const GrowthState &,
                                             const GrowthCatalog &,
                                             const CommonCatalog &,
                                             const StableId &, const StableId &,
                                             bool etiquette_only = false);
bool growth_manual_window(const CommonStore &, const GrowthState &,
                          const StableId &);
Outcome<BoundaryValue> growth_rest_rows(const CommonStore &,
                                        const GrowthState &,
                                        const GrowthCatalog &,
                                        const GrowthAccess &, const StableId &);
Outcome<BoundaryValue>
query_growth_view(const CommonStore &, const GrowthState &,
                  const GrowthCatalog &, const CommonCatalog &,
                  const SocialState &, const InteractionState &,
                  const GrowthAccess &, const StableId &actor_id,
                  const StableId &query_id, const BoundaryValue &args);
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthState> decode_growth_capability_chain_seed(const BoundaryValue &,
                                                         const GrowthCatalog &);
Outcome<BoundaryValue>
encode_growth_capability_chain_state(const GrowthState &,
                                     const GrowthCatalog &);
Status visit_growth_capability_chain_checkpoint(CodecVisitor &, GrowthState &,
                                                const GrowthCatalog &);
} // namespace hanmo::resident
