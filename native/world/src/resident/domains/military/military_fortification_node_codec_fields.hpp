// 六槽的唯一字段 callback；JSON、binary、projection 共用，不补来源字段。
#pragma once
#include "military_fortification_nodes.hpp"
namespace hanmo::resident::military_fortification_node_fields {
// 职责：保原声明次序和整数/Id 类型；业务来源须由各真正 owning reader 认证。
template <class Archive> Status value(Archive &c, FortificationNodeState &row) {
  c.field("stronghold_id", row.stronghold_id);
  c.field("topology_id", row.topology_id);
  c.field("node_id", row.node_id);
  c.field("integrity", row.integrity);
  c.field("revision", row.revision);
  c.field("last_change_source_id", row.last_change_source_id);
  return c.status();
}
}
