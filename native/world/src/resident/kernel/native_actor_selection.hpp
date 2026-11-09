// 新政策只选择已在有效快照中的原人物；不生成身份、属性、资产、关系或义务。
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
// 仅识别明确注册的人物选择政策；旧两人政策与新有界名单分别保留版本身份。
bool native_existing_actor_policy(const StableId& policy);
// 冷准入核作者版本对应的成人名单及逐人外貌政策；不要求本次请求已选人物。
Status validate_native_actor_choices(const BoundaryValue& effective_data,const BoundaryValue& scenario);
// 显式请求必须属于完整作者表；失败返回空ID，成功仍是作者原ID且输入不变。
Outcome<StableId> select_native_actor(const BoundaryValue& effective_data,const BoundaryValue& scenario,const std::optional<StableId>& requested_actor_id);
}
