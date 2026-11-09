#include "visit_reply_native.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,{},kind).error};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
void bound(const WorldBundle&w,const CharacterConditionSources&body,const FamilyContinuationPeopleRead4&people,const ProfileControlEligibilitySources&control){
 if(!w.catalog||!w.catalog->common||!w.catalog->interaction||!w.catalog->interaction->visit_messages||!w.interaction.visit_messages||!w.catalog->visit_message_replies||!w.interaction.visit_message_replies||!visit_reply_life_version(w.catalog->visit_message_replies->version)||w.catalog->visit_message_replies->version!=w.interaction.visit_message_replies->version||!w.catalog->family||!w.catalog->family_life||!w.catalog->family_life->enabled||w.catalog->family_life->selected_version!=4||!w.catalog->actor_foundation||!w.actor_foundation)fail("VisitReplyLifeOriginalRootsRequired",ErrorKind::unsupported_capability);
 if(w.interaction.social_occupancy&&!w.catalog->actor_social_occupancy_initial)fail("VisitReplyOccupancyOriginalSeedRequired",ErrorKind::unsupported_capability);
 const bool authored=visit_reply_authored_version(w.catalog->visit_message_replies->version);if(authored!=bool(w.catalog->visit_reply_intents)||(authored&&(!w.catalog->director||!w.catalog->director->visit_reception||!w.interaction.visit_reception_claims)))fail("VisitReplyOriginalAuthorshipRequired",ErrorKind::unsupported_capability);
 if(&body.common!=&w.common||&body.state!=&*w.actor_foundation||&body.catalog!=w.catalog->actor_foundation.get()||body.deaths!=&w.family_life||body.family_catalog!=w.catalog->family.get()||body.shared!=w.catalog->common.get()||!people.bound_to(w.common,w.family_life)||&control.common!=&w.common||&control.family!=&w.family||&control.catalog!=w.catalog->family.get()||&control.shared!=w.catalog->common.get()||control.foundation!=&*w.actor_foundation||control.foundation_catalog!=w.catalog->actor_foundation.get()||people.control_continuation()!=control.control_continuation)fail("VisitReplyFamilyCandidateMismatch");
 if(!body.life_sources||!control.actual_clock)fail("VisitReplyActualFamilySourcesRequired",ErrorKind::unsupported_capability);auto now=take(w.actual_phase_moment());if(take(compare_world_moments(body.actual_now,now))||take(compare_world_moments(take(people.read_current_moment()),now))||take(compare_world_moments(take(control.actual_clock->read_current_moment()),now)))fail("VisitReplyActualFamilyClockMismatch");
}
}
NativeVisitReplyLifeRead::NativeVisitReplyLifeRead(
    const WorldBundle &w, const CharacterConditionWorldRead &owner,
    const CharacterConditionSources &body,
    const FamilyContinuationPeopleRead4 &people,
    const ProfileControlEligibilitySources &control)
    : world_(w), body_owner_(owner), people_(people), controls_(control),
      clock_(w),
      commands_(
          w.catalog->visit_message_replies->version == 4 &&
                  w.catalog->director && w.catalog->director->subjects2 &&
                  w.director.subject2 &&
                  w.director.subject2->command_binding_version == 1
              ? std::optional<VisitAuthoredCommandRead>{{w.common, w.director,
                                                         *w.catalog->director,
                                                         w.catalog->identity,
                                                         clock_}}
              : std::nullopt),
      legacy_deaths_(w.family_death_read()),
      occupancy_initial_(
          w.interaction.social_occupancy
              ? std::optional<
                    NativeActorSocialOccupancyInitialRead>{std::in_place,
                                                           ActorSocialOccupancyInitialContext{
                                                               w.common,
                                                               *w.catalog
                                                                    ->common,
                                                               *w.actor_foundation,
                                                               *w.catalog
                                                                    ->actor_foundation,
                                                               w.catalog->family
                                                                   .get(),
                                                               *w.catalog
                                                                    ->actor_social_occupancy_initial,
                                                               clock_,
                                                               body.born_sources}}
              : std::nullopt),
      occupancy_(
          occupancy_initial_
              ? std::optional<
                    NativeActorSocialOccupancyRead>{std::in_place, w.common,
                                                    w.interaction,
                                                    *occupancy_initial_, clock_,
                                                    w.catalog->interaction
                                                        .get()}
              : std::nullopt),
      messages_{w.common,
                *w.interaction.visit_messages,
                *w.catalog->interaction->visit_messages,
                *w.catalog->interaction,
                w.interaction,
                *w.catalog->family_life,
                w.family_life,
                *body.life_sources,
                clock_},
      authorship_(
          w.catalog->visit_reply_intents
              ? std::optional<
                    VisitReplyAuthorshipRead>{{*w.catalog->visit_reply_intents,
                                               *w.catalog->director
                                                    ->visit_reception,
                                               *w.interaction
                                                    .visit_reception_claims,
                                               w.interaction,
                                               [this](Integer tick,
                                                      const StableId &id)
                                                   -> Outcome<StableId> {
                                                 const auto *h =
                                                     world_.common.state()
                                                         .facts.find(id);
                                                 if (!h || h->id != id ||
                                                     tick < 0)
                                                   return {
                                                       false,
                                                       {},
                                                       common_failure(
                                                           "VisitReplyControlOr"
                                                           "iginalCutRequired",
                                                           id,
                                                           ErrorKind::internal)
                                                           .error};
                                                 auto now =
                                                     clock_
                                                         .read_current_moment();
                                                 if (!now.ok)
                                                   return {
                                                       false, {}, now.error};
                                                 if (tick >
                                                     now.value.half_day_tick)
                                                   return {
                                                       false,
                                                       {},
                                                       common_failure(
                                                           "VisitReplyControlCu"
                                                           "tFromFuture",
                                                           id,
                                                           ErrorKind::internal)
                                                           .error};
                                                 auto player =
                                                     read_profile_control_before_fact(
                                                         controls_,
                                                         {h->domain, h->kind,
                                                          id});
                                                 return player.ok
                                                            ? Outcome<
                                                                  StableId>{true,
                                                                            player
                                                                                .value
                                                                                .actor_id,
                                                                            {}}
                                                            : Outcome<StableId>{
                                                                  false,
                                                                  {},
                                                                  player.error};
                                               }}}
              : std::nullopt),
      life_{messages_,
            *w.catalog->common,
            *w.catalog->visit_message_replies,
            *w.interaction.visit_message_replies,
            people_,
            legacy_deaths_,
            authorship_ ? &*authorship_ : nullptr,
            occupancy_ ? &*occupancy_ : nullptr},
      context_{w.common,
               *w.catalog->common,
               *w.interaction.visit_messages,
               *w.catalog->interaction->visit_messages,
               *w.catalog->interaction,
               *w.catalog->visit_message_replies,
               *w.interaction.visit_message_replies,
               legacy_deaths_,
               authorship_ ? &*authorship_ : nullptr,
               &life_,
               commands_ ? &*commands_ : nullptr} {
  if (authorship_)
    authorship_->life = &life_;
}
Outcome<std::unique_ptr<NativeVisitReplyLifeRead>>NativeVisitReplyLifeRead::create(const WorldBundle&w,const CharacterConditionWorldRead&owner,const FamilyContinuationPeopleRead4&people,const ProfileControlEligibilitySources&control){return guard<std::unique_ptr<NativeVisitReplyLifeRead>>([&]{auto body=take(owner.sources());bound(w,body,people,control);return std::unique_ptr<NativeVisitReplyLifeRead>(new NativeVisitReplyLifeRead(w,owner,body,people,control));});}
Outcome<const VisitReplyReadContext*>NativeVisitReplyLifeRead::context()const{return guard<const VisitReplyReadContext*>([&]{auto body=take(body_owner_.sources());bound(world_,body,people_,controls_);if(body.life_sources!=&messages_.causes||&life_.state!=&*world_.interaction.visit_message_replies||&messages_.state!=&*world_.interaction.visit_messages)fail("VisitReplyOwningLeaseChanged");return &context_;});}
}
