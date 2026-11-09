// 个人战跨域协调只借同一个WorldBundle；领域业务仍由personal拥有，中央保唯一提交。
#pragma once
#include "world_bundle.hpp"
namespace hanmo::resident {
Outcome<PersonalQuote> preview_personal_integration(const WorldBundle &,
                                                    const StableId &,
                                                    const PersonalCommand &);
Outcome<PersonalReceipt> apply_personal_integration(WorldBundle &,
                                                    const StableId &,
                                                    const PersonalCommand &,
                                                    const PersonalQuote &);
// 只在真实source1选择下核原每场started初值；冷恢复不重建快照。
Status validate_native_personal_initial_attributes_sources(const WorldBundle &);
} // namespace hanmo::resident
