#include "native_catalog_decode.hpp"
#include "resident/storage/common_codec.hpp"
#include "native_initial_state.hpp"
#include "json_decoded_world_seed.hpp"
#include "catalog_retention.hpp"
#include "world_copy_plan.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
const std::vector<StableId>& NativeCatalogCapabilities::operator()(){
    if(!value_)value_.emplace(boundary::strings(boundary::field(data_,"required_capabilities","data"),"required_capabilities"));
    return *value_;
}

CatalogIdentity copy_native_catalog_identity_for_decode(const CatalogIdentity& source){
 if(checkpoint_boundary_decode_active()){

  checkpoint_decode_boundary_array(std::size_t{0},1);
 }
 return source;
}

void reserve_native_catalog_modules(CatalogIdentity& identity,std::size_t count){
 auto& values=identity.module_versions;if(count<=values.capacity())return;
 if(count>values.max_size()||count>SIZE_MAX/sizeof(std::pair<StableId,StableId>)){
  if(checkpoint_boundary_decode_active())checkpoint_decode_boundary_failure("CheckpointCollectionTooLarge",{"catalog/module_versions"},ErrorKind::internal);
  throw std::length_error("native catalog module capacity");
 }

 checkpoint_decode_boundary_array(count,sizeof(std::pair<StableId,StableId>));
 values.reserve(count);
}
void append_native_catalog_module(CatalogIdentity& identity,std::string_view name,std::string_view version){
 auto& values=identity.module_versions;
 if(values.size()==values.capacity()){
  const auto maximum=values.max_size(),size=values.size();
  if(size==maximum){
   if(checkpoint_boundary_decode_active())checkpoint_decode_boundary_failure("CheckpointCollectionTooLarge",{"catalog/module_versions"},ErrorKind::internal);
   throw std::length_error("native catalog module capacity");
  }
  const auto next=size>maximum-size?maximum:(size?size*2:1);
  reserve_native_catalog_modules(identity,next);
 }
 // Both successful string allocations are preceded by their original-job
 // ordinary charge. The vector's existing strings move without deep copies.
 auto key=checkpoint_decode_boundary_text(name);
 auto value=checkpoint_decode_boundary_text(version);
 values.emplace_back(std::move(key),std::move(value));
}
NativeCatalogDecodeValue& NativeCatalogDecodeValue::operator=(NativeCatalogDecodeValue&& other)noexcept{
 if(this!=&other){this->~NativeCatalogDecodeValue();new(this)NativeCatalogDecodeValue(std::move(other));}return *this;
}
Status NativeCatalogDecodeValue::retain(CheckpointRetentionCensus& census)const{
 if(!census.accounts(ordinary_))ordinary_.retain_control(census);
 // This partial ordinary token is not a universal catalog credit. Its actual
 // source owners must still be enumerated; no untracked pointer is accepted.
 return retain_prepared_catalog(catalog_,census);
}
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog_for_install(const BoundaryValue& source,const StableId& scenario,const CatalogIdentity& expected,CheckpointJobBudget& job,const FieldBattleResourceRead* resources){
 try{
  auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);
  if(!resource.ok)throw checkpoint_job_detail::FailureException{std::move(resource.error)};
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),job);
  ScopedCheckpointBoundaryDecode fields(budget,job,resource.value);
  auto decoded=prepare_native_catalog(source,scenario,expected,resources);
  if(!decoded.ok)return {false,{},std::move(decoded.error)};
  // The exact original alias retains only this catalog's connected charges.
  // All fallible alias preparation precedes the noexcept reservation transfer.
  decoded.value=seal_json_decoded_catalog(std::move(decoded.value),budget,job);
  return decoded;
 }catch(const std::bad_alloc&){throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
  catch(const std::length_error&){throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};}
}
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog_for_install(const BoundaryValue& source,const StableId& scenario,const CatalogIdentity& expected,const FieldBattleResourceRead* resources){
 auto created=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
 if(!created.ok)throw checkpoint_job_detail::FailureException{std::move(created.error)};
 return prepare_native_catalog_for_install(source,scenario,expected,created.value,resources);
}
CheckpointJobResult<NativeCatalogDecodeValue> decode_native_catalog_owned(const BoundaryValue& source,const StableId& scenario,const CatalogIdentity& expected,const CapacityProfile& capacity,CheckpointJobBudget& job,std::shared_ptr<CheckpointMemoryResource> resource,const FieldBattleResourceRead* resources){
 try{
  ScopedCheckpointMemory memory(resource);CodecDecodeBudget budget(capacity,job);
  ScopedCheckpointBoundaryDecode fields(budget,job,resource);
  auto decoded=prepare_native_catalog(source,scenario,expected,resources);
  if(!decoded.ok)return {false,{},job.capture_failure(std::move(decoded.error))};
  NativeCatalogDecodeValue result;result.ordinary_=budget.take_job_reservation();result.catalog_=std::move(decoded.value);
  return {true,std::move(result),{}};
 }catch(checkpoint_job_detail::FailureException& failure){return {false,{},std::move(failure.error)};}
  catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
  catch(const std::length_error&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};}
}
}
