// 原两项本人输入的严格边界与只读预检，不分配ID、不写获知或组织状态。
#pragma once
#include "political_foundation_decisions.hpp"
namespace hanmo::resident {
inline constexpr const char *create_foundation_intent_command =
    "create_foundation_intent";
inline constexpr const char *decide_foundation_support_command =
    "decide_foundation_support";
using FoundationPersonalCommand =
    std::variant<CreateFoundationIntentCommand, DecideFoundationSupportCommand>;
bool foundation_personal_command_kind(const StableId &) noexcept;
const char *
foundation_personal_command_kind(const FoundationPersonalCommand &) noexcept;
Outcome<FoundationPersonalCommand>
decode_foundation_personal_command(const StableId &kind,
                                   const BoundaryValue &params);
Outcome<BoundaryValue>
encode_foundation_personal_command(const FoundationPersonalCommand &);
Status preview_player_foundation_intent(const FoundationPersonalSourceContext &,
                                        const StableId &actual_actor,
                                        const CreateFoundationIntentCommand &,
                                        const ReadStamp &,
                                        const ReadStampAuthority &,
                                        const DomainReadVersionSource &);
Status preview_player_foundation_support(const FoundationSupportContext &,
                                         const StableId &actual_actor,
                                         const DecideFoundationSupportCommand &,
                                         const ReadStamp &,
                                         const ReadStampAuthority &,
                                         const DomainReadVersionSource &);
} // namespace hanmo::resident
