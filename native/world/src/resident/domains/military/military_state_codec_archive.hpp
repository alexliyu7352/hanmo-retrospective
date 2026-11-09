// Domain root composition only. Original Common typed-table codec and original
// JSON/projection archives remain the sole encoding/decoding mechanisms.
#pragma once
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <type_traits>
namespace hanmo::resident::military_state_codec {
template<class Archive,class Keys>struct RootArchive {
 Archive& original;
 std::string_view key_error="MilitaryOwnedRowKeyMismatch";
 std::string_view populated_error="MilitaryOwnedRootAlreadyPopulated";
 Status status()const{return original.status();}
 Status reject(std::string_view code,std::string_view path={}){return reject_at(code,path);}
 Status field(std::string_view name,Integer& value){return original.field(name,value);}
 template<class T,class Fields>Status object(std::string_view name,T& value,Fields fields){
  return original.object(name,[&](auto& child){return fields(child,value);});
 }
 template<class T,class Fields>Status table(std::string_view name,OrderedTable<T>& rows,Fields fields){
  static_assert(Keys::template owns<T>,"only this original root's finite row types may use its transfer");
  if(!original.status().ok)return original.status();
  if(original.reading()&&!rows.empty())return reject_at(populated_error,name);
  if(!original.reading())for(const auto& row:rows.rows())if(!Keys{}(row.first,row.second))return reject_at(key_error,name);
  auto transfer=[](auto& destination,auto& source)->Status{
   // This finite root's original implicit value transfer may fail. Discard the
   // complete private candidate; never copy/retry or select another transfer.
   destination.emplace_back(std::move(source));return common_success();
  };
  auto result=[&]{
   if constexpr(std::is_same_v<Archive,CodecVisitor>)
    return common_checkpoint::table(original,name,rows,[&](CodecVisitor& child,const StableId& key,T& row){
     auto visited=fields(child,row);
     if(visited.ok&&!Keys{}(key,row))return child.reject(key_error,name);
     return visited;
    },transfer);
   else if constexpr(std::is_same_v<Archive,ActorFoundationJsonVisitor>)return original.table(name,rows,fields,transfer);
   else return original.table(name,rows,fields);
  }();
  if(!result.ok)return result;
  for(const auto& row:rows.rows())if(!Keys{}(row.first,row.second))return reject_at(key_error,name);
  return original.status();
 }
private:
 Status reject_at(std::string_view code,std::string_view path){
  if constexpr(std::is_same_v<Archive,ActorFoundationProjectionArchive>)
   return original.reject(checkpoint_decode_boundary_text(code),checkpoint_decode_boundary_text(path));
  else return original.reject(code,path);
 }
};
}
