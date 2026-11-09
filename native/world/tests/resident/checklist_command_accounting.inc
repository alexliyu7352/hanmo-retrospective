// 共用提交逐条核真实命令与独立 Opening；保留原 PIN、回执及历史裁剪合同。

namespace checklist_command_accounting {

// 只解码这次原 submit 前的银行；派生读缓存不安装回正在运行的世界。
std::unique_ptr<WorldBundle>
snapshot(const Bytes &bank,
         const std::shared_ptr<const PreparedCatalog> &catalog) {
  auto seed = must(decode_world_checkpoint(bank, *catalog),
                   "accounting decode original pre-submit bank");
  auto world = std::make_unique<WorldBundle>(catalog, std::move(seed));
  must(restore_native_transport_derived_values_in_candidate(*world),
       "accounting restore original transport read cache");
  must(restore_native_complete_director_wake_cache_in_candidate(*world),
       "accounting restore original director read cache");
  must(world->validate(), "accounting validate original pre-submit snapshot");
  return world;
}

// 比较原持久回执全部字段及存在位；不手开 profile，也不以显示投影回退。
// 此完成窗的基础 variant 使用原默认访客；不支持的 profile 明确阻止核账。
// 冷快照不含 MilitaryEffect 的 integration_request/casualty_source_ids，
// 包括 AdvanceDetails 内嵌值；这里不声称比较了这些仅热返回字段。
Bytes persistent_receipt(const std::shared_ptr<const CommitResult> &receipt) {
  require(bool(receipt), "accounting original cached receipt required");
  Bytes bytes;
  CodecVisitor writer(bytes);
  auto borrowed = receipt;
  must(visit_resident_receipt(writer, borrowed),
       "accounting original persistent receipt visitor");
  must(writer.finish(), "accounting complete original receipt fields");
  return bytes;
}

// 原历史的字段顺序也是持久值；按原前缀逐项保持，不仅比较显示结果 ID。
bool same_history(const HistoryRecord &before, const HistoryRecord &after) {
  return before.tick == after.tick && before.kind == after.kind &&
         before.actor_id == after.actor_id &&
         before.result_id == after.result_id &&
         before.field_order == after.field_order;
}

// 只收原专业 owner 枚举的实际行动身份；重复源不能算第二次真实提交。
class ActionIds final : public ActionTimeOwnerIdSink {
public:
  std::set<StableId> ids;
  Status append(std::string_view id) override {
    if (id.empty() || !ids.insert(StableId(id)).second)
      return common_failure("ChecklistDuplicateOriginalAction", StableId(id));
    return common_success();
  }
};

// 该读片段在返回前结束，不能把前态 owner 或 read stamp 借到后态。
std::set<StableId> original_actions(const WorldBundle &world) {
  const auto at = must(world.actual_phase_moment(), "accounting pre actual M");
  const auto phase =
      must(world.enter_phase_read_context("checklist_command_accounting",
                                          "common", {at, at}, at),
           "accounting pre original action read phase");
  const auto owner = must(world.complete_exact_time_bindings(),
                          "accounting pre original action owner");
  require(bool(owner), "accounting pre original action owner missing");
  ActionIds result;
  for (const auto &registration : owner->reads().runtime.action_times)
    must(owner->reads().action_sources.enumerate_action_time_owners(
             registration, result),
         "accounting enumerate original pre action owners");
  return std::move(result.ids);
}

// 来源标识只作本次核账去重与证据；不能写入世界成为第二份成果账。
StableId source_key(const SourceRef &source) {
  return source.domain + "/" + source.kind + "/" + source.id;
}

// 用同后态实际 M 下的原注册表核 NPC 行动，不按本次碰巧出现的 actor 白名单。
SourceRef action_source(const WorldBundle &before, const WorldBundle &after,
                        const std::set<StableId> &old_actions,
                        const CommonCommandSourceView &command,
                        const StableId &action) {
  require(!action.empty() && !old_actions.count(action) &&
              !before.common.action(action),
          "accounting NPC receipt must name a newly submitted original action");
  const auto at =
      must(after.actual_phase_moment(), "accounting action actual M");
  const auto phase =
      must(after.enter_phase_read_context("checklist_command_accounting",
                                          "common", {at, at}, at),
           "accounting post original action read phase");
  const auto owner = must(after.complete_exact_time_bindings(),
                          "accounting post original action owner");
  require(bool(owner), "accounting post original action owner missing");
  const ActionTimeRegistration *registration = nullptr;
  for (const auto &candidate : owner->reads().runtime.action_times) {
    if (candidate.action_kind != command.result->kind)
      continue;
    require(!registration, "accounting original action registration aliased");
    registration = &candidate;
  }
  require(registration,
          "accounting unsupported NPC action source registration: " +
              command.result->kind);
  const auto proof = must(owner->reads().action_sources.read_action_time_owner(
                              *registration, action),
                          "accounting NPC original action source");
  const auto started =
      must(owner->reads().action_sources.read_action_time_source(
               proof.started_source),
           "accounting NPC original start moment");
  const auto previous =
      must(before.actual_phase_moment(), "accounting original window start");
  require(proof.action_ref == action &&
              proof.action_kind == command.result->kind &&
              proof.actor_id == command.history->actor_id &&
              proof.source_schema == registration->source_schema &&
              proof.original_execution_source.domain ==
                  registration->owner_domain &&
              !proof.original_execution_source.id.empty() &&
              started.half_day_tick == command.result->tick &&
              must(compare_world_moments(started, previous),
                   "accounting action starts within submit window") >= 0 &&
              must(compare_world_moments(started, at),
                   "accounting action has no future source") <= 0,
          "accounting NPC action, command, actor and actual source must match");
  return proof.original_execution_source;
}

// 零时接受沿原军令 source reader；旧 campaign 或别人的 source 不能抵一笔命令。
SourceRef campaign_source(const WorldBundle &before, const WorldBundle &after,
                          const CommonCommandSourceView &command,
                          const MilitaryEffect &effect) {
  require(command.result->kind == "campaign_accept" && effect.success &&
              !effect.campaign_id.empty(),
          "accounting unsupported NPC instant military source: " +
              command.result->kind);
  for (const auto &old : before.military.campaigns)
    require(old.id != effect.campaign_id,
            "accounting NPC command reused an old campaign acceptance");
  const auto at =
      must(after.actual_phase_moment(), "accounting campaign actual M");
  const auto phase =
      must(after.enter_phase_read_context("checklist_command_accounting",
                                          "military", {at, at}, at),
           "accounting original campaign source read phase");
  const auto owner = must(after.complete_exact_time_bindings(),
                          "accounting original campaign owner");
  require(bool(owner), "accounting original campaign owner missing");
  const auto military =
      must(owner->read_military(at), "accounting actual military source lease");
  require(bool(military), "accounting actual military source lease missing");
  const auto bindings = military->bindings();
  require(bindings.action_time && bindings.office,
          "accounting actual military authority sources missing");
  const MilitaryAuthorityTimeContext context{
      bindings.common, bindings.military, *bindings.action_time,
      bindings.elapsed, *bindings.office};
  const SourceRef source{"military", "campaign_accepted", effect.campaign_id};
  const auto *campaign =
      must(read_military_campaign_acceptance_source(context, source, 1),
           "accounting original campaign acceptance");
  require(campaign && campaign->exact_acceptance &&
              campaign->id == effect.campaign_id &&
              campaign->actor_id == command.history->actor_id &&
              campaign->accepted_tick == command.result->tick,
          "accounting campaign receipt and original acceptance must match");
  // CampaignAccept 的回执不携带 office_id；原 acceptance reader
  // 已核原职权来源。
  const auto &accepted = campaign->exact_acceptance->accepted_at;
  require(must(compare_world_moments(accepted,
                                     must(before.actual_phase_moment(),
                                          "accounting campaign window start")),
               "accounting campaign within actual window") >= 0 &&
              must(compare_world_moments(accepted, at),
                   "accounting campaign has no future source") <= 0,
          "accounting campaign original moment must lie inside submit window");
  return source;
}

// 原 move_local 返回 Done，没有 source backlink；只接受同 actor 全窗双向唯一。
// 多次移动不能按 tick、ID 字串或任意排序猜配对；前后位置由原完整历史核验。
SourceRef local_move_source(const WorldBundle &before, const WorldBundle &after,
                            const CommonCommandSourceView &command,
                            Integer actor_command_count) {
  require(
      actor_command_count == 1,
      "accounting unsupported multiple local moves for one actor in window");
  require(before.movement_exact && after.movement_exact &&
              command.result->details &&
              std::holds_alternative<Done>(command.result->details->data),
          "accounting original local move roots and Done receipt required");
  const LocalMoveSource *original = nullptr;
  for (const auto &[key, candidate] :
       after.movement_exact->local_moves.rows()) {
    if (before.movement_exact->local_moves.find(key) ||
        candidate.actor_id != command.history->actor_id)
      continue;
    require(!original,
            "accounting unsupported ambiguous local move original sources");
    require(key == candidate.id,
            "accounting original local move source key must match");
    original = &candidate;
  }
  require(original,
          "accounting local move command needs one new actual source");
  // verify 的 after 只取已成功完整 validate 的 run.snapshot()；该原校验已调用
  // validate_native_local_move_world_sources，逐前驱验证
  // from/to，不再逐命令重复全验。
  const SourceRef source{"travel", "local_move", original->id};
  const auto *validated = must(
      read_local_move_source(after.common, *after.movement_exact, source, 1),
      "accounting original local move source and Common prefix");
  require(
      validated == original &&
          original->occurred_at.half_day_tick == command.result->tick &&
          must(
              compare_world_moments(original->occurred_at,
                                    must(before.actual_phase_moment(),
                                         "accounting local move window start")),
              "accounting local move begins within actual window") >= 0 &&
          must(compare_world_moments(original->occurred_at,
                                     must(after.actual_phase_moment(),
                                          "accounting local move window end")),
               "accounting local move has no future source") <= 0,
      "accounting unique local move command, actor and source must match");
  return source;
}

// 不以 result 数字相邻、当前岗位或同 tick 猜来源；只借原采用记录的完整身份。
DirectorAcceptedCommandView inspection_acceptance(
    const WorldBundle &before, const WorldBundle &after,
    const CommonCommandSourceView &command) {
  require(command.history && command.result && command.result->ok &&
              command.result->kind == "inspect" && command.result->details &&
              std::holds_alternative<InteractionEffect>(
                  command.result->details->data) &&
              before.catalog.get() == after.catalog.get() &&
              after.catalog->director && before.director.subject2 &&
              after.director.subject2,
          "accounting inspection original command and Director owners required");
  const DirectorAdoptionResult2 *decision = nullptr;
  for (const auto &[key, row] : after.director.subject2->adoption_results.rows()) {
    if (!row.accepted_command ||
        row.accepted_command->result_id != command.result->result_id)
      continue;
    require(!decision && !before.director.subject2->adoption_results.find(key) &&
                row.subject.kind == SubjectKind::actor &&
                row.subject.id == command.history->actor_id &&
                row.registration_epoch > 0 && row.decision_seq > 0 &&
                key == director_adoption_key2(row.subject,
                                               row.registration_epoch,
                                               row.decision_seq),
            "accounting inspection adoption identity must be new and unique");
    decision = &row;
  }
  require(decision,
          "accounting inspection requires its original adopted command");
  const auto view = must(
      read_director_accepted_command(
          after.common, after.director, *after.catalog->director,
          after.catalog->identity,
          {command.history->actor_id, decision->registration_epoch,
           decision->decision_seq, command.result->kind, command.request_seq,
           command.result->result_id}),
      "accounting inspection original actor/epoch/decision/policy/receipt join");
  require(view.decision == decision &&
              view.command == &*decision->accepted_command &&
              view.actual_result == command.result &&
              view.command->actual_effect.source_schema == 1 &&
              is_public_supply_inspection_source(
                  view.command->actual_effect.source_ref),
          "accounting inspection accepted command must borrow its real receipt");
  return view;
}

// 只编码一条 Inspect 直接依赖的原 Report/观察时间，可选本人 Knowledge/取得时间。
// 使用原选定 codec 全字段，不手写缩减 Report，也不拿整个 Social 表禁合法新增。
V inspection_social_values(const WorldBundle &world,
                           const PublicSupplyInspection &inspection,
                           bool include_knowledge) {
  require(world.social.dynamic && world.social.dynamic->time2,
          "accounting inspection original social owner required");
  const auto &social = *world.social.dynamic;
  const auto *report = social.reports.find(inspection.report_id);
  require(report, "accounting inspection original report missing");
  DynamicSocialState selected;
  selected.version = social.version;
  selected.time2.emplace();
  selected.reports.insert(inspection.report_id, *report);
  const OwnedSocialIndexKey report_key(
      SocialTimeKey{ReportObservationKey{inspection.report_id}});
  const auto *observed = social.time2->source_time_bindings.find(report_key.value());
  require(observed, "accounting inspection original report time binding missing");
  selected.time2->source_time_bindings.insert(report_key.value(), *observed);
  if (include_knowledge) {
    const SubjectRef actor{SubjectKind::actor, inspection.actor_id};
    const auto *known = find_social_knowledge(social, actor, inspection.report_id);
    const OwnedSocialIndexKey receiver_key(actor);
    const OwnedSocialIndexKey knowledge_key(
        SocialTimeKey{SocialKnowledgeKey{actor, inspection.report_id}});
    const auto *learned =
        social.time2->source_time_bindings.find(knowledge_key.value());
    require(known && learned,
            "accounting inspection original retained Knowledge missing");
    OrderedTable<KnownReport> knowledge;
    knowledge.insert(inspection.report_id, *known);
    selected.known_reports.insert(receiver_key.value(), knowledge);
    selected.time2->source_time_bindings.insert(knowledge_key.value(), *learned);
  }
  return must(encode_dynamic_social_time2(selected, social.version),
              "accounting inspection original social values codec");
}

// 后态互相一致不够：Inspect 引用的旧观察/报告必须仍是前态原值。
// 窗内新观察/报告合法；旧 inspection 的本人知识/精确绑定亦不因重看刷新。
void preserve_inspection_evidence(const WorldBundle &before,
                                  const WorldBundle &after,
                                  const PublicSupplyInspection &inspection,
                                  bool retained_inspection) {
  const auto &id = inspection.original_observation.source_ref.id;
  if (const auto *old = before.local_economy.supply_observations.find(id)) {
    const auto *now = after.local_economy.supply_observations.find(id);
    require(now, "accounting inspection original supply observation missing");
    const auto &a = old->observation;
    const auto &b = now->observation;
    require(old->id == now->id && old->tick == now->tick &&
                old->publicly_visible == now->publicly_visible &&
                a.source_key == b.source_key &&
                a.organization_id == b.organization_id &&
                a.site_id == b.site_id && a.inventory_id == b.inventory_id &&
                a.item_id == b.item_id && a.source_fact_id == b.source_fact_id &&
                a.target_amount == b.target_amount &&
                a.physical_amount == b.physical_amount &&
                a.source_revision == b.source_revision,
            "accounting inspection original supply observation changed");
  }
  if (before.social.dynamic &&
      before.social.dynamic->reports.find(inspection.report_id))
    exact(inspection_social_values(before, inspection, retained_inspection),
          inspection_social_values(after, inspection, retained_inspection),
          "accounting inspection original report/Knowledge/time values changed");
}

// 冷读核完整取得链，源 reader 核公告、原供给 observation、既有 report 和因果顺序；
// 本处另核本人 Knowledge 及精确取得绑定，不能把有 inspection 行当作已知报告。
SourceRef inspection_chain(const WorldBundle &before, const WorldBundle &after,
                           const CommonCommandSourceView &command,
                           const SourceRef &source,
                           const WorldMoment *accepted_at = nullptr) {
  require(before.public_supply_inspection && after.public_supply_inspection &&
              after.catalog->interaction && after.catalog->local_economy &&
              after.catalog->public_supply_inspection &&
              before.social.dynamic && after.social.dynamic &&
              after.social.dynamic->time2 &&
              !before.public_supply_inspection->results.find(source.id) &&
              !before.common.state().facts.find(source.id),
          "accounting inspection requires a new original source and owners");
  WorldCurrentMomentRead clock(after);
  const PublicSupplyInspectionReadContext context{
      after.common, after.social, *after.catalog->interaction,
      *after.catalog->local_economy, after.local_economy,
      *after.catalog->public_supply_inspection,
      *after.public_supply_inspection, clock};
  const auto *original = must(read_public_supply_inspection_source(context,
                                                                  source, 1),
                              "accounting inspection observation/report chain");
  require(original && original->id == source.id &&
              original->actor_id == command.history->actor_id &&
              original->inspected_at.half_day_tick == command.result->tick &&
              must(compare_world_moments(
                       original->inspected_at,
                       must(before.actual_phase_moment(),
                            "accounting inspection window start")),
                   "accounting inspection begins in actual window") >= 0 &&
              (!accepted_at ||
               must(compare_world_moments(original->inspected_at, *accepted_at),
                    "accounting inspection original acceptance moment") == 0),
          "accounting inspection actor and original command moment mismatch");
  preserve_inspection_evidence(before, after, *original, false);
  const SubjectRef actor{SubjectKind::actor, command.history->actor_id};
  const auto &social = *after.social.dynamic;
  // 原 selected_native_public_supply_inspection 准入要求 common-cut 能力；
  // 未选旧 Social profile 不补字段，也不套用本 Inspect/1 取得规则。
  require(social_knowledge_cut_version(social.version),
          "accounting inspection selected Knowledge cut profile required");
  const auto *known = find_social_knowledge(social, actor, original->report_id);
  const OwnedSocialIndexKey knowledge_key(
      SocialTimeKey{SocialKnowledgeKey{actor, original->report_id}});
  const auto *binding =
      social.time2->source_time_bindings.find(knowledge_key.value());
  const auto *key = binding ? std::get_if<SocialKnowledgeKey>(&binding->key)
                            : nullptr;
  const auto *time = binding ? std::get_if<SocialExactSource>(&binding->time_source)
                             : nullptr;
  require(!find_social_knowledge(*before.social.dynamic, actor,
                                 original->report_id) &&
              known && known->report_id == original->report_id &&
              same_social_subject(known->receiver, actor) &&
              same_social_subject(known->source_subject, actor) &&
              !known->source_message_id &&
              known->learned_tick == original->inspected_at.half_day_tick &&
              key && key->report_id == original->report_id &&
              same_social_subject(key->receiver, actor) && time &&
              time->original_timed_source_ref.source_schema == 1 &&
              source_key(time->original_timed_source_ref.source_ref) ==
                  source_key(source) &&
              source_key(binding->actual_source_ref) == source_key(source),
          "accounting inspection must have its actor's original Knowledge");
  must(validate_social_knowledge_cut(after.common, social, *binding),
       "accounting inspection Knowledge original causal cut");
  // public_supply_inspection.hpp 声明独占候选先写源再写唯一 Knowledge；
  // 只适配此选定 writer 的真实末头，不改通用 Knowledge 的 source<=cut 规则。
  require(binding->learned_after_common_fact &&
              source_key(*binding->learned_after_common_fact) == source_key(source),
          "accounting inspection Knowledge must retain original acquisition cut");
  return source;
}

// NPC 的原 typed 回执没有 prop/source backlink；原 adopted binding 是唯一入口。
SourceRef inspection_source(const WorldBundle &before, const WorldBundle &after,
                            const CommonCommandSourceView &command) {
  const auto accepted = inspection_acceptance(before, after, command);
  return inspection_chain(before, after, command,
                           accepted.command->actual_effect.source_ref,
                           &accepted.command->accepted_at);
}

// 玩家命令有原 Inspect payload；只在同 actor/prop 唯一新增时核链。
// 普通道具 Inspect 和重看已知报告可无新增，不为凑数量伪造 Director adoption。
SourceRef player_inspection_source(const WorldBundle &before,
                                   const WorldBundle &after,
                                   const CommonCommandSourceView &command,
                                   const CommandEnvelope &request) {
  const auto *interaction =
      std::get_if<InteractionCommand>(&request.payload->data);
  const auto *inspect = interaction ? std::get_if<Inspect>(interaction) : nullptr;
  require(inspect && command.result->details &&
              std::holds_alternative<InteractionEffect>(
                  command.result->details->data),
          "accounting player inspection requires its original typed payload");
  if (!after.public_supply_inspection)
    return {};
  require(bool(before.public_supply_inspection),
          "accounting inspection original pre-state required");
  const PublicSupplyInspection *selected = nullptr;
  for (const auto &[key, row] : after.public_supply_inspection->results.rows()) {
    if (before.public_supply_inspection->results.find(key) ||
        row.actor_id != command.history->actor_id)
      continue;
    require(!selected && row.prop_id == inspect->prop_id,
            "accounting player inspection source must be unique and match prop");
    selected = &row;
  }
  return selected ? inspection_chain(
                        before, after, command,
                        {"interaction", public_supply_inspection_source_kind,
                         selected->id})
                  : SourceRef{};
}

// 全窗可以有多个 NPC 和多个 Inspect；按原 source 一对一覆盖，不套单命令 +1。
// 原前缀逐值/顺序保留；遗漏命令、重用来源或孤立新源都不能被总数相等掩盖。
void inspection_coverage(const WorldBundle &before, const WorldBundle &after,
                         const std::set<StableId> &domain_sources) {
  require(bool(before.public_supply_inspection) ==
              bool(after.public_supply_inspection),
          "accounting inspection owner selection changed inside window");
  if (!after.public_supply_inspection)
    return;
  const auto &old = *before.public_supply_inspection;
  const auto &now = *after.public_supply_inspection;
  require(old.version == 1 && now.version == 1 &&
              old.results.size() <= now.results.size() &&
              old.results.order().size() == old.results.size() &&
              now.results.order().size() == now.results.size(),
          "accounting inspection original source prefix shape changed");
  for (std::size_t i = 0; i < now.results.size(); ++i) {
    const auto &[key, row] = now.results.rows()[i];
    require(key == row.id && now.results.order()[i] == key,
            "accounting inspection original source key/order mismatch");
    if (i < old.results.size()) {
      const auto &[old_key, previous] = old.results.rows()[i];
      require(old_key == key && old.results.order()[i] == key &&
                  previous.id == row.id && previous.actor_id == row.actor_id &&
                  previous.prop_id == row.prop_id &&
                  previous.report_id == row.report_id &&
                  source_key(previous.original_observation.source_ref) ==
                      source_key(row.original_observation.source_ref) &&
                  previous.original_observation.source_schema ==
                      row.original_observation.source_schema &&
                  previous.source_key == row.source_key &&
                  previous.settlement_id == row.settlement_id &&
                  previous.place_id == row.place_id &&
                  must(compare_world_moments(previous.inspected_at,
                                             row.inspected_at),
                       "accounting inspection original prefix moment") == 0,
              "accounting inspection original source prefix changed");
      preserve_inspection_evidence(before, after, previous, true);
    } else {
      require(!old.results.find(key) &&
                  domain_sources.count(source_key(
                      {"interaction", public_supply_inspection_source_kind,
                       key})) == 1,
              "accounting every new inspection source needs its sole command");
    }
  }
}

// 按真实 typed 回执找原业务源；不支持的映射是测试核账缺口，不冒称产品错误。
SourceRef npc_source(const WorldBundle &before, const WorldBundle &after,
                     const std::set<StableId> &old_actions,
                     const CommonCommandSourceView &command,
                     const std::map<StableId, Integer> &local_move_counts) {
  require(bool(command.result->details),
          "accounting NPC original typed details required");
  if (command.result->kind == "inspect")
    return inspection_source(before, after, command);
  if (command.result->kind == "move_local") {
    const auto count = local_move_counts.find(command.history->actor_id);
    require(count != local_move_counts.end(),
            "accounting original local move command count required");
    return local_move_source(before, after, command, count->second);
  }
  if (const auto *started =
          std::get_if<ActionStarted>(&command.result->details->data))
    return action_source(before, after, old_actions, command,
                         started->action_id);
  if (const auto *military =
          std::get_if<MilitaryEffect>(&command.result->details->data)) {
    if (!military->action_id.empty())
      return action_source(before, after, old_actions, command,
                           military->action_id);
    return campaign_source(before, after, command, *military);
  }
  throw std::runtime_error(
      "accounting unsupported NPC original source mapping: " +
      command.result->kind);
}

// 逐个原 period 身份交叉核 Social/Economic，再经原图验证每个 provider。
// 多个 provider 证明同一次 Opening，不按 provider 件数累加世界 revision。
Integer opening_commits(const WorldBundle &before, const WorldBundle &after,
                        const CommandEnvelope &request) {
  require(before.world_timeline && after.world_timeline &&
              before.social_period_sources && after.social_period_sources &&
              before.production_cycles && after.production_cycles,
          "accounting actual selected period owners required");
  std::set<StableId> social, economic;
  for (const auto &[key, row] : after.social_period_sources->openings.rows()) {
    if (before.social_period_sources->openings.find(key))
      continue;
    require(key == row.period_id && !key.empty(),
            "accounting actual social opening identity required");
    social.insert(key);
  }
  for (const auto &[key, row] :
       after.production_cycles->period_openings.rows()) {
    if (before.production_cycles->period_openings.find(key))
      continue;
    require(key == row.period_id && !row.source_id.empty(),
            "accounting actual economic opening identity required");
    economic.insert(key);
  }
  require(
      social == economic,
      "accounting independent opening owners must name the same new periods");
  const bool opens =
      std::holds_alternative<AdvanceCommand>(request.payload->data) &&
      before.world_timeline->boundary_state == PeriodBoundaryState::ready;
  require(
      social.size() == (opens ? 1U : 0U),
      "accounting original advance/Ready boundary must match actual openings");
  Integer count = 0;
  for (const auto &id : social) {
    const auto *social_row = after.social_period_sources->openings.find(id);
    const auto *economic_row =
        after.production_cycles->period_openings.find(id);
    require(
        social_row && economic_row &&
            must(compare_world_moments(social_row->opened_at,
                                       economic_row->opening_at),
                 "accounting same original opening moment") == 0 &&
            must(compare_world_moments(
                     social_row->opened_at,
                     must(before.actual_phase_moment(),
                          "accounting original opening boundary")),
                 "accounting opening begins at original boundary") == 0 &&
            social_row->through_tick == economic_row->through_tick &&
            after.world_timeline->period_id == id &&
            after.world_timeline->opening_receipt_ref == id,
        "accounting original Opening references must identify one real period");
    // 原图逐 provider 自开同后态的读片段，外层不得再开一层或借留旧 stamp。
    require(!after.has_active_read_phase(),
            "accounting period validator owns its original read phases");
    // 此图来自同一已锁目录，不为取图构造需要当前读片段的完整领域 owner。
    require(after.catalog && before.catalog.get() == after.catalog.get() &&
                after.catalog->complete_exact_time_runtime,
            "accounting same original selected Opening runtime required");
    must(validate_exact_registered_period_sources(
             after.catalog->complete_exact_time_runtime->phase_graph, after,
             {id, social_row->opened_at, social_row->through_tick},
             after.world_timeline->boundary_state),
         "accounting all original period provider sources");
    ++count;
  }
  return count;
}

// 用后态原领域 PIN 与完整旧记录重放原裁剪；只改独立副本，不补造新回执。
// 新回执若已被裁掉，上层缺证即失败；这里绝不靠 revision 差补回它。
V verify_retention(const WorldBundle &before, const WorldBundle &after,
                   const std::set<StableId> &old_history_ids) {
  auto expected = std::make_unique<WorldBundle>(after);
  const auto &old = before.common.state();
  const auto &now = after.common.state();
  auto &candidate = expected->common.state();
  candidate.results = old.results;
  for (const auto &[key, receipt] : now.results.rows()) {
    const auto *previous = old.results.find(key);
    if (previous) {
      require(persistent_receipt(*previous) == persistent_receipt(receipt),
              "accounting old original receipt bytes changed: " + key);
    } else {
      candidate.results.set(key, receipt);
    }
  }
  candidate.history = old.history;
  for (const auto &row : now.history)
    if (!old_history_ids.count(row.result_id))
      candidate.history.push_back(row);
  must(prune_complete_command_sources(*expected),
       "accounting original PIN plus 64 receipts and 2000 histories pruning");
  require(candidate.results.size() == now.results.size(),
          "accounting cache deletion must match original pruning contract");
  for (std::size_t i = 0; i < candidate.results.size(); ++i) {
    const auto &wanted = candidate.results.rows()[i];
    const auto &actual = now.results.rows()[i];
    require(wanted.first == actual.first &&
                persistent_receipt(wanted.second) ==
                    persistent_receipt(actual.second),
            "accounting retained original receipt bytes/order mismatch");
  }
  require(candidate.history.size() == now.history.size(),
          "accounting history deletion must match original pruning contract");
  for (std::size_t i = 0; i < candidate.history.size(); ++i)
    require(same_history(candidate.history[i], now.history[i]),
            "accounting retained original history bytes/order mismatch");
  Integer evicted_receipts = 0, evicted_history = 0;
  for (const auto &[key, receipt] : old.results.rows())
    if (!now.results.find(key))
      ++evicted_receipts;
  std::set<StableId> retained_history;
  for (const auto &row : now.history)
    retained_history.insert(row.result_id);
  for (const auto &row : old.history)
    if (!retained_history.count(row.result_id))
      ++evicted_history;
  return boundary::obj({{"original_prune_verified", V{true}},
                        {"old_persistent_receipts_unchanged", V{true}},
                        {"old_receipts_legally_evicted", V{evicted_receipts}},
                        {"old_histories_legally_evicted", V{evicted_history}},
                        {"unpinned_receipt_limit", V{Integer{64}}},
                        {"unpinned_history_limit", V{Integer{2000}}}});
}

// 只有逐条原回执、seq、history、领域源和 Opening 均闭合，才接受 revision 实数。
// observation 为空时纯核验；失败报告仅保存已观测实值，不补造未到达的通过位。
bool verify(Run &run, const Bytes &before_bank, const CommandEnvelope &request,
            const std::shared_ptr<const CommitResult> &returned,
            const fs::path &observation) {
  Integer openings = 0, player_count = 0, command_count = 0;
  Integer npc_count = 0, outer_publications = 0;
  bool openings_verified = false, outer_publication_verified = false;
  V before_revision, after_revision, candidate_revision, publication_revision;
  V retention;
  V::Array rows;
  StableId stage = "decode_original_banks";
  const auto report = [&](bool complete, const StableId &error) {
    if (observation.empty())
      return;
    write_json(
        observation / "command-accounting.json",
        boundary::obj(
            {{"schema", V{"hanmo.completion-command-accounting.v3"}},
             {"complete", V{complete}},
             {"stage", V{stage}},
             {"failure", V{error}},
             {"opening_commits", V{openings}},
             {"opening_sources_verified", V{openings_verified}},
             {"counter_scope", V{"fully_verified_entries_only"}},
             {"real_command_results", V{command_count}},
             {"command_commits", V{command_count}},
             {"command_commits_meaning", V{"real_results_not_publications"}},
             {"npc_results", V{npc_count}},
             {"outer_publications", V{outer_publications}},
             {"outer_publication_verified", V{outer_publication_verified}},
             {"player_receipts", V{player_count}},
             {"candidate_start_revision", candidate_revision},
             {"publication_revision", publication_revision},
             {"before_revision", before_revision},
             {"after_revision", after_revision},
             {"commands", V{rows}},
             {"retention", retention},
             {"receipt_equality_scope",
              V{"complete_original_persisted_receipt_bytes"}},
             {"hot_receipt_equality_claimed", V{false}}}));
  };
  try {
    const auto before = snapshot(before_bank, run.catalog);
    const auto after = run.snapshot();
    const auto &old = before->common.state();
    const auto &now = after->common.state();
    before_revision = V{old.identity.revision};
    after_revision = V{now.identity.revision};
    stage = "original_request_and_sequence";
    require(
        request.payload && returned && returned->ok &&
            request.actor_id == old.metadata.player_actor_id &&
            request.actor_id == now.metadata.player_actor_id,
        "accounting original successful player request and receipt required");
    const auto *old_player_seq = old.request_sequences.find(request.actor_id);
    const auto *new_player_seq = now.request_sequences.find(request.actor_id);
    require(old_player_seq && new_player_seq &&
                *old_player_seq < std::numeric_limits<Integer>::max() &&
                request.request_seq == *old_player_seq + 1 &&
                *new_player_seq == request.request_seq,
            "accounting player sequence must increase exactly once");

    stage = "actual_new_receipts_and_histories";
    std::set<StableId> old_history_ids, old_result_ids;
    for (const auto &row : old.history)
      require(old_history_ids.insert(row.result_id).second,
              "accounting old original histories must be unique");
    for (const auto &[key, receipt] : old.results.rows()) {
      require(bool(receipt), "accounting old cached receipt missing");
      require(old_result_ids.insert(receipt->result_id).second,
              "accounting old original receipts must be unique");
    }
    std::map<StableId, CommonCommandSourceView> new_results;
    for (const auto &[key, receipt] : now.results.rows()) {
      if (old.results.find(key))
        continue;
      require(bool(receipt), "accounting new cache entry has no real receipt");
      require(!old_result_ids.count(receipt->result_id) &&
                  !old_history_ids.count(receipt->result_id),
              "accounting new command cannot reuse an old result identity");
      const auto source =
          must(read_common_command_source(after->common,
                                          std::string_view(receipt->result_id)),
               "accounting original cache/history/sequence source");
      require(source.result == receipt.get() && source.cache_key == key &&
                  new_results.emplace(receipt->result_id, source).second,
              "accounting each command must have one unique real receipt");
    }
    std::vector<CommonCommandSourceView> commands;
    std::set<StableId> new_history;
    // 原 history 的实际追加序就是命令封口序；不按 ID、tick 或猜出的序号配对。
    for (const auto &row : now.history) {
      if (old_history_ids.count(row.result_id))
        continue;
      const auto found = new_results.find(row.result_id);
      require(
          found != new_results.end(),
          "accounting evidence missing: newly committed receipt was evicted "
          "or absent for actual history " +
              row.result_id);
      require(new_history.insert(row.result_id).second,
              "accounting duplicate new original command history");
      commands.push_back(found->second);
    }
    require(
        new_results.size() == new_history.size(),
        "accounting new receipts and original histories must be one-to-one");
    stage = "original_retention_contract";
    retention = verify_retention(*before, *after, old_history_ids);

    std::map<StableId, Integer> local_move_counts;
    bool needs_action_source = false;
    for (const auto &command : commands) {
      if (command.result->kind == "move_local")
        ++local_move_counts[command.history->actor_id];
      if (command.history->actor_id != request.actor_id &&
          command.result->kind != "move_local")
        needs_action_source = true;
    }
    const auto old_actions =
        needs_action_source ? original_actions(*before) : std::set<StableId>{};
    stage = "actual_opening_sources";
    openings = opening_commits(*before, *after, request);
    openings_verified = true;
    require(old.identity.revision <=
                std::numeric_limits<Integer>::max() - openings,
            "accounting opening revision overflow");
    const Integer candidate_start = old.identity.revision + openings;
    require(candidate_start < std::numeric_limits<Integer>::max(),
            "accounting publication revision overflow");
    const Integer published_revision = candidate_start + 1;
    candidate_revision = V{candidate_start};
    publication_revision = V{published_revision};
    std::map<StableId, Integer> sequences;
    for (const auto &[actor, sequence] : old.request_sequences.rows())
      sequences.emplace(actor, sequence);
    std::set<StableId> domain_sources;
    stage = "per_command_original_sources";
    for (const auto &command : commands) {
      const auto &receipt = *command.result;
      const auto &actor = command.history->actor_id;
      const auto previous_sequence = sequences.find(actor);
      rows.push_back(boundary::obj(
          {{"actor", V{actor}},
           {"sequence", V{command.request_seq}},
           {"kind", V{receipt.kind}},
           {"result_id", V{receipt.result_id}},
           {"revision", V{receipt.revision}},
           {"prior_sequence", previous_sequence == sequences.end()
                                  ? V{}
                                  : V{previous_sequence->second}},
           {"candidate_start_revision", V{candidate_start}},
           {"expected_publication_revision", V{published_revision}},
           {"verified", V{false}}}));
      require(previous_sequence != sequences.end(),
              "accounting unsupported new actor original initial sequence: " +
                  actor);
      require(previous_sequence->second < std::numeric_limits<Integer>::max() &&
                  command.request_seq == previous_sequence->second + 1 &&
                  receipt.revision == published_revision,
              "accounting evidence missing or mismatch: actual command "
              "sequence must advance once and each receipt must share R+1");
      ++previous_sequence->second;
      SourceRef domain;
      if (actor == request.actor_id) {
        const auto *cached = now.results.find(StableId(command.cache_key));
        require(command.request_seq == request.request_seq && cached &&
                    receipt.kind == request.payload->kind &&
                    persistent_receipt(returned) == persistent_receipt(*cached),
                "accounting returned player result must equal its sole real "
                "cached receipt");
        if (receipt.kind == "move_local")
          domain = local_move_source(*before, *after, command,
                                     local_move_counts.at(actor));
        if (receipt.kind == "inspect")
          domain = player_inspection_source(*before, *after, command, request);
      } else {
        domain = npc_source(*before, *after, old_actions, command,
                            local_move_counts);
      }
      if (!domain.id.empty())
        require(
            domain_sources.insert(source_key(domain)).second,
            "accounting two commands cannot reuse one original domain source");
      if (actor == request.actor_id)
        ++player_count;
      else
        ++npc_count;
      ++command_count;
      rows.back() = boundary::obj(
          {{"actor", V{actor}},
           {"sequence", V{command.request_seq}},
           {"kind", V{receipt.kind}},
           {"result_id", V{receipt.result_id}},
           {"revision", V{receipt.revision}},
           {"original_common_source",
            V{source_key({"common", "command_result", receipt.result_id})}},
           {"original_domain_source", V{source_key(domain)}},
           {"verified", V{true}}});
    }
    stage = "complete_command_source_coverage";
    inspection_coverage(*before, *after, domain_sources);
    if (after->movement_exact) {
      require(bool(before->movement_exact),
              "accounting original local move pre-state required");
      for (const auto &[key, move] :
           after->movement_exact->local_moves.rows()) {
        if (before->movement_exact->local_moves.find(key))
          continue;
        require(
            key == move.id &&
                domain_sources.count(source_key({"travel", "local_move", key})),
            "accounting every new local move source needs its sole command");
      }
    }
    for (const auto &[actor, sequence] : now.request_sequences.rows()) {
      const auto expected = sequences.find(actor);
      require(expected != sequences.end(),
              "accounting unsupported new actor original initial sequence: " +
                  actor);
      require(sequence == expected->second,
              "accounting evidence missing: actor sequence changed without a "
              "retained real command receipt");
    }
    for (const auto &[actor, sequence] : sequences) {
      const auto *actual = now.request_sequences.find(actor);
      require(actual && *actual == sequence,
              "accounting original actor sequence missing or unmatched");
    }
    require(
        player_count == 1 && command_count >= 1 &&
            now.identity.revision == published_revision &&
            returned->revision == now.identity.revision,
        "accounting evidence missing or mismatch: revision must equal actual "
        "Opening plus one outer publication, not the real result count");
    // 本次唯一 public 新请求的原身份与全部来源闭合后，才记一次外层发布。
    outer_publications = 1;
    outer_publication_verified = true;
    stage = "complete";
    report(true, {});
    return true;
  } catch (const std::exception &error) {
    // 留证失败不能覆盖原核账首错；外层仍有原 submit 的完整自动捕获。
    try {
      report(false, error.what());
    } catch (...) {
    }
    throw;
  }
}
} // namespace checklist_command_accounting
