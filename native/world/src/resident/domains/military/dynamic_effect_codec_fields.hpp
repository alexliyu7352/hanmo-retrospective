// S2 §2.4 fixed outer order; finite original result records share one field table.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_effect_model.hpp"
#include "dynamic_session_codec_fields.hpp"
#include "dynamic_action_codec_fields.hpp"
namespace hanmo::resident::dynamic_effect_codec {
template<class C>Status value(C&, DynamicLandActivationResult&);
template<class C>Status value(C&, DynamicTacticPointChange2&);
template<class C>Status value(C&, DynamicLandReformChange&);
template<class C>Status value(C&, DynamicSupportConsumption2&);
template<class C>Status value(C&, DynamicLandMemberAllocation&);
template<class C>Status value(C&, DynamicLandMoraleChange&);
template<class C>Status value(C&, DynamicBattleMoraleChange2&);
template<class C>Status value(C&, DynamicLandStatusChange&);
template<class C>Status value(C&, DynamicBattleVesselChange2&);
template<class C>Status value(C&, DynamicMilitaryEffectOnceKey2&);
template<class C>Status value(C&, DynamicBattleEffectGroup2&);
template<class C>Status value(C&, DynamicBattleStrikeResult2&);
template<class C>Status value(C&, SourceRef&);
template<class C>Status value(C&, WorldMoment&);
template<class C>Status value(C&, DynamicMilitaryRandomSlot&);
template<class C>Status value(C&, DynamicMilitaryPosture&);
template<class C>Status value(C&, DynamicMilitaryStatus&);
template<class C>Status value(C&, DynamicMilitaryStatusKind&);
template<class C>Status value(C&, DynamicMilitaryDamageLeaf&);
template<class C>Status value(C&, DynamicLandActivationFailure&);
template<class C>Status value(C&, DynamicLandStrikeFailure&);
template<class C>Status value(C&, DynamicMoraleLayer2&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C,class T>Status value(C&,std::optional<T>&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T>Status field(C& c,const char* name,std::vector<T>& v){return c.sequence(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status value(C& c,std::optional<T>& v){return field(c,"optional",v);}
template<std::size_t I=0,class C,class V>bool construct(C& c,V& v,Integer tag){if constexpr(I<std::variant_size_v<V>){if(tag==Integer(I)){if(!prepare_checkpoint_default_values<std::variant_alternative_t<I,V>>(c).ok)return true;v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}return false;}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){Integer tag=Integer(v.index());c.field("tag",tag);if(c.reading()&&c.status().ok&&!construct(c,v,tag))return c.reject("UnknownDynamicEffectVariant");if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& item){return value(x,item);},v);});return c.status();}
template<class C>Status value(C& c,SourceRef& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryRandomSlot& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryPosture& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryStatus& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryStatusKind& v){return dynamic_session_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryDamageLeaf& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,DynamicLandActivationFailure& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>4))return c.reject("UnknownDynamicEffectEnum");if(c.reading()&&c.status().ok)v=DynamicLandActivationFailure(tag);return c.status();}
template<class C>Status value(C& c,DynamicLandStrikeFailure& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>6))return c.reject("UnknownDynamicEffectEnum");if(c.reading()&&c.status().ok)v=DynamicLandStrikeFailure(tag);return c.status();}
template<class C>Status value(C& c,DynamicMoraleLayer2& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>2))return c.reject("UnknownDynamicEffectEnum");if(c.reading()&&c.status().ok)v=DynamicMoraleLayer2(tag);return c.status();}
template<class C>Status value(C& c,DynamicLandActivationResult& v){field(c,"effect_ref",v.effect_ref);field(c,"action_ref",v.action_ref);field(c,"unit_ref",v.unit_ref);field(c,"action_instance_ordinal",v.action_instance_ordinal);field(c,"effect_ordinal",v.effect_ordinal);field(c,"original_action_kind",v.original_action_kind);field(c,"original_tactic_ref",v.original_tactic_ref);field(c,"failure",v.failure);field(c,"original_point_cost",v.original_point_cost);field(c,"actual_target_unit_refs",v.actual_target_unit_refs);field(c,"installed_posture",v.installed_posture);field(c,"original_qualification_sources",v.original_qualification_sources);return c.status();}
template<class C>Status value(C& c,DynamicTacticPointChange2& v){field(c,"unit_ref",v.unit_ref);field(c,"before_points",v.before_points);field(c,"delta",v.delta);field(c,"cap",v.cap);field(c,"after_points",v.after_points);field(c,"actual_cost_source_ref",v.actual_cost_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicLandReformChange& v){field(c,"unit_ref",v.unit_ref);field(c,"actual_confused_source",v.actual_confused_source);field(c,"actual_activation_ref",v.actual_activation_ref);return c.status();}
template<class C>Status value(C& c,DynamicSupportConsumption2& v){field(c,"supporter_unit_ref",v.supporter_unit_ref);field(c,"target_unit_ref",v.target_unit_ref);field(c,"activation_ref",v.activation_ref);field(c,"consumed_by_effect_ref",v.consumed_by_effect_ref);field(c,"actual_shared_members",v.actual_shared_members);return c.status();}
template<class C>Status value(C& c,DynamicLandMemberAllocation& v){field(c,"member_id",v.member_id);field(c,"unit_ref",v.unit_ref);field(c,"effect_ref",v.effect_ref);field(c,"direct_target_unit_ref",v.direct_target_unit_ref);field(c,"support_share",v.support_share);field(c,"resulting_status",v.resulting_status);return c.status();}
template<class C>Status value(C& c,DynamicLandMoraleChange& v){field(c,"unit_ref",v.unit_ref);field(c,"before_morale",v.before_morale);field(c,"delta",v.delta);field(c,"after_morale",v.after_morale);return c.status();}
template<class C>Status value(C& c,DynamicBattleMoraleChange2& v){field(c,"layer",v.layer);field(c,"original_change",v.original_change);return c.status();}
template<class C>Status value(C& c,DynamicLandStatusChange& v){field(c,"unit_ref",v.unit_ref);field(c,"kind",v.kind);field(c,"before",v.before);field(c,"after",v.after);field(c,"actual_activation_ref",v.actual_activation_ref);return c.status();}
template<class C>Status value(C& c,DynamicBattleVesselChange2& v){field(c,"vessel_ref",v.vessel_ref);field(c,"before_integrity",v.before_integrity);field(c,"actual_damage",v.actual_damage);field(c,"after_integrity",v.after_integrity);field(c,"original_effect_refs",v.original_effect_refs);field(c,"actual_result_refs",v.actual_result_refs);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryEffectOnceKey2& v){field(c,"session_ref",v.session_ref);field(c,"action_ref",v.action_ref);field(c,"effect_ordinal",v.effect_ordinal);if(c.status().ok&&(v.effect_ordinal<0||v.effect_ordinal>1))return c.reject("DynamicMilitaryEffectOrdinalInvalid");return c.status();}
template<class C>Status value(C& c,DynamicBattleEffectGroup2& v){field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"effective_at",v.effective_at);field(c,"solver_ref",v.solver_ref);field(c,"due_effect_refs",v.due_effect_refs);field(c,"precondition_source_refs",v.precondition_source_refs);field(c,"geometry_basis_refs",v.geometry_basis_refs);field(c,"posture_effects",v.posture_effects);field(c,"strike_results",v.strike_results);field(c,"rng_slots",v.rng_slots);field(c,"member_allocations",v.member_allocations);field(c,"morale_changes",v.morale_changes);field(c,"status_changes",v.status_changes);field(c,"vessel_changes",v.vessel_changes);field(c,"condition_source_refs",v.condition_source_refs);field(c,"population_source_refs",v.population_source_refs);field(c,"objective_result_ref",v.objective_result_ref);field(c,"terminal_refs",v.terminal_refs);field(c,"once_keys",v.once_keys);return c.status();}
template<class C>Status value(C& c,DynamicBattleStrikeResult2& v){auto& r=v.original_result;field(c,"effect_ref",r.effect_ref);field(c,"action_ref",r.action_ref);field(c,"attacker_unit_ref",r.attacker_unit_ref);field(c,"target_unit_ref",r.target_unit_ref);field(c,"action_instance_ordinal",r.action_instance_ordinal);field(c,"effect_ordinal",r.effect_ordinal);field(c,"original_damage_leaf",r.original_damage_leaf);field(c,"original_damage_version",r.original_damage_version);field(c,"failure",r.failure);field(c,"raw",r.raw);field(c,"potential",r.potential);field(c,"chance",r.chance);field(c,"flank_morale",r.flank_morale);field(c,"tactic_morale",r.tactic_morale);field(c,"actual_target_count",v.actual_target_count);field(c,"actual_support_count",v.actual_support_count);field(c,"consumed_support_activation_ref",v.consumed_support_activation_ref);field(c,"supporter_unit_ref",v.supporter_unit_ref);field(c,"target_member_refs",v.target_member_refs);field(c,"support_member_refs",v.support_member_refs);return c.status();}
}
