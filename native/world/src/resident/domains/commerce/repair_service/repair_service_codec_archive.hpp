#pragma once
#include "repair_service_codec.hpp"
#include "repair_service_fields.hpp"
#include "resident/domains/growth/growth_fixed_service_codec.hpp"
#include "resident/domains/craft/repair_fields.hpp"
#include "resident/domains/craft/craft_codec_archive.hpp"
#include "resident/kernel/world_timeline_codec_fields.hpp"
namespace hanmo::resident::repair_service_codec {
using craft_codec::Vector;using craft_codec::Optional;using craft_codec::Table;
template<class E>struct Names;
template<>struct Names<RepairServiceFeeTrigger>{inline static constexpr std::pair<RepairServiceFeeTrigger,const char*> rows[]={{RepairServiceFeeTrigger::actual_start,"actual_start"},{RepairServiceFeeTrigger::repair_completed,"repair_completed"}};};
template<class C,class T>void record(C&c,T&v){
 if constexpr(std::is_same_v<T,RepairPolicyRef>||std::is_same_v<T,ItemConditionHead>)repair_fields::fields(c,v);
 else if constexpr(std::is_same_v<T,OriginalUniqueItemTransferEffect>)unique_item_transfer_fields::fields(c,v);
 else if constexpr(std::is_same_v<T,ObligationRef>)obligation_ref_fields(c,v);
 else repair_service_fields::fields(c,v);
}
template<class T>inline constexpr bool original=std::is_same_v<T,Owner>||std::is_same_v<T,ReservationRef>||std::is_same_v<T,WorldMoment>||std::is_same_v<T,ActorConditionHead>||std::is_same_v<T,SourceRef>||std::is_same_v<T,MomentOwnerSource>||std::is_same_v<T,CargoSelection>||std::is_same_v<T,FacilitySlotRef>||std::is_same_v<T,LotPropertiesRef>||std::is_same_v<T,CraftFacilityClaimUntil>;
template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);template<class T>void decode(const BoundaryValue&,T&,std::string_view);template<class T>Status visit(CodecVisitor&,std::string_view,T&);
struct Project{ProjectionWriter&w;Status result=common_success();void actor_route(const char*n,const LifeActorRoute&v,const StableId&actor){if(result.ok)result=w.object(n,[&](auto&out){return emit_life_actor_route_fields(out,v,actor);});}template<class T>void operator()(const char*n,const T&v){if(result.ok)result=emit(w,n,v);}};
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
struct Decode{
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;std::size_t count=0;
 template<class T>void operator()(const char* name,T& value){if(count==32)boundary::fail("RepairServiceFieldCountOverflow",path);++count;craft_codec::checked(json_field(archive,input,name,value,path));}
 void actor_route(const char* name,LifeActorRoute& value,const StableId& actor){
  if(count==32)boundary::fail("RepairServiceFieldCountOverflow",path);++count;boundary::field(input,name,path);
  craft_codec::checked(archive.object(name,[&](auto& child){
   auto emit_original=[&](ProjectionWriter& out,const LifeActorRoute& row){return emit_life_actor_route_fields(out,row,actor);};
   if(ScopedCheckpointJsonDecode::current())return child.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget){return decode_life_actor_route(node,actor,budget);},emit_original);
   return child.foreign_record(value,[&](const BoundaryValue& node){return decode_life_actor_route(node,actor);},emit_original);
  }));
 }
 void finish(){if(boundary::object(input,path).size()!=count)boundary::fail("RepairServiceFieldCoverageMismatch",path);craft_codec::checked(archive.finish());}
};
struct Visit{CodecVisitor&c;Status result=common_success();void actor_route(const char*n,LifeActorRoute&v,const StableId&actor){if(result.ok)result=c.object(n,[&](auto&out){return visit_life_actor_route(out,v,actor);});}template<class T>void operator()(const char*n,T&v){if(result.ok)result=visit(c,n,v);}};
template<class T>Status emit(ProjectionWriter&w,std::string_view name,const T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,v);
 else if constexpr(original<T>)return craft_codec::emit(w,name,v);
 else if constexpr(std::is_enum_v<T>){for(const auto&row:Names<T>::rows)if(v==row.first)return w.field(name,StableId{row.second});return w.reject("UnknownRepairServiceEnum",name);}
 else if constexpr(Vector<T>::value)return w.sequence(name,v,[](auto&out,const auto&row){return emit(out,"",row);});
 else if constexpr(Optional<T>::value){if(v)return emit(w,name,*v);return w.null(name);}
 else if constexpr(Table<T>::value)return w.keyed_dictionary(name,{},[&](auto&out){for(const auto&row:v.rows()){auto s=emit(out,row.first,row.second);if(!s.ok)return s;}return out.status();});
 else return w.object(name,[&](auto&out){Project f{out};record(f,const_cast<T&>(v));return f.result.ok?out.status():f.result;});
}
template<class T>inline constexpr bool exact_table_row=std::is_same_v<T,RepairServiceOfferSource>||std::is_same_v<T,RepairServiceAssentSource>||std::is_same_v<T,RepairServiceContractSource>||std::is_same_v<T,RepairCustodyReceivedSource>||std::is_same_v<T,RepairServiceChargeSource>||std::is_same_v<T,RepairServicePaymentSource>||std::is_same_v<T,RepairServiceTerminationSettlementSource>||std::is_same_v<T,RepairItemReturnedSource>;
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(original<T>||std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return craft_codec::json_value(archive,input,value,path);
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(input,{path});craft_codec::checked(archive.field("",named));for(const auto& row:Names<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("UnknownRepairServiceEnum",path);}
 else{craft_codec::checked(archive.defer_duplicate_field_check_to_finish());Decode fields{archive,input,path};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(node,child_path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("CraftUnsignedIntegerOutOfRange",child_path);craft_codec::checked(archive.field(name,number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(node,{child_path});craft_codec::checked(archive.field(name,named));for(const auto& row:Names<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("UnknownRepairServiceEnum",child_path);}
 else if constexpr(Vector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,craft_codec::index_path(child_path,position));},craft_codec::json_scalar<typename Vector<T>::Value>);}
 else if constexpr(Optional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},craft_codec::json_scalar<typename Optional<T>::Value>);
 else if constexpr(Table<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateRepairServiceRecord",{child_path,"/",rows[position].first});return result;};using Row=typename Table<T>::Value;if constexpr(exact_table_row<Row>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();});else return archive.table(name,value,read);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,craft_codec::json_scalar<T>);craft_codec::checked(json_value(archive,input,value,path));craft_codec::checked(archive.finish());}
template<class T>Status visit(CodecVisitor&c,std::string_view name,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(original<T>)return craft_codec::checkpoint(c,name,v);
 else if constexpr(std::is_enum_v<T>){StableId named;if(!c.reading()){bool found=false;for(const auto&row:Names<T>::rows)if(v==row.first){named=row.second;found=true;}if(!found)return c.reject("UnknownRepairServiceEnum",name);}c.field(name,named);if(!c.status().ok)return c.status();for(const auto&row:Names<T>::rows)if(named==row.second){if(c.reading())v=row.first;return c.status();}return c.reject("UnknownRepairServiceEnum",name);}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[](auto&out,auto&row){return visit(out,"item",row);});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[](auto&out,auto&row){return visit(out,"value",row);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,name,v,[](auto&out,auto&row){return visit(out,"record",row);});
 else return c.object(name,[&](auto&out){Visit f{out};record(f,v);return f.result.ok?out.status():f.result;});
}
template<class T>Outcome<BoundaryValue> encode(const T&v){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=emit(w,"",v);if(s.ok)s=w.finish();return s.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},s.error};}
}
