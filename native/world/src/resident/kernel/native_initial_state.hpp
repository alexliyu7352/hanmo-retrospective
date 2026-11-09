// 原生新局只从已准入目录建立完整typed初态；失败不安装世界或消耗活世界实例序号。
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"

namespace hanmo::resident {
class FieldBattleResourceRead;
// 创始人与外貌仅是冷请求；fixed_actor内容拒绝覆盖，不在运行状态保存BoundaryValue。
struct NativeInitRequest {
    StableId scenario_id,world_seed;
    CatalogIdentity expected_catalog_identity;
    std::optional<StableId> founder_selection;
    std::optional<BoundaryValue> appearance_request;
    std::optional<BoundaryValue> character_creation_request;
    // 冷请求中的收藏资格，不进入世界银行；只在显式人物卡新局政策接受。
    std::optional<std::vector<StableId>> unlocked_actor_ids;
};
// 构造完成后仍由ResidentWorld::install执行组合校验与成功发布时的execution epoch分配。
struct NativeInitialWorld {
    std::shared_ptr<const PreparedCatalog> catalog;
    InitialWorldSeed seed;
};
// 冷恢复只准备同一选中目录并校验原始身份，不建立人物/资产或运行任何初始化。
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog(const BoundaryValue& catalog,const StableId& scenario_id,const CatalogIdentity& expected,const FieldBattleResourceRead* resources=nullptr);
// 只接受显式原生宏观profile；旧profile继续使用既有合法cold seed入口。
Outcome<NativeInitialWorld> make_native_initial_world(const BoundaryValue& catalog,const NativeInitRequest& request,const FieldBattleResourceRead* resources=nullptr);
} // 目录JSON只存在于本次冷构造调用，不能成为任何领域的运行权威。
