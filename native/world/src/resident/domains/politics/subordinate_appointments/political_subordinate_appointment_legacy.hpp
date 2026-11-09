// 原politics writer仍守原Grant与协议；新profile仅禁止同一原空缺被两套未决提名占用。
#pragma once
#include "political_subordinate_appointment_rules.hpp"
namespace hanmo::resident {
Status preview_political_subordinate_legacy_command(const PoliticalSubordinateAppointmentContext&,const PoliticsCommand&);
}
