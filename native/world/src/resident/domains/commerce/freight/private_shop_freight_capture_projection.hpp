// 同一活动捕获内串接原单recipe批次投影；结果仍待中央完整coverage成功，不能作永久货史。
#pragma once
#include "private_shop_freight_recipe_projection.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;
// 只由真实Common活动捕获建立的拥有值前界；调用者不可替换其中原批/前缀/身份。
class PrivateShopFreightProjectionBoundary {
public:
 PrivateShopFreightProjectionBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopFreightProjectionBoundary> capture_private_shop_freight_projection_boundary(const CommonStore&,const std::vector<PrivateShopFreightCargoSelection>&);
 friend struct PrivateShopFreightCaptureProjectionAccess;
};
// 每个实际recipe都保留位置，即使本批未受影响；不只列净损益而遗漏中间变化。
struct PrivateShopFreightCaptureProjection {
 WorldIdentity source_identity;
 std::size_t recipe_begin=0,recipe_end=0;
 std::vector<PrivateShopFreightRecipeProjection> recipes;
 std::vector<PrivateShopFreightLotFrame> remaining_lots;
};
// 原采购/携运权源另作先验；这里仅在当次真实Common取存在且唯一的指定原lot。
Outcome<PrivateShopFreightProjectionBoundary> capture_private_shop_freight_projection_boundary(
 const CommonStore&,const std::vector<PrivateShopFreightCargoSelection>& original_selection);
// 逐真实recipe串接原帧及分裂/转移后继，核最终当前实批；无写入、无隐含finish或新receipt。
// 必须仍持同capture。返回只是provisional；caller随后原finish_resource_execution_coverage成功
// 才能采用，失败整候选及这个值都撤回。端点净零直写、历史权源、跨提交连续性仍未证明。
Outcome<PrivateShopFreightCaptureProjection> read_private_shop_freight_capture_projection(
 const CommonStore&,const PrivateShopFreightProjectionBoundary&);
}
