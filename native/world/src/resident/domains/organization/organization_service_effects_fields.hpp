// Pure original member sequence, shared by codecs and the sole K9 owned graph.
// No validation, allocation, traversal engine or source-authority inference.
#pragma once
#include "organization_service_effects_model.hpp"
namespace hanmo::resident::organization_service_effects_fields {
template<class T,class F>void role_change(T& value,F&& field){
    field("before",value.before);field("after",value.after);
}
template<class T,class F>void effect(T& value,F&& field){
    field("duty_id",value.duty_id);field("role_id",value.role_id);field("actor_id",value.actor_id);
    field("duty_active_before",value.duty_active_before);field("duty_active_after",value.duty_active_after);
    field("role_enabled_change",value.role_enabled_change);
}
template<class T,class F>void payload(T& value,F&& field){field("changes",value.changes);}
} // namespace hanmo::resident::organization_service_effects_fields
