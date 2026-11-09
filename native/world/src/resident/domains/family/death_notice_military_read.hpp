// 军事亲历DTO到S4文书内容来源的薄适配，不扩大观察范围或写入任一域。
#pragma once
#include "death_notice_source.hpp"
namespace hanmo::resident {
struct MilitaryState;struct NavalState;
// 调用者已验本候选完整军事/Naval历史；所借三个对象须保持同一候选与生命周期。
FamilyDeathWitnessRead family_military_death_witness_read(const CommonStore&,const MilitaryState&,const NavalState*);
}
