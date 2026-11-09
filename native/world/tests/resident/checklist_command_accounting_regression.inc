// Inspect 核账回归：仅读固定的真实失败窗，反例只改独立内存副本，不安装/写回。
// 合同出处：以下 C/I 均在 docs/superpowers/specs/2026-10-13-world-core-design/：
// C = core-spatial-contracts.md，I = interaction-world-requirements.md。
// 00-integrated-contract.md §3/§5—8 是唯一主权、原命令及有限知识的整合入口。
// 状态边界：C 的历史页首为 DRAFT/非施工授权；I 是领域设计稿，KQ1 是拟议API。
// 本次适用范围来自用户明确要求的「命令身份 → 原 receipt → inspection →
// 供给观测 → 既有 report → 人物 Knowledge」逐环验证和只改三个测试文件授权；
// 下列条款是已存在的设计依据，不声称这些历史稿整体已联签或已实测通过。
// 1. 命令身份→原 receipt：C §4.1、§4.3、§6.1 保原 world/epoch/actor/seq、
//    typed kind/result 和重放身份；C §7.3 第5—7项要求真实采用、唯一驱动路由，
//    不借玩家序列或另发 revision。这里把选定 Director/6 原记录的
//    actor/registration_epoch/decision_seq/policy/kind/request_seq/result_id
//    全部反读；具体 C++ 布局用 director_command_acceptance.hpp 的原 API，
//    不将 native_director_actor_commands.cpp 实现反向当作需求。
//    registration_epoch/decision_seq/policy_ref 的具名设计出自
//    docs/superpowers/specs/2026-10-15-native-world-core-spec/core/
//    native-core-specification.md §6.1（DRAFT）：原三元采用键和原Actor上下文。
// 2. 原 receipt→inspection：I §5 I01、I17、§6、§7.1；实际 Inspect 才是
//    本人合法发现来源，公开/同处可见不等已获知，重复和恢复不重做旧成果。
//    所以借原 accepted_command.actual_effect，拒 result ID 算术与最后一行猜测。
// 3. inspection→供给 observation：C §8.1—8.2 的真实原 owner/必要直接原因；
//    I §14.4 KQ1-B KnownSupply 明列 source_key、fact_id、source_revision、
//    observation_tick；I §7.1 要实际读取公告。读原 notice→source_key 与
//    local_supply_observation，不按物品同名、今日库存或今日位置重建历史。
// 4. observation→既有 report：C §8.1 的 Fact/Report 分层，I §14.4 KQ1-A
//    第6项与 KQ1-B 要保原 observed/learned 与供给报告字段，不回源库刷新数量；
//    inspection 只能引用先存在的 observation/report，不能制造第二份观察。
// 5. report→本人 Knowledge：I §5 I01、§14.4 KQ1-A/B 的 observer 真实
//    KnownReport；§14.5 列 receiver/report_id/source_message_id?/learned_tick/
//    source_subject；C §8.4 明确本人真正取得，旁人/机构知道不等本人知道。
//    核原精确 time_source、actual_source_ref 和因果切点，拒缺环或串人。
//    本次选定 public_supply_inspection/1 的声明是先写 inspection 源再写唯一
//    Knowledge，故实际取得切点应为该源本身，通用 source≤cut 不等原cut。
//    这是 public_supply_inspection.hpp 的选定API适配；原准入
//    selected_native_public_supply_inspection 强制 common-cut 能力，不向合法
//    未选旧profile增字段，也不改通用Knowledge产品规则。
//    I §7.1 旧消息不刷成当前真相、KQ1-B 不回源库刷新 quantity，要求被引用的
//    pre已有 observation、Report及其原时间来源保值；旧inspection的Knowledge
//    亦不能刷新。此窄依赖保存不冻结无关表，也不禁止窗内生成真实新观察。
// 精确绑定另见 docs/superpowers/specs/2026-10-15-native-world-core-spec/core/
// knowledge-receipt-time-contract.md（自标 PROPOSED）；这里只说明选定读口，
// 不把它或 native-core-specification 的 DRAFT 状态冒称已联签正式新政策。
// API 适配与上述合同的适用性仍须独审；本文件存在不等于回归已经实跑。
namespace checklist_command_accounting_regression {
namespace accounting = checklist_command_accounting;

// 输入身份固定到原故障窗，不接受以另一份“类似”世界替换反例前提。
const StableId pre_sha =
    "fc6f56e246c5e77865981eda6d4b2e50689790341ac1d4b00c9fd42e5ded9027";
const StableId post_sha =
    "729349d1ae415cba6dd9050555fd594b1b37988d576782336edede9ec5d87860";

// 只读清单中的绝对原件；大小和完整摘要都必须匹配。
Bytes input_bank(const V &input, const StableId &key, std::size_t size,
                 const StableId &sha) {
  const fs::path path = boundary::get_string(input, key, "inspection input");
  require(path.is_absolute(), "inspection original bank path must be absolute");
  const auto bytes = read_bytes(path);
  require(bytes.size() == size && picosha2::hash256_hex_string(bytes) == sha,
          "inspection original bank identity mismatch: " + key);
  return bytes;
}

// 仅 RED 故意错修：按原件选定的键读候选 actual_effect，不核 result_id。
// 缺采用行或 accepted_command 时，错误地以同 actor/tick 的唯一新增源代替采用；
// 这不是合法身份依据。候选有回执时仍沿原缺检路径，不向其余反例增设守卫。
// 回退保留真实源/观察/Knowledge 链检查；不返回原件缓存源或硬编码成功。
// 本次 missing_adoption 只删整行，未另测 accepted_command.reset()。
// 不改产品或正式 helper；同一组反例应使 RED 真实 exit 1。
SourceRef deliberately_incomplete_mapping(
    const WorldBundle &before, const WorldBundle &after,
    const CommonCommandSourceView &command, const StableId &adoption_key) {
  const auto *decision =
      after.director.subject2->adoption_results.find(adoption_key);
  if (decision && decision->accepted_command)
    return decision->accepted_command->actual_effect.source_ref;
  require(before.public_supply_inspection && after.public_supply_inspection,
          "RED missing-adoption fallback requires inspection owners");
  const PublicSupplyInspection *selected = nullptr;
  for (const auto &[key, row] : after.public_supply_inspection->results.rows()) {
    if (before.public_supply_inspection->results.find(key) ||
        row.actor_id != command.history->actor_id ||
        row.inspected_at.half_day_tick != command.result->tick)
      continue;
    require(!selected, "RED missing-adoption fallback requires a unique source");
    selected = &row;
  }
  require(selected, "RED missing-adoption fallback requires a new source");
  return accounting::inspection_chain(
      before, after, command,
      {"interaction", public_supply_inspection_source_kind, selected->id});
}

// 每例持一个新副本；setup 错误不算 helper 拒绝，真实拒绝还须匹配预期层。
template <class Setup, class Check>
void rejected_case(const WorldBundle &before, const WorldBundle &after,
                   const StableId &name, const StableId &expected,
                   Setup setup, Check check, V::Array &results,
                   Integer &unexpected) {
  auto candidate = std::make_unique<WorldBundle>(after);
  setup(*candidate);
  StableId error;
  try {
    check(before, *candidate);
  } catch (const std::exception &failure) {
    error = failure.what();
  }
  const bool rejected = !error.empty() && error.find(expected) != StableId::npos;
  const bool witness = name == "wrong_observation_source" ||
                       name == "missing_supply_observation" ||
                       name == "mixed_npc_inspection_actor";
  results.push_back(boundary::obj({{"case", V{name}},
                                    {"expected_error", V{expected}},
                                    {"actual_error", V{error}},
                                    {"checker_accepted", V{error.empty()}},
                                    {"core_sensitivity_witness", V{witness}},
                                    {"rejected_at_expected_layer", V{rejected}}}));
  if (!rejected)
    ++unexpected;
}

// 固定真实窗包含一笔 Inspect、两笔其他 NPC 命令和玩家 Advance；不硬编码人名。
void run_mode(Run &run, const V &prepared, const StableId &prepared_bytes,
               const fs::path &root, bool deliberate_bad_fix) {
  require(picosha2::hash256_hex_string(prepared_bytes) ==
              "2840aad48f2993ce55634df727e8b15e8ace8076b47e9deeb139cb0debc10677",
          "inspection regression requires the unchanged original F");
  const auto output = root / "results";
  require(!fs::exists(output), "inspection regression refuses existing output");
  const auto input = read_json(root / "command-accounting-input.json");
  const auto pre = input_bank(input, "pre_bank", 468110, pre_sha);
  const auto post = input_bank(input, "post_bank", 518316, post_sha);
  run.restore(prepared, post);
  require(run.bytes() == post, "inspection regression changed original bank");
  const auto before = accounting::snapshot(pre, run.catalog);
  const auto after = run.snapshot();
  const auto old_actions = accounting::original_actions(*before);
  std::map<StableId, Integer> moves;
  std::set<StableId> sources, actors;
  StableId inspection_result, other_actor, adoption_key;
  SourceRef inspection;
  std::shared_ptr<const CommitResult> returned;
  Integer count = 0;
  for (const auto &[key, receipt] : after->common.state().results.rows()) {
    if (before->common.state().results.find(key))
      continue;
    const auto command = must(
        read_common_command_source(after->common,
                                    std::string_view(receipt->result_id)),
        "inspection regression original Common command");
    if (command.history->actor_id == player) {
      returned = receipt;
      continue;
    }
    ++count;
    actors.insert(command.history->actor_id);
    // 只在未突变原件中定位键；probe 不按候选 epoch/seq 重算或按 result_id 找行。
    if (receipt->kind == "inspect")
      for (const auto &[key, row] : after->director.subject2->adoption_results.rows())
        if (row.accepted_command &&
            row.accepted_command->result_id == receipt->result_id)
          adoption_key = key;
    const auto source = deliberate_bad_fix && receipt->kind == "inspect"
                            ? deliberately_incomplete_mapping(
                                  *before, *after, command, adoption_key)
                            : accounting::npc_source(*before, *after,
                                                       old_actions, command,
                                                       moves);
    require(sources.insert(accounting::source_key(source)).second,
            "inspection regression two NPCs reused an original source");
    if (receipt->kind == "inspect") {
      require(inspection_result.empty(),
              "inspection regression fixed original sample changed");
      inspection_result = receipt->result_id;
      inspection = source;
    } else {
      other_actor = command.history->actor_id;
    }
  }
  require(count == 3 && actors.size() == 3 && returned &&
              returned->kind == "advance" && !inspection_result.empty() &&
              !other_actor.empty(),
          "inspection regression needs the real three-NPC mixed window");
  accounting::inspection_coverage(*before, *after, sources);
  require(!adoption_key.empty(), "inspection regression original adoption absent");
  const auto original = *after->public_supply_inspection->results.find(inspection.id);
  const SubjectRef reader{SubjectKind::actor, original.actor_id};
  const OwnedSocialIndexKey receiver_key(reader);
  const OwnedSocialIndexKey knowledge_key(
      SocialTimeKey{SocialKnowledgeKey{reader, original.report_id}});
  const OwnedSocialIndexKey observation_key(
      SocialTimeKey{ReportObservationKey{original.report_id}});
  const auto *later_fact =
      after->common.state().facts.find(after->common.state().facts.order().back());
  require(later_fact && later_fact->id != inspection.id,
          "inspection cut counterexample requires a real later Common fact");
  const SourceRef later_source{later_fact->domain, later_fact->kind, later_fact->id};
  fs::create_directories(output);
  if (!deliberate_bad_fix) {
    const auto &identity = before->common.state().identity;
    const auto *sequence = after->common.state().request_sequences.find(player);
    require(sequence, "inspection regression original player sequence missing");
    const CommandEnvelope request{
        identity.world_id, identity.execution_epoch, *sequence,
        identity.revision, player,
        std::make_shared<const CommandPayload>(
            CommandPayload{"advance", AdvanceCommand{}})};
    require(accounting::verify(run, pre, request, returned, output),
            "inspection regression full original command accounting failed");
  }
  const auto probe = [&](const WorldBundle &old, const WorldBundle &candidate) {
    const auto command = must(
        read_common_command_source(candidate.common,
                                    std::string_view(inspection_result)),
        "inspection regression original command under test");
    if (deliberate_bad_fix)
      (void)deliberately_incomplete_mapping(old, candidate, command, adoption_key);
    else
      (void)accounting::npc_source(old, candidate, old_actions, command, moves);
  };
  V::Array cases;
  Integer unexpected = 0;
  const auto check = [&](const StableId &name, const StableId &expected,
                          auto mutate) {
    rejected_case(*before, *after, name, expected, mutate, probe, cases,
                   unexpected);
  };
  check("missing_adoption", "requires its original adopted command", [&](auto &w) {
    w.director.subject2->adoption_results.erase(adoption_key);
  });
  check("wrong_registration_epoch", "adoption identity", [&](auto &w) {
    ++w.director.subject2->adoption_results.find(adoption_key)->registration_epoch;
  });
  check("wrong_decision_sequence", "adoption identity", [&](auto &w) {
    ++w.director.subject2->adoption_results.find(adoption_key)->decision_seq;
  });
  check("wrong_policy", "original actor/epoch/decision/policy/receipt join", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->policy_id.clear();
  });
  check("wrong_command_kind", "original actor/epoch/decision/policy/receipt join", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->command_kind = "office_accept";
  });
  check("wrong_request_sequence", "original actor/epoch/decision/policy/receipt join", [&](auto &w) {
    ++w.director.subject2->adoption_results.find(adoption_key)->accepted_command->request_seq;
  });
  check("wrong_result_identity", "requires its original adopted command", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->result_id = returned->result_id;
  });
  check("mixed_npc_accepted_actor", "original actor/epoch/decision/policy/receipt join", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->actor_id = other_actor;
  });
  check("wrong_effect_source", "original actor/epoch/decision/policy/receipt join", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->actual_effect.source_ref = original.original_observation.source_ref;
  });
  check("wrong_effect_schema", "must borrow its real receipt", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->actual_effect.source_schema = 2;
  });
  check("receipt_id_is_not_inspection_id", "observation/report chain", [&](auto &w) {
    auto &decision = *w.director.subject2->adoption_results.find(adoption_key);
    auto &source = decision.accepted_command->actual_effect.source_ref;
    source.id = inspection_result;
    decision.actual_outcome_refs = {source};
  });
  check("duplicate_adoption_receipt", "adoption identity", [&](auto &w) {
    auto decision = *w.director.subject2->adoption_results.find(adoption_key);
    ++decision.decision_seq;
    require(w.director.subject2->adoption_results.insert(
                director_adoption_key2(decision.subject,
                                       decision.registration_epoch,
                                       decision.decision_seq), decision),
            "inspection duplicate-adoption setup must add a distinct key");
  });
  check("missing_inspection", "observation/report chain", [&](auto &w) {
    w.public_supply_inspection->results.erase(inspection.id);
  });
  check("missing_inspection_header", "observation/report chain", [&](auto &w) {
    w.common.state().facts.erase(inspection.id);
  });
  check("mixed_npc_inspection_actor", "actor and original command moment mismatch", [&](auto &w) {
    w.public_supply_inspection->results.find(inspection.id)->actor_id = other_actor;
  });
  check("wrong_observation_source", "observation/report chain", [&](auto &w) {
    w.public_supply_inspection->results.find(inspection.id)->original_observation.source_ref = inspection;
  });
  check("missing_supply_observation", "observation/report chain", [&](auto &w) {
    w.local_economy.supply_observations.erase(original.original_observation.source_ref.id);
  });
  check("missing_existing_report", "observation/report chain", [&](auto &w) {
    w.social.dynamic->reports.erase(original.report_id);
  });
  check("coordinated_rewrite_original_supply_report", "original supply observation changed", [&](auto &w) {
    auto &supply = w.local_economy.supply_observations.find(
        original.original_observation.source_ref.id)->observation;
    require(supply.physical_amount < std::numeric_limits<Integer>::max(),
            "inspection original quantity must allow the bounded mutation");
    ++supply.physical_amount;
    w.social.dynamic->reports.find(original.report_id)->observation.supply->physical_amount = supply.physical_amount;
  });
  check("rewrite_original_report_metadata", "original report/Knowledge/time values changed", [&](auto &w) {
    w.social.dynamic->reports.find(original.report_id)->origin_subject.id = other_actor;
  });
  check("rewrite_original_report_time_binding", "original report/Knowledge/time values changed", [&](auto &w) {
    w.social.dynamic->time2->source_time_bindings.find(observation_key.value())->actual_source_ref = inspection;
  });
  check("future_inspection_moment", "observation/report chain", [&](auto &w) {
    ++w.public_supply_inspection->results.find(inspection.id)->inspected_at.half_day_tick;
  });
  check("wrong_acceptance_fraction", "actor and original command moment mismatch", [&](auto &w) {
    w.director.subject2->adoption_results.find(adoption_key)->accepted_command->accepted_at.sub_half_day =
        must(normalize_fraction("1", "2"), "inspection fractional mutation");
  });
  check("wrong_observation_schema", "observation/report chain", [&](auto &w) {
    ++w.public_supply_inspection->results.find(inspection.id)->original_observation.source_schema;
  });
  check("missing_own_knowledge", "actor's original Knowledge", [&](auto &w) {
    w.social.dynamic->known_reports.find(receiver_key.value())->erase(original.report_id);
  });
  check("mixed_npc_knowledge_receiver", "actor's original Knowledge", [&](auto &w) {
    w.social.dynamic->known_reports.find(receiver_key.value())->find(original.report_id)->receiver.id = other_actor;
  });
  check("missing_knowledge_time_binding", "actor's original Knowledge", [&](auto &w) {
    w.social.dynamic->time2->source_time_bindings.erase(knowledge_key.value());
  });
  check("wrong_knowledge_source", "actor's original Knowledge", [&](auto &w) {
    w.social.dynamic->time2->source_time_bindings.find(knowledge_key.value())->actual_source_ref = original.original_observation.source_ref;
  });
  check("missing_knowledge_cut", "Knowledge original causal cut", [&](auto &w) {
    w.social.dynamic->time2->source_time_bindings.find(knowledge_key.value())->learned_after_common_fact.reset();
  });
  check("wrong_knowledge_time_schema", "actor's original Knowledge", [&](auto &w) {
    auto *time = std::get_if<SocialExactSource>(
        &w.social.dynamic->time2->source_time_bindings.find(knowledge_key.value())->time_source);
    require(time, "inspection schema mutation needs original exact source");
    ++time->original_timed_source_ref.source_schema;
  });
  check("new_knowledge_cut_rebound_to_later_fact", "must retain original acquisition cut", [&](auto &w) {
    w.social.dynamic->time2->source_time_bindings.find(knowledge_key.value())->learned_after_common_fact = later_source;
  });
  rejected_case(*after, *after, "reused_old_inspection", "adoption identity",
                [](auto &) {}, probe, cases, unexpected);
  {
    auto old_source = std::make_unique<WorldBundle>(*before);
    require(old_source->public_supply_inspection->results.insert(original.id,
                                                                 original),
            "inspection reused-source setup must add the original row");
    rejected_case(*old_source, *after, "reused_source_with_new_adoption",
                  "requires a new original source", [](auto &) {}, probe,
                  cases, unexpected);
  }
  // 覆盖专属反例只进 GREEN；RED 不能以跳过检查的空 lambda 冒已杀错修。
  V::Array components;
  if (!deliberate_bad_fix) {
    const auto coverage = [](const auto &old, const auto &candidate) {
      accounting::inspection_coverage(old, candidate, {});
    };
    rejected_case(*before, *after, "unaccounted_new_inspection", "every new inspection source",
                  [](auto &) {}, coverage, cases, unexpected);
    rejected_case(*after, *after, "rewritten_old_inspection", "original source prefix changed",
                  [&](auto &w) {
                    w.public_supply_inspection->results.find(inspection.id)->place_id.clear();
                  }, coverage, cases, unexpected);
    rejected_case(*after, *after, "rewritten_old_knowledge_cut", "original report/Knowledge/time values changed",
                  [&](auto &w) {
                    w.social.dynamic->time2->source_time_bindings.find(knowledge_key.value())->learned_after_common_fact = later_source;
                  }, coverage, cases, unexpected);
    rejected_case(*after, *after, "rewritten_old_inspection_dependencies", "original supply observation changed",
                  [&](auto &w) {
                    auto &supply = w.local_economy.supply_observations.find(
                        original.original_observation.source_ref.id)->observation;
                    ++supply.physical_amount;
                    w.social.dynamic->reports.find(original.report_id)->observation.supply->physical_amount = supply.physical_amount;
                  }, coverage, cases, unexpected);
    // 下面只验 helper 的表基数和 typed-payload 分派，不声称第二次真实 submit。
    accounting::inspection_coverage(*after, *after, {});
    const auto command = must(read_common_command_source(
                                  after->common, std::string_view(inspection_result)),
                              "inspection typed-payload component source");
    const auto &identity = before->common.state().identity;
    const CommandEnvelope direct{
        identity.world_id, identity.execution_epoch, command.request_seq,
        identity.revision, reader.id,
        std::make_shared<const CommandPayload>(
            CommandPayload{"inspect", InteractionCommand{Inspect{original.prop_id}}})};
    require(accounting::source_key(accounting::player_inspection_source(
                *before, *after, command, direct)) == accounting::source_key(inspection),
            "inspection direct typed-payload component must retain real source");
    require(accounting::player_inspection_source(*after, *after, command, direct).id.empty(),
            "inspection no-new-source component must not invent an effect");
    components.push_back(boundary::obj({{"case", V{"typed_inspect_new_and_unchanged_source"}},
                                        {"scope", V{"helper_component_not_public_player_submit"}},
                                        {"passed", V{true}}}));
    StableId ordinary_prop;
    for (const auto &prop : after->catalog->interaction->props)
      if (!after->catalog->public_supply_inspection->notice_sources.find(prop.id)) {
        ordinary_prop = prop.id;
        break;
      }
    if (!ordinary_prop.empty()) {
      auto ordinary = direct;
      ordinary.payload = std::make_shared<const CommandPayload>(
          CommandPayload{"inspect", InteractionCommand{Inspect{ordinary_prop}}});
      require(accounting::player_inspection_source(*after, *after, command,
                                                   ordinary).id.empty(),
              "ordinary Inspect component must not invent a public source");
    }
    components.push_back(boundary::obj({{"case", V{"ordinary_non_notice_inspect"}},
                                        {"scope", V{"helper_component_not_public_player_submit"}},
                                        {"status", V{ordinary_prop.empty() ? "NOT_COVERED" : "PASS"}},
                                        {"reason", V{ordinary_prop.empty() ? "unchanged_F_has_no_non_notice_prop" : ""}}}));
    auto multiple = std::make_unique<WorldBundle>(*after);
    auto second = original;
    second.id += ":coverage-component";
    second.actor_id = other_actor;
    require(multiple->public_supply_inspection->results.insert(second.id, second),
            "inspection coverage component requires a second distinct row");
    auto complete = sources;
    complete.insert(accounting::source_key(
        {"interaction", public_supply_inspection_source_kind, second.id}));
    accounting::inspection_coverage(*before, *multiple, complete);
    components.push_back(boundary::obj({{"case", V{"two_inspection_source_coverage"}},
                                        {"scope", V{"table_cardinality_component_not_real_multi_inspect_submit"}},
                                        {"passed", V{true}}}));
    rejected_case(*before, *multiple, "second_inspection_not_accounted", "every new inspection source",
                  [](auto &) {}, [&](const auto &old, const auto &candidate) {
                    accounting::inspection_coverage(old, candidate, sources);
                  }, cases, unexpected);
  }
  require(run.bytes() == post,
          "inspection negative tests changed the installed original world");
  write_json(output / "inspection-regression.json",
             boundary::obj({{"schema", V{"hanmo.inspect-accounting-regression.v1"}},
                            {"mode", V{deliberate_bad_fix ? "deliberately_incomplete_mapping" : "actual_helper"}},
                            {"original_pre_sha256", V{pre_sha}},
                            {"original_post_sha256", V{post_sha}},
                            {"real_npc_commands", V{count}},
                            {"real_distinct_npcs", V{Integer{3}}},
                            {"real_inspect_commands", V{Integer{1}}},
                            {"original_installed_bytes_unchanged", V{true}},
                            {"unexpected_counterexample_acceptances_or_errors", V{unexpected}},
                            {"components", V{std::move(components)}},
                            {"cases", V{std::move(cases)}}}));
  require(unexpected == 0,
          "inspection counterexample sensitivity: " + std::to_string(unexpected) +
              " cases did not reject at the required layer");
  std::cout << "COMMAND-ACCOUNTING-INSPECT: real mixed NPC window and source-chain counterexamples PASS\n";
}
} // namespace checklist_command_accounting_regression
