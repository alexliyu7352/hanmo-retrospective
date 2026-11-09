// 作者委托和动态机会共用既有有限引用；从任务模型提取，不改变协议或身份。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct OfferRef { enum class Kind { authored,runtime };Kind kind=Kind::authored;StableId id; };
}
