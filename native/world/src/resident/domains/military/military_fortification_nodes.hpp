// S2 原 Fortification 六槽的有限持值集合；不产生修城进度、初态或来源。
// 首次工程根使用原三 ID 的 typed tuple，不加 wire ID 或拼接字串键。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
struct MilitaryCatalog;
// 职责：逐槽保留原节点记录；revision 仅为原 Integer，不补初值/递增政策。
// last_change_source_id 仍为原 Id，不把它改造成未经原 owner 认证的 SourceRef。
struct FortificationNodeState {
  StableId stronghold_id, topology_id, node_id;
  Integer integrity = 0, revision = 0;
  StableId last_change_source_id;
};
// 职责：按原三 ID 全 tuple 严格稳定序拥有六槽行；没有另一份索引真值。
// 空集合没有节点来源，查找缺失仍拒绝，不推满血或从旧围城迁移。
struct MilitaryFortificationNodes {
  std::vector<FortificationNodeState> rows;
};
// 职责：只核身份非空、完整度非负及原 tuple 严格序，拒重复/乱序不修补。
Status
validate_military_fortification_node_order(const MilitaryFortificationNodes &);
// 职责：借真实原内容核 stronghold→topology→node/max_hp，不生成状态。
// 不认证 last_change 的业务 payload，不声明完整变更来源或同 World 身份。
Status
validate_military_fortification_node_catalog(const MilitaryFortificationNodes &,
                                             const MilitaryCatalog &);
// 职责：全根结构/内容校验后按三原 ID 借用唯一行；缺失准确拒绝。
// 返回指针只借此集合，不是可跨恢复保留的 World source/provider。
Outcome<const FortificationNodeState *> read_military_fortification_node(
    const MilitaryFortificationNodes &, const MilitaryCatalog &,
    const StableId &stronghold_id, const StableId &topology_id,
    const StableId &node_id);
}
