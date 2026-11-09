#pragma once
#include <memory>
#include "dynamic_result_time2.hpp"
#include "resident/domains/military/dynamic_verified_source.hpp"
#include "resident/domains/military/duty/military_duty_source_read.hpp"
#include "resident/domains/military/training_exact_result_read.hpp"
#include "resident/domains/military/naval_repair_exact.hpp"
namespace hanmo::resident {
bool native_military_supporting_source2(const SourceRef&);
struct MilitaryTrainingResultAccess2 {
 const MilitaryActionTimeState& state;const ActionElapsedStore& elapsed;const ExactTrainingReadContext& original;
};
struct MilitaryNavalRepairResultAccess2 {
 const NavalState& naval;const MilitaryActionTimeState& state;
 const ActionElapsedStore& elapsed;const ExactNavalRepairReadContext& original;
};
struct MilitarySupportingResultAccess2 {
 const CommonStore& common;const MilitaryState& military;const MilitaryExactState& exact;
 const DynamicMilitaryVerifiedSourceContext& dynamic;
 const MilitaryDutyState& duty;const MilitaryDutyContext& duty_original;
 std::optional<MilitaryTrainingResultAccess2> training;
 std::optional<MilitaryNavalRepairResultAccess2> naval_repair;
};
// Source timing and validation belong to S2. These technical applied sources
// provide no Report content and no inferred observer/participant knowledge.
class NativeMilitarySupportingResultRead2 final:public ExactResultOwnerRead {
 MilitarySupportingResultAccess2 x_;
public:
 explicit NativeMilitarySupportingResultRead2(MilitarySupportingResultAccess2 x):x_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}

namespace hanmo::resident {
struct WorldBundle;
class CompleteExactWorldBindingOwner;
struct NativeCompleteWorldCoreSources;
// 有限 Field /1 原军事来源；不因此选择旧 dynamic /2。
bool native_field_military_result_kind(const StableId &domain, const StableId &kind);
// 借真实同 World 的 Complete/core；构造不获取业务租约。每次读取保活
// 原专业租约至全部借用结束，不在冷读时创建事实或伪造时刻。
Outcome<std::unique_ptr<const ExactResultOwnerRead>> make_native_field_military_result_read(
    const WorldBundle &, const CompleteExactWorldBindingOwner &,
    const NativeCompleteWorldCoreSources &);
}
