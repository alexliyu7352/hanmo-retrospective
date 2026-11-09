// Explicit original Birth1/Birth4 selection. One existing FamilyBirthState.
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/domains/family/born_character_source.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<Integer> selected_native_family_birth_version(const CatalogIdentity&);
Outcome<Integer> read_native_family_birth_selection(const BoundaryValue&);
Outcome<BirthArrangementCatalog> decode_native_family_birth_catalog(const BoundaryValue&,const CatalogIdentity&);
Status prepare_native_dependent_provision_catalog(const BoundaryValue& data,PreparedCatalog&);
Status validate_native_family_birth_catalog(const PreparedCatalog&);
Status validate_native_family_birth_layout(const WorldBundle&);
Status validate_native_family_birth_bindings(const WorldBundle&,const BornCharacterSourceContext&);
Outcome<FamilyBirthState> decode_native_family_birth(const BoundaryValue&,Integer);
Status visit_native_family_birth(CodecVisitor&,FamilyBirthState&,Integer);
}
