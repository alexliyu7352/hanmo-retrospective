// 原Moment/CanonicalFraction与Common位置编解码复用原owner；不压成tick或double。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "economic_work_sources.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_projection.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <type_traits>
namespace hanmo::resident::economic_work_codec {
template<class T>struct EnumNames;
template<class T>struct IsVector:std::false_type{};
template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};
template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};
template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct EnumNames<EconomicWorkKind>{inline static constexpr std::pair<EconomicWorkKind,const char*> names[]={{EconomicWorkKind::agronomy_operation,"agronomy_operation"},{EconomicWorkKind::industrial_operation,"industrial_operation"},{EconomicWorkKind::facility_construction,"facility_construction"}};static constexpr const auto& rows(){return names;}};
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,WorldMoment& v){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
template<class A>void record(A& a,ProductionSegmentSource& v){a("unit_id",v.unit_id);a("cycle_source_ref",v.cycle_source_ref);a("from",v.from);a("to",v.to);a("condition_source_refs",v.condition_source_refs);a("asset_use_source_refs",v.asset_use_source_refs);a("participation_refs",v.participation_refs);a("actual_supply_refs",v.actual_supply_refs);a("batch_capacity",v.batch_capacity);a("labor_rate_numerator",v.labor_rate_numerator);a("labor_rate_denominator",v.labor_rate_denominator);}
template<class A>void record(A& a,AccumulatedProductionState2& v){a("cycle_source_ref",v.cycle_source_ref);a("unit_id",v.unit_id);a("start_tick",v.start_tick);a("due_tick",v.due_tick);a("last_accounted_at",v.last_accounted_at);a("active_input_source_refs",v.active_input_source_refs);a("capacity_unit_sum",v.capacity_unit_sum);a("labor_unit_numerator",v.labor_unit_numerator);a("labor_denominator",v.labor_denominator);a("original_labor_remainder_ref",v.original_labor_remainder_ref);a("last_segment_key",v.last_segment_key);a("settlement_receipt_ref",v.settlement_receipt_ref);a("active_input",v.active_input);a("active_labor_allocation_refs",v.active_labor_allocation_refs);a("active_anchor",v.active_anchor);}
template<class A>void record(A& a,EconomicProductionProfile& v){a("unit_id",v.unit_id);a("work_kind",v.work_kind);a("rule_version",v.rule_version);a("worker_halfdays_per_batch",v.worker_halfdays_per_batch);a("labor_denominator",v.labor_denominator);}
template<class A>void record(A& a,EconomicWorkCatalog& v){a("production_profiles",v.production_profiles);}
template<class A>void record(A& a,AppliedProductionSegment& v){a("id",v.id);a("source",v.source);a("close_cause",v.close_cause);a("capacity_units_added",v.capacity_units_added);a("labor_numerator_added",v.labor_numerator_added);a("actual_worker_halfdays",v.actual_worker_halfdays);a("actual_labor_allocation_refs",v.actual_labor_allocation_refs);a("actual_anchor",v.actual_anchor);}
template<class A>void record(A& a,EconomicLaborRemainderSource& v){a("id",v.id);a("unit_id",v.unit_id);a("value",v.value);a("previous_source_ref",v.previous_source_ref);a("settlement_receipt_ref",v.settlement_receipt_ref);}
template<class A>void record(A& a,EconomicProductionSettlementSource& v){a("receipt_id",v.receipt_id);a("cycle_source_ref",v.cycle_source_ref);a("capacity_limit",v.capacity_limit);a("labor_limit",v.labor_limit);a("actual_worker_halfdays",v.actual_worker_halfdays);a("labor_remainder",v.labor_remainder);a("remainder_before_ref",v.remainder_before_ref);a("remainder_after_ref",v.remainder_after_ref);}
template<class A>void record(A& a,ScenarioOriginRef& v){a("catalog_digest",v.catalog_digest);a("scenario_id",v.scenario_id);a("row_key",v.row_key);a("initial_tick",v.initial_tick);}
template<class A>void record(A& a,EconomicPeriodOpening& v){a("source_id",v.source_id);a("period_id",v.period_id);a("opening_at",v.opening_at);a("through_tick",v.through_tick);a("existing_activity_ids",v.existing_activity_ids);a("existing_action_ids",v.existing_action_ids);}
template<class A>void record(A& a,EconomicWorkSourcesState& v){a("cycles",v.cycles);a("active_cycles_by_unit",v.active_cycles_by_unit);a("segments",v.segments);a("labor_remainders",v.labor_remainders);a("settlements",v.settlements);if(a.calendar_v3){a("calendar_origin",v.calendar_origin);a("period_openings",v.period_openings);}}
template<class T> BoundaryValue encode(const T& value);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&);
template<class T> void decode(const BoundaryValue&,T&,std::string_view);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&);
struct Projector {ProjectionWriter& writer;bool calendar_v3=false;Status result=common_success();Status status()const{return result.ok?writer.status():result;}template<class T>void operator()(const char* key,const T& v){if(result.ok)result=project(writer,key,v);}};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
inline Status checked(Status value){if(!value.ok)throw boundary::DecodeFailure{value.error};return value;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;bool calendar_v3=false;std::size_t count=0;
 template<class T>void operator()(const char* key,T& value){++count;checked(json_field(archive,input,key,value,path));}
 void finish(){const auto& rows=boundary::object(input,path);if(rows.size()!=count)boundary::fail("EconomicWorkFieldCoverageMismatch",path);for(std::size_t i=0;i<rows.size();++i){if(!archive.has_visited_field(rows[i].first))boundary::fail_parts("UnsupportedResidentField",{path,"/",rows[i].first});for(std::size_t j=0;j<i;++j)if(rows[i].first==rows[j].first)boundary::fail_parts("DuplicateResidentField",{path,"/",rows[i].first});}checked(archive.finish());}
};
template<class T>inline constexpr bool exact_table_row=std::is_same_v<T,AccumulatedProductionState2>||std::is_same_v<T,AppliedProductionSegment>||std::is_same_v<T,EconomicLaborRemainderSource>||std::is_same_v<T,EconomicProductionSettlementSource>||std::is_same_v<T,EconomicPeriodOpening>;
struct Checkpointer {
    CodecVisitor& codec;bool calendar_v3=false;Status result=common_success();Status status()const{return result.ok?codec.status():result;}
    template<class T> void operator()(const char* key,T& v) { if(result.ok)result=checkpoint(codec,key,v); }
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T> Status project(ProjectionWriter& w,std::string_view name,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,name,value);
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return w.reject("InvalidEconomicExactFraction",name);return w.object(name,[&](auto& x){x.field("numerator",encoded.value.numerator);return x.field("denominator",encoded.value.denominator);});}
 else if constexpr(std::is_enum_v<T>){for(const auto& p:EnumNames<T>::rows())if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return w.sequence(name,value,[](auto& x,const auto& v){return project(x,"",v);});
 else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:value.rows()){auto status=project(x,p.first,p.second);if(!status.ok)return status;}return x.status();});
 else return w.object(name,[&](auto& x){Projector fields{x};record(fields,const_cast<T&>(value));return fields.status();});
}
template<class T> BoundaryValue encode(const T& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,Location>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};else return {true,boundary::decode_location(node,checkpoint_decode_boundary_text(path)),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[](ProjectionWriter& writer,const T& row){return project(writer,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","economic_work_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_same_v<T,CanonicalFraction>){for(const auto& field:boundary::object(input,path))if(field.first!="numerator"&&field.first!="denominator")boundary::fail_parts("UnsupportedResidentField",{path,"/",field.first});checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction fields;checked(json_field(archive,input,"numerator",fields.numerator,path));checked(json_field(archive,input,"denominator",fields.denominator,path));auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.finish();}
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:EnumNames<T>::rows())if(name==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_enum_v<T>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));for(const auto& row:EnumNames<T>::rows())if(text==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position));},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};using Row=typename IsTable<T>::Value;if constexpr(exact_table_row<Row>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},json_scalar<Row>);else return archive.table(name,value,read,json_scalar<Row>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_same_v<T,Location>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::location(x,value);});
    else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](CodecVisitor& x){EncodedFraction fields;if(!x.reading()){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return x.reject("InvalidEconomicExactFraction");fields=std::move(encoded.value);}x.field("numerator",fields.numerator);x.field("denominator",fields.denominator);if(x.reading()&&x.status().ok){auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)return x.reject("InvalidEconomicExactFraction");value=decoded.value;}return x.status();});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows())if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows())if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v);});
    else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[&](auto& x,auto& v){return checkpoint(x,"record",v);});
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x};record(a,value);return a.status();});
}
}
namespace hanmo::resident {
namespace {
Status selected_economic_source_schema(const EconomicWorkSourcesState& v,EconomicWorkSourceSchema schema){
 if(schema==EconomicWorkSourceSchema::production_sources2)return !v.calendar_origin&&v.period_openings.empty()?common_success():common_failure("UnselectedEconomicCalendarTail");
 if(schema!=EconomicWorkSourceSchema::production_calendar3)return common_failure("UnsupportedEconomicWorkSourceSchema");
 return v.calendar_origin?common_success():common_failure("EconomicCalendarOriginRequired");
}
}
Outcome<BoundaryValue> encode_economic_work_sources(const EconomicWorkSourcesState& v,EconomicWorkSourceSchema schema){BoundaryValue out;BoundaryProjectionWriter w(out);auto checked=emit_economic_work_sources(w,v,schema);if(checked.ok)checked=w.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(out),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
Outcome<EconomicWorkSourcesState> decode_economic_work_sources(const BoundaryValue& v,EconomicWorkSourceSchema schema){try{if(schema!=EconomicWorkSourceSchema::production_sources2&&schema!=EconomicWorkSourceSchema::production_calendar3)boundary::fail("UnsupportedEconomicWorkSourceSchema","/economic_work_sources");ActorFoundationJsonVisitor archive(v);economic_work_codec::checked(archive.defer_duplicate_field_check_to_finish());economic_work_codec::checked(archive.prepare_default_values<EconomicWorkSourcesState>());EconomicWorkSourcesState out;economic_work_codec::Decoder fields{archive,v,"/economic_work_sources",schema==EconomicWorkSourceSchema::production_calendar3};economic_work_codec::record(fields,out);fields.finish();auto checked=selected_economic_source_schema(out,schema);if(!checked.ok)return {false,{},checked.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_economic_work_catalog(const EconomicWorkCatalog& v){try{return {true,economic_work_codec::encode(v),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<EconomicWorkCatalog> decode_economic_work_catalog(const BoundaryValue& v){try{auto out=economic_work_codec::decode_root<EconomicWorkCatalog>(v,"/economic_work_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_economic_work_sources(CodecVisitor& c,EconomicWorkSourcesState& v,EconomicWorkSourceSchema schema){if(schema!=EconomicWorkSourceSchema::production_sources2&&schema!=EconomicWorkSourceSchema::production_calendar3)return c.reject("UnsupportedEconomicWorkSourceSchema");if(!c.reading()){auto valid=selected_economic_source_schema(v,schema);if(!valid.ok)return c.reject(valid.error.original.code);}economic_work_codec::Checkpointer fields{c,schema==EconomicWorkSourceSchema::production_calendar3};economic_work_codec::record(fields,v);if(!fields.status().ok)return fields.status();if(c.status().ok){auto valid=selected_economic_source_schema(v,schema);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();}
Status emit_economic_work_sources(ProjectionWriter& w,const EconomicWorkSourcesState& v,EconomicWorkSourceSchema schema){auto valid=selected_economic_source_schema(v,schema);if(!valid.ok)return w.reject(valid.error.original.code);return w.object("",[&](auto& x){economic_work_codec::Projector fields{x,schema==EconomicWorkSourceSchema::production_calendar3};economic_work_codec::record(fields,const_cast<EconomicWorkSourcesState&>(v));return fields.status();});}
}
