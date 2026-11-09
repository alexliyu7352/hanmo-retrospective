// Original S5 Named birth consumption. The complete World owns every root.
#pragma once
#include "local_economy_model.hpp"
#include "resident/domains/family/born_character_source.hpp"
namespace hanmo::resident {
struct BornPopulationSourceContext {
 const CommonStore& common;const LocalEconomyState& economy;const LocalEconomyCatalog& catalog;
 const BornCharacterSourceContext& births;const CurrentMomentRead& clock;
 Integer selected_local_economy=0;
};
// Selection is explicit. Zero is only the old generic, unselected caller.
bool named_birth_population_selected(Integer selected_local_economy)noexcept;
Status validate_named_birth_population_selected_layout(const LocalEconomyState&,Integer selected_local_economy);
Status validate_born_population_source_bindings(const BornPopulationSourceContext&);
// Source schema 1 is mandatory, independent of the physical LE version.
// Returned pointers borrow the real table and cannot outlive its next mutation.
Outcome<const NamedBirthPopulationTransition*> read_named_birth_population_transition(
 const BornPopulationSourceContext&,const SourceRef&,Integer source_schema);
Status validate_named_birth_population_transitions(const BornPopulationSourceContext&);
// The outer World candidate rolls back Common and LE together on any failure.
// No provision, ration, cohort, actor, inventory or wallet is changed here.
Outcome<const NamedBirthPopulationTransition*> record_named_birth_population_transition_in_candidate(
 CommonStore&,LocalEconomyState&,const BornPopulationSourceContext&,const BirthFactRef&);
}
