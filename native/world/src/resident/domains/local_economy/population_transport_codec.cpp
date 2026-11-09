// 一套严格字段遍历同时提供JSON/检查点；家庭买方purpose只在显式扩展可用。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "population_transport.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "market_service_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_projection.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <type_traits>
namespace hanmo::resident::population_transport_codec {
template<class T>struct EnumNames;
template<>struct EnumNames<HouseholdServiceEventKind>{inline static constexpr std::pair<HouseholdServiceEventKind,const char*> names[]={{HouseholdServiceEventKind::accepted,"accepted"},{HouseholdServiceEventKind::resumed,"resumed"},{HouseholdServiceEventKind::suspended,"suspended"},{HouseholdServiceEventKind::completed,"completed"},{HouseholdServiceEventKind::terminated,"terminated"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<MarketStaffingRule>{inline static constexpr std::pair<MarketStaffingRule,const char*> names[]={{MarketStaffingRule::one_qualified_worker_per_service_slot,"one_qualified_worker_per_service_slot"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PublicMarketLaborReleaseReason>{inline static constexpr std::pair<PublicMarketLaborReleaseReason,const char*> names[]={{PublicMarketLaborReleaseReason::completed,"completed"},{PublicMarketLaborReleaseReason::mutually_cancelled,"mutually_cancelled"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PublicMarketStaffingEventKind>{inline static constexpr std::pair<PublicMarketStaffingEventKind,const char*> names[]={{PublicMarketStaffingEventKind::accepted,"accepted"},{PublicMarketStaffingEventKind::qualification_changed,"qualification_changed"},{PublicMarketStaffingEventKind::stop_at_boundary,"stop_at_boundary"},{PublicMarketStaffingEventKind::completed,"completed"}};static constexpr const auto& rows(){return names;}};
template<class A,class T>auto record(A& a,T& v)->decltype(public_market_record(a,v)){public_market_record(a,v);}
template<class T>struct IsVector:std::false_type{};
template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};
template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};
template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct EnumNames<PopulationPartitionPolicy>{inline static constexpr std::pair<PopulationPartitionPolicy,const char*> names[]={{PopulationPartitionPolicy::balanced,"balanced"},{PopulationPartitionPolicy::labor_eligible_only,"labor_eligible_only"},{PopulationPartitionPolicy::nonlabor_only,"nonlabor_only"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PopulationSelectionPurpose>{inline static constexpr std::pair<PopulationSelectionPurpose,const char*> names[]={{PopulationSelectionPurpose::sea_transfer,"sea_transfer"},{PopulationSelectionPurpose::custody_disposition,"custody_disposition"},{PopulationSelectionPurpose::residence,"residence"},{PopulationSelectionPurpose::private_enrollment,"private_enrollment"},{PopulationSelectionPurpose::crew_service,"crew_service"},{PopulationSelectionPurpose::household_purchase,"household_purchase"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PopulationSelectionPhase>{inline static constexpr std::pair<PopulationSelectionPhase,const char*> names[]={{PopulationSelectionPhase::reserved,"reserved"},{PopulationSelectionPhase::materialized,"materialized"},{PopulationSelectionPhase::released,"released"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PopulationTransportPhase>{inline static constexpr std::pair<PopulationTransportPhase,const char*> names[]={{PopulationTransportPhase::active,"active"},{PopulationTransportPhase::handover_pending,"handover_pending"},{PopulationTransportPhase::released,"released"}};static constexpr const auto& rows(){return names;}};
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,CivilianPopulationRef& v){a("cohort_id",v.cohort_id);a("population_revision",v.population_revision);a("transport_binding_id",v.transport_binding_id);}
template<class A>void record(A& a,PopulationSourceRevision& v){a("cohort_id",v.cohort_id);a("revision",v.revision);a("condition_source_ref",v.condition_source_ref);a("location_binding_ref",v.location_binding_ref);a("active_selection_refs",v.active_selection_refs);}
template<class A>void record(A& a,PopulationSelection& v){a("id",v.id);a("owner_business_ref",v.owner_business_ref);a("source",v.source);a("selected_persons",v.selected_persons);a("selected_labor_eligible",v.selected_labor_eligible);a("partition_policy",v.partition_policy);a("created_tick",v.created_tick);a("release_gate",v.release_gate);a("state",v.state);a("result_transfer_ref",v.result_transfer_ref);a("purpose",v.purpose);}
template<class A>void record(A& a,PopulationSelectionResult& v){a("selection_id",v.selection_id);a("source_cohort_ref",v.source_cohort_ref);a("result_cohort_ref",v.result_cohort_ref);a("transfer_ref",v.transfer_ref);a("condition_ref",v.condition_ref);a("provision_handoff_ref",v.provision_handoff_ref);a("obligation_continuation_refs",v.obligation_continuation_refs);a("location_binding_ref",v.location_binding_ref);}
template<class A>void record(A& a,PopulationTransportBinding& v){a("binding_id",v.binding_id);a("transport_unit_id",v.transport_unit_id);a("transport_cohort_id",v.transport_cohort_id);a("source_population_transfer_ref",v.source_population_transfer_ref);a("labor_allocation_refs",v.labor_allocation_refs);a("registration_source_ref",v.registration_source_ref);a("created_tick",v.created_tick);a("state",v.state);}
template<class A>void record(A& a,PopulationLaborChange& v){a("allocation_id",v.allocation_id);a("before_workers",v.before_workers);a("after_workers",v.after_workers);a("before_through_tick",v.before_through_tick);a("after_through_tick",v.after_through_tick);}
template<class A>void record(A& a,PopulationRevisionSource& v){a("cohort_id",v.cohort_id);a("revision_before",v.revision_before);a("revision_after",v.revision_after);a("actual_source",v.actual_source);a("condition_after",v.condition_after);a("location_after",v.location_after);if(a.selected_version==3)a("labor_changes",v.labor_changes);}
template<class A>void record(A& a,PopulationTransportRelease& v){a("binding_id",v.binding_id);a("actual_handover_source",v.actual_handover_source);a("effective_tick",v.effective_tick);}
template<class A>void record(A& a,PopulationTransportState& v){a("source_revisions",v.source_revisions);a("selections",v.selections);a("materializations",v.materializations);a("transport_bindings",v.transport_bindings);a("revision_sources",v.revision_sources);a("releases",v.releases);a("public_market_labor_origins",v.public_market_labor_origins);a("public_market_labor_transitions",v.public_market_labor_transitions);}
template<class T> BoundaryValue encode(const T& value,Integer selected_version);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&,Integer selected_version);
template<class T> void decode(const BoundaryValue&,T&,std::string_view,Integer selected_version);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&,Integer selected_version);
struct Projector {ProjectionWriter& writer;Integer selected_version;template<class T>void operator()(const char* key,const T& v){project(writer,key,v,selected_version);}};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view,Integer);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view,Integer);
inline Status checked(Status status){if(!status.ok)throw boundary::DecodeFailure{status.error};return status;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;Integer selected_version;
 template<class T>void operator()(const char* key,T& value){checked(json_field(archive,input,key,value,path,selected_version));}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
struct Checkpointer {
    CodecVisitor& codec;Integer selected_version;
    template<class T> void operator()(const char* key,T& v) { checkpoint(codec,key,v,selected_version); }
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T> Status project(ProjectionWriter& w,std::string_view name,const T& value,Integer selected_version){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,name,value);
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return w.reject("InvalidEconomicExactFraction",name);return w.object(name,[&](auto& x){x.field("numerator",encoded.value.numerator);return x.field("denominator",encoded.value.denominator);});}
 else if constexpr(IsMarketWireVariant<T>::value)return w.object(name,[&](auto& x){x.field("tag",static_cast<Integer>(value.index()));return std::visit([&](const auto& v){return project(x,"payload",v,selected_version);},value);});
 else if constexpr(std::is_enum_v<T>){for(const auto& p:EnumNames<T>::rows())if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return w.sequence(name,value,[&](auto& x,const auto& v){return project(x,"",v,selected_version);});
 else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value,selected_version);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:value.rows()){project(x,p.first,p.second,selected_version);if(!x.status().ok)break;}return x.status();});
 else return w.object(name,[&](auto& x){Projector fields{x,selected_version};record(fields,const_cast<T&>(value));return x.status();});
}
template<class T> BoundaryValue encode(const T& value,Integer selected_version){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value,selected_version);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path,Integer selected_version){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,Location>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};else return {true,boundary::decode_location(node,checkpoint_decode_boundary_text(path)),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[selected_version](ProjectionWriter& writer,const T& row){return project(writer,"",row,selected_version);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","population_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_same_v<T,CanonicalFraction>){for(const auto& field:boundary::object(input,path))if(field.first!="numerator"&&field.first!="denominator")boundary::fail_parts("UnsupportedResidentField",{path,"/",field.first});checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction fields;checked(json_field(archive,input,"numerator",fields.numerator,path,selected_version));checked(json_field(archive,input,"denominator",fields.denominator,path,selected_version));auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.finish();}
 else if constexpr(IsMarketWireVariant<T>::value){
  for(const auto& row:boundary::object(input,path))if(row.first!="tag"&&row.first!="payload")boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});
  auto tag=boundary::integer(boundary::field(input,"tag",path),checkpoint_decode_boundary_join({path,"/tag"}));if(tag<0||static_cast<std::size_t>(tag)>=std::variant_size_v<T>)boundary::fail("UnsupportedMarketWireVariant",path);
  checked(archive.defer_duplicate_field_check_to_finish());checked(archive.field("tag",tag));checked(select_market_wire_variant(archive,value,static_cast<std::size_t>(tag)));
  auto status=std::visit([&](auto& row){return json_field(archive,input,"payload",row,path,selected_version);},value);checked(status);return archive.finish();
 }
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:EnumNames<T>::rows())if(name==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,selected_version};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path,Integer selected_version){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_enum_v<T>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));for(const auto& row:EnumNames<T>::rows())if(text==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position),selected_version);},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path,selected_version);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}),selected_version);for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};using Row=typename IsTable<T>::Value;if constexpr(std::is_same_v<Row,PublicMarketLaborOrigin>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},json_scalar<Row>);else return archive.table(name,value,read,json_scalar<Row>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path,selected_version);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path,Integer selected_version){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path,selected_version));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path,Integer selected_version){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path,selected_version));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value,Integer selected_version) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](CodecVisitor& x){EncodedFraction fields;if(!x.reading()){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return x.reject("InvalidEconomicExactFraction");fields=std::move(encoded.value);}x.field("numerator",fields.numerator);x.field("denominator",fields.denominator);if(x.reading()&&x.status().ok){auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)return x.reject("InvalidEconomicExactFraction");value=decoded.value;}return x.status();});
    else if constexpr(IsMarketWireVariant<T>::value)return c.object(name,[&](auto& x){Integer tag=x.reading()?0:static_cast<Integer>(value.index());x.field("tag",tag);if(!x.status().ok)return x.status();if(tag<0||tag>=static_cast<Integer>(std::variant_size_v<T>))return x.reject("UnsupportedMarketWireVariant");if(x.reading()){auto selected=select_market_wire_variant(x,value,static_cast<std::size_t>(tag));if(!selected.ok)return selected;}return std::visit([&](auto& v){return checkpoint(x,"payload",v,selected_version);},value);});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows())if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows())if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v,selected_version);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v,selected_version);});
    else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[&](auto& x,auto& v){return checkpoint(x,"record",v,selected_version);});
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x,selected_version};record(a,value);return x.status();});
}
}
namespace hanmo::resident {namespace {
Status population_codec_profile(const PopulationTransportState& v,bool household,Integer selected){if(selected<1||selected>3||v.version!=selected)return common_failure("UnsupportedPopulationTransportVersion");if(selected!=3)for(const auto& source:v.revision_sources)if(!source.labor_changes.empty())return common_failure("PopulationLaborHistoryProfileRequired",source.actual_source.id);if(!household)for(const auto& s:v.selections.rows())if(s.second.purpose==PopulationSelectionPurpose::household_purchase)return common_failure("HouseholdPurchasePopulationProfileRequired",s.first);return common_success();}
}
BoundaryValue encode_population_transport(const PopulationTransportState& v,bool household,Integer selected){auto checked=population_codec_profile(v,household,selected);if(!checked.ok)boundary::fail(checked.error.original.code,"/civilian_transport_population");return population_transport_codec::encode(v,selected);}
Outcome<PopulationTransportState> decode_population_transport(const BoundaryValue& v,bool household,Integer selected){try{if(selected<1||selected>3)boundary::fail("UnsupportedPopulationTransportVersion","/civilian_transport_population");auto out=population_transport_codec::decode_root<PopulationTransportState>(v,"/civilian_transport_population",selected);out.version=selected;auto checked=population_codec_profile(out,household,selected);if(!checked.ok)return {false,{},checked.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_population_transport(CodecVisitor& c,PopulationTransportState& v,bool household,Integer selected){if(selected<1||selected>3)return c.reject("UnsupportedPopulationTransportVersion");if(c.reading())v.version=selected;else{auto checked=population_codec_profile(v,household,selected);if(!checked.ok)return c.reject(checked.error.original.code);}population_transport_codec::Checkpointer fields{c,selected};population_transport_codec::record(fields,v);if(!c.status().ok)return c.status();auto checked=population_codec_profile(v,household,selected);return checked.ok?c.status():c.reject(checked.error.original.code);}
Status emit_population_transport(ProjectionWriter& w,const PopulationTransportState& v,bool household,Integer selected){auto checked=population_codec_profile(v,household,selected);if(!checked.ok)return w.reject(checked.error.original.code,"/civilian_transport_population");return population_transport_codec::project(w,"",v,selected);}
}
