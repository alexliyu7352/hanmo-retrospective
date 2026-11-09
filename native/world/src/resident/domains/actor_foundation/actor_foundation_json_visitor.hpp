// 本域冷JSON与binary共享typed字段；reader只借输入树，绝不复制整BoundaryValue。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <string_view>
namespace hanmo::resident {
// A selected physical row borrows its actual input key and payload. Selectors
// may validate then skip a row, but never synthesize or own a Boundary wrapper.
struct ActorFoundationJsonTableRow {
 const StableId* key=nullptr;
 const BoundaryValue* payload=nullptr;
};
class ActorFoundationJsonVisitor {
public:
 explicit ActorFoundationJsonVisitor(BoundaryValue& output,bool scalar=false):json_depth_(false),output_(&output),scalar_(scalar){output.data=scalar?BoundaryValue::Data{std::monostate{}}:BoundaryValue::Data{BoundaryValue::Dict{}};}
 explicit ActorFoundationJsonVisitor(const BoundaryValue& input,bool scalar=false):input_(&input),scalar_(scalar){}
 ActorFoundationJsonVisitor(const ActorFoundationJsonVisitor&)=delete;
 ActorFoundationJsonVisitor(ActorFoundationJsonVisitor&&)=delete;
 bool reading()const{return input_!=nullptr;}
 // Explicit original-schema order/range mode. The borrowed dictionary is
 // unchanged; take/finish use the same visited-key owner over this exact slice.
 // Default callers retain the original unordered whole-object behavior.
 Status ordered_fields(std::size_t first=0,std::size_t last=std::numeric_limits<std::size_t>::max()){
  if(!result_.ok)return result_;
  if(!reading())return result_;
  if(scalar_||foreign_||touched_||ordered_)return reject("CheckpointJsonOrderedFieldModeRequired");
  const auto* dict=input_->as<BoundaryValue::Dict>();if(!dict)return reject("InvalidResidentObject");
  if(last==std::numeric_limits<std::size_t>::max())last=dict->size();
  if(first>last||last>dict->size())return reject("CheckpointJsonFieldRangeMismatch");
  ordered_=true;field_first_=field_cursor_=first;field_limit_=last;return result_;
 }
 bool has_next_field(std::string_view name)const noexcept{
  const auto* dict=input_?input_->as<BoundaryValue::Dict>():nullptr;
  return ordered_&&dict&&field_cursor_<field_limit_&&(*dict)[field_cursor_].first==name;
 }
 // Some original domain formats validate the first value and their field
 // coverage before rejecting duplicate physical keys. Select that ordering
 // explicitly on this read object, before its first field; the same finish()
 // still rejects every duplicate and the default ordering stays unchanged.
 Status defer_duplicate_field_check_to_finish(){
  if(!result_.ok)return result_;
  if(!reading()||scalar_||foreign_||touched_)return reject("CheckpointJsonDuplicatePolicyOrder");
  deferred_duplicate_check_=true;return result_;
 }
 // Explicit original open-object schema only. Unknown values remain borrowed
 // and untouched; duplicate, required-field, scalar and size checks still run.
 Status allow_unvisited_fields_on_finish(){
  if(!result_.ok)return result_;
  if(!reading()||scalar_||foreign_||touched_||ordered_)return reject("CheckpointJsonOpenFieldPolicyOrder");
  allow_unvisited_fields_=true;return result_;
 }
 // Borrowed query for original strict-schema adapters, with no second visited-key collector.
 bool has_written_field(std::string_view name)const noexcept{if(!output_||scalar_)return false;const auto* rows=output_->as<BoundaryValue::Dict>();if(!rows)return false;for(const auto& row:*rows)if(row.first==name)return true;return false;}
 // Borrow the sole original visited-key list; no second set or copied keys.
 bool has_visited_field(std::string_view name)const noexcept{for(const auto& field:used_)if(field==name)return true;return false;}
 Status status()const{return result_;}
 Status reject(std::string_view code,std::string_view path="actor_foundation"){if(result_.ok){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->failure(code,{path},ErrorKind::rule);result_=common_failure(StableId(code),StableId(path));}return result_;}
 Status finish(){if(!result_.ok||scalar_||foreign_)return result_;const auto* dict=node().as<BoundaryValue::Dict>();if(!dict)return reject("InvalidResidentObject");if(dict->size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge");for(std::size_t n=0;n<dict->size();++n)for(std::size_t prior=0;prior<n;++prior)if((*dict)[n].first==(*dict)[prior].first)return reject("DuplicateResidentField",(*dict)[n].first);if(reading()&&!allow_unvisited_fields_&&used_.size()!=(ordered_?field_limit_-field_first_:dict->size()))return reject("UnsupportedResidentField");return result_;}
 template<class T>Status field(std::string_view name,T& value){if(!begin_fields())return result_;if(reading()){const auto* n=scalar_?input_:take(name);if(!n)return result_;const auto* p=n->as<T>();if(!p)return reject("InvalidActorFoundationJsonScalar",name);if constexpr(std::is_same_v<T,std::string>){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->text(value,*p);else value=*p;}else value=*p;}else if(scalar_)output_->data=value;else put(name,BoundaryValue{value});return result_;}
 // Unsigned fields are schema-selected; negative, float, bool and text reject.
 Status unsigned_field(std::string_view name,std::uint64_t& value){
  if(!begin_fields())return result_;
  if(reading()){
   const auto* n=scalar_?input_:take(name);if(!n)return result_;
   if(const auto* p=n->as<std::uint64_t>())value=*p;
   else if(const auto* p=n->as<Integer>();p&&*p>=0)value=static_cast<std::uint64_t>(*p);
   else return reject("InvalidActorFoundationUnsignedInteger",name);
  }else if(scalar_)*output_=BoundaryValue{value};else put(name,BoundaryValue{value});
  return result_;
 }
 template<class F>Status object(std::string_view name,F visit){if(!begin_fields())return result_;if(reading()){const auto* n=take(name);if(!n)return result_;ActorFoundationJsonVisitor child(*n);absorb(visit(child));absorb(child.finish());}else{BoundaryValue value;ActorFoundationJsonVisitor child(value);absorb(visit(child));absorb(child.finish());if(result_.ok)put(name,std::move(value));}return result_;}
 template<class T,class F>Status optional(std::string_view name,std::optional<T>& value,F visit,bool scalar_elements=is_scalar<T>(),bool omitted=false){if(!begin_fields())return result_;if(reading()){if(omitted){if(!ordered_)return reject("CheckpointJsonOmittedOrderedFieldsRequired",name);if(!has_next_field(name)){value.reset();return result_;}}const auto* n=take(name);if(!n)return result_;return decode_optional_node(*n,name,value,std::move(visit),scalar_elements,omitted);}else if(!value){if(!omitted)put(name,{});}else{BoundaryValue out;ActorFoundationJsonVisitor child(out);child.scalar_=scalar_elements;absorb(visit(child,*value));absorb(child.finish());if(result_.ok)put(name,std::move(out));}return result_;}
 // Read an actual nullable root through the same optional payload decoder.
 // Ordinary commands need no job; an active job keeps its actual JSON owner.
 template<class T,class F>Status root_optional(std::optional<T>& value,std::string_view name,F visit,bool scalar_elements=is_scalar<T>()){
  if(!result_.ok)return result_;
  if(scalar_||foreign_||touched_||ordered_)return reject("ForeignActorFoundationRecordNotExclusive",name);
  if(!reading())return reject("CheckpointJsonRootOptionalReaderRequired",name);
  if(!ScopedCheckpointJsonDecode::current()&&(current_checkpoint_memory()||ScopedCheckpointBoundaryDecode::current()))return reject("CheckpointJsonRootOptionalContextRequired",name);
  foreign_=true;return decode_optional_node(*input_,name,value,std::move(visit),scalar_elements,false);
 }
 // Exposes the same original default-construction precharge to explicit
 // domain variants/optionals. No object is constructed and writers are inert.
 template<class T>Status prepare_default_values(std::uint64_t count=1){
  if(!result_.ok)return result_;
  require_read_context();
  if(reading())if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<T>(count);
  return result_;
 }
 // Existing callers keep scalar elements for the same original scalar T.
 // Dynamic {value:...} arrays explicitly select object elements instead.
 template<class T,class Allocator,class F>Status sequence(std::string_view name,std::vector<T,Allocator>& rows,F visit,bool scalar_elements=is_scalar<T>()){
  if(!reading())return write_sequence(name,rows,std::move(visit),scalar_elements);
  if(!begin_fields())return result_;
  const auto* n=take(name);if(!n)return result_;
  return decode_sequence_node(*n,name,rows,std::move(visit),scalar_elements);
 }
 // Consume an actual raw array through the same named-sequence decode body.
 // This whole-root read is exclusive. Ordinary no-job command decoding uses
 // the same body; an active decode/job must retain its actual JSON owner.
 template<class T,class Allocator,class F>Status root_sequence(std::vector<T,Allocator>& rows,std::string_view name,F visit,bool scalar_elements=is_scalar<T>()){
  if(!result_.ok)return result_;
  if(scalar_||foreign_||touched_||ordered_)return reject("ForeignActorFoundationRecordNotExclusive",name);
  if(!reading())return reject("CheckpointJsonRootSequenceReaderRequired",name);
  if(!ScopedCheckpointJsonDecode::current()&&(current_checkpoint_memory()||ScopedCheckpointBoundaryDecode::current()))return reject("CheckpointJsonRootSequenceContextRequired",name);
  foreign_=true;return decode_sequence_node(*input_,name,rows,std::move(visit),scalar_elements);
 }
 // The original sequence writer loop also accepts borrowed filtered ranges.
 // Range may itself be const; mutable original sequence callbacks stay mutable.
 template<class Range,class F>Status write_sequence(std::string_view name,Range& rows,F visit,bool scalar_elements){
  if(!begin_fields())return result_;if(reading())return reject("CheckpointWriteSequenceOnReader",name);
  if(rows.size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);
  BoundaryValue::Array output;output.reserve(rows.size());
  for(auto& row:rows){BoundaryValue value;ActorFoundationJsonVisitor child(value);child.scalar_=scalar_elements;absorb(visit(child,row));absorb(child.finish());if(!begin_fields())return result_;output.push_back(std::move(value));}
  put(name,BoundaryValue{std::move(output)});return result_;
 }
 // Decode a physical sub-array into the same private typed candidate tail.
 // On failure the enclosing domain candidate is discarded, never published.
 template<class T,class Allocator,class F>Status append_sequence(std::string_view name,std::vector<T,Allocator>& rows,F visit){
  static_assert(std::is_nothrow_move_constructible_v<T>,"JSON append requires non-copying noexcept row transfer");
  return append_sequence(name,rows,std::move(visit),[](std::vector<T,Allocator>& destination,T& source)->Status{destination.emplace_back(std::move(source));return common_success();});
 }
 template<class T,class Allocator,class F,class Transfer>Status append_sequence(std::string_view name,std::vector<T,Allocator>& rows,F visit,Transfer&& transfer){
  if(!begin_fields())return result_;if(!reading())return reject("CheckpointReadSequenceOnWriter",name);
  const auto* n=take(name);if(!n)return result_;const auto* values=n->as<BoundaryValue::Array>();if(!values)return reject("InvalidResidentArray",name);
  const auto before=rows.size();if(before>CodecVisitor::max_items||values->size()>CodecVisitor::max_items-before)return reject("CheckpointCollectionTooLarge",name);
  const auto total=before+values->size();if(!validate_append_allocator(rows,total,name).ok)return result_;
  if(total>rows.capacity()){
   std::vector<T,Allocator> candidate(rows.get_allocator());if(auto* budget=ScopedCheckpointJsonDecode::current())budget->reserve_block(candidate,total);candidate.reserve(total);
   for(auto& source:rows){const auto size=candidate.size(),capacity=candidate.capacity();const auto* storage=candidate.data();auto moved=transfer(candidate,source);if(!moved.ok){absorb(std::move(moved));return result_;}if(candidate.size()!=size+1||candidate.capacity()!=capacity||candidate.data()!=storage)return reject("CheckpointTypedTransferShapeMismatch",name);}
   rows.swap(candidate);
  }
  // The required block already exists. Only fresh rows are default-constructed;
  // no previous row is moved/copied by vector resize on this path.
  if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<T>(values->size());
  rows.resize(total);
  for(std::size_t i=0;i<values->size()&&result_.ok;++i){ActorFoundationJsonVisitor child((*values)[i]);child.scalar_=is_scalar<T>();absorb(visit(child,rows[before+i]));absorb(child.finish());}
  return result_;
 }
 // Adopt an already decoded optional record into its sole owning candidate.
 // No synthetic array or wire field is introduced by this memory operation.
 template<class T,class Allocator>Status append_decoded_value(std::vector<T,Allocator>& rows,T&& value,std::string_view record_kind){
  static_assert(std::is_nothrow_move_constructible_v<T>,"JSON append requires non-copying noexcept row transfer");
  return append_decoded_value(rows,std::move(value),record_kind,[](std::vector<T,Allocator>& destination,T& source)->Status{destination.emplace_back(std::move(source));return common_success();});
 }
 // A named owner transfer may copy only its known fallible fields through their
 // actual allocator. Every old row uses that same explicit transfer on growth.
 // Failure poisons the enclosing private decode candidate; moved sources must
 // never be retried or published. No unknown-T copy fallback is permitted.
 template<class T,class Allocator,class Transfer>Status append_decoded_value(std::vector<T,Allocator>& rows,T&& value,std::string_view record_kind,Transfer&& transfer){
  if(!result_.ok)return result_;if(!reading())return reject("CheckpointReadSequenceOnWriter",record_kind);
  if(rows.size()>=CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",record_kind);
  if(!validate_append_allocator(rows,rows.size()+1,record_kind).ok)return result_;
  for(auto& row:rows)if(&row==&value)return reject("CheckpointDecodedAppendAliasesSource",record_kind);
  auto append=[&](std::vector<T,Allocator>& destination,T& source)->Status{
   const auto size=destination.size(),capacity=destination.capacity();const auto* storage=destination.data();if(size>=capacity)return reject("CheckpointTypedTransferCapacityRequired",record_kind);
   auto moved=transfer(destination,source);if(!moved.ok){absorb(std::move(moved));return result_;}
   if(destination.size()!=size+1||destination.capacity()!=capacity||destination.data()!=storage)return reject("CheckpointTypedTransferShapeMismatch",record_kind);return result_;
  };
  if(rows.size()==rows.capacity()){
   std::vector<T,Allocator> candidate(rows.get_allocator());if(auto* budget=ScopedCheckpointJsonDecode::current())budget->reserve_block(candidate,rows.size()+1);candidate.reserve(rows.size()+1);
   for(auto& source:rows)if(!append(candidate,source).ok)return result_;
   if(!append(candidate,value).ok)return result_;rows.swap(candidate);
  }else if(!append(rows,value).ok)return result_;
  return result_;
 }
 // 新同job表读使用一次typed rows加一次原adopt；未知可抛行迁移必须由owner给第四参数。
 template<class T,class F>Status table(std::string_view name,OrderedTable<T>& rows,F visit,bool scalar_elements=false){
  if(reading()&&ScopedCheckpointJsonDecode::current()){
   if constexpr(std::is_nothrow_move_constructible_v<typename OrderedTable<T>::Row>)return table(name,rows,std::move(visit),[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},scalar_elements);
   else ScopedCheckpointJsonDecode::current()->fail("CheckpointJsonTableTransferRequired",name);
  }
  return legacy_table(name,rows,std::move(visit),scalar_elements);
 }
 template<class T,class F,class Transfer,std::enable_if_t<!std::is_same_v<std::decay_t<Transfer>,bool>,int> =0>Status table(std::string_view name,OrderedTable<T>& rows,F visit,Transfer&& transfer,bool scalar_elements=false){
  auto* budget=ScopedCheckpointJsonDecode::current();if(!reading()||!budget)return legacy_table(name,rows,std::move(visit),scalar_elements);
  if(!begin_fields())return result_;const auto* n=take(name);if(!n)return result_;
  return decode_table_node(*n,name,rows,std::move(visit),std::forward<Transfer>(transfer),scalar_elements);
 }

 // A raw dictionary is the whole physical table, not a synthesized named
 // child. This consumes the same original decoder/adoption as table().
 // A live same-job scope is mandatory; this is not a legacy/budget bypass.
 template<class T,class F>Status root_table(OrderedTable<T>& rows,std::string_view name,F visit,bool scalar_elements=false){
  if(!result_.ok)return result_;
  if(!reading()||!ScopedCheckpointJsonDecode::current())return reject("CheckpointJsonRootTableContextRequired",name);
  if constexpr(std::is_nothrow_move_constructible_v<typename OrderedTable<T>::Row>)return root_table(rows,name,std::move(visit),[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},scalar_elements);
  else ScopedCheckpointJsonDecode::current()->fail("CheckpointJsonTableTransferRequired",name);
 }
 template<class T,class F,class Transfer,std::enable_if_t<!std::is_same_v<std::decay_t<Transfer>,bool>,int> =0>Status root_table(OrderedTable<T>& rows,std::string_view name,F visit,Transfer&& transfer,bool scalar_elements=false){
  if(!result_.ok)return result_;
  if(scalar_||foreign_||touched_)return reject("ForeignActorFoundationRecordNotExclusive",name);
  if(!reading()||!ScopedCheckpointJsonDecode::current())return reject("CheckpointJsonRootTableContextRequired",name);
  foreign_=true;
  return decode_table_node(*input_,name,rows,std::move(visit),std::forward<Transfer>(transfer),scalar_elements);
 }
 // Select original rows from an actual dictionary or array root. The selector
 // runs once per physical row, immediately before that selected payload is
 // decoded, so domain validation/error order is not changed by a count pass.
 // select(row) -> Outcome<optional<ActorFoundationJsonTableRow>>;
 // visit(child, typed_value, actual_key) -> Status. Nullopt means validated skip.
 template<class Range,class T,class Select,class F>Status root_table_rows(OrderedTable<T>& rows,std::string_view name,Select select,F visit,bool scalar_elements=false){
  if(!result_.ok)return result_;
  if constexpr(std::is_nothrow_move_constructible_v<typename OrderedTable<T>::Row>)return root_table_rows<Range>(rows,name,std::move(select),std::move(visit),[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},scalar_elements);
  else return reject("CheckpointJsonTableTransferRequired",name);
 }
 template<class Range,class T,class Select,class F,class Transfer,std::enable_if_t<!std::is_same_v<std::decay_t<Transfer>,bool>,int> =0>Status root_table_rows(OrderedTable<T>& rows,std::string_view name,Select select,F visit,Transfer&& transfer,bool scalar_elements=false){
  static_assert(std::is_same_v<Range,BoundaryValue::Dict>||std::is_same_v<Range,BoundaryValue::Array>,"selected JSON table requires its actual dictionary or array");
  if(!result_.ok)return result_;
  if(scalar_||foreign_||touched_||ordered_)return reject("ForeignActorFoundationRecordNotExclusive",name);
  if(!reading())return reject("CheckpointJsonRootTableReaderRequired",name);
  if(!ScopedCheckpointJsonDecode::current()&&(current_checkpoint_memory()||ScopedCheckpointBoundaryDecode::current()))return reject("CheckpointJsonRootTableContextRequired",name);
  const auto* physical=input_->as<Range>();
  if(!physical){if constexpr(std::is_same_v<Range,BoundaryValue::Dict>)return reject("InvalidResidentObject",name);else return reject("InvalidResidentArray",name);}
  foreign_=true;
  return decode_table_rows(*physical,name,rows,std::move(select),std::move(visit),std::forward<Transfer>(transfer),scalar_elements,false);
 }

 // The original record decoder/emitter exclusively owns this whole object.
 // No regular fields may precede/follow it; no borrowed input tree is copied.
 template<class T,class Decode,class Emit>Status foreign_record(T& value,Decode decode,Emit emit){
  if(!result_.ok)return result_;if(scalar_||foreign_||touched_)return reject("ForeignActorFoundationRecordNotExclusive");foreign_=true;
  if constexpr(std::is_invocable_v<Decode,const BoundaryValue&,ScopedCheckpointJsonDecode&>){if(reading()&&!ScopedCheckpointJsonDecode::current()){result_=common_failure("CheckpointJsonForeignContextRequired");result_.error.kind=ErrorKind::internal;return result_;}}
  if(reading()){auto decoded=[&](){if constexpr(std::is_invocable_v<Decode,const BoundaryValue&,ScopedCheckpointJsonDecode&>){return decode(*input_,*ScopedCheckpointJsonDecode::current());}else{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->fail("CheckpointJsonForeignRecordBudgetRequired");return decode(*input_);}}();if(!decoded.ok){absorb({false,{},decoded.error});return result_;}value=std::move(decoded.value);}
  else{BoundaryProjectionWriter out(*output_);absorb(out.object("",[&](ProjectionWriter& w){return emit(w,static_cast<const T&>(value));}));if(result_.ok)absorb(out.finish());}
  return result_;
 }
private:
 template<class T,class F>Status decode_optional_node(const BoundaryValue& node,std::string_view name,std::optional<T>& value,F visit,bool scalar_elements,bool reject_null){
  if(node.as<std::monostate>()){if(reject_null)return reject("InvalidActorFoundationJsonScalar",name);value.reset();return result_;}
  if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<T>();
  value.emplace();ActorFoundationJsonVisitor child(node);child.scalar_=scalar_elements;absorb(visit(child,*value));absorb(child.finish());return result_;
 }
 template<class T,class Allocator,class F>Status decode_sequence_node(const BoundaryValue& node,std::string_view name,std::vector<T,Allocator>& rows,F visit,bool scalar_elements){
  const auto* a=node.as<BoundaryValue::Array>();if(!a)return reject("InvalidResidentArray",name);
  if(a->size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);
  if(auto* budget=ScopedCheckpointJsonDecode::current()){
   budget->collection(name,a->size());budget->reserve_block(rows,a->size());budget->default_values<T>(a->size());
   std::vector<T,Allocator> fresh(rows.get_allocator());fresh.reserve(a->size());
   fresh.resize(a->size());rows.swap(fresh);
  }else rows.resize(a->size());
  for(std::size_t i=0;i<a->size()&&result_.ok;++i){ActorFoundationJsonVisitor child((*a)[i]);child.scalar_=scalar_elements;absorb(visit(child,rows[i]));absorb(child.finish());}
  return result_;
 }
 template<class T,class F,class Transfer>Status decode_table_node(const BoundaryValue& node,std::string_view name,OrderedTable<T>& rows,F visit,Transfer&& transfer,bool scalar_elements){
  auto* budget=ScopedCheckpointJsonDecode::current();
  if(!reading()||!budget)return reject("CheckpointJsonTableContextRequired",name);
  const auto* dict=node.as<BoundaryValue::Dict>();if(!dict)return reject("InvalidResidentObject",name);
  auto select=[](const BoundaryValue::Dict::value_type& item)->Outcome<std::optional<ActorFoundationJsonTableRow>>{return {true,ActorFoundationJsonTableRow{&item.first,&item.second},{}};};
  return decode_table_rows(*dict,name,rows,select,[&](auto& child,T& value,const StableId&){return visit(child,value);},std::forward<Transfer>(transfer),scalar_elements,true);
 }
 template<class Range,class T,class Select,class F,class Transfer>Status decode_table_rows(const Range& physical,std::string_view name,OrderedTable<T>& rows,Select select,F visit,Transfer&& transfer,bool scalar_elements,bool all_rows_selected){
  auto* budget=ScopedCheckpointJsonDecode::current();using Row=typename OrderedTable<T>::Row;
  if(budget)budget->collection(name,physical.size());else if(physical.size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);
  CheckpointVector<Row> decoded;if(budget){budget->reserve_block(decoded,physical.size());if(all_rows_selected)budget->default_values<Row>(physical.size());}
  // This is actual reserved scratch capacity, not fabricated rows. Only the
  // selected rows construct defaults, keys and payloads; adoption uses size().
  decoded.reserve(physical.size());
  for(const auto& item:physical){
   auto selected=select(item);if(!selected.ok){absorb({false,{},std::move(selected.error)});return result_;}if(!selected.value)continue;
   const auto& source=*selected.value;if(!source.key||!source.payload)return reject("CheckpointJsonSelectedTableRowRequired",name);
   if(budget&&!all_rows_selected)budget->default_values<Row>();
   decoded.emplace_back();auto& row=decoded.back();if(budget)budget->text(row.first,*source.key);else row.first=*source.key;
   ActorFoundationJsonVisitor child(*source.payload,scalar_elements);absorb(visit(child,row.second,*source.key));absorb(child.finish());if(!result_.ok)return result_;
   for(std::size_t i=0;i+1<decoded.size();++i)if(decoded[i].first==row.first)return reject("DuplicateCheckpointKey",name);
  }
  OrderedTable<T> candidate;auto adopted=candidate.adopt_checkpoint_rows(std::move(decoded),std::forward<Transfer>(transfer));if(!adopted.ok){absorb(std::move(adopted));return result_;}rows=std::move(candidate);return result_;
 }
 template<class T,class F>Status legacy_table(std::string_view name,OrderedTable<T>& rows,F visit,bool scalar_elements){if(reading()&&ScopedCheckpointJsonDecode::current())ScopedCheckpointJsonDecode::current()->fail("CheckpointJsonLegacyTableBypass",name);if(!begin_fields())return result_;if(reading()){const auto* n=take(name);if(!n)return result_;const auto* dict=n->as<BoundaryValue::Dict>();if(!dict)return reject("InvalidResidentObject",name);if(dict->size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);OrderedTable<T> candidate;for(const auto& item:*dict){T row;ActorFoundationJsonVisitor child(item.second,scalar_elements);absorb(visit(child,row));absorb(child.finish());if(!begin_fields())return result_;if(!candidate.insert(item.first,std::move(row)))return reject("DuplicateCheckpointKey",name);}rows=std::move(candidate);}else{BoundaryValue::Dict output;output.reserve(rows.size());for(const auto& item:rows.rows()){BoundaryValue out;ActorFoundationJsonVisitor child(out,scalar_elements);absorb(visit(child,const_cast<T&>(item.second)));absorb(child.finish());if(!begin_fields())return result_;output.emplace_back(item.first,std::move(out));}put(name,BoundaryValue{std::move(output)});}return result_;}
 CheckpointJsonVisitDepth json_depth_;
 const BoundaryValue* input_=nullptr;BoundaryValue* output_=nullptr;bool scalar_=false,foreign_=false,touched_=false,deferred_duplicate_check_=false,ordered_=false,allow_unvisited_fields_=false;std::size_t field_first_=0,field_cursor_=0,field_limit_=0;Status result_=common_success();CheckpointVector<std::string_view> used_;
 template<class T,class Allocator>Status validate_append_allocator(const std::vector<T,Allocator>& rows,std::size_t total,std::string_view kind){
  if(total>rows.max_size()||total>std::numeric_limits<std::size_t>::max()/sizeof(T))return reject("CheckpointCollectionTooLarge",kind);
  if(auto* budget=ScopedCheckpointJsonDecode::current())budget->collection(kind,total);
  const auto job=current_checkpoint_memory();
  if constexpr(std::is_same_v<Allocator,CheckpointAllocator<T>>){
   const auto resource=rows.get_allocator().resource();
   if(bool(job)!=bool(resource)||(job&&!job->shares_job(*resource)))return reject("CheckpointCandidateMemoryJobMismatch",kind);
  }else if(job&&!ScopedCheckpointJsonDecode::current())return reject("CheckpointJsonAppendReservationRequired",kind);
  return result_;
 }
 // The depth constructor already guards initial active-memory reads. Recheck
 // before defaults/fields in case a job or Boundary scope changed afterward.
 // A real job failure must unwind before a caller constructs a default value.
 void require_read_context()const{
  if(reading()&&!ScopedCheckpointJsonDecode::current()&&(current_checkpoint_memory()||ScopedCheckpointBoundaryDecode::current()))
   checkpoint_decode_boundary_failure("CheckpointJsonReadContextRequired",{"actor_foundation"},ErrorKind::internal);
 }
 bool begin_fields(){if(!result_.ok)return false;require_read_context();if(foreign_){reject("ForeignActorFoundationRecordNotExclusive");return false;}touched_=true;return true;}
 template<class T>static constexpr bool is_scalar(){return std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,std::uint64_t>;}
 const BoundaryValue& node()const{return input_?*input_:*output_;}
 void absorb(Status s){if(result_.ok&&!s.ok)result_=std::move(s);}
 const BoundaryValue* take(std::string_view key){const auto* rows=input_->as<BoundaryValue::Dict>();if(!rows){reject("InvalidResidentObject",key);return nullptr;}const auto first=ordered_?field_first_:std::size_t{0};const auto limit=ordered_?field_limit_:rows->size();if(ordered_&&(field_cursor_>=limit||(*rows)[field_cursor_].first!=key)){reject(field_cursor_>=limit?"MissingResidentField":"ResidentFieldOrderMismatch",key);return nullptr;}const BoundaryValue* found=nullptr;std::string_view actual_key;for(std::size_t i=first;i<limit;++i)if(const auto& row=(*rows)[i];row.first==key){if(found){if(deferred_duplicate_check_)continue;reject("DuplicateResidentField",key);return nullptr;}found=&row.second;actual_key=row.first;}if(!found){reject("MissingResidentField",key);return nullptr;}for(const auto& used:used_)if(used==actual_key){reject("DuplicateActorFoundationVisit",key);return nullptr;}used_.push_back(actual_key);if(ordered_)++field_cursor_;return found;}
 void put(std::string_view key,BoundaryValue value){auto* rows=output_->as<BoundaryValue::Dict>();if(!rows){reject("InvalidResidentObject",key);return;}for(const auto& row:*rows)if(row.first==key){reject("DuplicateActorFoundationVisit",key);return;}rows->emplace_back(StableId(key),std::move(value));}
};
}
