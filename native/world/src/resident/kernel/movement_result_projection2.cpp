#include "movement_result_projection2.hpp"
#include "exact_result_capture_reuse.hpp"
#include "native_domain_read_versions.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/domains/travel/local_move_source.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> bad(const char*c,const StableId&id){return {false,{},common_failure(c,id,ErrorKind::internal).error};}
bool is(const StableId&v,std::initializer_list<const char*> names){for(auto*n:names)if(v==n)return true;return false;}
const MovementPrefixState* prefix(const MovementResultProjectionAccess&a,const StableId&id){return a.movement.prefixes.find(id);}
StableId rule(const MovementPrefixState&p){return std::visit([](const auto&v){return v.catalog_rule_ref;},p.basis);}
StableId execution_id(const MovementExecutionRef&e){return std::visit([](const auto&v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PersonalMovementExecution>||std::is_same_v<T,GroupMovementExecution>)return v.journey_id;else if constexpr(std::is_same_v<T,CustodyExitMovementExecution>)return v.escape_action_id;else return v.landing_operation_id;},e);}
Outcome<ExactOwnerFactRead> no_projection(bool found,const StableId&id){if(!found)return bad<ExactOwnerFactRead>("MovementOriginalPayloadMissing",id);ExactOwnerFactRead result;result.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;return {true,std::move(result),{}};}
}
bool native_movement_result_kind(const FactRecord&h){return h.domain=="travel"&&is(h.kind,{"local_move","movement_prefix","movement_unit","movement_arrival","movement_contact","movement_expense","travel_exertion","travel_exertion_basis","movement_unit_close","terrain_credit_disposition","group_journey","group_movement_opening","group_journey_step","group_movement_control","transport_registration","transport_release"});}
Outcome<WorldMoment> read_native_movement_source_moment(const MovementResultProjectionAccess&a,const MomentOwnerSource&s){const auto&ref=s.source_ref;if(ref.kind=="local_move"){auto row=read_local_move_source(a.common,a.movement,ref,s.source_schema);if(!row.ok)return {false,{},row.error};return {true,row.value->occurred_at,{}};}if(is(ref.kind,{"group_movement_opening","group_journey_step","group_movement_control","transport_registration","transport_release"})){if(!a.carriers||!a.groups||!a.registrations)return bad<WorldMoment>("GroupMovementResultReadRequired",ref.id);return read_group_source_moment(a.common,*a.carriers,a.movement,*a.groups,*a.registrations,ref,s.source_schema);}return read_movement_source_moment(a.movement,ref,s.source_schema);}
Outcome<ExactOwnerFactRead> read_movement_fact_v2(const MovementResultProjectionAccess&a,const FactRecord&h){
 if(!native_movement_result_kind(h))return {false,{},common_failure("UnsupportedMovementFactKind",h.id,ErrorKind::unsupported_capability).error};const auto*header=a.common.state().facts.find(h.id);if(!header||header->domain!=h.domain||header->kind!=h.kind)return bad<ExactOwnerFactRead>("MovementFactHeaderMismatch",h.id);
 if(h.kind=="movement_prefix")return no_projection(a.movement.prefixes.find(h.id),h.id);if(h.kind=="movement_unit")return no_projection(a.movement.units.find(h.id),h.id);if(h.kind=="group_journey")return no_projection(a.carriers&&a.carriers->journeys.find(h.id),h.id);
 auto at=read_native_movement_source_moment(a,{{h.domain,h.kind,h.id},1});if(!at.ok)return {false,{},at.error};
 // These scope/control sources have no original public DynamicFact payload. Their true exact source remains addressable.
 if(is(h.kind,{"local_move","group_movement_opening","group_journey_step","group_movement_control","transport_registration","transport_release","travel_exertion_basis"})){auto out=no_projection(true,h.id);out.value.supporting_sources.push_back({{h.domain,h.kind,h.id},1});return out;}
 DynamicFactViewV2 f;f.id=h.id;f.kind=h.kind;f.occurred_at=at.value;f.time_basis=OwnerFactMoment{{h.domain,h.kind,h.id},1};const MovementPrefixState*p=nullptr;
 if(h.kind=="movement_arrival"){const auto*v=a.movement.arrivals.find(h.id);if(!v)return bad<ExactOwnerFactRead>("MovementArrivalSourceRequired",h.id);p=prefix(a,v->prefix_id);f.subject_id=execution_id(v->execution);f.outcome="arrived";f.cause_fact_ids={v->prefix_id};}
 else if(h.kind=="movement_contact"){const auto*v=a.movement.contacts.find(h.id);if(!v)return bad<ExactOwnerFactRead>("MovementContactSourceRequired",h.id);p=prefix(a,v->first_prefix_id);f.subject_id=v->id;f.outcome="contact";f.cause_fact_ids={v->first_prefix_id,v->second_prefix_id};}
 else if(h.kind=="movement_expense"){const auto*v=a.movement.expenses.find(h.id);if(!v)return bad<ExactOwnerFactRead>("MovementExpenseSourceRequired",h.id);p=prefix(a,v->prefix_id);f.subject_id=execution_id(v->execution);f.outcome="paid";f.rule_version=v->original_cost_rule_ref;f.cause_fact_ids={v->prefix_id};}
 else if(h.kind=="travel_exertion"){const auto*v=a.movement.exertions.find(h.id);if(!v)return bad<ExactOwnerFactRead>("TravelExertionSourceRequired",h.id);p=prefix(a,v->prefix_id);f.subject_id=execution_id(v->execution);f.outcome="provided";for(const auto&actor:v->actual_participants)f.participants.push_back({{SubjectKind::actor,actor},"participant"});f.cause_fact_ids={v->prefix_id};}
 else if(h.kind=="terrain_credit_disposition"){const auto*v=a.movement.credit_dispositions.find(h.id);if(!v)return bad<ExactOwnerFactRead>("TerrainCreditSourceRequired",h.id);p=prefix(a,v->prefix_id);f.subject_id=execution_id(v->execution);f.outcome="disposed";f.cause_fact_ids={v->prefix_id};}
 else if(h.kind=="movement_unit_close"){const auto*v=a.movement.unit_closes.find(h.id);if(!v)return bad<ExactOwnerFactRead>("MovementCloseSourceRequired",h.id);p=prefix(a,v->prefix_id);f.subject_id=execution_id(v->execution);switch(v->kind){case MovementCloseKind::carry:f.outcome="carry";break;case MovementCloseKind::arrival:f.outcome="arrival";break;case MovementCloseKind::contact:f.outcome="contact";break;case MovementCloseKind::cancelled:f.outcome="cancelled";break;}f.cause_fact_ids={v->prefix_id};}
 if(!p)return bad<ExactOwnerFactRead>("MovementFactBasisMissing",h.id);if(f.rule_version.empty())f.rule_version=rule(*p);if(f.rule_version.empty())return bad<ExactOwnerFactRead>("MovementFactRuleMissing",h.id);for(const auto&id:f.cause_fact_ids){const auto*cause=a.common.state().facts.find(id);if(!cause||cause->domain!="travel"||cause->kind!="movement_prefix")return bad<ExactOwnerFactRead>("MovementFactCauseMissing",id);}
 ExactOwnerFactRead out;out.fact=std::move(f);return {true,std::move(out),{}};
}
}


#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_world_scope.hpp"
#include "native_movement_boundary_catalog.hpp"
#include "native_movement_boundary_continuation_catalog.hpp"
#include "native_transport_catalog.hpp"
#include "native_group_member_handoffs.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_phase.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_boundary_continuation_source.hpp"
#include "resident/domains/travel/transport_manifest_history.hpp"
#include "resident/domains/politics/treaty/native_field_group_passage.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_winner_continuation.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"

namespace hanmo::resident {
// 这里只列当前Field/S1已有producer的有限补充集；原通用movement分派不变。
bool native_field_movement_result_kind(const StableId& domain,
                                       const StableId& kind) noexcept {
  return (domain == "politics" && kind == "political_passage_decided") ||
      (domain == "travel" && is(kind, {
      "group_military_supply_rebind", "group_transport_expense", "movement_unit_boundary",
      "movement_basis_transition", "movement_contact_installation",
      "movement_contact_continuation", "movement_boundary_contact",
      "movement_boundary_continuation", "group_contact_member_release",
      "group_member_loss_handoff", "boundary_crossing",
      "transport_entry_manifest"}));
}
namespace {
// 原通行决定拥有 /2 正文；其余有限 S1 来源仍为 /1，两个 P10 入口共用此判定。
Integer field_movement_source_schema(const StableId& domain,
                                     const StableId& kind) noexcept {
  return domain == "politics" && kind == "political_passage_decided" ? 2 : 1;
}
// 只借原完整owner；每次实际读取栈才构造业务租约，构造期不getter/不全验。
class NativeFieldMovementResultRead final : public ExactResultOwnerRead,
                                           public CheckpointAllocatedObject {
  const WorldBundle& world_;
  const CompleteExactWorldBindingOwner& complete_;
  const NativeCompleteWorldCoreSources& core_;

  struct CaptureRead final : ExactResultCaptureScratch {
    // Destruction order: Field's borrowed group context dies before Motion.
    std::shared_ptr<const MotionExactReadLease> motion;
    std::unique_ptr<NativeFieldWorldScope> field;
    std::shared_ptr<const ReadStamp> stamp;
    NativeDomainReadVersionSource versions;
    const WorldBundle *world;
    const CompleteExactWorldBindingOwner *complete;
    const NativeCompleteWorldCoreSources *core;
    explicit CaptureRead(const WorldBundle& w,
        const CompleteExactWorldBindingOwner& c,
        const NativeCompleteWorldCoreSources& source)
        :versions(w),world(&w),complete(&c),core(&source){}
  };
  bool same_roots(const MotionExactReadBinding& w) const noexcept {
    return &w.common==&world_.common && world_.movement_exact &&
        &w.movement==&*world_.movement_exact && world_.spatial_carriers &&
        &w.carriers==&*world_.spatial_carriers && world_.group_movement_executions &&
        &w.group_executions==&*world_.group_movement_executions &&
        &w.travel_catalog==world_.catalog->travel.get() && w.group_context;
  }
  std::unique_ptr<CaptureRead> original_read(const WorldMoment& actual) const {
    using namespace movement_detail;
    auto motion=take(complete_.read_motion(actual));
    if(!motion)fail("FieldMovementResultMotionOwnerRequired");
    const auto w=motion->bindings();
    if(!same_roots(w))fail("FieldMovementResultSameWorldRequired");
    auto field=take(NativeFieldWorldScope::create_from_original_core(world_,core_));
    take(field->bind_actual_group_context(*w.group_context));
    take(validate_native_movement_boundary_layout(world_));
    take(validate_native_movement_boundary_continuation_layout(world_));
    take(validate_native_group_member_handoff_layout(world_));
    take(validate_motion_exact_sources(w,actual));
    auto held=std::unique_ptr<CaptureRead>(new CaptureRead(world_,complete_,core_));
    held->motion=std::move(motion);held->field=std::move(field);
    return held;
  }
  bool reusable(const CaptureRead& held,const WorldMoment& actual,
      const ExactResultCaptureReuse& frame) const {
    if(held.world!=&world_ || held.complete!=&complete_ || held.core!=&core_ ||
       frame.common!=&world_.common || !held.stamp || !held.motion || !held.field ||
       !world_.has_active_read_phase())return false;
    // Check the authority BEFORE touching any retained borrowed bindings:
    // a write/rollback may have replaced their owning optional/vector roots.
    // Empty resource/domain lists do not omit the candidate slice/savepoint
    // and identity checks in ReadStampAuthority::validate.
    if(!world_.read_stamps->validate(*held.stamp,world_.common.state().identity,
        ReadMode::phase_current,world_.common,held.versions).ok)return false;
    const auto moment=compare_world_moments(frame.at,actual);
    if(!moment.ok || moment.value)return false;
    return held.field->bound_to(world_) && same_roots(held.motion->bindings());
  }

  // 时点只取已校验的原不可变行；不以Common tick或当前M冒历史发生时刻。
  WorldMoment original_moment(const MomentOwnerSource& source,
      const MotionExactReadBinding& w,const NativeFieldWorldScope& field) const {
    using namespace movement_detail;
    const auto& ref=source.source_ref;
    if (source.source_schema!=field_movement_source_schema(ref.domain,ref.kind) ||
        !native_field_movement_result_kind(ref.domain,ref.kind))
      fail("UnsupportedFieldMovementResultSource",ref.id);
    take(read_actual_common_fact_position(world_.common,ref));
    const auto* x=field.exact_contacts();
    const auto* relations=field.diplomacy();
    const auto* members=field.member_releases();
    if (!x || !relations || !members || !w.group_context ||
        !members->bound_to(w.common,w.movement,w.group_executions))
      fail("FieldMovementResultOriginalOwnerRequired",ref.id);
    const auto& catalog=*world_.catalog->field_battle;
    if(ref.domain=="politics" && ref.kind=="political_passage_decided") {
      const auto* passage=dynamic_cast<const NativeFieldGroupPassage*>(
          &w.group_context->passage);
      if(!world_.catalog->field_group_passage || !world_.political_passage || !passage)
        fail("FieldMovementPoliticalPassageOwnerRequired",ref.id);
      const auto* decision=take(read_political_passage_decision(
          w.common,*world_.political_passage,ref));
      if(decision->decision.source_schema_version!=2)
        fail("FieldMovementPoliticalDecisionSchemaRequired",ref.id);
      // 原具体 owner 拒绝别 World/副本行，并核原军籍、目录许可与实际时钟。
      take(passage->validate_historical_decision(*decision));
      const BoundaryCrossingReceipt* crossing=nullptr;
      for(const auto& row:w.carriers.crossings.rows()) {
        if(!same_source(row.second.decision_source_ref,ref))continue;
        if(crossing || row.first!=row.second.receipt_id)
          fail("FieldMovementPoliticalCrossingNotUnique",ref.id);
        crossing=&row.second;
      }
      if(!crossing)fail("FieldMovementPoliticalCrossingRequired",ref.id);
      // 借原 stored/full 门核 capture→decision→crossing 真 cut、历史清单、
      // 实际正片和选型外效果；不是仅凭头或当前 tick 为决定背书。
      take(passage->validate_crossing_decision(*crossing));
      return decision->occurred_at;
    }
    if(ref.kind=="group_military_supply_rebind") {
      const auto* row=take(read_group_military_supply_rebind_source(w.common,w.carriers,w.movement,w.group_executions,ref,1));
      take(w.group_context->actual_sources.validate_military_supply_rebind(*row));
      return row->occurred_at;
    }
    if(ref.kind=="group_transport_expense")
      return take(read_verified_group_expense_basis(w.movement,w.carriers,
          w.group_executions,w.travel_catalog,ref,1)).source->occurred_at;
    if(ref.kind=="movement_unit_boundary" || ref.kind=="movement_basis_transition")
      return take(read_movement_source_moment(w.movement,ref,1));
    if(ref.kind=="movement_contact_installation") {
      const auto* row=w.movement.contact_installations.find(ref.id);
      const auto* contact=row?w.movement.contacts.find(row->contact_ref):nullptr;
      if(!row || row->id!=ref.id || !contact ||
          !same_moment(row->occurred_at,contact->occurred_at))
        fail("FieldMovementInstallationSourceRequired",ref.id);
      take(w.contacts.validate_actual_contact_installation(*contact,
          {row->actual_business_source,row->business_source_schema}));
      if(take(read_actual_common_fact_position(w.common,row->actual_business_source))>=
         take(read_actual_common_fact_position(w.common,ref)))
        fail("FieldMovementInstallationSourceOrderRequired",ref.id);
      return row->occurred_at;
    }
    if(ref.kind=="movement_boundary_contact")
      return take(read_movement_boundary_contact_source(*x,catalog,ref)).contact->occurred_at;
    if(ref.kind=="movement_boundary_continuation")
      return take(read_movement_boundary_continuation_source(*x,*relations,catalog,ref))
          .continuation->occurred_at;
    if(ref.kind=="movement_contact_continuation") {
      const auto* row=w.movement.contact_continuations.find(ref.id);
      const auto* prefix=row?w.movement.prefixes.find(row->prefix_id):nullptr;
      const auto* group=prefix?std::get_if<GroupMovementExecution>(&prefix->execution):nullptr;
      const auto* army=group?std::get_if<ArmyCarrierRef>(&group->carrier):nullptr;
      if(!row || row->id!=ref.id || !army || row->actual_resolution_source.domain!="military" ||
          row->actual_resolution_source.kind!="field_battle_segment")
        fail("FieldWinnerContinuationSourceRequired",ref.id);
      const auto terminal=take(read_field_segment_settlement_source(
          x->common,x->military,x->field,row->actual_resolution_source,&x->clock));
      if(!terminal.session || !terminal.terminal)
        fail("FieldWinnerContinuationTerminalRequired",ref.id);
      const auto original=take(read_field_winner_continuation(*x,catalog,
          terminal.session->battle_id,army->army_id,&ref));
      if(original.continuation!=row)
        fail("FieldWinnerContinuationSourceMismatch",ref.id);
      return row->occurred_at;
    }
    if(ref.kind=="group_contact_member_release") {
      const auto* row=w.group_executions.contact_member_releases.find(ref.id);
      if(!row || row->id!=ref.id)
        fail("GroupMemberReleaseOriginalSourceRequired",ref.id);
      take(members->validate_actual_group_member_release(*row));
      return row->occurred_at;
    }
    if(ref.kind=="group_member_loss_handoff") {
      const auto* row=w.group_executions.member_loss_handoffs.find(ref.id);
      if(!row || row->id!=ref.id)
        fail("GroupMemberLossOriginalSourceRequired",ref.id);
      take(members->validate_actual_group_member_loss(*row));
      return row->occurred_at;
    }
    if(ref.kind=="boundary_crossing") {
      const auto* row=w.carriers.crossings.find(ref.id);
      if(!row || row->receipt_id!=ref.id)
        fail("GroupCrossingSourceRequired",ref.id);
      take(validate_group_boundary_crossing_source(w.common,w.group_executions,w.carriers,w.movement,w.travel_catalog,*row));
      take(w.group_context->passage.validate_crossing_decision(*row));
      return row->entry_at;
    }
    if(ref.kind=="transport_entry_manifest") {
      const auto* row=w.group_executions.entry_manifests.find(ref.id);
      if(!row || row->id!=ref.id)
        fail("TransportEntryManifestSourceRequired",ref.id);
      const auto* original=take(read_historical_transport_entry_manifest(w.common,
          w.carriers,w.movement,w.group_executions,w.group_context->manifest_history,
          row->execution,row->entry_ordinal,row->boundary,row->manifest.manifest_revision,
          row->manifest.composition_revision));
      if(original!=row)fail("TransportEntryManifestSourceMismatch",ref.id);
      return row->occurred_at;
    }
    fail("UnsupportedFieldMovementResultSource",ref.id);
  }
public:
  NativeFieldMovementResultRead(const WorldBundle& world,
      const CompleteExactWorldBindingOwner& complete,
      const NativeCompleteWorldCoreSources& core)
      :world_(world),complete_(complete),core_(core){}
  Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource& source) const override {
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "P10.field_movement.moment", &world_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
    return movement_detail::guard<WorldMoment>([&]{
      using namespace movement_detail;
      if(source.source_schema!=field_movement_source_schema(
          source.source_ref.domain,source.source_ref.kind) ||
          !native_field_movement_result_kind(source.source_ref.domain,source.source_ref.kind))
        fail("UnsupportedFieldMovementResultSource",source.source_ref.id);
      WorldCurrentMomentRead clock(world_);
      const auto actual=take(clock.read_current_moment());
      // Only this exact lexical P10 capture may retain a fully validated
      // owning read. Direct callers and nested/different captures stay isolated.
      auto* frame=ExactResultCaptureReuse::active;
      if(frame && frame->common!=&world_.common)frame=nullptr;
      CaptureRead* held=nullptr;
      if(frame && frame->client==this && frame->scratch) {
        auto* candidate=static_cast<CaptureRead*>(frame->scratch.get());
        if(reusable(*candidate,actual,*frame))held=candidate;
        else { frame->scratch.reset();frame->client=nullptr; }
      }
      std::unique_ptr<CaptureRead> local;
      if(!held){local=original_read(actual);held=local.get();}
      // Target-specific original source/occurrence checks always run in their
      // original order. Only identical full Motion validation is reused.
      const auto at=original_moment(source,held->motion->bindings(),*held->field);
#ifdef HANMO_TEST_EXACT_CAPTURE_REUSE_ASSERT
      if(!local) {
        // Separate checking build: reconstruct the old owner/graph from scratch
        // and run the original full validation and target read on EVERY hit.
        auto original=original_read(actual);
        const auto checked=original_moment(source,original->motion->bindings(),
                                            *original->field);
        if(take(compare_world_moments(at,checked))!=0)
          fail("P10ReadScopeReuseReplayMismatch",source.source_ref.id);
        ++frame->checked_hits;
      }
#endif
      if(take(compare_world_moments(at,actual))>0)
        fail("FieldMovementResultFutureSource",source.source_ref.id);
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "P10.field_movement.original", &world_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
      // Preserve all original rule/refusal gates before attempting an optional
      // cache ticket. No active phase or failed issue simply leaves old behavior.
      if(local && frame && !frame->client && world_.has_active_read_phase()) {
        auto ticket=WorldCurrentReadStampRead::capture_binding(world_,local->versions,{});
        if(ticket.ok && ticket.value) {
          local->stamp=std::move(ticket.value);
          if(reusable(*local,actual,*frame)) {
            frame->client=this;frame->scratch=std::move(local);
          }
        }
      }
      return at;
    });
  }
  Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord& head) const override {
    return movement_detail::guard<ExactOwnerFactRead>([&]{
      const auto* original=world_.common.state().facts.find(head.id);
      if(!original || original->id!=head.id || original->domain!=head.domain ||
          original->kind!=head.kind)
        movement_detail::fail("ExactFactHeaderMismatch",head.id);
      // 原S1内部源没有公众DynamicFact/观察者，不制造第二条付费或完成事实。
      ExactOwnerFactRead out;
      out.payload_memory=CheckpointCopyLease::reserve();
      out.supporting_sources.reserve(1);
      MomentOwnerSource source{{checkpoint_fresh_string_copy(head.domain),
          checkpoint_fresh_string_copy(head.kind),checkpoint_fresh_string_copy(head.id)},
          field_movement_source_schema(head.domain,head.kind)};
      movement_detail::take(read_selected_source_moment(source));
      out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;
      out.supporting_sources.push_back(std::move(source));
      return out;
    });
  }
};
} // namespace
Outcome<std::unique_ptr<const ExactResultOwnerRead>> make_native_field_movement_result_read(
    const WorldBundle& world,const CompleteExactWorldBindingOwner& owner,
    const NativeCompleteWorldCoreSources& core) {
  return movement_detail::guard<std::unique_ptr<const ExactResultOwnerRead>>([&]()
      ->std::unique_ptr<const ExactResultOwnerRead>{
    using namespace movement_detail;
    if(!world.catalog)fail("FieldMovementResultCatalogRequired");
    const auto selected=take(selected_native_movement_boundary_version(world.catalog->identity));
    if(!selected)return {};
    take(selected_native_movement_boundary_continuation_version(world.catalog->identity));
    take(selected_native_group_member_handoff_versions(world.catalog->identity));
    const auto versions=take(selected_native_transport_versions(world.catalog->identity));
    if(versions.travel!=4 || versions.group!=6 || versions.manifest!=2 ||
        !world.movement_exact || !world.field_battle || !world.spatial_carriers ||
        !world.group_movement_executions || !world.catalog->field_battle)
      fail("FieldMovementResultOriginalRootsRequired");
    if(&core.characters.early.world!=&world ||
        &core.characters.early.complete!=&owner)
      fail("FieldMovementResultSameWorldRequired");
    return std::unique_ptr<const ExactResultOwnerRead>(
        new NativeFieldMovementResultRead(world,owner,core));
  });
}
} // namespace hanmo::resident
