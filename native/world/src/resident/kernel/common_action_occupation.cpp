#include "common_action_occupation.hpp"
namespace hanmo::resident {
Status adopt_started_action_in_candidate(CommonStore& common,const ActionCoreView& action){
    if(action.id.empty()||action.kind.empty()||action.actor_id.empty())return common_failure("InvalidActionPreparation");
    if(common.action(action.id))return common_failure("DuplicateActionId",action.id);
    for(std::size_t i=0;i<action.participants.size();++i){
        const auto& id=action.participants[i];const auto* actor=common.actor(id);
        if(!actor)return common_failure("UnknownActor",id);
        if(!actor->action_id.empty())return common_failure("ActorBusy",id);
        for(std::size_t j=0;j<i;++j)if(action.participants[j]==id)return common_failure("DuplicateActionParticipant",id);
    }
    ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;
    auto status=common.apply_action(insert);if(!status.ok)return status;
    for(const auto& id:action.participants){ActorMutation occupation;occupation.actor_id=id;occupation.action_id=action.id;status=common.apply_actor(occupation);if(!status.ok)return status;}
    return common_success();
}
Status release_action_occupation_in_candidate(CommonStore& common,const StableId& id){
    const auto* action=common.action(id);if(!action)return common_failure("UnknownAction",id,ErrorKind::internal);
    ResourceRecipe recipe;recipe.operations.emplace_back(ReleaseActionResources{id});
    auto status=common.apply_resources(recipe);if(!status.ok)return status;
    for(const auto& person:action->participants){
        const auto* actor=common.actor(person);
        if(actor&&actor->action_id==id){ActorMutation change;change.actor_id=person;change.action_id=StableId{};status=common.apply_actor(change);if(!status.ok)return status;}
    }
    return common_success();
}
Status erase_released_action_in_candidate(CommonStore& common,const StableId& id){
    const auto* action=common.action(id);if(!action)return common_failure("UnknownAction",id,ErrorKind::internal);
    for(const auto& person:action->participants){const auto* actor=common.actor(person);if(actor&&actor->action_id==id)return common_failure("ActionOccupationReleaseRequired",id,ErrorKind::internal);}
    // CommonStore's original erase consumes the actual stored row and needs
    // only its identity; no second copy of all Action fields is constructed.
    ActionMutation erase;erase.kind=ActionMutation::Kind::erase;erase.action.id=id;
    return common.apply_action(erase);
}
Status pause_action_participation_in_candidate(CommonStore& common,const StableId& id){
    const auto* action=common.action(id);if(!action)return common_failure("UnknownAction",id,ErrorKind::internal);
    if(action->participants.empty())return common_failure("ActionParticipationPauseUnavailable",id);
    for(const auto& person:action->participants){const auto* actor=common.actor(person);if(!actor||actor->action_id!=id)return common_failure("ActionParticipationMismatch",person);}
    for(const auto& person:action->participants){ActorMutation change;change.actor_id=person;change.action_id=StableId{};auto status=common.apply_actor(change);if(!status.ok)return status;}
    ActionMutation mutation;mutation.action.id=id;mutation.participants.emplace();
    return common.apply_action(mutation);
}
Status resume_action_participation_in_candidate(CommonStore& common,const StableId& id,const std::vector<StableId>& original_participants){
    const auto* action=common.action(id);if(!action)return common_failure("UnknownAction",id,ErrorKind::internal);
    if(!action->participants.empty()||original_participants.empty())return common_failure("ActionParticipationResumeUnavailable",id);
    for(std::size_t i=0;i<original_participants.size();++i){const auto& person=original_participants[i];const auto* actor=common.actor(person);if(!actor)return common_failure("UnknownActor",person);if(!actor->action_id.empty())return common_failure("ActorBusy",person);for(std::size_t j=0;j<i;++j)if(original_participants[j]==person)return common_failure("DuplicateActionParticipant",person);}
    for(const auto& person:original_participants){ActorMutation change;change.actor_id=person;change.action_id=id;auto status=common.apply_actor(change);if(!status.ok)return status;}
    ActionMutation mutation;mutation.action.id=id;mutation.participants=original_participants;
    return common.apply_action(mutation);
}
}
