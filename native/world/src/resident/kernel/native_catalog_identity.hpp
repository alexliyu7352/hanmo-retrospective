// 宿主只读原作者目录身份；真实准入仍由 prepare_native_catalog 独立派生并比较
// expected。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
// 复用原完整选型顺序与工程 decode
// job；不安装、不运行初态、不生成权利或改写输入。 业务拒绝保原
// Outcome；技术错误保持拥有原首错的 FailureException，不当作空身份成功。
Outcome<CatalogIdentity>
read_native_catalog_identity(const BoundaryValue &,
                             const StableId &scenario_id);
}
