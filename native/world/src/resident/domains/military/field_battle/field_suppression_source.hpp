#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
struct FieldBattleParameters;
Status validate_field_suppression_sources(const FieldBattleSession &,
                                          const FieldBattleParameters &);
} // namespace hanmo::resident
