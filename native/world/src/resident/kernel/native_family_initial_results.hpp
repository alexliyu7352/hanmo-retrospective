// 同一原Family有限旁支source2分派；旧recognized schema1仍归旧reader。
#pragma once
#include "dynamic_result_time2.hpp"
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
struct CollateralControlState;
// 原头地址与实际payload共同定分派，不据kind把旧认可翻成source2。
bool native_family_collateral_result_header2(const CommonStore &,
                                             const CollateralControlState &,
                                             const FactRecord &) noexcept;
std::shared_ptr<const ExactResultOwnerRead>
make_native_family_initial_results(const NativeCompleteWorldCoreSources &);
} // namespace hanmo::resident
