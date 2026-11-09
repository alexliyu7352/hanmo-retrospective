// The original GovernanceProgram roots, explicitly selected in the complete World.
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/politics/governance_targets/governance_program.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct NativeGovernanceProgramVersions {Integer catalog=0,state=0;};
Outcome<NativeGovernanceProgramVersions> selected_native_governance_program_versions(const CatalogIdentity&);
Status append_native_governance_program_modules(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_governance_program_catalog(const BoundaryValue&,PreparedCatalog&);
Status validate_native_governance_program_catalog(const PreparedCatalog&);
Status initialize_native_governance_program_root(const PreparedCatalog&,InitialWorldSeed&);
Status validate_native_governance_program_layout(const WorldBundle&);
Outcome<GovernanceProgramState> decode_native_governance_programs(const BoundaryValue&,Integer);
Status visit_native_governance_programs(CodecVisitor&,GovernanceProgramState&,Integer);
Status emit_native_governance_programs_projection(ProjectionWriter&,std::string_view,const GovernanceProgramState&,Integer);
}
