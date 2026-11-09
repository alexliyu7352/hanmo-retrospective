// One real military ActionTime owner; no second elapsed state or timer.
#pragma once
#include "office_exact.hpp"
#include "formation_exact.hpp"
#include "training_exact.hpp"
#include "return_exact.hpp"
#include "resupply_exact.hpp"
#include <optional>
#include <variant>
namespace hanmo::resident {
// Supplied only by the trusted selected catalogue. Presence of a context or
// historical row never implicitly selects a kind.
struct MilitaryActionTimeReadSelection {
 bool office_accept=false,army_form=false,military_train=false;
 bool army_return=false,army_resupply=false;
};
struct MilitaryActionTimeReadContexts {
 std::optional<ExactOfficeReadContext> office;
 std::optional<ExactFormationReadContext> formation;
 std::optional<ExactTrainingReadContext> training;
 std::optional<ExactReturnReadContext> returning;
 std::optional<ExactResupplyReadContext> resupply;
};
// Copies only finite original read contexts (references and budget). Each call
// constructs the corresponding concrete reader on its stack, bound to these
// same roots. The enclosing World lease owns all borrowed professional sources.
class NativeMilitaryActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;
 const MilitaryActionTimeState& state_;const ActionElapsedStore& elapsed_;
 MilitaryActionTimeReadSelection selected_;MilitaryActionTimeReadContexts contexts_;
 using Reader=std::variant<NativeOfficeActionTimeRead,NativeFormationActionTimeRead,
  NativeMilitaryTrainingActionTimeRead,NativeReturnActionTimeRead,NativeResupplyActionTimeRead>;
 void check_bindings()const;
 Reader reader(const StableId& actual_kind)const;
 StableId original_action_kind(const StableId& action)const;
 Reader registered_reader(const ActionTimeRegistration&)const;
public:
 NativeMilitaryActionTimeRead(const CommonStore& c,const MilitaryState& m,
  const MilitaryActionTimeState& s,const ActionElapsedStore& e,
  MilitaryActionTimeReadSelection selected,MilitaryActionTimeReadContexts contexts)
  :common_(c),military_(m),state_(s),elapsed_(e),selected_(selected),contexts_(std::move(contexts)){}
 bool bound_to(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,
  const ActionElapsedStore& e)const noexcept{return &common_==&c&&&military_==&m&&&state_==&s&&&elapsed_==&e;}
 // Source-only reverse coverage before registration. Does not invoke the S0
 // elapsed closure and cannot recursively prove its own full time state.
 Status validate_selected_sources()const;
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
// Only the five actual original C04/C05/C06/C07 readers above are represented.
// Other original kinds need their own complete source reader before selection.
std::vector<ActionTimeRegistration> selected_military_action_time_registrations(
 const MilitaryActionTimeReadSelection&);
}
