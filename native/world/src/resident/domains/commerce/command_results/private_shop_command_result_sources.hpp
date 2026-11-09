// 临时来源投影保创建条款，剔除原生命周期允许变化的缓存；不复制为新永久业务账。
#pragma once
#include "private_shop_command_results.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct PrivateShopCommandFactSource {StableId id,kind;Integer tick=0;BoundaryValue stable_payload;};
// 精确原13类表/头全覆盖，按真实Common顺序；每项同时核canonical实例号。
Outcome<std::vector<PrivateShopCommandFactSource>> read_private_shop_command_fact_sources(const CommonStore&,const PrivateShopState&,ProjectionWriter* projection=nullptr);
// 原Family三类真实控制载荷与头的有限投影；有控制头而无原状态明确拒绝。
Outcome<std::vector<PrivateShopCommandFactSource>> read_private_shop_command_control_sources(const CommonStore&,const FamilyState*,const FamilyLegacyState*);
// 真正同World前后差分；旧源不许删除/更改创建条款，新源必须本次实际分配且tick在提交区间。
Outcome<std::vector<PrivateShopCommandFactSource>> private_shop_new_command_fact_sources(
    const CommonStore& before_common,const PrivateShopState& before_shops,const CommonStore& common,const PrivateShopState& shops);
}
