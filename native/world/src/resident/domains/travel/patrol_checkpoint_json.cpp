// Patrol源与binary同一有限字段表；原单位历史只引用，不重复编码运动。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "patrol_checkpoint.hpp"
#include "movement_exact_detail.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_projection.hpp"
#include <cmath>
#include <set>
namespace hanmo::resident {namespace patrol_json {using namespace movement_detail;using V=BoundaryValue;
const V& get(const V& v,const char* name){const auto* x=v.find(name);if(!x)fail("MissingPatrolCheckpointField",name);return *x;}
void fields(const V& v,std::initializer_list<const char*> allowed){const auto* d=v.as<V::Dict>();if(!d||d->size()!=allowed.size())fail("PatrolCheckpointRecordFieldsMismatch");auto key=allowed.begin();for(const auto& row:*d)if(row.first!=*key++)fail("PatrolCheckpointRecordFieldOrderMismatch",row.first);}
void read(const V&,SourceRef&);
Status emit(ProjectionWriter&,std::string_view,const SourceRef&);
void read(const V&,SubjectRef&);
Status emit(ProjectionWriter&,std::string_view,const SubjectRef&);
void read(const V&,TransportUnitRef&);
Status emit(ProjectionWriter&,std::string_view,const TransportUnitRef&);
void read(const V&,ActorCarrierRef&);
Status emit(ProjectionWriter&,std::string_view,const ActorCarrierRef&);
void read(const V&,PartyCarrierRef&);
Status emit(ProjectionWriter&,std::string_view,const PartyCarrierRef&);
void read(const V&,ArmyCarrierRef&);
Status emit(ProjectionWriter&,std::string_view,const ArmyCarrierRef&);
void read(const V&,FleetCarrierRef&);
Status emit(ProjectionWriter&,std::string_view,const FleetCarrierRef&);
void read(const V&,TransportUnitCarrierRef&);
Status emit(ProjectionWriter&,std::string_view,const TransportUnitCarrierRef&);
void read(const V&,PersonalMovementExecution&);
Status emit(ProjectionWriter&,std::string_view,const PersonalMovementExecution&);
void read(const V&,GroupMovementExecution&);
Status emit(ProjectionWriter&,std::string_view,const GroupMovementExecution&);
void read(const V&,CustodyExitMovementExecution&);
Status emit(ProjectionWriter&,std::string_view,const CustodyExitMovementExecution&);
void read(const V&,ShoreAssaultMovementExecution&);
Status emit(ProjectionWriter&,std::string_view,const ShoreAssaultMovementExecution&);
void read(const V&,OriginalMovementAnchor&);
Status emit(ProjectionWriter&,std::string_view,const OriginalMovementAnchor&);
void read(const V&,RoadFractionalAnchor&);
Status emit(ProjectionWriter&,std::string_view,const RoadFractionalAnchor&);
void read(const V&,TerrainContinuousAnchor&);
Status emit(ProjectionWriter&,std::string_view,const TerrainContinuousAnchor&);
void read(const V&,PatrolFinalMotionSource&);
Status emit(ProjectionWriter&,std::string_view,const PatrolFinalMotionSource&);
void read(const V&,PatrolCheckpointArrival2&);
Status emit(ProjectionWriter&,std::string_view,const PatrolCheckpointArrival2&);
void read(const V&,SubjectKind&);
Status emit(ProjectionWriter&,std::string_view,const SubjectKind&);
void read(const V&,StableId&);
Status emit(ProjectionWriter&,std::string_view,const StableId&);
void read(const V&,Integer&);
Status emit(ProjectionWriter&,std::string_view,const Integer&);
void read(const V&,bool&);
Status emit(ProjectionWriter&,std::string_view,const bool&);
void read(const V&,double&);
Status emit(ProjectionWriter&,std::string_view,const double&);
void read(const V&,WorldMoment&);
Status emit(ProjectionWriter&,std::string_view,const WorldMoment&);
void read(const V&,CanonicalFraction&);
Status emit(ProjectionWriter&,std::string_view,const CanonicalFraction&);
void read(const V&,Location&);
Status emit(ProjectionWriter&,std::string_view,const Location&);
template<class T>void read(const V&,std::vector<T>&);template<class T>void read(const V&,std::optional<T>&);template<class...T>void read(const V&,std::variant<T...>&);
template<class T>Status emit(ProjectionWriter&,std::string_view,const std::vector<T>&);template<class T>Status emit(ProjectionWriter&,std::string_view,const std::optional<T>&);template<class...T>Status emit(ProjectionWriter&,std::string_view,const std::variant<T...>&);
void read(const V& v,StableId& x){const auto* p=v.as<StableId>();if(!p)fail("PatrolCheckpointScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const StableId& v){return w.field(name,v);}
void read(const V& v,Integer& x){const auto* p=v.as<Integer>();if(!p)fail("PatrolCheckpointScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const Integer& v){return w.field(name,v);}
void read(const V& v,bool& x){const auto* p=v.as<bool>();if(!p)fail("PatrolCheckpointScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const bool& v){return w.field(name,v);}
void read(const V& v,double& x){const auto* p=v.as<double>();if(!p)fail("PatrolCheckpointScalarTypeMismatch");if(!std::isfinite(*p))fail("NonCanonicalPatrolBinary64");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const double& v){return w.field(name,v);}
void read(const V& v,WorldMoment& x){x=take(exact_fields::decode_json<WorldMoment>(v,[](auto& c,auto& a){return exact_fields::moment(c,a);}));}
Status emit(ProjectionWriter& w,std::string_view name,const WorldMoment& v){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::moment(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
void read(const V& v,CanonicalFraction& x){x=take(exact_fields::decode_json<CanonicalFraction>(v,[](auto& c,auto& a){return exact_fields::fraction(c,a);}));}
Status emit(ProjectionWriter& w,std::string_view name,const CanonicalFraction& v){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::fraction(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
void read(const V& v,Location& x){try{x=boundary::decode_location(v,"patrol/location",true);}catch(const boundary::DecodeFailure& e){throw Failure{e.error};}}
Status emit(ProjectionWriter& w,std::string_view name,const Location& v){return emit_common_location(w,StableId{name},v,{},true);}
template<class T>void read(const V& v,std::vector<T>& out){const auto* a=v.as<V::Array>();if(!a||a->size()>CodecVisitor::max_items)fail("PatrolCheckpointArrayRequired");out.resize(a->size());for(std::size_t i=0;i<a->size();++i)read((*a)[i],out[i]);}
template<class T>void read(const V& v,std::optional<T>& out){if(v.as<std::monostate>()){out.reset();return;}out.emplace();read(v,*out);}
template<std::size_t I=0,class Variant>bool construct(Variant& out,Integer tag){if constexpr(I<std::variant_size_v<Variant>){if(tag==Integer(I)){out.template emplace<I>();return true;}return construct<I+1>(out,tag);}return false;}
template<class...T>void read(const V& v,std::variant<T...>& out){fields(v,{"variant","value"});Integer tag=0;read(get(v,"variant"),tag);if(!construct(out,tag))fail("InvalidPatrolCheckpointVariant");std::visit([&](auto& x){read(get(v,"value"),x);},out);}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::vector<T>& v){return w.sequence(name,v,[&](auto& x,const auto& item){return emit(x,"",item);});}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::optional<T>& v){return v?emit(w,name,*v):w.null(name);}
template<class...T>Status emit(ProjectionWriter& w,std::string_view name,const std::variant<T...>& v){return w.object(name,[&](auto& x){x.field("variant",Integer(v.index()));return std::visit([&](const auto& item){return emit(x,"value",item);},v);});}
void read(const V& v,SubjectKind& x){Integer tag=0;read(v,tag);if(tag<0||tag>2)fail("InvalidPatrolCheckpointEnum");x=SubjectKind(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const SubjectKind& v){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidPatrolCheckpointEnum");return emit(w,name,tag);}
void read(const V& v,SourceRef& x){fields(v,{"domain","kind","id"});read(get(v,"domain"),x.domain);read(get(v,"kind"),x.kind);read(get(v,"id"),x.id);}
Status emit(ProjectionWriter& w,std::string_view name,const SourceRef& v){return w.object(name,[&](auto& x){emit(x,"domain",v.domain);emit(x,"kind",v.kind);emit(x,"id",v.id);return x.status();});}
void read(const V& v,SubjectRef& x){fields(v,{"kind","id"});read(get(v,"kind"),x.kind);read(get(v,"id"),x.id);}
Status emit(ProjectionWriter& w,std::string_view name,const SubjectRef& v){return w.object(name,[&](auto& x){emit(x,"kind",v.kind);emit(x,"id",v.id);return x.status();});}
void read(const V& v,TransportUnitRef& x){fields(v,{"subject","transport_unit_id"});read(get(v,"subject"),x.subject);read(get(v,"transport_unit_id"),x.transport_unit_id);}
Status emit(ProjectionWriter& w,std::string_view name,const TransportUnitRef& v){return w.object(name,[&](auto& x){emit(x,"subject",v.subject);emit(x,"transport_unit_id",v.transport_unit_id);return x.status();});}
void read(const V& v,ActorCarrierRef& x){fields(v,{"actor_id"});read(get(v,"actor_id"),x.actor_id);}
Status emit(ProjectionWriter& w,std::string_view name,const ActorCarrierRef& v){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id);return x.status();});}
void read(const V& v,PartyCarrierRef& x){fields(v,{"party_id"});read(get(v,"party_id"),x.party_id);}
Status emit(ProjectionWriter& w,std::string_view name,const PartyCarrierRef& v){return w.object(name,[&](auto& x){emit(x,"party_id",v.party_id);return x.status();});}
void read(const V& v,ArmyCarrierRef& x){fields(v,{"army_id"});read(get(v,"army_id"),x.army_id);}
Status emit(ProjectionWriter& w,std::string_view name,const ArmyCarrierRef& v){return w.object(name,[&](auto& x){emit(x,"army_id",v.army_id);return x.status();});}
void read(const V& v,FleetCarrierRef& x){fields(v,{"fleet_id"});read(get(v,"fleet_id"),x.fleet_id);}
Status emit(ProjectionWriter& w,std::string_view name,const FleetCarrierRef& v){return w.object(name,[&](auto& x){emit(x,"fleet_id",v.fleet_id);return x.status();});}
void read(const V& v,TransportUnitCarrierRef& x){fields(v,{"transport_unit_ref","composition_revision"});read(get(v,"transport_unit_ref"),x.transport_unit_ref);read(get(v,"composition_revision"),x.composition_revision);}
Status emit(ProjectionWriter& w,std::string_view name,const TransportUnitCarrierRef& v){return w.object(name,[&](auto& x){emit(x,"transport_unit_ref",v.transport_unit_ref);emit(x,"composition_revision",v.composition_revision);return x.status();});}
void read(const V& v,PersonalMovementExecution& x){fields(v,{"action_id","journey_id","segment_ordinal"});read(get(v,"action_id"),x.action_id);read(get(v,"journey_id"),x.journey_id);read(get(v,"segment_ordinal"),x.segment_ordinal);}
Status emit(ProjectionWriter& w,std::string_view name,const PersonalMovementExecution& v){return w.object(name,[&](auto& x){emit(x,"action_id",v.action_id);emit(x,"journey_id",v.journey_id);emit(x,"segment_ordinal",v.segment_ordinal);return x.status();});}
void read(const V& v,GroupMovementExecution& x){fields(v,{"journey_id","carrier","segment_ordinal"});read(get(v,"journey_id"),x.journey_id);read(get(v,"carrier"),x.carrier);read(get(v,"segment_ordinal"),x.segment_ordinal);}
Status emit(ProjectionWriter& w,std::string_view name,const GroupMovementExecution& v){return w.object(name,[&](auto& x){emit(x,"journey_id",v.journey_id);emit(x,"carrier",v.carrier);emit(x,"segment_ordinal",v.segment_ordinal);return x.status();});}
void read(const V& v,CustodyExitMovementExecution& x){fields(v,{"escape_action_id","exit_terms_id","segment_ordinal"});read(get(v,"escape_action_id"),x.escape_action_id);read(get(v,"exit_terms_id"),x.exit_terms_id);read(get(v,"segment_ordinal"),x.segment_ordinal);}
Status emit(ProjectionWriter& w,std::string_view name,const CustodyExitMovementExecution& v){return w.object(name,[&](auto& x){emit(x,"escape_action_id",v.escape_action_id);emit(x,"exit_terms_id",v.exit_terms_id);emit(x,"segment_ordinal",v.segment_ordinal);return x.status();});}
void read(const V& v,ShoreAssaultMovementExecution& x){fields(v,{"landing_operation_id","crossing_action_id","segment_ordinal"});read(get(v,"landing_operation_id"),x.landing_operation_id);read(get(v,"crossing_action_id"),x.crossing_action_id);read(get(v,"segment_ordinal"),x.segment_ordinal);}
Status emit(ProjectionWriter& w,std::string_view name,const ShoreAssaultMovementExecution& v){return w.object(name,[&](auto& x){emit(x,"landing_operation_id",v.landing_operation_id);emit(x,"crossing_action_id",v.crossing_action_id);emit(x,"segment_ordinal",v.segment_ordinal);return x.status();});}
void read(const V& v,OriginalMovementAnchor& x){fields(v,{"location","actual_source"});read(get(v,"location"),x.location);read(get(v,"actual_source"),x.actual_source);}
Status emit(ProjectionWriter& w,std::string_view name,const OriginalMovementAnchor& v){return w.object(name,[&](auto& x){emit(x,"location",v.location);emit(x,"actual_source",v.actual_source);return x.status();});}
void read(const V& v,RoadFractionalAnchor& x){fields(v,{"edge_id","origin","progress","actual_source"});read(get(v,"edge_id"),x.edge_id);read(get(v,"origin"),x.origin);read(get(v,"progress"),x.progress);read(get(v,"actual_source"),x.actual_source);}
Status emit(ProjectionWriter& w,std::string_view name,const RoadFractionalAnchor& v){return w.object(name,[&](auto& x){emit(x,"edge_id",v.edge_id);emit(x,"origin",v.origin);emit(x,"progress",v.progress);emit(x,"actual_source",v.actual_source);return x.status();});}
void read(const V& v,TerrainContinuousAnchor& x){fields(v,{"map_id","origin","x_bits","y_bits","actual_prefix_close_source","geometry_profile_ref"});read(get(v,"map_id"),x.map_id);read(get(v,"origin"),x.origin);read(get(v,"x_bits"),x.x_bits);read(get(v,"y_bits"),x.y_bits);read(get(v,"actual_prefix_close_source"),x.actual_prefix_close_source);read(get(v,"geometry_profile_ref"),x.geometry_profile_ref);}
Status emit(ProjectionWriter& w,std::string_view name,const TerrainContinuousAnchor& v){return w.object(name,[&](auto& x){emit(x,"map_id",v.map_id);emit(x,"origin",v.origin);emit(x,"x_bits",v.x_bits);emit(x,"y_bits",v.y_bits);emit(x,"actual_prefix_close_source",v.actual_prefix_close_source);emit(x,"geometry_profile_ref",v.geometry_profile_ref);return x.status();});}
void read(const V& v,PatrolFinalMotionSource& x){fields(v,{"id","prefix_id","unit_ref","basis_ref","execution","occurred_at","cursor","actual_anchor","final_contact_source"});read(get(v,"id"),x.id);read(get(v,"prefix_id"),x.prefix_id);read(get(v,"unit_ref"),x.unit_ref);read(get(v,"basis_ref"),x.basis_ref);read(get(v,"execution"),x.execution);read(get(v,"occurred_at"),x.occurred_at);read(get(v,"cursor"),x.cursor);read(get(v,"actual_anchor"),x.actual_anchor);read(get(v,"final_contact_source"),x.final_contact_source);}
Status emit(ProjectionWriter& w,std::string_view name,const PatrolFinalMotionSource& v){return w.object(name,[&](auto& x){emit(x,"id",v.id);emit(x,"prefix_id",v.prefix_id);emit(x,"unit_ref",v.unit_ref);emit(x,"basis_ref",v.basis_ref);emit(x,"execution",v.execution);emit(x,"occurred_at",v.occurred_at);emit(x,"cursor",v.cursor);emit(x,"actual_anchor",v.actual_anchor);emit(x,"final_contact_source",v.final_contact_source);return x.status();});}
void read(const V& v,PatrolCheckpointArrival2& x){fields(v,{"source_fact_id","journey_id","final_motion_source_ref","actor_or_group_ref","checkpoint_ref","arrived_at"});read(get(v,"source_fact_id"),x.source_fact_id);read(get(v,"journey_id"),x.journey_id);read(get(v,"final_motion_source_ref"),x.final_motion_source_ref);read(get(v,"actor_or_group_ref"),x.actor_or_group_ref);read(get(v,"checkpoint_ref"),x.checkpoint_ref);read(get(v,"arrived_at"),x.arrived_at);}
Status emit(ProjectionWriter& w,std::string_view name,const PatrolCheckpointArrival2& v){return w.object(name,[&](auto& x){emit(x,"source_fact_id",v.source_fact_id);emit(x,"journey_id",v.journey_id);emit(x,"final_motion_source_ref",v.final_motion_source_ref);emit(x,"actor_or_group_ref",v.actor_or_group_ref);emit(x,"checkpoint_ref",v.checkpoint_ref);emit(x,"arrived_at",v.arrived_at);return x.status();});}
template<class T>void read_table(const V& v,OrderedTable<T>& out){const auto* rows=v.as<V::Dict>();if(!rows||rows->size()>CodecVisitor::max_items)fail("PatrolCheckpointTableRequired");std::vector<typename OrderedTable<T>::Row> parsed;parsed.reserve(rows->size());for(const auto& row:*rows){T value;read(row.second,value);parsed.emplace_back(row.first,std::move(value));}if(!out.adopt_checkpoint_rows(std::move(parsed)))fail("DuplicatePatrolCheckpointTableKey");}
template<class T>Status emit_table(ProjectionWriter& w,const char* name,const OrderedTable<T>& table){return w.object(name,[&](auto& x){for(const auto& row:table.rows()){emit(x,row.first,row.second);if(!x.status().ok)break;}return x.status();});}
}
Status decode_patrol_checkpoint_sources(const BoundaryValue& v,MovementExactState& state){return movement_detail::status([&]{patrol_json::fields(v,{"final_motion_sources","arrivals"});OrderedTable<PatrolFinalMotionSource> motions;OrderedTable<PatrolCheckpointArrival2> arrivals;patrol_json::read_table(patrol_json::get(v,"final_motion_sources"),motions);patrol_json::read_table(patrol_json::get(v,"arrivals"),arrivals);state.patrol_final_motions=std::move(motions);state.patrol_checkpoint_arrivals=std::move(arrivals);});}
Status emit_patrol_checkpoint_sources(ProjectionWriter& w,const MovementExactState& state){patrol_json::emit_table(w,"final_motion_sources",state.patrol_final_motions);patrol_json::emit_table(w,"arrivals",state.patrol_checkpoint_arrivals);return w.status();}
Outcome<BoundaryValue> export_patrol_checkpoint_sources(const MovementExactState& state){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(w.object("",[&](auto& x){return emit_patrol_checkpoint_sources(x,state);}));movement_detail::take(w.finish());return out;});}
}
