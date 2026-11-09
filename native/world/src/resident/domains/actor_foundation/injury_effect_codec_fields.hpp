// V4.2 immutable injury result fields. The enclosing physical selection decides
// whether Applied exists; old versions never infer it from populated fields.
#pragma once
#include "character_origin_codec_detail.hpp"
namespace hanmo::resident::injury_effect_codec {
template<class C>Status receipt_ref(C& c,ConditionReceiptRefV2& value){c.field("result_id",value.result_id);return value.result_id.empty()?c.reject("ConditionReceiptRefRequired"):c.status();}
template<class C>Status recovery_ref(C& c,RecoveryResolutionRefV2& value){c.field("result_id",value.result_id);return value.result_id.empty()?c.reject("RecoveryResolutionRefRequired"):c.status();}
template<class C>Status origin(C& c,ConditionEffectOriginRef& value){Integer tag=static_cast<Integer>(value.index());c.field("tag",tag);if(tag<0||tag>1)return c.reject("InvalidConditionEffectOrigin");if(c.reading())value=tag==0?ConditionEffectOriginRef{ConditionReceiptRefV2{}}:ConditionEffectOriginRef{InitialConditionSeedRef{}};if(tag==0)return receipt_ref(c,std::get<ConditionReceiptRefV2>(value));return character_origin_codec::seed(c,std::get<InitialConditionSeedRef>(value));}
template<class C>Status identity(C& c,ConditionEffectIdentity& value){c.field("actor_id",value.actor_id);c.field("episode_id",value.episode_id);c.object("origin_result",[&](auto& v){return origin(v,value.origin_result);});c.field("effect_key",value.effect_key);return value.actor_id.empty()||value.episode_id.empty()||value.effect_key.empty()?c.reject("ConditionEffectIdentityRequired"):c.status();}
template<class C>Status state(C& c,const char* name,ConditionEffectState& value){Integer tag=static_cast<Integer>(value);c.field(name,tag);if(tag<0||tag>2)return c.reject("InvalidConditionEffectState");if(c.reading())value=static_cast<ConditionEffectState>(tag);return c.status();}
template<class C>Status episode(C& c,ConditionEpisodeV2& value,bool actual_death=false){
 c.field("episode_id",value.episode_id);c.field("actor_id",value.actor_id);c.object("condition_rule_ref",[&](auto& v){return character_origin_codec::rule(v,value.condition_rule_ref);});
 c.object("origin",[&](auto& v){Integer tag=static_cast<Integer>(value.origin.index());v.field("tag",tag);if(tag<0||tag>1)return v.reject("InvalidConditionEpisodeOrigin");if(v.reading())value.origin=tag==0?CharacterConditionSourceRef{InitialConditionSeedRef{}}:CharacterConditionSourceRef{ConditionReceiptRefV2{}};if(tag==0)return character_origin_codec::seed(v,std::get<InitialConditionSeedRef>(value.origin));return receipt_ref(v,std::get<ConditionReceiptRefV2>(value.origin));});
 c.object("started_at",[&](auto& v){return character_origin_codec::at(v,value.started_at,false);});c.object("stage_ref",[&](auto& v){return character_origin_codec::rule(v,value.stage_ref);});c.field("stage_revision",value.stage_revision);
 c.optional("next_calendar_due",value.next_calendar_due,[](auto& v,Integer& x){v.field("value",x);return x<0?v.reject("InvalidConditionCalendarDue"):v.status();});
 // Terminal payloads are not admitted until their actual lifecycle adapter is
 // present; a non-null field is never silently dropped or treated as closed.
 c.optional("terminal_source_ref",value.terminal_source_ref,[&](auto& v,auto& x){
  if(!actual_death)return v.reject("ConditionEpisodeTerminalSourceRequired");
  Integer tag=static_cast<Integer>(x.index());v.field("tag",tag);
  if(tag!=1)return v.reject("ConditionEpisodeRecoveryLifecycleRequired");
  if(v.reading())x=DeathRef{};
  auto& death=std::get<DeathRef>(x);v.field("domain",death.domain);v.field("kind",death.kind);v.field("id",death.id);
  return death.domain!="family_life"||death.kind!="family_death"||death.id.empty()?v.reject("InvalidConditionEpisodeDeathRef"):v.status();
 });
 return value.episode_id.empty()||value.actor_id.empty()||value.stage_revision<0?c.reject("InvalidConditionEpisode"):c.status();
}
template<class C>Status contribution(C& c,ConditionEffectContribution& value){identity(c,value.identity);state(c,"current_state",value.current_state);return c.optional("last_resolution_ref",value.last_resolution_ref,[](auto& v,auto& x){return recovery_ref(v,x);});}
template<class C>Status recovery(C& c,RecoveryResolutionV2& value){
 c.field("result_id",value.result_id);c.field("actor_id",value.actor_id);
 c.object("actual_recovery_source",[&](auto& v){Integer tag=static_cast<Integer>(value.actual_recovery_source.index());v.field("tag",tag);if(tag!=0)return v.reject("OriginalRecoverySourceAdapterRequired");return v.object("condition_receipt_ref",[&](auto& x){return receipt_ref(x,std::get<RestProvidedRecoverySource>(value.actual_recovery_source).condition_receipt_ref);});});
 c.object("rule_ref",[&](auto& v){return character_origin_codec::rule(v,value.rule_ref);});c.object("effective_at",[&](auto& v){return character_origin_codec::at(v,value.effective_at,false);});
 c.sequence("target_effects",value.target_effects,[](auto& v,RecoveryEffectTarget& x){v.object("effect_identity",[&](auto& a){return identity(a,x.effect_identity);});v.object("gate_ref",[&](auto& a){return character_origin_codec::rule(a,x.gate_ref);});state(v,"state_before",x.state_before);state(v,"state_after",x.state_after);return v.optional("remaining_effect_amount",x.remaining_effect_amount,[](auto& a,Integer& n){a.field("value",n);return n<0?a.reject("InvalidRemainingInjuryEffectAmount"):a.status();});});
 c.field("condition_revision_before",value.condition_revision_before);c.field("condition_revision_after",value.condition_revision_after);
 return value.result_id.empty()||value.actor_id.empty()||value.condition_revision_before<0||value.condition_revision_after<=value.condition_revision_before?c.reject("InvalidRecoveryResolution"):c.status();
}
template <class C, class Rule>
Status effect_with_rule(C &c, InjuryEffectItem &value, Rule &&rule) {
  c.field("effect_key", value.effect_key);
  c.object("effect_payload", [&](auto &v) {
    Integer tag = static_cast<Integer>(value.effect_payload.index());
    v.field("tag", tag);
    if (tag < 0 || tag > 1)
      return v.reject("InvalidInjuryEffectPayload");
    if (v.reading())
      value.effect_payload =
          tag == 0
              ? decltype(value.effect_payload){InjuryAttributeReduction{}}
              : decltype(value.effect_payload){InjuryActivityRestriction{}};
    if (tag == 0) {
      auto &x = std::get<InjuryAttributeReduction>(value.effect_payload);
      v.field("attribute_id", x.attribute_id);
      v.object("reducer_ref", [&](auto &a) { return rule(a, x.reducer_ref); });
      v.field("unit_id", x.unit_id);
      v.field("amount", x.amount);
      if (x.attribute_id != "administration" && x.attribute_id != "charisma" &&
          x.attribute_id != "intellect" && x.attribute_id != "leadership" &&
          x.attribute_id != "martial")
        return v.reject("InvalidInjuryAttributeId");
      return x.amount < 0 ? v.reject("InvalidInjuryReductionAmount")
                          : v.status();
    }
    auto &x = std::get<InjuryActivityRestriction>(value.effect_payload);
    v.object("registered_purpose",
             [&](auto &a) { return rule(a, x.registered_purpose); });
    return v.object("restriction_ref",
                    [&](auto &a) { return rule(a, x.restriction_ref); });
  });
  c.object("retention", [&](auto &v) {
    Integer tag = static_cast<Integer>(value.retention.index());
    v.field("tag", tag);
    if (tag < 0 || tag > 1)
      return v.reject("InvalidInjuryRetention");
    if (v.reading())
      value.retention =
          tag == 0 ? decltype(value.retention){RecoverableInjuryEffect{}}
                   : decltype(value.retention){PermanentInjuryEffect{}};
    if (tag == 0)
      return v.object("original_recovery_gate_ref", [&](auto &a) {
        return rule(a, std::get<RecoverableInjuryEffect>(value.retention)
                           .original_recovery_gate_ref);
      });
    return v.status();
  });
  c.object("original_effect_rule_ref", [&](auto &v) {
    return rule(v, value.original_effect_rule_ref);
  });
  return value.effect_key.empty() ? c.reject("InjuryEffectKeyRequired")
                                  : c.status();
}
// 存档/运行原入口仍强制完整三字段引用；只冷作者显式入口可传锁绑定器。
template <class C> Status effect(C &c, InjuryEffectItem &value) {
  return effect_with_rule(c, value, [](auto &v, ActorFoundationRuleRef &ref) {
    return character_origin_codec::rule(v, ref);
  });
}
template<class C>Status selected(C& c,SelectedInjuryResult& result,bool full,bool applied){
 Integer tag=static_cast<Integer>(result.kind);c.field("tag",tag);
 if(tag<0||tag>2||(!full&&tag)||(tag==2&&!applied))return c.reject("UnsupportedConditionInjuryResult");
 if(c.reading())result.kind=static_cast<SelectedInjuryResult::Kind>(tag);
 if(tag==0){if(c.reading()){result.injury_rule_ref={};result.result_choice_ref={};}else if(!result.injury_rule_ref.id.empty()||result.injury_rule_ref.version||!result.injury_rule_ref.catalog_digest.empty()||!result.result_choice_ref.id.empty()||result.result_choice_ref.version||!result.result_choice_ref.catalog_digest.empty())return c.reject("InactiveConditionInjuryPayload");}
 else {c.object("injury_rule_ref",[&](auto& v){return character_origin_codec::rule(v,result.injury_rule_ref);});c.object("result_choice_ref",[&](auto& v){return character_origin_codec::rule(v,result.result_choice_ref);});}
 if(tag!=2){if(c.reading()){result.actual_decision_source_ref.reset();result.effect_items.clear();}else if(result.actual_decision_source_ref||!result.effect_items.empty())return c.reject("InactiveConditionInjuryEffects");return c.status();}
 // This first source adapter is deterministic. A later real decision adapter
 // must supply its own finite source fields before a non-null value is usable.
 c.optional("actual_decision_source_ref",result.actual_decision_source_ref,[](auto& v,auto&){return v.reject("InjuryActualDecisionSourceRequired");});
 if(!c.reading()&&result.effect_items.empty())return c.reject("AppliedInjuryEffectsRequired");
 if(!c.reading()&&result.effect_items.size()>64)return c.reject("InjuryEffectCountExceeded");
 c.sequence("effect_items",result.effect_items,[](auto& v,auto& x){return effect(v,x);});
 if(result.effect_items.empty())return c.reject("AppliedInjuryEffectsRequired");
 if(result.effect_items.size()>64)return c.reject("InjuryEffectCountExceeded");
 for(std::size_t i=0;i<result.effect_items.size();++i)for(std::size_t j=0;j<i;++j)if(result.effect_items[i].effect_key==result.effect_items[j].effect_key)return c.reject("DuplicateInjuryEffectKey");
 return c.status();
}
}
