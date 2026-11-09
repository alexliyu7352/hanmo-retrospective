// S2唯一军事权源值；政治Office/Grant历史只引用原唯一registry。
#pragma once
#include "resident/domains/organization/authority_registry.hpp"
namespace hanmo::resident {
struct LegacyMilitaryOfficeRoot {
    StableId profile_id;Integer military_schema_version=0;
    StableId military_office_id,office_definition_id,holder_actor_id;bool enabled=false;
    std::optional<StableId> original_appointment_id;
    StableId organization_id,garrison_id,command_scope;WorldIdentity source_world;
    std::optional<SourceRef> original_source_ref;
};
struct OrganizationOfficeGrantRoot {
    StableId office_id,grant_id;Integer authority_epoch=0;
    StableId appointment_source_id,organization_id;ScopeRef scope;
    AuthorityBasisRef authority_basis_ref;
};
struct PrivateServiceCommandRoot {
    StableId agreement_id;Integer accepted_terms_revision=0;
    StableId command_assignment_id;Integer assignment_revision=0;
    StableId commander_actor_id,principal_id;std::vector<StableId> controlled_force_ids;
    Integer valid_from_tick=0;std::optional<Integer> valid_until_tick;StableId accepted_source_id;
};
using MilitaryAuthoritySource=std::variant<LegacyMilitaryOfficeRoot,OrganizationOfficeGrantRoot,PrivateServiceCommandRoot>;
enum class MilitaryAuthorityScope { self,unit,army,named_forces };
struct MilitaryAuthorityRef {SubjectRef principal;StableId acting_actor_id;MilitaryAuthoritySource source;MilitaryAuthorityScope allowed_scope=MilitaryAuthorityScope::self;std::vector<StableId> allowed_target_ids;};
// The original occupation address is not another lock or inferred Action lease.
struct MilitaryParticipationRef {StableId actor_id,owner_activity_id,occupation_source_id;Integer occupation_version=0;};
}
