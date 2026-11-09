// 原接管三域真实未选门；每次借同 World 反扫，不保存成功票或提供空接管能力。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 只证明接管选择、三域账及权源确实不存在；不是完整 LE/Task/生产来源验证。
Status validate_unselected_political_takeover_sources(const WorldBundle &);
} // namespace hanmo::resident
