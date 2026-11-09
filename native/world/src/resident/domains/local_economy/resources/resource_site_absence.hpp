// 无资源点的有限入口；不充当 resource_sites/1 的恢复或采集 provider。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 只接受锁内未选；任何已选版本均保留明确的未实现门。
Status validate_unselected_resource_sites_catalog(const PreparedCatalog &);
// 每次实际消费及冷准入重核原根，不能缓存一次成功代替来源反验。
Status validate_unselected_resource_sites_world(const WorldBundle &);
} // namespace hanmo::resident
