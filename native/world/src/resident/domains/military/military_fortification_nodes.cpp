#include "resident/kernel/catalog_section_observation.hpp"
#include "military_fortification_nodes.hpp"
#include "military_model.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <tuple>

namespace hanmo::resident {
namespace {
// 职责：借三原 ID 作无歧义比较，不复制键、不生成业务身份。
auto key(const FortificationNodeState &row) {
  return std::tie(row.stronghold_id, row.topology_id, row.node_id);
}
// 职责：在真实原内容集合按原 ID 找唯一行；重复内容拒绝，不选第一项。
template <class T>
Outcome<const T *>
find_unique_definition(const std::vector<T> &rows, const StableId &id,
                       const StableId &missing, const StableId &duplicate) {
  const T *found = nullptr;
  for (const auto &row : rows) {
    if (row.id != id)
      continue;
    if (found)
      return {false, nullptr, common_failure(duplicate, id).error};
    found = &row;
  }
  if (!found)
    return {false, nullptr, common_failure(missing, id).error};
  return {true, found, {}};
}
}
// 职责：拒绝非法身份和不规范集合，不为原 revision 添加未经约定的范围。
Status validate_military_fortification_node_order(
    const MilitaryFortificationNodes &nodes) {
  const FortificationNodeState *previous = nullptr;
  for (const auto &row : nodes.rows) {
    if (row.stronghold_id.empty() || row.topology_id.empty() ||
        row.node_id.empty() || row.last_change_source_id.empty())
      return common_failure("FortificationNodeIdentityRequired", row.node_id);
    if (row.integrity < 0)
      return common_failure("FortificationNodeIntegrityOutOfRange",
                            row.node_id);
    if (previous && key(*previous) == key(row))
      return common_failure("DuplicateFortificationNode", row.node_id);
    if (previous && !(key(*previous) < key(row)))
      return common_failure("FortificationNodeOrderMismatch", row.node_id);
    previous = &row;
  }
  return common_success();
}
// 职责：逐行使用原 stronghold 内容与真正拓扑节点范围，无旧战果反推。
Status validate_military_fortification_node_catalog(
    const MilitaryFortificationNodes &nodes, const MilitaryCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 81, "MilitaryCatalog", "validate_military_fortification_node_catalog", &catalog);

  auto ordered = validate_military_fortification_node_order(nodes);
  if (!ordered.ok)
    return ordered;
  for (const auto &row : nodes.rows) {
    auto stronghold =
        find_unique_definition(catalog.strongholds, row.stronghold_id,
                               "UnknownFortificationStronghold",
                               "DuplicateFortificationStrongholdDefinition");
    if (!stronghold.ok)
      return {false, {}, std::move(stronghold.error)};
    if (stronghold.value->topology_id != row.topology_id)
      return common_failure("FortificationNodeTopologyMismatch", row.node_id);
    auto topology =
        find_unique_definition(catalog.siege_topologies, row.topology_id,
                               "UnknownFortificationTopology",
                               "DuplicateFortificationTopologyDefinition");
    if (!topology.ok)
      return {false, {}, std::move(topology.error)};
    auto node = find_unique_definition(
        topology.value->nodes, row.node_id, "UnknownFortificationTopologyNode",
        "DuplicateFortificationTopologyNodeDefinition");
    if (!node.ok)
      return {false, {}, std::move(node.error)};
    if (row.integrity > node.value->max_hp)
      return common_failure("FortificationNodeIntegrityOutOfRange",
                            row.node_id);
  }
  return common_success();
}
// 职责：全根核唯一性和真实内容，再用原 tuple 二分借行；不制造默认结果。
Outcome<const FortificationNodeState *> read_military_fortification_node(
    const MilitaryFortificationNodes &nodes, const MilitaryCatalog &catalog,
    const StableId &stronghold_id, const StableId &topology_id,
    const StableId &node_id) {
  auto valid = validate_military_fortification_node_catalog(nodes, catalog);
  if (!valid.ok)
    return {false, nullptr, std::move(valid.error)};
  const auto wanted = std::tie(stronghold_id, topology_id, node_id);
  const auto found =
      std::lower_bound(nodes.rows.begin(), nodes.rows.end(), wanted,
                       [](const auto &row, const auto &requested) {
                         return key(row) < requested;
                       });
  if (found == nodes.rows.end() || key(*found) != wanted)
    return {false, nullptr,
            common_failure("UnknownFortificationNode", node_id).error};
  return {true, &*found, {}};
}
}
