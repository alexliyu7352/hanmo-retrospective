// 共用测试观察只保存本次真实边界；失败不重送命令，也不覆盖原产品首错。
namespace checklist_commit_observation {
fs::path automatic_root;
V input_identity;
Integer attempt_sequence = 0;
Integer capture_sequence = 0;
std::map<std::shared_ptr<const CommandPayload>, V,
         std::owner_less<std::shared_ptr<const CommandPayload>>>
    original_parameters;

// 原边界参数与真实解码 payload 同时持有，不能因地址复用关联到另一条命令。
void remember_parameters(const std::shared_ptr<const CommandPayload> &payload,
                         const V &parameters) {
  original_parameters.emplace(payload, parameters);
}

// 每个真实进程按模式独立取证；已有目录必须拒绝，不能覆盖历史失败。
void configure(const fs::path &output, const StableId &mode,
               const StableId &fixture_path, const StableId &fixture_bytes) {
  automatic_root = output / "command-observations" / mode;
  require(!fs::exists(automatic_root), "observation output already exists");
  fs::create_directories(automatic_root);
  input_identity = boundary::obj(
      {{"mode", V{mode}},
       {"fixture_path", V{fixture_path}},
       {"fixture_bytes", V{fixture_bytes.size()}},
       {"fixture_sha256", V{picosha2::hash256_hex_string(fixture_bytes)}},
       {"source_identity", V{"Exact commit, native tree and executable are "
                             "frozen by the external runner"}}});
}

// optional 的不存在位保留为 null，不把默认值伪装成已返回字段。
template <class T> V optional_value(const std::optional<T> &value) {
  return value ? V{*value} : V{};
}

// 原错误全部字段逐项保留，包含原投影可能省略的分类、上下文和存在位。
V error_value(const RuleError &error) {
  return boundary::obj(
      {{"original", boundary::obj({{"code", V{error.original.code}},
                                   {"path", V{error.original.path}}})},
       {"kind", V{static_cast<Integer>(error.kind)}},
       {"phase", V{error.phase}},
       {"domain", V{error.domain}},
       {"command", V{error.command}},
       {"source_id", V{error.source_id}},
       {"army_id", optional_value(error.army_id)},
       {"reason", optional_value(error.reason)},
       {"limit", optional_value(error.limit)},
       {"feasibility", optional_value(error.feasibility)},
       {"maximum", optional_value(error.maximum)},
       {"new_total", optional_value(error.new_total)},
       {"required_level", optional_value(error.required_level)},
       {"actual_level", optional_value(error.actual_level)},
       {"skill_id", optional_value(error.skill_id)},
       {"actor_id", optional_value(error.actor_id)},
       {"battle_id", optional_value(error.battle_id)},
       {"battle_id_after_path", V{error.battle_id_after_path}}});
}

// WorldId 的十六个原字节不经显示字符串或哈希替换。
V world_id_value(const WorldId &id) {
  V::Array bytes;
  for (const auto byte : id)
    bytes.push_back(V{static_cast<Integer>(byte)});
  return V{std::move(bytes)};
}

// 保存原身份，不以 revision 差值推算任何回执数量。
V identity_value(const WorldIdentity &identity) {
  return boundary::obj({{"world_id", world_id_value(identity.world_id)},
                        {"execution_epoch", V{identity.execution_epoch}},
                        {"revision", V{identity.revision}},
                        {"tick", V{identity.tick}}});
}

// Outcome 的成功位和完整错误独立保存，失败时不把默认 identity 当真值。
V identity_outcome(const Outcome<WorldIdentity> &outcome) {
  return boundary::obj({{"ok", V{outcome.ok}},
                        {"value", identity_value(outcome.value)},
                        {"value_is_valid", V{outcome.ok}},
                        {"error", error_value(outcome.error)}});
}

// codec 成功和值分开保存；编码失败不能变成一份假的业务回执。
V projection_outcome(const Outcome<V> &outcome) {
  return boundary::obj({{"ok", V{outcome.ok}},
                        {"value", outcome.value},
                        {"error", error_value(outcome.error)}});
}

// 原 envelope 和真实 variant/target 从同一请求读取，不重建或再提交命令。
V request_value(const CommandEnvelope &request) {
  V payload;
  if (request.payload) {
    const auto *advance = std::get_if<AdvanceCommand>(&request.payload->data);
    payload = boundary::obj(
        {{"kind", V{request.payload->kind}},
         {"variant_index", V{request.payload->data.index()}},
         {"advance_variant_present", V{advance != nullptr}},
         {"requested_to_present",
          V{advance && advance->requested_to.has_value()}},
         {"requested_to",
          advance && advance->requested_to
              ? projection_outcome(encode_world_moment2(*advance->requested_to))
              : V{}},
         {"original_boundary_parameters",
          original_parameters.count(request.payload)
              ? original_parameters.at(request.payload)
              : V{}},
         {"parameters_available",
          V{advance != nullptr ||
            original_parameters.count(request.payload) != 0}}});
  }
  return boundary::obj({{"world_id", world_id_value(request.world_id)},
                        {"execution_epoch", V{request.execution_epoch}},
                        {"request_seq", V{request.request_seq}},
                        {"expected_revision", V{request.expected_revision}},
                        {"actor_id", V{request.actor_id}},
                        {"payload_present", V{bool(request.payload)}},
                        {"payload", std::move(payload)}});
}

// 先存不依赖回执编码器的原值；深层 typed 字段另由原 visitor 保存。
V submit_value(const Outcome<std::shared_ptr<const CommitResult>> &submitted) {
  V value;
  if (submitted.value) {
    const auto &receipt = *submitted.value;
    value = boundary::obj(
        {{"ok", V{receipt.ok}},
         {"kind", V{receipt.kind}},
         {"tick", V{receipt.tick}},
         {"revision", V{receipt.revision}},
         {"result_id", V{receipt.result_id}},
         {"field_order", boundary::string_array(receipt.field_order)},
         {"details_present", V{bool(receipt.details)}},
         {"details_variant_index",
          receipt.details ? V{receipt.details->data.index()} : V{}},
         {"details_field_order",
          receipt.details ? boundary::string_array(receipt.details->field_order)
                          : V{}}});
  }
  return boundary::obj({{"ok", V{submitted.ok}},
                        {"value_present", V{bool(submitted.value)}},
                        {"error", error_value(submitted.error)},
                        {"value", std::move(value)}});
}

// 每个观察步骤独立隔离异常；诊断错误仅记日志，不改变测试通过条件。
template <class F>
void stage(const char *name, V::Array &errors, F action) noexcept {
  try {
    action();
  } catch (const std::exception &error) {
    try {
      errors.push_back(
          boundary::obj({{"stage", V{name}}, {"exception", V{error.what()}}}));
      std::cerr << "command observation " << name << ": " << error.what()
                << '\n';
    } catch (...) {
      // 连错误记录都失败时也不能覆盖原提交的断言或错误。
    }
  } catch (...) {
    try {
      errors.push_back(boundary::obj(
          {{"stage", V{name}}, {"exception", V{"unknown exception"}}}));
      std::cerr << "command observation " << name << ": unknown exception\n";
    } catch (...) {
    }
  }
}

// JSON pointer 转义只标识原导出位置，不构造新的来源记录。
StableId pointer_token(const StableId &token) {
  StableId escaped;
  for (const auto c : token) {
    if (c == '~')
      escaped += "~0";
    else if (c == '/')
      escaped += "~1";
    else
      escaped += c;
  }
  return escaped;
}

// 保留与当前原 period 标识相等的原字段/键及所属行；完整世界另存以免筛选丢源。
void period_rows(const V &value, const StableId &period, const StableId &path,
                 V::Array &rows) {
  if (const auto *object = value.as<V::Dict>()) {
    for (const auto &[key, child] : *object) {
      const auto child_path = path + "/" + pointer_token(key);
      const auto *text = child.as<StableId>();
      if (key == period || (text && *text == period)) {
        rows.push_back(boundary::obj({{"path", V{child_path}},
                                      {"original_value", child},
                                      {"containing_record", value}}));
      }
      period_rows(child, period, child_path, rows);
    }
  } else if (const auto *array = value.as<V::Array>()) {
    for (std::size_t i = 0; i < array->size(); ++i)
      period_rows((*array)[i], period, path + "/" + std::to_string(i), rows);
  }
}

// 只解码已留存的原 bank 并导出拥有值；不
// install/restore、推进、修复或重放世界。
void save_sources(const fs::path &directory, const StableId &prefix,
                  const Bytes &bank,
                  const std::shared_ptr<const PreparedCatalog> &catalog) {
  auto decoded = decode_world_checkpoint(bank, *catalog);
  write_json(directory / (prefix + "-bank-decode-outcome.json"),
             boundary::obj({{"ok", V{decoded.ok}},
                            {"error", error_value(decoded.error)}}));
  if (!decoded.ok)
    return;
  const WorldBundle snapshot(catalog, std::move(decoded.value));
  const auto exported = export_world_state(snapshot);
  write_json(directory / (prefix + "-state-export-outcome.json"),
             boundary::obj({{"ok", V{exported.ok}},
                            {"error", error_value(exported.error)}}));
  if (!exported.ok)
    return;
  write_json(directory / (prefix + "-source-state.json"), exported.value);
  V::Dict command_sources;
  for (const auto *name : {"request_sequences", "results", "history"}) {
    const auto *value = exported.value.find(name);
    command_sources.push_back({name, value ? *value : V{}});
  }
  write_json(directory / (prefix + "-command-sources.json"),
             V{std::move(command_sources)});
  const auto *timeline = exported.value.find("world_timeline");
  const auto *state = timeline ? timeline->find("state") : nullptr;
  const auto *id = state ? state->find("period_id") : nullptr;
  const auto *period = id ? id->as<StableId>() : nullptr;
  V::Array rows;
  if (period && !period->empty())
    period_rows(exported.value, *period, "", rows);
  write_json(directory / (prefix + "-opening-period-sources.json"),
             boundary::obj(
                 {{"world_timeline", timeline ? *timeline : V{}},
                  {"original_period_matches", V{std::move(rows)}},
                  {"complete_source_file", V{prefix + "-source-state.json"}},
                  {"classification",
                   V{"period references are not player command receipts"}}}));
}

// 保存一次既有 submit 的原结果和后态；所有观察阶段都在原断言前结束。
void capture(const fs::path &directory, Run &run,
             const CommandEnvelope &request, const Bytes &before,
             const WorldIdentity &identity, const WorldIdentity &previewed,
             const Outcome<std::shared_ptr<const CommitResult>> &submitted,
             const Outcome<WorldIdentity> &after,
             bool full_sources = true) noexcept {
  const auto capture_id = ++capture_sequence;
  V::Array errors;
  Outcome<Bytes> after_bank;
  bool checkpoint_returned = false;
  stage("directory", errors, [&] { fs::create_directories(directory); });
  stage("primary original values", errors, [&] {
    write_json(
        directory / "submit-original-values.json",
        boundary::obj({{"schema", V{"hanmo.original-submit-observation.v2"}},
                       {"request", request_value(request)},
                       {"pre_identity", identity_value(identity)},
                       {"previewed_identity", identity_value(previewed)},
                       {"preview_assertion_evidence",
                        V{"See preview-checks.json; capture does not infer a "
                          "successful assertion"}},
                       {"submit", submit_value(submitted)},
                       {"post_identity", identity_outcome(after)}}));
  });
  stage("original post checkpoint", errors, [&] {
    after_bank = run.world.checkpoint();
    checkpoint_returned = true;
  });
  stage("original advance target", errors, [&] {
    const auto *advance =
        request.payload ? std::get_if<AdvanceCommand>(&request.payload->data)
                        : nullptr;
    write_json(directory / "request-target-outcome.json",
               boundary::obj(
                   {{"advance_variant_present", V{advance != nullptr}},
                    {"requested_to_present",
                     V{advance && advance->requested_to.has_value()}},
                    {"encoder", advance && advance->requested_to
                                    ? projection_outcome(encode_world_moment2(
                                          *advance->requested_to))
                                    : V{}}}));
  });
  stage("original pre bank", errors,
        [&] { write_bytes(directory / "pre-submit.hmbank", before); });
  stage("post checkpoint outcome", errors, [&] {
    write_json(directory / "post-checkpoint-outcome.json",
               boundary::obj({{"returned", V{checkpoint_returned}},
                              {"ok", V{after_bank.ok}},
                              {"error", error_value(after_bank.error)},
                              {"value_bytes", V{after_bank.value.size()}}}));
  });
  if (checkpoint_returned)
    stage("original post bank", errors, [&] {
      write_bytes(directory / (after_bank.ok ? "post-submit.hmbank"
                                             : "post-checkpoint-value.bin"),
                  after_bank.value);
    });
  stage("original typed receipt fields", errors, [&] {
    Bytes bytes;
    CodecVisitor writer(bytes);
    auto pointer = submitted.value;
    const auto visited = visit_resident_receipt(writer, pointer);
    const auto finished = writer.finish();
    write_bytes(directory / "submit-typed-fields.bin", bytes);
    write_json(directory / "submit-typed-fields-outcome.json",
               boundary::obj({{"visit_ok", V{visited.ok}},
                              {"visit_error", error_value(visited.error)},
                              {"finish_ok", V{finished.ok}},
                              {"finish_error", error_value(finished.error)},
                              {"complete", V{visited.ok && finished.ok}},
                              {"bytes", V{bytes.size()}},
                              {"codec", V{"original visit_resident_receipt; "
                                          "default unexpanded profile"}}}));
  });
  stage("original receipt projection", errors, [&] {
    write_json(directory / "submit-receipt-encoder-outcome.json",
               projection_outcome(encode_resident_receipt(submitted)));
  });
  if (full_sources)
    stage("pre original sources", errors,
          [&] { save_sources(directory, "pre", before, run.catalog); });
  if (full_sources && checkpoint_returned && after_bank.ok)
    stage("post original sources", errors, [&] {
      save_sources(directory, "post", after_bank.value, run.catalog);
    });
  stage("observer status", errors, [&] {
    write_json(directory /
                   ("observer-status-" + std::to_string(capture_id) + ".json"),
               boundary::obj({{"capture_returned", V{true}},
                              {"observer_exceptions", V{errors}}}));
  });
}

// 原始回传总先保存；析构只在失败/断言展开时补全状态，绝不再次 submit。
class Attempt {
  Run &run_;
  const CommandEnvelope request_;
  const Bytes before_;
  const WorldIdentity identity_;
  const int exceptions_ = std::uncaught_exceptions();
  Outcome<WorldIdentity> previewed_, after_;
  Outcome<std::shared_ptr<const PreviewResult>> preview_;
  Outcome<std::shared_ptr<const CommitResult>> submitted_;
  bool preview_returned_ = false, submit_returned_ = false;
  bool preview_retained_ = false, submit_retained_ = false;
  bool preview_ok_ = false, submit_ok_ = false;
  V::Array errors_;

  // 在没有 submit 回传时保留真实可读后态，并显式标记没有回传值。
  void incomplete_return() noexcept {
    stage("incomplete return post identity", errors_, [&] {
      write_json(directory / "post-identity.json",
                 identity_outcome(run_.world.identity()));
    });
    stage("incomplete return post checkpoint", errors_, [&] {
      const auto bank = run_.world.checkpoint();
      write_json(directory / "post-checkpoint-outcome.json",
                 boundary::obj({{"returned", V{true}},
                                {"ok", V{bank.ok}},
                                {"error", error_value(bank.error)},
                                {"value_bytes", V{bank.value.size()}}}));
      if (bank.ok) {
        write_bytes(directory / "post-submit.hmbank", bank.value);
        save_sources(directory, "post", bank.value, run_.catalog);
      }
    });
  }

public:
  const fs::path directory;

  // 变更世界之前落盘原 bank/envelope；保存失败在提交前停止，不丢失产品首错。
  Attempt(Run &run, const CommandEnvelope &request, const Bytes &before,
          const WorldIdentity &identity, const StableId &purpose)
      : run_(run), request_(request), before_(before), identity_(identity),
        previewed_{true, identity, {}},
        directory(automatic_root /
                  (std::to_string(++attempt_sequence) + "-" + purpose)) {
    require(!automatic_root.empty() && !fs::exists(directory),
            "unique automatic command observation directory required");
    fs::create_directories(directory);
    write_bytes(directory / "pre-submit.hmbank", before_);
    write_json(directory / "pre-submit.json",
               boundary::obj({{"input", input_identity},
                              {"request", request_value(request_)},
                              {"pre_identity", identity_value(identity_)},
                              {"submit_returned", V{false}},
                              {"pre_bank_bytes", V{before_.size()}},
                              {"pre_bank_sha256",
                               V{picosha2::hash256_hex_string(before_)}}}));
  }

  // 原 preview Outcome 与实际编码结果分开保存，失败也没有伪造 receipt。
  void preview(
      const Outcome<std::shared_ptr<const PreviewResult>> &result) noexcept {
    preview_returned_ = true;
    preview_ok_ = result.ok;
    stage("original preview raw outcome", errors_, [&] {
      write_json(directory / "preview-original-values.json",
                 boundary::obj({{"ok", V{result.ok}},
                                {"value_present", V{bool(result.value)}},
                                {"error", error_value(result.error)}}));
    });
    stage("retain original preview outcome", errors_, [&] {
      preview_ = result;
      preview_retained_ = true;
    });
    stage("preview returned identity", errors_, [&] {
      previewed_ = run_.world.identity();
      write_json(directory / "previewed-identity.json",
                 identity_outcome(previewed_));
    });
    stage("original preview projection", errors_, [&] {
      write_json(directory / "preview-encoder-outcome.json",
                 projection_outcome(
                     encode_resident_preview(result, identity_.revision)));
    });
  }

  // 返回结果先于任何后态读取/编码落盘，失败时不依赖可抛异常的Outcome复制。
  void submitted(
      const Outcome<std::shared_ptr<const CommitResult>> &result) noexcept {
    submit_returned_ = true;
    submit_ok_ = result.ok;
    stage("original submit raw outcome", errors_, [&] {
      write_json(directory / "raw-submit-return.json", submit_value(result));
    });
    stage("retain original submit outcome", errors_, [&] {
      submitted_ = result;
      submit_retained_ = true;
    });
    stage("submit returned identity", errors_,
          [&] { after_ = run_.world.identity(); });
    capture(directory, run_, request_, before_, identity_,
            previewed_.ok ? previewed_.value : identity_, result, after_,
            false);
  }

  // 每个子断言在 require 前保留实际布尔值，不能仅有最终失败字符串。
  void checks(const StableId &name, const V &values) noexcept {
    stage("actual assertion values", errors_,
          [&] { write_json(directory / (name + ".json"), values); });
  }

  // 失败自动补全原状态及逐文件摘要；任何观察异常不覆盖原业务错误。
  ~Attempt() noexcept {
    const bool failed = std::uncaught_exceptions() > exceptions_ ||
                        (preview_returned_ && !preview_ok_) ||
                        (submit_returned_ && !submit_ok_);
    if (failed) {
      if (submit_retained_)
        capture(directory, run_, request_, before_, identity_,
                previewed_.ok ? previewed_.value : identity_, submitted_,
                after_, true);
      else
        incomplete_return();
      stage("failure original pre sources", errors_,
            [&] { save_sources(directory, "pre", before_, run_.catalog); });
    }
    stage("automatic attempt status", errors_, [&] {
      write_json(
          directory / "attempt-status.json",
          boundary::obj({{"preview_returned", V{preview_returned_}},
                         {"submit_returned", V{submit_returned_}},
                         {"preview_outcome_retained", V{preview_retained_}},
                         {"submit_outcome_retained", V{submit_retained_}},
                         {"failure_observed", V{failed}},
                         {"observer_exceptions", V{errors_}},
                         {"no_repeat_submission", V{true}}}));
    });
    const auto errors_before_manifest = errors_.size();
    stage("automatic evidence manifest", errors_, [&] {
      V::Array files;
      for (const auto &entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file() ||
            entry.path().filename() == "manifest.json")
          continue;
        const auto bytes = read_text(entry.path());
        files.push_back(boundary::obj(
            {{"path", V{entry.path().filename().string()}},
             {"bytes", V{bytes.size()}},
             {"sha256", V{picosha2::hash256_hex_string(bytes)}}}));
      }
      write_json(
          directory / "manifest.json",
          boundary::obj({{"schema", V{"hanmo.original-command-evidence.v1"}},
                         {"files", V{std::move(files)}}}));
    });
    if (errors_.size() != errors_before_manifest)
      stage("persist manifest failure", errors_, [&] {
        write_json(directory / "manifest-failure.json",
                   boundary::obj({{"manifest_available", V{false}},
                                  {"observer_exceptions", V{errors_}}}));
      });
  }
};
} // namespace checklist_commit_observation
