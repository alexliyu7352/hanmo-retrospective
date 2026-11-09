#include "local_move_source.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 局部位置只比较原聚落/场所身份，不把来源字符串当坐标。
bool same_place(const SettlementLocation& a,const SettlementLocation& z) {
  return a.settlement_id==z.settlement_id && a.place_id==z.place_id;
}
// 原 Common tap 必须恰为本次 ActorMutation 的前后值，含同值写入。
void actual_draft(const CommonStore& c,const LocalMoveSource& row,Integer sequence) {
  const auto& drafts=c.actor_location_execution_drafts();
  if(!c.actor_location_capture_identity() || sequence<1 || std::size_t(sequence)!=drafts.size())
    fail("LocalMoveActualCaptureRequired",row.id);
  const auto& d=drafts[std::size_t(sequence-1)];
  const auto* from=d.before?std::get_if<SettlementLocation>(&*d.before):nullptr;
  const auto* to=std::get_if<SettlementLocation>(&d.after);
  if(!d.write_identity || d.actor_id!=row.actor_id || d.sequence!=sequence || !from || !to ||
     !same_place(*from,row.from) || !same_place(*to,row.to))
    fail("LocalMoveActualWriteMismatch",row.id);
}
}
Status validate_local_move_value(const LocalMoveSource& row,ExactArithmeticBudget b) {
 return status([&]{
  if(row.id.empty() || row.actor_id.empty() || row.before_head.empty() ||
     row.from.settlement_id.empty() || row.from.place_id.empty() ||
     row.to.settlement_id.empty() || row.to.place_id.empty() ||
     row.before_prefix.common_fact_count<0 ||
     bool(row.before_prefix.last_common_fact)!=(row.before_prefix.common_fact_count!=0))
    fail("InvalidLocalMoveSourceShape",row.id);
  take(encode_world_moment(row.occurred_at,b));
  for(std::size_t i=0;i<row.before_head.size();++i) {
    const auto& h=row.before_head[i];
    if(!source_valid(h.source_ref) || h.source_schema<=0 ||
       h.source_schema!=take(actor_spatial_history_source_schema(h.source_ref)))
      fail("LocalMoveOriginalHeadRequired",row.id);
    for(std::size_t j=0;j<i;++j)
      if(same_source(h.source_ref,row.before_head[j].source_ref))
        fail("DuplicateLocalMoveOriginalHead",row.id);
  }
 });
}
Outcome<const LocalMoveSource*> read_local_move_source(const CommonStore& c,
    const MovementExactState& state,const SourceRef& source,Integer schema,ExactArithmeticBudget b) {
 world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "local_move.reader", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {}, -1});
 return guard<const LocalMoveSource*>([&]{
  if(schema!=1 || source.domain!="travel" || source.kind!="local_move")
    fail("LocalMoveSourceRequired",source.id);
  const auto* row=state.local_moves.find(source.id);
  if(!row || row->id!=source.id || !c.actor(row->actor_id)) fail("LocalMoveSourceRequired",source.id);
  take(validate_local_move_value(*row,b));
  const auto before=take(read_actual_common_fact_prefix(c,row->before_prefix));
  if(take(read_actual_common_fact_position(c,source))!=before)
    fail("LocalMoveOriginalPrefixMismatch",source.id);
  for(const auto& h:row->before_head) {
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "local_move.before_head", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {"fact", h.source_ref.domain, h.source_ref.kind, h.source_ref.id, h.source_schema}, world_stage_observation::observed_cut(before)});
    if(h.source_ref.domain=="actor_foundation" && h.source_ref.kind=="scenario_placement") {
      if(h.source_ref.id!=row->actor_id) fail("LocalMoveOriginalActorMismatch",source.id);
    } else if(take(read_actual_common_fact_position(c,h.source_ref))>=before)
      fail("LocalMoveOriginalHeadOrderRequired",source.id);
  }
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "local_move.validated", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {}, world_stage_observation::observed_cut(before), row->occurred_at.half_day_tick, true});
  return row;
 });
}
Status validate_local_move_headers(const CommonState& c,const MovementExactState* s,
                                   bool selected,ExactArithmeticBudget b) {
 return status([&]{
  if(selected && !s) fail("LocalMoveOriginalRootRequired");
  if(s) for(const auto& [key,row]:s->local_moves.rows()) {
    if(!selected || key!=row.id) fail("UnselectedLocalMoveSources",key);
    take(validate_local_move_value(row,b));
    const auto* head=c.facts.find(key);
    if(!head || head->id!=key || head->domain!="travel" || head->kind!="local_move")
      fail("LocalMoveOriginalHeaderRequired",key);
  }
  for(const auto& [key,head]:c.facts.rows()) if(head.kind=="local_move")
    if(!selected || !s || head.id!=key || head.domain!="travel" || !s->local_moves.find(key))
      fail("LocalMovePhysicalCoverageMismatch",key);
  // 行与头同时删除也不能遗留后继运动冻结的局部位置来源。
  const auto retained_ref=[&](const SourceRef& source) -> const LocalMoveSource* {
    if(source.kind!="local_move") return nullptr;
    const auto* row=s?s->local_moves.find(source.id):nullptr;
    const auto* head=c.facts.find(source.id);
    if(!selected || source.domain!="travel" || !row || !head ||
       head->domain!="travel" || head->kind!="local_move")
      fail("LocalMoveRetainedSourceRequired",source.id);
    return row;
  };
  const auto retained_anchor=[&](const MovementAnchor& anchor,
      const SourceRef* owner=nullptr,const WorldMoment* at=nullptr) {
    std::visit([&](const auto& value) {
      using T=std::decay_t<decltype(value)>;
      const SourceRef& ref=[&]() -> const SourceRef& {
        if constexpr(std::is_same_v<T,TerrainContinuousAnchor>) return value.actual_prefix_close_source;
        else return value.actual_source;
      }();
      if(const auto* row=retained_ref(ref)) {
        if(at && take(compare_world_moments(row->occurred_at,*at,b))>0)
          fail("LocalMoveRetainedSourceInFuture",ref.id);
        if(owner && !same_source(ref,*owner) &&
           take(read_actual_common_fact_position(c,ref))>=take(read_actual_common_fact_position(c,*owner)))
          fail("LocalMoveRetainedSourceOrderRequired",ref.id);
        if constexpr(std::is_same_v<T,OriginalMovementAnchor>) {
          const auto* at=std::get_if<SettlementLocation>(&value.location);
          if(!at || !same_place(*at,row->to)) fail("LocalMoveRetainedAnchorMismatch",ref.id);
        } else fail("LocalMoveRetainedAnchorMismatch",ref.id);
      }
    },anchor);
  };
  const auto retained_basis=[&](const MovementBasis& basis,const SourceRef& owner,const WorldMoment& at) {
    std::visit([&](const auto& value){retained_anchor(value.start_anchor,&owner,&at);retained_anchor(value.end_anchor,&owner,&at);},basis);
  };
  if(s) {
    for(const auto& [key,row]:s->local_moves.rows()) {
      (void)key; for(const auto& head:row.before_head) retained_ref(head.source_ref);
    }
    for(const auto& [key,row]:s->prefixes.rows()) {
      const SourceRef owner{"travel","movement_prefix",key};
      retained_basis(row.basis,owner,row.last_settled_at);
      if(const auto* closed=std::get_if<MovementClosed>(&row.lifecycle))
        retained_anchor(closed->actual_anchor,&owner,&row.last_settled_at);
    }
    for(const auto& [key,row]:s->exertion_bases.rows())
      retained_basis(row.actual_basis,{"travel","travel_exertion_basis",key},row.opened_at);
    for(const auto& [key,row]:s->unit_closes.rows()) {
      const SourceRef owner{"travel","movement_unit_close",key};
      retained_anchor(row.actual_anchor,&owner,&row.occurred_at);
    }
    for(const auto& [key,row]:s->arrivals.rows()) {
      const SourceRef owner{"travel","movement_arrival",key};
      retained_anchor(row.actual_anchor,&owner,&row.occurred_at);
    }
    for(const auto& [key,row]:s->basis_transitions.rows()) {
      const SourceRef owner{"travel","movement_basis_transition",key};
      retained_anchor(row.actual_anchor,&owner,&row.occurred_at);
    }
    for(const auto& [key,row]:s->boundary_contacts.rows()) {
      const SourceRef owner{"travel","movement_boundary_contact",key};
      retained_anchor(row.actual_anchor,&owner,&row.occurred_at);
    }
    for(const auto& [key,row]:s->patrol_final_motions.rows()) {
      const SourceRef owner{"travel","patrol_final_motion",key};
      retained_anchor(row.actual_anchor,&owner,&row.occurred_at);
    }
    for(const auto& [key,row]:s->spatial_bindings.rows()) {
      (void)key; retained_ref(row.binding_source);
      if(const auto* stable=std::get_if<StableMovementWriter>(&row.writer)) retained_anchor(stable->anchor,&row.binding_source);
    }
  }
  if(s) for(const auto& [key,binding]:s->spatial_bindings.rows())
    if(binding.binding_source.kind=="local_move") {
      const auto* row=s->local_moves.find(binding.binding_source.id);
      const auto* stable=std::get_if<StableMovementWriter>(&binding.writer);
      const auto* original=stable?std::get_if<OriginalMovementAnchor>(&stable->anchor):nullptr;
      const auto* place=original?std::get_if<SettlementLocation>(&original->location):nullptr;
      if(!selected || !row || binding.binding_source.domain!="travel" ||
         binding.subject_kind!="actor" || binding.subject_id!=row->actor_id || key!="actor:"+row->actor_id ||
         !place || !same_place(*place,row->to) || !same_source(original->actual_source,binding.binding_source))
        fail("LocalMovePhysicalBindingMismatch",key);
    }
 });
}
Outcome<BoundSpatialLocationWrite> apply_local_move_with_source_in_candidate(
    CommonStore& c,MovementExactState& state,const TravelCatalog& catalog,
    const StableId& actor,const StableId& place,const WorldMoment& at,
    const ActorSpatialLocationView& before,ExactArithmeticBudget b) {
 return guard<BoundSpatialLocationWrite>([&]{
  const auto* original=c.actor(actor);
  const auto* from=original?std::get_if<SettlementLocation>(&original->location):nullptr;
  const auto* actual_before=std::get_if<SettlementLocation>(&before.location_snapshot);
  const auto* target=catalog.places.find(place);
  const auto* origin=from?catalog.places.find(from->place_id):nullptr;
  if(!from || !actual_before || before.actor_id!=actor || !same_place(*from,*actual_before) ||
     before.actual_location_sources.empty() || !target || !origin ||
     origin->settlement_id!=from->settlement_id ||
     std::find(origin->passages.begin(),origin->passages.end(),place)==origin->passages.end())
    fail("LocalMoveOriginalPassageRequired",actor);
  const auto anchor=take(require_actor_ordinary_movement_anchor(before));
  const auto* location=std::get_if<OriginalMovementAnchor>(anchor);
  const auto* site=location?std::get_if<SettlementLocation>(&location->location):nullptr;
  if(!site || !same_place(*site,*from)) fail("LocalMoveOriginalPositionRequired",actor);
  if(const auto* binding=state.spatial_bindings.find("actor:"+actor);
     binding && !std::holds_alternative<StableMovementWriter>(binding->writer))
    fail("LocalMoveStableWriterRequired",actor);
  if(!c.actor_location_capture_identity()) fail("LocalMoveActualCaptureRequired",actor);
  const auto prefix=take(read_current_common_fact_prefix(c));
  LocalMoveSource row; row.id=take(c.next_id("local_move")); row.actor_id=actor; row.occurred_at=at;
  row.before_prefix.common_fact_count=prefix.common_fact_count;
  if(prefix.last_common_fact) row.before_prefix.last_common_fact=SourceRef{
      prefix.last_common_fact->domain,prefix.last_common_fact->kind,prefix.last_common_fact->id};
  for(const auto& h:before.actual_location_sources) row.before_head.push_back({h.source,h.schema});
  row.from=*from; row.to={target->settlement_id,place};
  take(validate_local_move_value(row,b));
  if(c.actor_location_execution_drafts().size()>=std::size_t(std::numeric_limits<Integer>::max()))
    fail("LocalMoveCaptureSequenceOverflow",actor);
  const auto sequence=Integer(c.actor_location_execution_drafts().size())+1;
  take(apply_move_local_location(c,catalog,actor,place));
  actual_draft(c,row,sequence);
  const SourceRef ref{"travel","local_move",row.id};
  const MovementAnchor after=OriginalMovementAnchor{row.to,ref};
  const auto id=row.id; state.local_moves.set(id,std::move(row));
  take(c.append_fact({id,"local_move","travel"}));
  take(read_local_move_source(c,state,ref,1,b));
  state.spatial_bindings.set("actor:"+actor,SpatialWriterBinding{"actor",actor,StableMovementWriter{after},ref});
  return take(bind_actual_spatial_location_write(c,state,actor,ref,1,after,sequence,b));
 });
}
Status validate_local_move_source_history(const ActorSpatialHeadSourceReadContext& x,
    const MilitarySpatialHandoffState* military,const NamedMilitaryCustodyPlacementRead* custody,
    const FieldBattleExitPlacementRead* field) {
 return status([&]{
  const auto now=take(x.spatial.current_moment());
  for(const auto& [key,value]:x.movement.local_moves.rows()) {
    const auto* row=take(read_local_move_source(x.common,x.movement,{"travel","local_move",key},1,x.budget));
    if(take(compare_world_moments(row->occurred_at,now,x.budget))>0) fail("LocalMoveSourceInFuture",key);
    const auto before=take(read_actor_spatial_location_at_actual_fact_prefix(x.common,x.common_catalog,
        x.movement,x.carriers,x.registrations,x.foundation,x.actors,x.spatial,row->actor_id,
        row->occurred_at,row->before_prefix,military,x.budget,x.family_catalog,x.births,x.military_actions,custody,field));
    const auto* anchor=take(require_actor_ordinary_movement_anchor(before));
    const auto* original=std::get_if<OriginalMovementAnchor>(anchor);
    const auto* site=original?std::get_if<SettlementLocation>(&original->location):nullptr;
    if(!site || !same_place(*site,row->from)) fail("LocalMoveOriginalPositionMismatch",key);
    // 真实 cut 的 canonical 头完整匹配；不能以最晚一个源掩盖多余或删去的别名。
    if(before.actual_location_sources.size()!=row->before_head.size())
      fail("LocalMoveOriginalHeadMismatch",key);
    for(std::size_t i=0;i<row->before_head.size();++i) {
      const auto& actual=before.actual_location_sources[i];
      const auto& frozen=row->before_head[i];
      if(actual.schema!=frozen.source_schema || !same_source(actual.source,frozen.source_ref))
        fail("LocalMoveOriginalHeadMismatch",key);
    }
  }
 });
}
} // namespace hanmo::resident
