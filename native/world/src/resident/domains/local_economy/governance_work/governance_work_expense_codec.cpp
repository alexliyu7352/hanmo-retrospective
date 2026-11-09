// One field description for JSON, bounded shape and checkpoint. /1 bytes preserved; /2 owns actual termination only; prepayment was removed before admission.
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "governance_work_expense_contract.hpp"
#include "governance_work_fee_policy.hpp"
#include "governance_work_expense_fields.hpp"
#include "resident/kernel/native_catalog_decode.hpp"
#include "../economic_payload_guard.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <type_traits>
namespace hanmo::resident::governance_work_codec {
template<class T>struct EnumNames;
template<class T>struct IsVariant:std::false_type{};
template<class...T>struct IsVariant<std::variant<T...>>:std::true_type{};
template<std::size_t I=0,class A,class T>Status select_variant(A& archive,T& value,std::size_t index){if constexpr(I<std::variant_size_v<T>){if(index==I){auto prepared=archive.template prepare_default_values<std::variant_alternative_t<I,T>>();if(!prepared.ok)return prepared;value.template emplace<I>();return archive.status();}return select_variant<I+1>(archive,value,index);}else return archive.reject("UnsupportedWorkFeeVariant");}
template<class T>struct IsVector:std::false_type{};template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct EnumNames<WorkFeeKind>{inline static constexpr std::pair<WorkFeeKind,const char*> names[]={{WorkFeeKind::startup,"startup"},{WorkFeeKind::measured_work,"measured_work"},{WorkFeeKind::completion,"completion"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkFeeTrigger>{inline static constexpr std::pair<WorkFeeTrigger,const char*> names[]={{WorkFeeTrigger::actual_start,"actual_start"},{WorkFeeTrigger::measured_work,"measured_work"},{WorkFeeTrigger::work_completed,"work_completed"},{WorkFeeTrigger::outcome_applied,"outcome_applied"},{WorkFeeTrigger::report_received,"report_received"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkFeeUnit>{inline static constexpr std::pair<WorkFeeUnit,const char*> names[]={{WorkFeeUnit::occurrence,"occurrence"},{WorkFeeUnit::qualified_work_half_days,"qualified_work_half_days"},{WorkFeeUnit::economic_worker_half_days,"economic_worker_half_days"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkFeeRounding>{inline static constexpr std::pair<WorkFeeRounding,const char*> names[]={{WorkFeeRounding::exact_integer,"exact_integer"},{WorkFeeRounding::cumulative_floor,"cumulative_floor"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkFeePaymentGate>{inline static constexpr std::pair<WorkFeePaymentGate,const char*> names[]={{WorkFeePaymentGate::pay_on_charge,"pay_on_charge"},{WorkFeePaymentGate::accepted_arrears,"accepted_arrears"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkCostCoverageKind>{inline static constexpr std::pair<WorkCostCoverageKind,const char*> names[]={{WorkCostCoverageKind::included,"included"},{WorkCostCoverageKind::separately_payable,"separately_payable"},{WorkCostCoverageKind::quotation_reference,"quotation_reference"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkKind>{inline static constexpr std::pair<WorkKind,const char*> names[]={{WorkKind::patrol,"patrol"},{WorkKind::agronomy_supervision,"agronomy_supervision"},{WorkKind::industry_supervision,"industry_supervision"},{WorkKind::construction_supervision,"construction_supervision"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<EconomicWorkKind>{inline static constexpr std::pair<EconomicWorkKind,const char*> names[]={{EconomicWorkKind::agronomy_operation,"agronomy_operation"},{EconomicWorkKind::industrial_operation,"industrial_operation"},{EconomicWorkKind::facility_construction,"facility_construction"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<SubjectKind>{inline static constexpr std::pair<SubjectKind,const char*> names[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};static constexpr const auto& rows(){return names;}};
template<class A>struct SelectedFields{
 A& archive;
 template<class T>void operator()(const char* name,T& value){
  if constexpr(std::is_same_v<T,CatalogIdentity>)return; // original injected catalog metadata, not wire
  else if constexpr(std::is_same_v<T,WorkAdvanceArrangement>){if(archive.version==1)archive(name,value);}
  else if constexpr(std::is_same_v<T,OrderedTable<GovernanceWorkTerminationSettlement>>){if(archive.version>=2)archive(name,value);}
  else{archive(name,value);if constexpr(std::is_same_v<T,Integer>)if(std::string_view(name)=="version")archive.version=value;}
 }
};
template<class A>void record(A&a,SourceRef&v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A&a,WorldMoment&v){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
template<class A>void record(A&a,MomentSpan&v){a("from",v.from);a("to",v.to);}
template<class A>void record(A&a,MomentOwnerSource&v){a("source_ref",v.source_ref);a("source_schema",v.source_schema);}
template<class A>void record(A&a,SubjectRef&v){a("kind",v.kind);a("id",v.id);}
template<class A>void record(A&a,SourceTermsKey&v){a("key",v.key);}
template<class A>void record(A&a,SourceFactTerms&v){a("source",v.source);}
template<class A>void record(A&a,Revision&v){a("revision",v.revision);}
template<class A>void record(A&a,WorkAuthorizationRef&v){a("program_ref",v.program_ref);a("target_line_id",v.target_line_id);a("terms_revision",v.terms_revision);a("accepted_task_id",v.accepted_task_id);}
template<class A>void record(A&a,WorkProgramAuthorizationScope&v){a("program_ref",v.program_ref);a("target_line_id",v.target_line_id);a("terms_revision",v.terms_revision);}
template<class A>void record(A&a,BudgetExecutionRef&v){a("budget_id",v.budget_id);a("budget_revision",v.budget_revision);a("policy_revision",v.policy_revision);a("approval_fact_id",v.approval_fact_id);}
template<class A>void record(A&a,BudgetPeriod&v){a("period_from_tick",v.period_from_tick);a("period_until_tick",v.period_until_tick);}
template<class A>void record(A&a,WorkRef&v){a("owner_domain",v.owner_domain);a("kind",v.kind);a("work_id",v.work_id);}
template<class A>void record(A&a,WorkActualStartRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkMeasuredReceiptRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkEconomicMeasurementRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkCompletedRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,PatrolSecurityAppliedRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,ProductionImprovementAppliedRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkFacilityActivatedRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkReportReceivedRef&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkFeeLine&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkCostCoverage&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkAdvanceArrangement&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,GovernanceWorkExpenseApproval&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,ProposedWorkExpenseTerms&v){SelectedFields<A> fields{a};governance_work_expense_fields::fields(fields,v);}
template<class A>void record(A&a,GovernanceWorkExpenseTerms&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkChargeKey&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,GovernanceWorkChargeSource&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,GovernanceWorkPaymentSource&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,WorkCommissionTerminal&v){a("task_id",v.task_id);a("accepted_task_ref",v.accepted_task_ref);a("work_id",v.work_id);a("task_terminal_fact_id",v.task_terminal_fact_id);a("source_schema",v.source_schema);}
template<class A>void record(A&a,GovernanceWorkTerminationSettlement&v){governance_work_expense_fields::fields(a,v);}
template<class A>void record(A&a,GovernanceWorkExpenseState&v){SelectedFields<A> fields{a};governance_work_expense_fields::fields(fields,v);}
template<class A>void record(A&a,GovernanceWorkFeePolicy&v){SelectedFields<A> fields{a};governance_work_expense_fields::fields(fields,v);}
// Author /1 retains only the original zero-valued legacy advance shape. /2 omits it.
template<class A>void record(A&a,GovernanceWorkFeeCatalog&v){SelectedFields<A> fields{a};governance_work_expense_fields::fields(fields,v);}
template<class T> BoundaryValue encode(const T& value);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&,Integer);
template<class T> void decode(const BoundaryValue&,T&,std::string_view,Integer);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&,Integer);
struct Projector {ProjectionWriter& writer;Integer version=1;template<class T>void operator()(const char* key,const T& v){project(writer,key,v,version);}};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view,Integer);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view,Integer);
inline Status checked(Status status){if(!status.ok)throw boundary::DecodeFailure{status.error};return status;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;Integer version=1;
 template<class T>void operator()(const char* key,T& value){checked(json_field(archive,input,key,value,path,version));}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
struct Checkpointer {
    CodecVisitor& codec;Integer version=1;
    template<class T> void operator()(const char* key,T& v) { checkpoint(codec,key,v,version); }
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T> Status project(ProjectionWriter& w,std::string_view name,const T& value,Integer version){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,name,value);
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return w.reject("InvalidEconomicExactFraction",name);return w.object(name,[&](auto& x){x.field("numerator",encoded.value.numerator);return x.field("denominator",encoded.value.denominator);});}
 else if constexpr(IsVariant<T>::value)return w.object(name,[&](auto& x){x.field("tag",static_cast<Integer>(value.index()));return std::visit([&](const auto& v){return project(x,"payload",v,version);},value);});
 else if constexpr(std::is_enum_v<T>){for(const auto& p:EnumNames<T>::rows())if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return w.sequence(name,value,[&](auto& x,const auto& v){return project(x,"",v,version);});
 else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value,version);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:value.rows()){project(x,p.first,p.second,version);if(!x.status().ok)break;}return x.status();});
 else return w.object(name,[&](auto& x){Projector fields{x,version};record(fields,const_cast<T&>(value));return x.status();});
}
template<class T> BoundaryValue encode(const T& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value,value.version);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path,Integer version){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,Location>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};else return {true,boundary::decode_location(node,checkpoint_decode_boundary_text(path)),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[version](ProjectionWriter& writer,const T& row){return project(writer,"",row,version);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","governance_work_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_same_v<T,CanonicalFraction>){for(const auto& field:boundary::object(input,path))if(field.first!="numerator"&&field.first!="denominator")boundary::fail_parts("UnsupportedResidentField",{path,"/",field.first});checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction fields;checked(json_field(archive,input,"numerator",fields.numerator,path,version));checked(json_field(archive,input,"denominator",fields.denominator,path,version));auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.finish();}
 else if constexpr(IsVariant<T>::value){
  for(const auto& row:boundary::object(input,path))if(row.first!="tag"&&row.first!="payload")boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});
  auto tag=boundary::integer(boundary::field(input,"tag",path),checkpoint_decode_boundary_join({path,"/tag"}));if(tag<0||static_cast<std::size_t>(tag)>=std::variant_size_v<T>)boundary::fail("UnsupportedWorkFeeVariant",path);
  checked(archive.defer_duplicate_field_check_to_finish());checked(archive.field("tag",tag));checked(select_variant(archive,value,static_cast<std::size_t>(tag)));
  auto status=std::visit([&](auto& row){return json_field(archive,input,"payload",row,path,version);},value);checked(status);return archive.finish();
 }
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:EnumNames<T>::rows())if(name==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,version};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path,Integer version){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_enum_v<T>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));for(const auto& row:EnumNames<T>::rows())if(text==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position),version);},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path,version);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}),version);for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};using Row=typename IsTable<T>::Value;if constexpr(std::is_same_v<Row,GovernanceWorkExpenseTerms>||std::is_same_v<Row,GovernanceWorkChargeSource>||std::is_same_v<Row,GovernanceWorkPaymentSource>||std::is_same_v<Row,GovernanceWorkTerminationSettlement>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},json_scalar<Row>);else return archive.table(name,value,read,json_scalar<Row>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path,version);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path,Integer version){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path,version));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path,Integer version){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path,version));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value,Integer version) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_same_v<T,Location>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::location(x,value);});
    else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](CodecVisitor& x){EncodedFraction fields;if(!x.reading()){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return x.reject("InvalidEconomicExactFraction");fields=std::move(encoded.value);}x.field("numerator",fields.numerator);x.field("denominator",fields.denominator);if(x.reading()&&x.status().ok){auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)return x.reject("InvalidEconomicExactFraction");value=decoded.value;}return x.status();});
    else if constexpr(IsVariant<T>::value)return c.object(name,[&](auto& x){Integer tag=x.reading()?0:static_cast<Integer>(value.index());x.field("tag",tag);if(!x.status().ok)return x.status();if(tag<0||tag>=static_cast<Integer>(std::variant_size_v<T>))return x.reject("UnsupportedWorkFeeVariant");if(x.reading()){auto selected=select_variant(x,value,static_cast<std::size_t>(tag));if(!selected.ok)return selected;}return std::visit([&](auto& v){return checkpoint(x,"payload",v,version);},value);});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows())if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows())if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v,version);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v,version);});
    else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[&](auto& x,auto& v){return checkpoint(x,"record",v,version);});
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x,version};record(a,value);return x.status();});
}
}
namespace hanmo::resident {
namespace {Status work_fee_codec_version(const GovernanceWorkExpenseState&v){if(v.version!=1&&v.version!=2)return common_failure("UnsupportedGovernanceWorkExpenseVersion");if(v.version==1&&!v.termination_settlements.empty())return common_failure("UnexpectedWorkSettlementSchema2");for(const auto&r:v.terms.rows())if(r.second.frozen.advance.advance_qian||(v.version==2&&!r.second.frozen.advance.eligible_fee_line_ids.empty()))return common_failure("ServicePrepaymentRemoved",r.first);return common_success();}}

Outcome<BoundaryValue> encode_governance_work_expenses(const GovernanceWorkExpenseState&v){try{auto valid=work_fee_codec_version(v);if(!valid.ok)return {false,{},valid.error};return {true,governance_work_codec::encode(v),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<GovernanceWorkExpenseState> decode_governance_work_expenses(const BoundaryValue&v){auto unique=validate_economic_payload_fields(v);if(!unique.ok)return {false,{},unique.error};try{auto out=governance_work_codec::decode_root<GovernanceWorkExpenseState>(v,"/governance_work_expense",1);auto valid=work_fee_codec_version(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status visit_governance_work_expenses(CodecVisitor&c,GovernanceWorkExpenseState&v){if(!c.reading()){auto valid=work_fee_codec_version(v);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}governance_work_codec::Checkpointer fields{c,v.version};governance_work_codec::record(fields,v);if(c.status().ok){auto valid=work_fee_codec_version(v);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}return c.status();}
Status emit_governance_work_expenses(ProjectionWriter&w,const GovernanceWorkExpenseState&v){auto valid=work_fee_codec_version(v);if(!valid.ok)return w.reject(valid.error.original.code,valid.error.original.path);return governance_work_codec::project(w,"",v,v.version);}
Outcome<GovernanceWorkFeeCatalog> decode_governance_work_fee_catalog(const BoundaryValue&v,const CatalogIdentity&identity){auto unique=validate_economic_payload_fields(v);if(!unique.ok)return {false,{},unique.error};try{auto out=governance_work_codec::decode_root<GovernanceWorkFeeCatalog>(v,"/governance_work_expense_policies",2);out.identity=copy_native_catalog_identity_for_decode(identity);auto valid=validate_governance_work_fee_catalog(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_governance_work_fee_catalog(const GovernanceWorkFeeCatalog&v){try{auto valid=validate_governance_work_fee_catalog(v);if(!valid.ok)return {false,{},valid.error};return {true,governance_work_codec::encode(v),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status emit_governance_work_fee_catalog(ProjectionWriter&w,const GovernanceWorkFeeCatalog&v){auto valid=validate_governance_work_fee_catalog(v);if(!valid.ok)return w.reject(valid.error.original.code,valid.error.original.path);return governance_work_codec::project(w,"",v,2);}
Status visit_governance_work_fee_catalog(CodecVisitor&c,GovernanceWorkFeeCatalog&v){
 if(!c.reading()){auto valid=validate_governance_work_fee_catalog(v);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}
 governance_work_codec::Checkpointer fields{c,v.version};governance_work_codec::record(fields,v);
 if(c.status().ok){auto valid=validate_governance_work_fee_catalog(v);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}
 return c.status();
}

bool is_governance_work_original_fact_kind(std::string_view kind){return kind=="governance_work_expense_terms"||kind=="governance_work_charge"||kind=="work_service_settled";}
namespace {
template<class Emit>Status with_governance_work_original_payload(const CommonStore&common,const GovernanceWorkExpenseState&state,const FactRecord&head,Emit emit){
 auto valid=work_fee_codec_version(state);if(!valid.ok)return valid;
 const auto*actual=common.state().facts.find(head.id);
 if(!actual||actual->id!=head.id||actual->domain!=head.domain||actual->kind!=head.kind||head.domain!="local_economy"||!is_governance_work_original_fact_kind(head.kind))return common_failure("GovernanceWorkOriginalHeaderRequired",head.id);
 std::size_t matches=0;
 auto check_rows=[&](const auto&rows,std::string_view kind)->Status{for(const auto&row:rows.rows()){
  const auto&id=[&]()->const StableId&{if constexpr(std::is_same_v<std::decay_t<decltype(row.second)>,GovernanceWorkExpenseTerms>)return row.second.expense_terms_id;else return row.second.id;}();
  if(row.first==head.id||id==head.id){if(row.first!=head.id||id!=head.id||head.kind!=kind)return common_failure("GovernanceWorkOriginalPayloadIdentity",head.id);++matches;}
 }return common_success();};
 valid=check_rows(state.terms,"governance_work_expense_terms");if(valid.ok)valid=check_rows(state.charges,"governance_work_charge");if(valid.ok)valid=check_rows(state.termination_settlements,"work_service_settled");if(!valid.ok)return valid;
 if(matches!=1)return common_failure("GovernanceWorkOriginalPayloadCoverage",head.id);
 const auto*terms=state.terms.find(head.id);const auto*charge=state.charges.find(head.id);const auto*settlement=state.termination_settlements.find(head.id);
 if(head.kind=="governance_work_expense_terms"){if(!terms||terms->expense_terms_id!=head.id)return common_failure("GovernanceWorkOriginalPayloadIdentity",head.id);return emit(*terms);}
 if(head.kind=="governance_work_charge"){if(!charge||charge->id!=head.id)return common_failure("GovernanceWorkOriginalPayloadIdentity",head.id);return emit(*charge);}
 if(state.version!=2||!settlement||settlement->id!=head.id)return common_failure("GovernanceWorkOriginalPayloadIdentity",head.id);return emit(*settlement);
}
}
Outcome<Integer> read_governance_work_original_fact_payload_schema(const CommonStore&common,const GovernanceWorkExpenseState&state,const FactRecord&head){auto checked=with_governance_work_original_payload(common,state,head,[](const auto&){return common_success();});if(!checked.ok)return {false,0,checked.error};return {true,1,{}};}
Status visit_governance_work_original_fact_headers(const CommonStore&common,const LocalEconomyState&economy,const GovernanceWorkExpenseState&state,GovernanceWorkOriginalFactHeaderSink&sink){
 auto valid=work_fee_codec_version(state);if(!valid.ok)return valid;
 auto rows=[&](const auto&table)->Status{for(const auto&row:table.rows()){const auto*head=common.state().facts.find(row.first);if(!head)return common_failure("GovernanceWorkOriginalHeaderRequired",row.first);auto schema=read_governance_work_original_fact_payload_schema(common,state,*head);if(!schema.ok)return {false,{},schema.error};}return common_success();};
 valid=rows(state.terms);if(valid.ok)valid=rows(state.charges);if(valid.ok)valid=rows(state.termination_settlements);if(!valid.ok)return valid;
 // Actual cash remains LE's original payload. This only binds the unique Work
 // paid-at child back to that real cash source and its actual earned charge.
 for(const auto&row:state.payments.rows()){
  const auto&child=row.second;const auto*payment=economy.payments.find(row.first);const auto*charge=state.charges.find(child.charge_id);const auto*head=common.state().facts.find(row.first);
  if(child.payment_id!=row.first||!payment||payment->id!=row.first||!charge||charge->id!=child.charge_id||payment->authority_ref!=child.charge_id||!head||head->id!=row.first||head->domain!="local_economy"||head->kind!="obligation_payment")return common_failure("GovernanceWorkOriginalPaymentIdentity",row.first);
  std::size_t matches=0;for(const auto&id:charge->original_payment_ids)if(id==row.first)++matches;if(matches!=1)return common_failure("GovernanceWorkOriginalPaymentCoverage",row.first);
 }
 for(const auto&row:state.charges.rows())for(const auto&id:row.second.original_payment_ids){const auto*payment=state.payments.find(id);if(!payment||payment->charge_id!=row.first)return common_failure("GovernanceWorkOriginalPaymentCoverage",id);}
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="local_economy"&&is_governance_work_original_fact_kind(row.second.kind)){if(row.first!=row.second.id)return common_failure("GovernanceWorkOriginalHeaderIdentity",row.first);auto schema=read_governance_work_original_fact_payload_schema(common,state,row.second);if(!schema.ok)return {false,{},schema.error};}
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="local_economy"&&is_governance_work_original_fact_kind(row.second.kind)){auto appended=sink.append(row.second,1);if(!appended.ok)return appended;}
 return common_success();
}
Status emit_governance_work_original_fact_payload_fields(ProjectionWriter&writer,const CommonStore&common,const GovernanceWorkExpenseState&state,const FactRecord&head){return with_governance_work_original_payload(common,state,head,[&](const auto&payload){governance_work_codec::Projector fields{writer,state.version};governance_work_codec::record(fields,const_cast<std::decay_t<decltype(payload)>&>(payload));return writer.status();});}

}
