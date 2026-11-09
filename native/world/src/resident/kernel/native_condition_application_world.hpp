// Original condition applications retain their sole receipt body in
// ActorFoundation.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class CommonStore;
class ProjectionWriter;
struct PreparedCatalog;
struct AuthoredActorFoundationState;
Status validate_native_condition_application_headers(
    const CommonStore &, const PreparedCatalog &,
    const AuthoredActorFoundationState *);
Status emit_native_condition_application_headers(
    ProjectionWriter &, const CommonStore &, const PreparedCatalog &,
    const AuthoredActorFoundationState *);
}
