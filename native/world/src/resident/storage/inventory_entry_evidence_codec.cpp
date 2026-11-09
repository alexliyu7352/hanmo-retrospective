// 三出口全部沿原Evidence字段表，WorldId/精确时点/optional保原wire形状。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "inventory_entry_evidence_fields.hpp"
#include "common_checkpoint.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
#include <type_traits>
namespace hanmo::resident {namespace entry_codec {
template<class T>struct Vector:std::false_type{};template<class T>struct Vector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct Optional:std::false_type{};template<class T>struct Optional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>Status emit(ProjectionWriter&,const StableId&,const T&,const CommonResourceCodecContext&);
template<class T>void decode(const BoundaryValue&,T&,const StableId&,const CommonResourceCodecContext&);
template<class T>Status checkpoint(CodecVisitor&,const StableId&,T&,const CommonResourceCodecContext&);
struct Project{ProjectionWriter&w;const CommonResourceCodecContext&flags;void reject(const StableId&code,const StableId&path){w.reject(code,path);}template<class T>void operator()(const char*name,const T&v){emit(w,name,v,flags);}};
struct Decode{const BoundaryValue&input;StableId path;const CommonResourceCodecContext&flags;std::array<const char*,16>keys{};std::size_t count=0;void reject(const StableId&code,const StableId&detail){boundary::fail(code,detail.empty()?path:detail);}template<class T>void operator()(const char*name,T&v){if(count>=keys.size())boundary::fail("InventoryEntryFieldLimit",path);keys[count++]=name;decode(boundary::field(input,name,path),v,path+"/"+name,flags);}void finish(){const auto&rows=boundary::object(input,path);if(rows.size()!=count)boundary::fail("InventoryEntryFieldCoverageMismatch",path);for(const auto&row:rows){bool found=false;for(std::size_t i=0;i<count;++i)if(row.first==keys[i])found=true;if(!found)boundary::fail("UnsupportedInventoryEntryField",path+"/"+row.first);}}};
struct Checkpoint{CodecVisitor&c;const CommonResourceCodecContext&flags;void reject(const StableId&code,const StableId&path){c.reject(code,path);}template<class T>void operator()(const char*name,T&v){checkpoint(c,name,v,flags);}};
template<class T>Status emit(ProjectionWriter&w,const StableId&name,const T&v,const CommonResourceCodecContext&flags){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return w.field(name,v);
 else if constexpr(std::is_same_v<T,WorldMoment>){auto encoded=encode_world_moment(v);if(!encoded.ok)return w.reject("InvalidInventoryEntryMoment",name);return w.object(name,[&](auto&out){out.field("half_day_tick",encoded.value.half_day_tick);return out.object("sub_half_day",[&](auto&f){f.field("numerator",encoded.value.sub_half_day.numerator);return f.field("denominator",encoded.value.sub_half_day.denominator);});});}
 else if constexpr(std::is_same_v<T,WorldId>)return w.sequence(name,v,[](auto&out,const auto&byte){return out.field("",static_cast<Integer>(byte));});
 else if constexpr(Vector<T>::value)return w.sequence(name,v,[&](auto&out,const auto&row){return emit(out,"",row,flags);});
 else if constexpr(Optional<T>::value){if(!v)return w.null(name);return emit(w,name,*v,flags);}
 else return w.object(name,[&](auto&out){Project a{out,flags};inventory_entry_fields::fields(a,const_cast<T&>(v),flags);return out.status();});
}
template<class T>void decode(const BoundaryValue&input,T&v,const StableId&path,const CommonResourceCodecContext&flags){
 if constexpr(std::is_same_v<T,StableId>)v=boundary::string(input,path);
 else if constexpr(std::is_same_v<T,Integer>)v=boundary::integer(input,path);
 else if constexpr(std::is_same_v<T,WorldMoment>){boundary::fields(input,{"half_day_tick","sub_half_day"},{},path);const auto&fraction=boundary::field(input,"sub_half_day",path);boundary::fields(fraction,{"numerator","denominator"},{},path+"/sub_half_day");auto at=decode_world_moment({boundary::get_int(input,"half_day_tick",path),{boundary::get_string(fraction,"numerator",path),boundary::get_string(fraction,"denominator",path)}});if(!at.ok)throw boundary::DecodeFailure{at.error};v=std::move(at.value);}
 else if constexpr(std::is_same_v<T,WorldId>){const auto&rows=boundary::array(input,path);if(rows.size()!=v.size())boundary::fail("InvalidWorldId",path);for(std::size_t i=0;i<rows.size();++i){auto byte=boundary::integer(rows[i],path);if(byte<0||byte>255)boundary::fail("InvalidWorldId",path);v[i]=static_cast<std::uint8_t>(byte);}}
 else if constexpr(Vector<T>::value){const auto&rows=boundary::array(input,path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",path);v.clear();for(const auto&row:rows){typename Vector<T>::Value value;decode(row,value,path+"/"+std::to_string(v.size()),flags);v.push_back(std::move(value));}}
 else if constexpr(Optional<T>::value){if(std::holds_alternative<std::monostate>(input.data))v.reset();else{typename Optional<T>::Value value;decode(input,value,path,flags);v=std::move(value);}}
 else{Decode a{input,path,flags};inventory_entry_fields::fields(a,v,flags);a.finish();}
}
template<class T>Status checkpoint(CodecVisitor&c,const StableId&name,T&v,const CommonResourceCodecContext&flags){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return c.field(name,v);
 else if constexpr(std::is_same_v<T,WorldMoment>)return c.object(name,[&](auto&out){EncodedWorldMoment encoded;if(!out.reading()){auto at=encode_world_moment(v);if(!at.ok)return out.reject("InvalidInventoryEntryMoment");encoded=std::move(at.value);}out.field("half_day_tick",encoded.half_day_tick);out.object("sub_half_day",[&](auto&f){f.field("numerator",encoded.sub_half_day.numerator);return f.field("denominator",encoded.sub_half_day.denominator);});if(out.reading()&&out.status().ok){auto at=decode_world_moment(encoded);if(!at.ok)return out.reject("InvalidInventoryEntryMoment");v=std::move(at.value);}return out.status();});
 else if constexpr(std::is_same_v<T,WorldId>){if(c.reading()){std::vector<Integer>bytes;c.sequence(name,bytes,[](auto&out,auto&byte){return out.field("byte",byte);});if(!c.status().ok)return c.status();if(bytes.size()!=v.size())return c.reject("InvalidWorldId");for(std::size_t i=0;i<bytes.size();++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidWorldId");v[i]=static_cast<std::uint8_t>(bytes[i]);}return c.status();}return c.sequence_view(name,v.size(),[&](auto&out,std::uint64_t i){Integer byte=v[static_cast<std::size_t>(i)];return out.field("byte",byte);});}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[&](auto&out,auto&row){Checkpoint a{out,flags};inventory_entry_fields::fields(a,row,flags);return out.status();});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[&](auto&out,auto&row){if constexpr(std::is_same_v<std::decay_t<decltype(row)>,StableId>)return out.field("value",row);else{Checkpoint a{out,flags};inventory_entry_fields::fields(a,row,flags);return out.status();}});
 else return c.object(name,[&](auto&out){Checkpoint a{out,flags};inventory_entry_fields::fields(a,v,flags);return out.status();});
}
}
Status emit_inventory_entry_evidence_fields(ProjectionWriter&w,const InventoryEntryEvidence&value,const CommonResourceCodecContext&flags){entry_codec::Project archive{w,flags};inventory_entry_fields::fields(archive,const_cast<InventoryEntryEvidence&>(value),flags);return w.status();}
Status emit_inventory_entry_evidence(ProjectionWriter&w,const StableId&name,const InventoryEntryEvidence&value,const CommonResourceCodecContext&flags){return w.object(name,[&](auto&out){return emit_inventory_entry_evidence_fields(out,value,flags);});}
BoundaryValue encode_inventory_entry_evidence(const InventoryEntryEvidence&value,const CommonResourceCodecContext&flags){BoundaryValue output;BoundaryProjectionWriter writer(output);auto status=emit_inventory_entry_evidence(writer,"",value,flags);if(status.ok)status=writer.finish();if(!status.ok)throw boundary::DecodeFailure{status.error};return output;}
Outcome<InventoryEntryEvidence> decode_inventory_entry_evidence(const BoundaryValue&input,const CommonResourceCodecContext&flags){try{InventoryEntryEvidence out;entry_codec::decode(input,out,"/inventory_entry_evidence",flags);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status visit_inventory_entry_evidence(CodecVisitor&c,InventoryEntryEvidence&value){const CommonResourceCodecContext flags{c.item_instance_refs_enabled,c.lot_property_refs_enabled,c.domain_activity_reservation_refs_enabled,c.carrier_location_enabled};entry_codec::Checkpoint archive{c,flags};inventory_entry_fields::fields(archive,value,flags);return c.status();}
}
