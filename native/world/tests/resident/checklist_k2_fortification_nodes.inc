// Fortification 原六槽承载组件，借唯一 sealed Prepared 的真实城防内容。
// 节点数值是隔离 codec 样本，未发布到世界、不声称 last_change 事实已认证。
namespace checklist_k2_fortification_nodes {
// 职责：原 job 内解码/复制，所有拥有值在其真实预算存活期内消费。
template <class F>
void in_job(F &&probe, const CapacityProfile &profile =
                           engineering_checkpoint_capacity_profile()) {
  auto job =
      CheckpointJobBudget::create(profile, engineering_slot_capacity_profile());
  require(job.ok, "fortification component job");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "fortification candidate resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(profile, job.value);
  ScopedCheckpointBoundaryDecode fields(budget, job.value, resource.value);
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::dynamic_military);
  probe(budget);
}
// 职责：借真正已有内容挑两个节点，只造明确隔离的值样本，不补世界节点初态。
MilitaryFortificationNodes sample(const MilitaryCatalog &catalog) {
  require(!catalog.strongholds.empty(),
          "shared catalog has original stronghold");
  const auto &stronghold = catalog.strongholds.front();
  const auto topology =
      std::find_if(catalog.siege_topologies.begin(),
                   catalog.siege_topologies.end(), [&](const auto &row) {
                     return row.id == stronghold.topology_id;
                   });
  require(topology != catalog.siege_topologies.end() &&
              topology->nodes.size() >= 2,
          "same immutable catalog owns original topology nodes");
  MilitaryFortificationNodes out;
  for (std::size_t i = 0; i < 2; ++i) {
    const auto &node = topology->nodes[i];
    out.rows.push_back({stronghold.id, topology->id, node.id, 0,
                        i == 0 ? Integer{-7} : Integer{12},
                        "component::unverified_node_change_source"});
  }
  std::sort(out.rows.begin(), out.rows.end(), [](const auto &a, const auto &b) {
    return std::tie(a.stronghold_id, a.topology_id, a.node_id) <
           std::tie(b.stronghold_id, b.topology_id, b.node_id);
  });
  return out;
}
// 职责：每次负例保原精确错误，job 抛出也不被归成泛成功。
void rejected(const V &input, const StableId &code) {
  bool seen = false;
  in_job([&](CodecDecodeBudget &) {
    try {
      const auto result = decode_military_fortification_nodes(input);
      require(!result.ok && result.error.original.code == code,
              "fortification JSON exact error: " + code + " got " +
                  result.error.original.code);
      seen = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error && error->original.code == code,
              "fortification job error: " + code);
      seen = true;
    }
  });
  require(seen, "fortification negative executed");
}
// 职责：普通拥有复制保非空 vector 与全部字段，源与副本分配独立。
void copy_and_retain(const MilitaryFortificationNodes &original) {
  const MilitaryFortificationNodes copy(original);
  exact(must(encode_military_fortification_nodes(original), "original nodes"),
        must(encode_military_fortification_nodes(copy), "copied nodes"),
        "six slots survive original owned copy");
  require(copy.rows.data() != original.rows.data(),
          "copy owns a distinct actual array");

}
// 职责：原 sealed 内容的拓扑、节点和 max_hp 决定结构边界，不借当前战果。
void lookup(const MilitaryFortificationNodes &nodes,
            const MilitaryCatalog &catalog) {
  must(validate_military_fortification_node_catalog(nodes, catalog),
       "original content linkage");
  for (const auto &row : nodes.rows) {
    const auto *found =
        must(read_military_fortification_node(nodes, catalog, row.stronghold_id,
                                              row.topology_id, row.node_id),
             "actual three-ID lookup");
    require(found == &row,
            "lookup borrows original row, not a replacement snapshot");
  }
  const auto missing = read_military_fortification_node(
      nodes, catalog, nodes.rows.front().stronghold_id,
      nodes.rows.front().topology_id, "missing_node");
  require(!missing.ok &&
              missing.error.original.code == "UnknownFortificationNode",
          "no default node");
  MilitaryFortificationNodes changed;
  changed.rows.push_back(nodes.rows.front());
  changed.rows.front().topology_id = "not_the_original_topology";
  auto invalid = validate_military_fortification_node_catalog(changed, catalog);
  require(!invalid.ok && invalid.error.original.code ==
                             "FortificationNodeTopologyMismatch",
          "real topology lock");
  changed.rows.front() = nodes.rows.front();
  changed.rows.front().integrity = std::numeric_limits<Integer>::max();
  invalid = validate_military_fortification_node_catalog(changed, catalog);
  require(!invalid.ok && invalid.error.original.code ==
                             "FortificationNodeIntegrityOutOfRange",
          "real node max_hp");
  changed.rows.front() = nodes.rows.front();
  changed.rows.front().node_id = "not_an_authored_node";
  invalid = validate_military_fortification_node_catalog(changed, catalog);
  require(!invalid.ok &&
              invalid.error.original.code == "UnknownFortificationTopologyNode",
          "real node existence");
}
// 职责：真实两 codec 往返与严格拒绝，不接完整 section 或原业务 writer。
void run(Run &world) {
  const auto before = world.bytes();
  require(bool(world.catalog->military),
          "same sealed catalog has military definitions");
  const auto &catalog = *world.catalog->military;
  V encoded;
  CodecVisitor::Buffer binary;
  in_job([&](CodecDecodeBudget &) {
    auto nodes = sample(catalog);
    lookup(nodes, catalog);
    encoded = must(encode_military_fortification_nodes(nodes),
                   "encode original six slots");
    copy_and_retain(nodes);
    CodecVisitor writer(binary);
    must(visit_military_fortification_nodes(writer, nodes),
         "write original typed nodes");
    must(writer.finish(), "finish typed node writer");
  });
  in_job([&](CodecDecodeBudget &) {
    const auto nodes = must(decode_military_fortification_nodes(encoded),
                            "decode original node array");
    lookup(nodes, catalog);
    exact(encoded,
          must(encode_military_fortification_nodes(nodes), "node JSON reemit"),
          "node JSON round trip");
    require(nodes.rows.front().revision == -7 ||
                nodes.rows.back().revision == -7,
            "no unapproved positive revision policy added");
  });
  in_job([&](CodecDecodeBudget &budget) {
    MilitaryFortificationNodes nodes;
    const auto &input = binary;
    CodecVisitor reader(input, budget.profile, "dynamic_military", budget);
    must(visit_military_fortification_nodes(reader, nodes),
         "decode original binary nodes");
    must(reader.finish(), "finish binary nodes");
    require(budget.job_bound(), "binary nodes stay in the original job");
    lookup(nodes, catalog);
    exact(encoded,
          must(encode_military_fortification_nodes(nodes),
               "binary node projection"),
          "node binary round trip");
  });
  // 数量大于余下真实字节时必须在构造任何行前拒绝；不再复写逐型 quota。
  auto malformed = binary;
  const std::size_t count_at = 1 + 8 + std::string_view("fortification_nodes").size();
  require(malformed.size() >= count_at + 8, "actual node sequence header");
  const std::uint64_t count = malformed.size() - (count_at + 8) + 1;
  for (unsigned byte = 0; byte < 8; ++byte)
    malformed[count_at + byte] = static_cast<std::uint8_t>(count >> (8 * byte));
  bool truncated_seen = false;
  in_job([&](CodecDecodeBudget &budget) {
    MilitaryFortificationNodes nodes;
    const auto check = [&](const RuleError &error) {
      require(error.original.code == "TruncatedCheckpoint",
              "damaged node count rejects on real remaining bytes");
      require(nodes.rows.empty(), "damaged count rejects before row allocation");
      truncated_seen = true;
    };
    try {
      const auto &input = malformed;
      CodecVisitor reader(input, budget.profile, "dynamic_military", budget);
      const auto result = visit_military_fortification_nodes(reader, nodes);
      require(!result.ok, "damaged binary node count rejected");
      check(result.error);
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error != nullptr, "damaged count keeps materializable first error");
      check(*error);
    }
  });
  require(truncated_seen, "actual remaining-byte refusal path reached");
  const auto no_context = decode_military_fortification_nodes(encoded);
  require(!no_context.ok &&
              no_context.error.original.code == "CheckpointJsonContextRequired",
          "new array cannot bypass original checkpoint job");
  rejected(V{V::Dict{}}, "InvalidResidentArray");
  auto swapped = encoded;
  std::swap((*swapped.as<V::Array>())[0], (*swapped.as<V::Array>())[1]);
  rejected(swapped, "FortificationNodeOrderMismatch");
  auto duplicate = encoded;
  duplicate.as<V::Array>()->insert(duplicate.as<V::Array>()->begin(),
                                   duplicate.as<V::Array>()->front());
  rejected(duplicate, "DuplicateFortificationNode");
  auto wrong_fields = encoded;
  auto &record = *wrong_fields.as<V::Array>()->front().as<V::Dict>();
  std::swap(record[0], record[1]);
  rejected(wrong_fields, "ResidentFieldOrderMismatch");
  auto missing = encoded;
  missing.as<V::Array>()->front().as<V::Dict>()->pop_back();
  rejected(missing, "MissingResidentField");
  auto extra = encoded;
  extra.as<V::Array>()->front().as<V::Dict>()->emplace_back("synthetic_id",
                                                            V{"forbidden"});
  rejected(extra, "UnsupportedResidentField");
  in_job([&](CodecDecodeBudget &) {
    const auto empty = must(decode_military_fortification_nodes(V{V::Array{}}),
                            "legal empty array");
    const auto absent = read_military_fortification_node(
        empty, catalog, "stronghold", "topology", "node");
    require(!absent.ok &&
                absent.error.original.code == "UnknownFortificationNode",
            "empty array is not ready source");
  });
  require(world.bytes() == before,
          "node component never alters installed state");
  std::cout << "K2 fortification nodes auxiliary: original six-slot "
               "array/tuple, sealed catalog "
               "lookup, JSON/binary and owned graph PASS; change source, "
               "writer, canonical "
               "section install and K2 acceptance NOT ASSERTED\n";
}
} // namespace checklist_k2_fortification_nodes
