// LIFE-9 整行后置；只证明当前 Complete 世界未安装法术，不提供法效业务。
#pragma once
#include "resident/model/common_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct WorldBundle;
Status validate_native_deferred_arcana_absence(const WorldBundle &);
bool native_deferred_arcana_command_kind(std::string_view) noexcept;
} // namespace hanmo::resident
