// META 原值继续普通深复制；首成员资源/decoded token 维持实际 owner 生命周期。
#include "resident_profile_copy.hpp"
#include "resident_profile_store.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
namespace hanmo::resident::storage {
CheckpointMemoryCharge reserve_profile_metadata_copy(const ProfileMetadataSnapshot&){return CheckpointMemoryCharge(0);}
ProfileMetadataSnapshot::ProfileMetadataSnapshot(const ProfileMetadataSnapshot& v):owned_memory(reserve_profile_metadata_copy(v)),name_space(v.name_space),generation(v.generation),wire_version(v.wire_version),records(v.records),standard_cards(v.standard_cards),free_cards(v.free_cards),standard_actors(v.standard_actors),free_actors(v.free_actors),acknowledgements(v.acknowledgements){}
ProfileMetadataSnapshot& ProfileMetadataSnapshot::operator=(const ProfileMetadataSnapshot& v){if(this!=&v){ProfileMetadataSnapshot fresh(v);*this=std::move(fresh);}return *this;}
ProfileMetadataSnapshot& ProfileMetadataSnapshot::operator=(ProfileMetadataSnapshot&& v)noexcept{if(this!=&v){this->~ProfileMetadataSnapshot();new(this)ProfileMetadataSnapshot(std::move(v));}return *this;}
void retain_profile_metadata(const ProfileMetadataSnapshot& value,CheckpointRetentionCensus& c){
    c.resource(value.owned_memory.resource());
    if(!c.accounts(value.decoded_memory))value.decoded_memory.retain_control(c);
    c.original_token(value.decoded_memory);
}
}
