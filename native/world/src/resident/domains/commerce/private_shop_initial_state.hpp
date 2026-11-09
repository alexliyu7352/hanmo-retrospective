// 原make_initial_private_shops的只读初态门；不创建、清空或迁移任何运行表。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class CommonStore;
struct PrivateShopCatalog;
struct PrivateShopState;
// 只用于中央已证明fresh初始化的候选：按原工厂同版本/同tick/全空表核已有根。
// 不代目录业务验收，也不授权冷载缺段默认补史；真正初局0/M门由原新局入口保留。
Status validate_original_private_shop_initial_state(const CommonStore &,
                                                    const PrivateShopCatalog &,
                                                    const PrivateShopState &);
} // namespace hanmo::resident
