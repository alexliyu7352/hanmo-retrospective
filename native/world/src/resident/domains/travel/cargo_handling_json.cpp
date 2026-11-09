// 同generate_codec.py唯一typed字段表生成JSON双向路径；shape与JSON调用同emit，无状态整表副本。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "cargo_handling.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <set>
namespace hanmo::resident {namespace cargo_json {using namespace movement_detail;using V=BoundaryValue;using Context=CommonResourceCodecContext;
const V& get(const V& v,const char* name){const auto* x=v.find(name);if(!x)fail("MissingCargoHandlingField",name);return *x;}
void fields(const V& v,std::initializer_list<const char*> allowed){const auto* d=v.as<V::Dict>();if(!d||d->size()!=allowed.size())fail("CargoHandlingRecordFieldsMismatch");auto key=allowed.begin();std::set<StableId> seen;for(const auto& row:*d){if(row.first!=*key++||!seen.insert(row.first).second)fail("CargoHandlingRecordFieldOrderMismatch",row.first);}}
void read(const V&, SourceRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const SourceRef&,const Context&);
void read(const V&, SubjectRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const SubjectRef&,const Context&);
void read(const V&, TransportUnitRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const TransportUnitRef&,const Context&);
void read(const V&, ActorCarrierRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const ActorCarrierRef&,const Context&);
void read(const V&, PartyCarrierRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const PartyCarrierRef&,const Context&);
void read(const V&, ArmyCarrierRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const ArmyCarrierRef&,const Context&);
void read(const V&, FleetCarrierRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const FleetCarrierRef&,const Context&);
void read(const V&, TransportUnitCarrierRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const TransportUnitCarrierRef&,const Context&);
void read(const V&, CargoSelection&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoSelection&,const Context&);
void read(const V&, TimeScaleRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const TimeScaleRef&,const Context&);
void read(const V&, ActionElapsedEntryRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const ActionElapsedEntryRef&,const Context&);
void read(const V&, NamedCargoHandlingWorkers&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const NamedCargoHandlingWorkers&,const Context&);
void read(const V&, CohortCargoHandlingWorkers&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CohortCargoHandlingWorkers&,const Context&);
void read(const V&, CargoHandlingTerms&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingTerms&,const Context&);
void read(const V&, NamedCargoHandlingTiming&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const NamedCargoHandlingTiming&,const Context&);
void read(const V&, CohortCargoHandlingTiming&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CohortCargoHandlingTiming&,const Context&);
void read(const V&, CargoHandlingExecution&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingExecution&,const Context&);
void read(const V&, CargoHandlingWorkResult&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingWorkResult&,const Context&);
void read(const V&, CargoHandlingResult&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingResult&,const Context&);
void read(const V&, SubjectKind&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const SubjectKind&,const Context&);
void read(const V&, CargoHandlingOperation&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingOperation&,const Context&);
void read(const V&, CargoHandlingPhase&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingPhase&,const Context&);
void read(const V&, CargoCohortServiceState&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoCohortServiceState&,const Context&);
void read(const V&, CargoHandlingResultStatus&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const CargoHandlingResultStatus&,const Context&);
void read(const V&, StableId&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const StableId&,const Context&);
void read(const V&, Integer&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const Integer&,const Context&);
void read(const V&, WorldMoment&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const WorldMoment&,const Context&);
void read(const V&, WorkUnits&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const WorkUnits&,const Context&);
void read(const V&, Owner&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const Owner&,const Context&);
void read(const V&, ReservationRef&,const Context&);
Status emit(ProjectionWriter&,std::string_view,const ReservationRef&,const Context&);
template<class T>void read(const V&,std::vector<T>&,const Context&);
template<class T>void read(const V&,std::optional<T>&,const Context&);
template<class...T>void read(const V&,std::variant<T...>&,const Context&);
template<class T>Status emit(ProjectionWriter&,std::string_view,const std::vector<T>&,const Context&);
template<class T>Status emit(ProjectionWriter&,std::string_view,const std::optional<T>&,const Context&);
template<class...T>Status emit(ProjectionWriter&,std::string_view,const std::variant<T...>&,const Context&);
void read(const V& v,StableId& x,const Context&){const auto* p=v.as<StableId>();if(!p)fail("CargoHandlingStringRequired");x=*p;}
void read(const V& v,Integer& x,const Context&){const auto* p=v.as<Integer>();if(!p)fail("CargoHandlingIntegerRequired");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const StableId& v,const Context&){return w.field(name,v);}
Status emit(ProjectionWriter& w,std::string_view name,const Integer& v,const Context&){return w.field(name,v);}
void read(const V& v,WorldMoment& x,const Context&){x=take(decode_world_moment2(v));}
void read(const V& v,WorkUnits& x,const Context&){auto decoded=exact_fields::decode_json<WorkUnits>(v,[](auto& c,auto& a){return exact_fields::work_units(c,a);});x=take(std::move(decoded));}
Status emit(ProjectionWriter& w,std::string_view name,const WorldMoment& v,const Context&){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::moment(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status emit(ProjectionWriter& w,std::string_view name,const WorkUnits& v,const Context&){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::work_units(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
void read(const V& v,Owner& x,const Context&){try{x=boundary::decode_owner(v,"cargo_handling/owner");}catch(const boundary::DecodeFailure& e){throw Failure{e.error};}}
void read(const V& v,ReservationRef& x,const Context& c){try{x=boundary::decode_reservation_ref(v,"cargo_handling/reservation",c.domain_activity_reservation_refs_enabled);}catch(const boundary::DecodeFailure& e){throw Failure{e.error};}}
Status emit(ProjectionWriter& w,std::string_view name,const Owner& v,const Context&){return emit_common_owner(w,StableId{name},v);}
Status emit(ProjectionWriter& w,std::string_view name,const ReservationRef& v,const Context& c){return emit_common_reservation(w,StableId{name},v,true,c.domain_activity_reservation_refs_enabled);}
template<class T>void read(const V& v,std::vector<T>& out,const Context& c){const auto* a=v.as<V::Array>();if(!a||a->size()>CodecVisitor::max_items)fail("CargoHandlingArrayRequired");out.resize(a->size());for(std::size_t i=0;i<a->size();++i)read((*a)[i],out[i],c);}
template<class T>void read(const V& v,std::optional<T>& out,const Context& c){if(v.as<std::monostate>()){out.reset();return;}out.emplace();read(v,*out,c);}
template<std::size_t I=0,class Variant>bool construct(Variant& out,Integer tag){if constexpr(I<std::variant_size_v<Variant>){if(tag==Integer(I)){out.template emplace<I>();return true;}return construct<I+1>(out,tag);}return false;}
template<class...T>void read(const V& v,std::variant<T...>& out,const Context& c){fields(v,{"variant","value"});Integer tag=0;read(get(v,"variant"),tag,c);if(!construct(out,tag))fail("InvalidCargoHandlingVariant");std::visit([&](auto& x){read(get(v,"value"),x,c);},out);}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::vector<T>& v,const Context& c){return w.sequence(name,v,[&](auto& x,const auto& item){return emit(x,"",item,c);});}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::optional<T>& v,const Context& c){return v?emit(w,name,*v,c):w.null(name);}
template<class...T>Status emit(ProjectionWriter& w,std::string_view name,const std::variant<T...>& v,const Context& c){return w.object(name,[&](auto& x){x.field("variant",Integer(v.index()));return std::visit([&](const auto& item){return emit(x,"value",item,c);},v);});}
void read(const V& v,SubjectKind& x,const Context& c){Integer tag=0;read(v,tag,c);if(tag<0||tag>2)fail("InvalidCargoHandlingEnum");x=SubjectKind(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const SubjectKind& v,const Context& c){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidCargoHandlingEnum");return emit(w,name,tag,c);}
void read(const V& v,CargoHandlingOperation& x,const Context& c){Integer tag=0;read(v,tag,c);if(tag<0||tag>1)fail("InvalidCargoHandlingEnum");x=CargoHandlingOperation(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingOperation& v,const Context& c){const Integer tag=Integer(v);if(tag<0||tag>1)return w.reject("InvalidCargoHandlingEnum");return emit(w,name,tag,c);}
void read(const V& v,CargoHandlingPhase& x,const Context& c){Integer tag=0;read(v,tag,c);if(tag<0||tag>5)fail("InvalidCargoHandlingEnum");x=CargoHandlingPhase(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingPhase& v,const Context& c){const Integer tag=Integer(v);if(tag<0||tag>5)return w.reject("InvalidCargoHandlingEnum");return emit(w,name,tag,c);}
void read(const V& v,CargoCohortServiceState& x,const Context& c){Integer tag=0;read(v,tag,c);if(tag<0||tag>2)fail("InvalidCargoHandlingEnum");x=CargoCohortServiceState(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoCohortServiceState& v,const Context& c){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidCargoHandlingEnum");return emit(w,name,tag,c);}
void read(const V& v,CargoHandlingResultStatus& x,const Context& c){Integer tag=0;read(v,tag,c);if(tag<0||tag>2)fail("InvalidCargoHandlingEnum");x=CargoHandlingResultStatus(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingResultStatus& v,const Context& c){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidCargoHandlingEnum");return emit(w,name,tag,c);}
void read(const V& v,SourceRef& x,const Context& c){fields(v,{"domain","kind","id"});read(get(v,"domain"),x.domain,c);read(get(v,"kind"),x.kind,c);read(get(v,"id"),x.id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const SourceRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"domain",v.domain,c);emit(x,"kind",v.kind,c);emit(x,"id",v.id,c);return x.status();});}
void read(const V& v,SubjectRef& x,const Context& c){fields(v,{"kind","id"});read(get(v,"kind"),x.kind,c);read(get(v,"id"),x.id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const SubjectRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"kind",v.kind,c);emit(x,"id",v.id,c);return x.status();});}
void read(const V& v,TransportUnitRef& x,const Context& c){fields(v,{"subject","transport_unit_id"});read(get(v,"subject"),x.subject,c);read(get(v,"transport_unit_id"),x.transport_unit_id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const TransportUnitRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"subject",v.subject,c);emit(x,"transport_unit_id",v.transport_unit_id,c);return x.status();});}
void read(const V& v,ActorCarrierRef& x,const Context& c){fields(v,{"actor_id"});read(get(v,"actor_id"),x.actor_id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const ActorCarrierRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id,c);return x.status();});}
void read(const V& v,PartyCarrierRef& x,const Context& c){fields(v,{"party_id"});read(get(v,"party_id"),x.party_id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const PartyCarrierRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"party_id",v.party_id,c);return x.status();});}
void read(const V& v,ArmyCarrierRef& x,const Context& c){fields(v,{"army_id"});read(get(v,"army_id"),x.army_id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const ArmyCarrierRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"army_id",v.army_id,c);return x.status();});}
void read(const V& v,FleetCarrierRef& x,const Context& c){fields(v,{"fleet_id"});read(get(v,"fleet_id"),x.fleet_id,c);}
Status emit(ProjectionWriter& w,std::string_view name,const FleetCarrierRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"fleet_id",v.fleet_id,c);return x.status();});}
void read(const V& v,TransportUnitCarrierRef& x,const Context& c){fields(v,{"transport_unit_ref","composition_revision"});read(get(v,"transport_unit_ref"),x.transport_unit_ref,c);read(get(v,"composition_revision"),x.composition_revision,c);}
Status emit(ProjectionWriter& w,std::string_view name,const TransportUnitCarrierRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"transport_unit_ref",v.transport_unit_ref,c);emit(x,"composition_revision",v.composition_revision,c);return x.status();});}
void read(const V& v,CargoSelection& x,const Context& c){fields(v,{"inventory_id","lot_id","quantity"});read(get(v,"inventory_id"),x.inventory_id,c);read(get(v,"lot_id"),x.lot_id,c);read(get(v,"quantity"),x.quantity,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoSelection& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"inventory_id",v.inventory_id,c);emit(x,"lot_id",v.lot_id,c);emit(x,"quantity",v.quantity,c);return x.status();});}
void read(const V& v,TimeScaleRef& x,const Context& c){fields(v,{"definition_id","version","units_per_half_day"});read(get(v,"definition_id"),x.definition_id,c);read(get(v,"version"),x.version,c);read(get(v,"units_per_half_day"),x.units_per_half_day,c);}
Status emit(ProjectionWriter& w,std::string_view name,const TimeScaleRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"definition_id",v.definition_id,c);emit(x,"version",v.version,c);emit(x,"units_per_half_day",v.units_per_half_day,c);return x.status();});}
void read(const V& v,ActionElapsedEntryRef& x,const Context& c){fields(v,{"action_ref"});read(get(v,"action_ref"),x.action_ref,c);}
Status emit(ProjectionWriter& w,std::string_view name,const ActionElapsedEntryRef& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"action_ref",v.action_ref,c);return x.status();});}
void read(const V& v,NamedCargoHandlingWorkers& x,const Context& c){fields(v,{"lead_actor_id","participant_actor_ids"});read(get(v,"lead_actor_id"),x.lead_actor_id,c);read(get(v,"participant_actor_ids"),x.participant_actor_ids,c);}
Status emit(ProjectionWriter& w,std::string_view name,const NamedCargoHandlingWorkers& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"lead_actor_id",v.lead_actor_id,c);emit(x,"participant_actor_ids",v.participant_actor_ids,c);return x.status();});}
void read(const V& v,CohortCargoHandlingWorkers& x,const Context& c){fields(v,{"allocation_request_refs"});read(get(v,"allocation_request_refs"),x.allocation_request_refs,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CohortCargoHandlingWorkers& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"allocation_request_refs",v.allocation_request_refs,c);return x.status();});}
void read(const V& v,CargoHandlingTerms& x,const Context& c){fields(v,{"terms_id","business_source","batch_ordinal","operation","carrier","source_inventory_id","destination_inventory_id","cargo","matching_owners","source_reservation","destination_owner","handling_rule_ref","accepted_rule_version","actual_site_or_anchor_ref","carrier_composition_source_ref","source_custody_ref","receiver_acceptance_source_ref","expense_authority_ref","cost_terms_ref","labor_selection","facility_claim_ref","accepted_at"});read(get(v,"terms_id"),x.terms_id,c);read(get(v,"business_source"),x.business_source,c);read(get(v,"batch_ordinal"),x.batch_ordinal,c);read(get(v,"operation"),x.operation,c);read(get(v,"carrier"),x.carrier,c);read(get(v,"source_inventory_id"),x.source_inventory_id,c);read(get(v,"destination_inventory_id"),x.destination_inventory_id,c);read(get(v,"cargo"),x.cargo,c);read(get(v,"matching_owners"),x.matching_owners,c);read(get(v,"source_reservation"),x.source_reservation,c);read(get(v,"destination_owner"),x.destination_owner,c);read(get(v,"handling_rule_ref"),x.handling_rule_ref,c);read(get(v,"accepted_rule_version"),x.accepted_rule_version,c);read(get(v,"actual_site_or_anchor_ref"),x.actual_site_or_anchor_ref,c);read(get(v,"carrier_composition_source_ref"),x.carrier_composition_source_ref,c);read(get(v,"source_custody_ref"),x.source_custody_ref,c);read(get(v,"receiver_acceptance_source_ref"),x.receiver_acceptance_source_ref,c);read(get(v,"expense_authority_ref"),x.expense_authority_ref,c);read(get(v,"cost_terms_ref"),x.cost_terms_ref,c);read(get(v,"labor_selection"),x.labor_selection,c);read(get(v,"facility_claim_ref"),x.facility_claim_ref,c);read(get(v,"accepted_at"),x.accepted_at,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingTerms& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"terms_id",v.terms_id,c);emit(x,"business_source",v.business_source,c);emit(x,"batch_ordinal",v.batch_ordinal,c);emit(x,"operation",v.operation,c);emit(x,"carrier",v.carrier,c);emit(x,"source_inventory_id",v.source_inventory_id,c);emit(x,"destination_inventory_id",v.destination_inventory_id,c);emit(x,"cargo",v.cargo,c);emit(x,"matching_owners",v.matching_owners,c);emit(x,"source_reservation",v.source_reservation,c);emit(x,"destination_owner",v.destination_owner,c);emit(x,"handling_rule_ref",v.handling_rule_ref,c);emit(x,"accepted_rule_version",v.accepted_rule_version,c);emit(x,"actual_site_or_anchor_ref",v.actual_site_or_anchor_ref,c);emit(x,"carrier_composition_source_ref",v.carrier_composition_source_ref,c);emit(x,"source_custody_ref",v.source_custody_ref,c);emit(x,"receiver_acceptance_source_ref",v.receiver_acceptance_source_ref,c);emit(x,"expense_authority_ref",v.expense_authority_ref,c);emit(x,"cost_terms_ref",v.cost_terms_ref,c);emit(x,"labor_selection",v.labor_selection,c);emit(x,"facility_claim_ref",v.facility_claim_ref,c);emit(x,"accepted_at",v.accepted_at,c);return x.status();});}
void read(const V& v,NamedCargoHandlingTiming& x,const Context& c){fields(v,{"action_ref","action_elapsed_source_ref"});read(get(v,"action_ref"),x.action_ref,c);read(get(v,"action_elapsed_source_ref"),x.action_elapsed_source_ref,c);}
Status emit(ProjectionWriter& w,std::string_view name,const NamedCargoHandlingTiming& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"action_ref",v.action_ref,c);emit(x,"action_elapsed_source_ref",v.action_elapsed_source_ref,c);return x.status();});}
void read(const V& v,CohortCargoHandlingTiming& x,const Context& c){fields(v,{"scale_ref","required_units","credited_units","last_settled_at","participation_source_refs","state"});read(get(v,"scale_ref"),x.scale_ref,c);read(get(v,"required_units"),x.required_units,c);read(get(v,"credited_units"),x.credited_units,c);read(get(v,"last_settled_at"),x.last_settled_at,c);read(get(v,"participation_source_refs"),x.participation_source_refs,c);read(get(v,"state"),x.state,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CohortCargoHandlingTiming& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"scale_ref",v.scale_ref,c);emit(x,"required_units",v.required_units,c);emit(x,"credited_units",v.credited_units,c);emit(x,"last_settled_at",v.last_settled_at,c);emit(x,"participation_source_refs",v.participation_source_refs,c);emit(x,"state",v.state,c);return x.status();});}
void read(const V& v,CargoHandlingExecution& x,const Context& c){fields(v,{"execution_id","terms_ref","business_source","batch_ordinal","phase","started_at","timing","labor_binding_refs","facility_claim_ref","progress_source_revision","work_result_ref","terminal_result_ref","waiting_cause_source_ref"});read(get(v,"execution_id"),x.execution_id,c);read(get(v,"terms_ref"),x.terms_ref,c);read(get(v,"business_source"),x.business_source,c);read(get(v,"batch_ordinal"),x.batch_ordinal,c);read(get(v,"phase"),x.phase,c);read(get(v,"started_at"),x.started_at,c);read(get(v,"timing"),x.timing,c);read(get(v,"labor_binding_refs"),x.labor_binding_refs,c);read(get(v,"facility_claim_ref"),x.facility_claim_ref,c);read(get(v,"progress_source_revision"),x.progress_source_revision,c);read(get(v,"work_result_ref"),x.work_result_ref,c);read(get(v,"terminal_result_ref"),x.terminal_result_ref,c);read(get(v,"waiting_cause_source_ref"),x.waiting_cause_source_ref,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingExecution& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"execution_id",v.execution_id,c);emit(x,"terms_ref",v.terms_ref,c);emit(x,"business_source",v.business_source,c);emit(x,"batch_ordinal",v.batch_ordinal,c);emit(x,"phase",v.phase,c);emit(x,"started_at",v.started_at,c);emit(x,"timing",v.timing,c);emit(x,"labor_binding_refs",v.labor_binding_refs,c);emit(x,"facility_claim_ref",v.facility_claim_ref,c);emit(x,"progress_source_revision",v.progress_source_revision,c);emit(x,"work_result_ref",v.work_result_ref,c);emit(x,"terminal_result_ref",v.terminal_result_ref,c);emit(x,"waiting_cause_source_ref",v.waiting_cause_source_ref,c);return x.status();});}
void read(const V& v,CargoHandlingWorkResult& x,const Context& c){fields(v,{"result_id","execution_ref","terms_ref","service_started_at","service_completed_at","timing_source_ref","labor_coverage_source_refs","facility_use_source_refs","cost_source_refs","rule_ref","rule_version"});read(get(v,"result_id"),x.result_id,c);read(get(v,"execution_ref"),x.execution_ref,c);read(get(v,"terms_ref"),x.terms_ref,c);read(get(v,"service_started_at"),x.service_started_at,c);read(get(v,"service_completed_at"),x.service_completed_at,c);read(get(v,"timing_source_ref"),x.timing_source_ref,c);read(get(v,"labor_coverage_source_refs"),x.labor_coverage_source_refs,c);read(get(v,"facility_use_source_refs"),x.facility_use_source_refs,c);read(get(v,"cost_source_refs"),x.cost_source_refs,c);read(get(v,"rule_ref"),x.rule_ref,c);read(get(v,"rule_version"),x.rule_version,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingWorkResult& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"result_id",v.result_id,c);emit(x,"execution_ref",v.execution_ref,c);emit(x,"terms_ref",v.terms_ref,c);emit(x,"service_started_at",v.service_started_at,c);emit(x,"service_completed_at",v.service_completed_at,c);emit(x,"timing_source_ref",v.timing_source_ref,c);emit(x,"labor_coverage_source_refs",v.labor_coverage_source_refs,c);emit(x,"facility_use_source_refs",v.facility_use_source_refs,c);emit(x,"cost_source_refs",v.cost_source_refs,c);emit(x,"rule_ref",v.rule_ref,c);emit(x,"rule_version",v.rule_version,c);return x.status();});}
void read(const V& v,CargoHandlingResult& x,const Context& c){fields(v,{"result_id","execution_ref","terms_ref","business_source","batch_ordinal","status","occurred_at","work_result_ref","actual_transfer_source_refs","actual_resource_effect_refs","actual_lot_flow_refs","carrier_manifest_change_refs","original_business_result_refs","released_labor_source_refs","released_claim_source_refs","reason_source_ref"});read(get(v,"result_id"),x.result_id,c);read(get(v,"execution_ref"),x.execution_ref,c);read(get(v,"terms_ref"),x.terms_ref,c);read(get(v,"business_source"),x.business_source,c);read(get(v,"batch_ordinal"),x.batch_ordinal,c);read(get(v,"status"),x.status,c);read(get(v,"occurred_at"),x.occurred_at,c);read(get(v,"work_result_ref"),x.work_result_ref,c);read(get(v,"actual_transfer_source_refs"),x.actual_transfer_source_refs,c);read(get(v,"actual_resource_effect_refs"),x.actual_resource_effect_refs,c);read(get(v,"actual_lot_flow_refs"),x.actual_lot_flow_refs,c);read(get(v,"carrier_manifest_change_refs"),x.carrier_manifest_change_refs,c);read(get(v,"original_business_result_refs"),x.original_business_result_refs,c);read(get(v,"released_labor_source_refs"),x.released_labor_source_refs,c);read(get(v,"released_claim_source_refs"),x.released_claim_source_refs,c);read(get(v,"reason_source_ref"),x.reason_source_ref,c);}
Status emit(ProjectionWriter& w,std::string_view name,const CargoHandlingResult& v,const Context& c){return w.object(name,[&](auto& x){emit(x,"result_id",v.result_id,c);emit(x,"execution_ref",v.execution_ref,c);emit(x,"terms_ref",v.terms_ref,c);emit(x,"business_source",v.business_source,c);emit(x,"batch_ordinal",v.batch_ordinal,c);emit(x,"status",v.status,c);emit(x,"occurred_at",v.occurred_at,c);emit(x,"work_result_ref",v.work_result_ref,c);emit(x,"actual_transfer_source_refs",v.actual_transfer_source_refs,c);emit(x,"actual_resource_effect_refs",v.actual_resource_effect_refs,c);emit(x,"actual_lot_flow_refs",v.actual_lot_flow_refs,c);emit(x,"carrier_manifest_change_refs",v.carrier_manifest_change_refs,c);emit(x,"original_business_result_refs",v.original_business_result_refs,c);emit(x,"released_labor_source_refs",v.released_labor_source_refs,c);emit(x,"released_claim_source_refs",v.released_claim_source_refs,c);emit(x,"reason_source_ref",v.reason_source_ref,c);return x.status();});}
template<class T>void read_table(const V& v,OrderedTable<T>& out,const Context& c){const auto* rows=v.as<V::Dict>();if(!rows||rows->size()>CodecVisitor::max_items)fail("CargoHandlingTableRequired");std::vector<typename OrderedTable<T>::Row> parsed;parsed.reserve(rows->size());for(const auto& row:*rows){T value;read(row.second,value,c);parsed.emplace_back(row.first,std::move(value));}if(!out.adopt_checkpoint_rows(std::move(parsed)))fail("DuplicateCargoHandlingTableKey");}
template<class T>Status emit_table(ProjectionWriter& w,const char* name,const OrderedTable<T>& table,const Context& c){return w.object(name,[&](auto& x){for(const auto& row:table.rows()){emit(x,row.first,row.second,c);if(!x.status().ok)break;}return x.status();});}
} // cargo_json
Outcome<CargoHandlingState> decode_cargo_handling_payloads(const BoundaryValue& v,const CommonResourceCodecContext& context){return movement_detail::guard<CargoHandlingState>([&]{cargo_json::fields(v,{"cargo_handling_terms","cargo_handling_executions","cargo_handling_work_results","cargo_handling_results"});CargoHandlingState state;cargo_json::read_table(cargo_json::get(v,"cargo_handling_terms"),state.terms,context);cargo_json::read_table(cargo_json::get(v,"cargo_handling_executions"),state.executions,context);cargo_json::read_table(cargo_json::get(v,"cargo_handling_work_results"),state.work_results,context);cargo_json::read_table(cargo_json::get(v,"cargo_handling_results"),state.results,context);return state;});}
Status emit_cargo_handling_projection(ProjectionWriter& w,const CargoHandlingState& state,const CommonResourceCodecContext& context){cargo_json::emit_table(w,"cargo_handling_terms",state.terms,context);cargo_json::emit_table(w,"cargo_handling_executions",state.executions,context);cargo_json::emit_table(w,"cargo_handling_work_results",state.work_results,context);cargo_json::emit_table(w,"cargo_handling_results",state.results,context);return w.status();}
Outcome<BoundaryValue> export_cargo_handling_payloads(const CargoHandlingState& state,const CommonResourceCodecContext& context){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(w.object("",[&](auto& x){return emit_cargo_handling_projection(x,state,context);}));movement_detail::take(w.finish());return out;});}
}
