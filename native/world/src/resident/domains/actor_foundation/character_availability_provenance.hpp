// Original availability dependencies, borrowed from the same source-local read.
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <array>
namespace hanmo::resident {
struct CharacterOriginRecordV2;struct RegistrationSourceV2;
struct InitialCapabilityAllocation;struct BirthFactRef;struct FactRecord;
enum class CharacterAvailabilityProvenanceSlot {
 original_origin,initial_condition_seed,created_creation,created_registration,
 birth_allocation,original_birth,born_creation,capacity_transition,capacity_cause,
 death,death_cause
};
struct CharacterAvailabilityProvenanceSlotSpec {
 CharacterAvailabilityProvenanceSlot slot;
 std::string_view name;
 bool established_source_identity;
};
// Consumers register these same names, never a second independent slot list.
inline constexpr std::array<CharacterAvailabilityProvenanceSlotSpec,11>
 character_availability_provenance_slots{{
  {CharacterAvailabilityProvenanceSlot::original_origin,"original_origin",false},
  {CharacterAvailabilityProvenanceSlot::initial_condition_seed,"initial_condition_seed",true},
  {CharacterAvailabilityProvenanceSlot::created_creation,"created_creation",true},
  {CharacterAvailabilityProvenanceSlot::created_registration,"created_registration",false},
  {CharacterAvailabilityProvenanceSlot::birth_allocation,"birth_allocation",false},
  {CharacterAvailabilityProvenanceSlot::original_birth,"original_birth",false},
  {CharacterAvailabilityProvenanceSlot::born_creation,"born_creation",false},
  {CharacterAvailabilityProvenanceSlot::capacity_transition,"capacity_transition",true},
  {CharacterAvailabilityProvenanceSlot::capacity_cause,"capacity_cause",true},
  {CharacterAvailabilityProvenanceSlot::death,"death",true},
  {CharacterAvailabilityProvenanceSlot::death_cause,"death_cause",true}
 }};
// Synchronous, read-only callbacks. Borrowed values last only through append.
// A sink must not mutate any candidate root. Stop at its first failure and
// discard accumulated output if the enclosing read fails. False means checked
// but not applied at the cut, not an optional dependency that may be omitted.
class CharacterAvailabilityProvenanceSink {
public:
 virtual ~CharacterAvailabilityProvenanceSink()=default;
 virtual Status append(CharacterAvailabilityProvenanceSlot,SourceRefBuildView,
  Integer actual_schema,bool applied_before_cut)=0;
 // These actual original rows/references have no independently established
 // retention SourceRef/schema mapping. Never infer one from a C++ type name.
 // A PIN adapter must resolve its real owner registration or fail explicitly.
 virtual Status original_origin(const CharacterOriginRecordV2&,bool applied_before_cut)=0;
 virtual Status created_registration(const RegistrationSourceV2&,bool applied_before_cut)=0;
 virtual Status birth_allocation(const InitialCapabilityAllocation&,bool applied_before_cut)=0;
 virtual Status original_birth(const BirthFactRef&,bool applied_before_cut)=0;
 virtual Status born_creation(const FactRecord&,bool applied_before_cut)=0;
};
}
