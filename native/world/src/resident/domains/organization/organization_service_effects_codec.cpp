#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_service_effects_codec.hpp"
#include "organization_service_effects_fields.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace {
bool service_parent(const OrganizationFact& fact){
    return fact.kind=="organization_member_response"||
        fact.kind=="organization_member_services_synchronized"||
        (fact.kind=="organization_office_changed"&&fact.office_change_payload&&
         fact.office_change_payload->schema_version==2);
}
template<class C,class T>Status value(C&,std::string_view,T&);
template<class C,class T>Status record(C& c,T& row){
    auto field=[&](std::string_view name,auto& member){value(c,name,member);};
    if constexpr(std::is_same_v<T,OrganizationRoleEnabledChange>)
        organization_service_effects_fields::role_change(row,field);
    else if constexpr(std::is_same_v<T,OrganizationServiceEffect>)
        organization_service_effects_fields::effect(row,field);
    else {
        static_assert(std::is_same_v<T,OrganizationServiceEffects>);
        organization_service_effects_fields::payload(row,field);
    }
    return c.status();
}
template<class C,class T>Status value(C& c,std::string_view name,T& row){
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,bool>)return c.field(name,row);
    else if constexpr(std::is_same_v<T,std::optional<OrganizationRoleEnabledChange>>)
        return c.optional(name,row,[](auto& child,auto& entry){return record(child,entry);});
    else {
        static_assert(std::is_same_v<T,std::vector<OrganizationServiceEffect>>);
        return c.sequence(name,row,[](auto& child,auto& entry){return record(child,entry);});
    }
}
// A transient canonical decode table, never a second durable root. Its payload
// is nothrow movable; default values, rows and adoption use the sole archive.
static_assert(std::is_nothrow_move_constructible_v<OrderedTable<OrganizationServiceEffects>::Row>);
template<class C>Status payload_table(C& c,OrderedTable<OrganizationServiceEffects>& rows){
    auto visit=[](auto& child,auto& payload){return record(child,payload);};
    if constexpr(std::is_same_v<C,CodecVisitor>)return common_checkpoint::table(c,"fact_effects",rows,visit);
    else return c.table("fact_effects",rows,visit);
}
// Reserve both actual borrowed graphs before their first ordinary copy.
// The independently charged table.set owns the lasting target, not this lease.
struct AttachedFact {
    CheckpointMemoryCharge copied_fields;
    OrganizationFact value;
    AttachedFact(const OrganizationFact& original,const OrganizationServiceEffects& effects)
        :copied_fields([&]{
            return CheckpointMemoryCharge(0);}()),
         value(original){value.service_effects.emplace(effects);}
};
Status attach(OrganizationState& state,const OrderedTable<OrganizationServiceEffects>& decoded){
    std::size_t position=0;
    // Validate exact coverage/order before touching any original fact.
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;
        if(row.first!=fact.id)return common_failure("OrganizationServiceEffectsFactKeyMismatch",row.first);
        if(fact.service_effects)return common_failure("OrganizationServiceEffectsAlreadyAttached",row.first);
        if(!service_parent(fact))continue;
        if(position>=decoded.size()||decoded.rows()[position].first!=row.first)
            return common_failure("OrganizationServiceEffectsCoverageMismatch",row.first);
        auto valid=validate_organization_service_effects_parent(fact,decoded.rows()[position].second);
        if(!valid.ok)return valid;
        ++position;
    }
    if(position!=decoded.size())return common_failure("UnexpectedOrganizationServiceEffectsFact");
    for(const auto& row:decoded.rows()){
        const auto* original=state.facts.find(row.first);
        AttachedFact attached(*original,row.second);
        state.facts.set(row.first,static_cast<const OrganizationFact&>(attached.value));
    }
    return validate_organization_service_effects(state,1);
}
template<class C>Status fields(C& c,OrganizationState& state,Integer selected_schema){
    if(!c.status().ok)return c.status();
    if(selected_schema!=1)return c.reject("UnsupportedOrganizationServiceEffectsSchema");
    if constexpr(std::is_same_v<C,CodecVisitor>)
        (void)0;
    if(!c.reading()){auto valid=validate_organization_service_effects(state,selected_schema);if(!valid.ok)return valid;}
    Integer schema=1;c.field("schema",schema);
    if(!c.status().ok)return c.status();
    if(schema!=1)return c.reject("UnsupportedOrganizationServiceEffectsSchema");
    auto prepared=prepare_checkpoint_default_values<OrderedTable<OrganizationServiceEffects>>(c);
    if(!prepared.ok)return prepared;
    OrderedTable<OrganizationServiceEffects> rows;
    // Const-ref insert is the canonical first-copy boundary, retaining its
    // actual table lease through emission. No uncharged projection collector.
    if(!c.reading())for(const auto& row:state.facts.rows())if(row.second.service_effects)
        if(!rows.insert(row.first,*row.second.service_effects))return c.reject("DuplicateOrganizationServiceEffectsFact",row.first);
    auto valid=payload_table(c,rows);
    if(valid.ok&&c.reading())valid=attach(state,rows);
    return valid;
}
}
Status validate_organization_service_effects_parent(const OrganizationFact& fact,const OrganizationServiceEffects& payload){
    if(!service_parent(fact)||fact.id.empty()||fact.organization_id.empty()||!fact.occurred_at||
       fact.occurred_at->half_day_tick!=fact.tick||fact.grant_change_payload)
        return common_failure("InvalidOrganizationServiceEffectsParent",fact.id);
    const bool office=fact.kind=="organization_office_changed";
    if(!office&&(fact.actor_id.empty()||fact.office_change_payload))
        return common_failure("InvalidOrganizationServiceEffectsParent",fact.id);
    if(office&&fact.office_id.empty())return common_failure("InvalidOrganizationServiceEffectsParent",fact.id);
    if(fact.kind=="organization_member_services_synchronized"&&payload.changes.empty())
        return common_failure("OrganizationServiceSynchronizationMustChange",fact.id);
    for(std::size_t i=0;i<payload.changes.size();++i){
        const auto& effect=payload.changes[i];
        if(effect.duty_id.empty()||effect.actor_id.empty()||!effect.duty_active_before||
           (effect.duty_active_after&&!effect.role_enabled_change))
            return common_failure("InvalidOrganizationServiceEffect",fact.id);
        if(effect.role_enabled_change&&(effect.role_id.empty()||
           effect.role_enabled_change->before==effect.role_enabled_change->after))
            return common_failure("InvalidOrganizationRoleEnabledChange",fact.id);
        if(office){
            const auto& original=*fact.office_change_payload;
            if(!original.old_holder||effect.actor_id!=*original.old_holder||effect.duty_active_after||
               (effect.role_enabled_change&&(!effect.role_enabled_change->before||effect.role_enabled_change->after)))
                return common_failure("OrganizationOfficeServiceEffectMismatch",fact.id);
        }else if(effect.actor_id!=fact.actor_id)return common_failure("OrganizationMemberServiceEffectMismatch",fact.id);
        for(std::size_t prior=0;prior<i;++prior){
            const auto& earlier=payload.changes[prior];
            if(earlier.duty_id==effect.duty_id)return common_failure("DuplicateOrganizationServiceDutyEffect",fact.id);
            if(earlier.role_enabled_change&&effect.role_enabled_change&&earlier.role_id==effect.role_id)
                return common_failure("DuplicateOrganizationServiceRoleEffect",fact.id);
        }
    }
    return common_success();
}
Status validate_organization_service_effects(const OrganizationState& state,Integer selected_schema){
    if(selected_schema!=0&&selected_schema!=1)return common_failure("UnsupportedOrganizationServiceEffectsSchema");
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;
        if(!selected_schema){
            if(fact.service_effects||fact.kind=="organization_member_services_synchronized")
                return common_failure("OrganizationServiceEffectsCodecRequired",row.first);
            continue;
        }
        if(row.first!=fact.id)return common_failure("OrganizationServiceEffectsFactKeyMismatch",row.first);
        if(service_parent(fact)){
            if(!fact.service_effects)return common_failure("OrganizationServiceEffectsPayloadRequired",row.first);
            auto valid=validate_organization_service_effects_parent(fact,*fact.service_effects);if(!valid.ok)return valid;
        }else if(fact.service_effects)return common_failure("UnexpectedOrganizationServiceEffectsFact",row.first);
    }
    return common_success();
}
Status decode_organization_service_effects(const BoundaryValue& input,OrganizationState& state,Integer schema){
    ActorFoundationJsonVisitor c(input);auto valid=fields(c,state,schema);if(valid.ok)valid=c.finish();return valid;
}
Status emit_organization_service_effects_projection(ProjectionWriter& writer,std::string_view name,const OrganizationState& state,Integer schema){
    return emit_projection_record<ActorFoundationProjectionArchive>(writer,name,state,
        [&](auto& c,auto& value){return fields(c,value,schema);});
}
Outcome<BoundaryValue> encode_organization_service_effects(const OrganizationState& state,Integer schema){
    BoundaryValue out;BoundaryProjectionWriter writer(out);
    auto valid=emit_organization_service_effects_projection(writer,"",state,schema);
    if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};
}
Status visit_organization_service_effects_checkpoint(CodecVisitor& c,OrganizationState& state,Integer schema){
    return fields(c,state,schema);
}
} // namespace hanmo::resident
