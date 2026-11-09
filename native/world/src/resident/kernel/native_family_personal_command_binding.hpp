// 原旁支三命令的纯类型配对；不构造业务 owner 或授予执行能力。
#pragma once
#include "hanmo/resident/command_contract.hpp"
#include <string_view>
namespace hanmo::resident {
struct CommandPayload;
bool native_family_personal_command_kind(std::string_view) noexcept;
bool native_family_personal_command_payload(const CommandPayload &) noexcept;
Status validate_native_family_personal_command_binding(const CommandPayload &);
} // namespace hanmo::resident
