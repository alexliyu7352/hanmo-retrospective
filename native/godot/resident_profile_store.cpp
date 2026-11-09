// 本机记录与空成果分区同一规范代；此codec没有文件I/O，全部介质仍由原唯一worker承担。
#include "resident_profile_store.hpp"
#include "resident_profile_merge.hpp"
#include "resident_profile_copy.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <set>

namespace hanmo::resident::storage {
namespace {
constexpr char magic[]="HM-META-PROFILE";
template<class T>Outcome<T> failure(const char* code){return {false,{},profile_admission_failure(code).error};}
void integer(Bytes& out,std::uint64_t v,unsigned width){for(unsigned i=0;i<width;++i)out.push_back(std::uint8_t(v>>(8*i)));}
void string(Bytes& out,const StableId& v){integer(out,v.size(),2);out.insert(out.end(),v.begin(),v.end());}
void name_space(Bytes& out,const ProfileNamespace& n){string(out,n.profile_uid);integer(out,static_cast<unsigned>(n.purpose),1);integer(out,n.preview_session_id?1:0,1);if(n.preview_session_id)string(out,*n.preview_session_id);}
bool limits_valid(const ProfileStoreLimits& l){const auto max=std::uint64_t(std::numeric_limits<Integer>::max());return l.max_records>0&&l.max_records<=65536&&l.max_file_bytes>0&&l.max_file_bytes<=max&&l.admission.max_capsule_bytes>0;}
// 本片producer真实关闭，故持久记录也只能为空基线local_created；未知完整成果格式保原件拒读。
Status snapshot_valid(const ProfileMetadataSnapshot& s,const ProfileStoreLimits& l){
    if(!limits_valid(l))return profile_admission_failure("InvalidProfileStoreCapacity");
    auto n=validate_profile_namespace(s.name_space);if(!n.ok)return n;
    if(s.generation<1||s.records.empty()||s.records.size()>l.max_records)return profile_admission_failure("InvalidProfileMetadataGeneration");
    std::set<WorldId> worlds;StableId prior;
    for(const auto& r:s.records){auto v=validate_admission_record(r,l.admission);if(!v.ok)return v;if(!same_profile_namespace(r.capsule.name_space,s.name_space))return profile_admission_failure("AdmissionNamespaceMismatch");if(r.origin!=AdmissionOrigin::local_created||!r.capsule.baseline.empty()||!r.allowed_history.empty())return profile_admission_failure("UnsupportedProfileDeliveryProjection",{},ErrorKind::unsupported_capability);if(r.record_generation!=0)return profile_admission_failure("UnsupportedAdmissionMutation",{},ErrorKind::unsupported_capability);if(!prior.empty()&&!(prior<r.capsule.admission_id))return profile_admission_failure("NonCanonicalAdmissionRecords");prior=r.capsule.admission_id;if(!worlds.insert(r.capsule.source_world_id).second)return profile_admission_failure("DuplicateLocalCreatedWorld");}
    return {true,{},{}};
}
class Reader {
public:
    explicit Reader(const Bytes& value):b(value){}
    bool take(std::size_t n){if(n>b.size()-at)return okay=false;at+=n;return true;}
    std::uint64_t number(unsigned width){if(!take(width))return 0;std::uint64_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint64_t(b[at-width+i])<<(8*i);return n;}
    StableId string(){const auto n=number(2);if(!okay||!n||n>256||!take(n)){okay=false;return {};}return StableId(reinterpret_cast<const char*>(b.data()+at-n),static_cast<std::size_t>(n));}
    ProfileNamespace name_space(){ProfileNamespace n;n.profile_uid=string();const auto purpose=number(1),present=number(1);if(purpose>2||present>1){okay=false;return n;}n.purpose=static_cast<ProfilePurpose>(purpose);if(present)n.preview_session_id=string();if(!validate_profile_namespace(n).ok)okay=false;return n;}
    const Bytes& b;std::size_t at=0;bool okay=true;
};
}
const AdmissionRecord* find_profile_admission(const ProfileMetadataSnapshot& s,const StableId& id)noexcept{for(const auto& r:s.records)if(r.capsule.admission_id==id)return &r;return nullptr;}
Outcome<Bytes> encode_profile_metadata(const ProfileMetadataSnapshot& s,const ProfileStoreLimits& l){try{
    if(s.wire_version==2)return encode_profile_metadata_complete(s,l);if(s.wire_version!=1)return failure<Bytes>("UnsupportedProfileMetadata");
    if(!s.standard_cards.empty()||!s.free_cards.empty()||!s.standard_actors.empty()||!s.free_actors.empty()||!s.acknowledgements.empty())return failure<Bytes>("UnsupportedProfileMetadataCollections");
    auto valid=snapshot_valid(s,l);if(!valid.ok)return {false,{},valid.error};
    std::uint64_t measured=sizeof(magic)-1+2+2+8+2+s.name_space.profile_uid.size()+2+4+20+32;if(s.name_space.preview_session_id)measured+=2+s.name_space.preview_session_id->size();
    std::vector<Bytes> capsules;capsules.reserve(s.records.size()); // 仍构造真实独立胶囊，不再累计工作内存估算。
    for(const auto& r:s.records){auto b=encode_slot_admission_capsule(r.capsule,l.admission);if(!b.ok)return b;const auto add=8+b.value.size()+1+8+4;if(add>l.max_file_bytes||measured>l.max_file_bytes-add)return failure<Bytes>("ProfileMetadataCapacityExceeded");measured+=add;capsules.push_back(std::move(b.value));}
    if(measured>l.max_file_bytes)return failure<Bytes>("ProfileMetadataCapacityExceeded");
    Bytes out;out.reserve(static_cast<std::size_t>(measured));out.insert(out.end(),magic,magic+sizeof(magic)-1);integer(out,1,2);integer(out,0,2);integer(out,s.generation,8);name_space(out,s.name_space);integer(out,s.records.size(),4);
    for(std::size_t i=0;i<s.records.size();++i){const auto& r=s.records[i];integer(out,capsules[i].size(),8);out.insert(out.end(),capsules[i].begin(),capsules[i].end());integer(out,static_cast<unsigned>(r.origin),1);integer(out,r.record_generation,8);integer(out,0,4);}
    // 两kind的standard/free集合及ack各有真实空计数；非空分支尚未选provider，reader明确拒绝。
    for(unsigned i=0;i<5;++i)integer(out,0,4);
    const auto hash=profile_sha256(out.data(),out.size());
    out.insert(out.end(),hash.begin(),hash.end());
    return {true,std::move(out),{}};
}catch(const std::bad_alloc&){return failure<Bytes>("ProfileMetadataResourceExhausted");}}
ProfileMetadataDecoded decode_profile_metadata(const Bytes& bytes,const ProfileStoreLimits& l){try{
    if(bytes.size()>=sizeof(magic)+1&&bytes[sizeof(magic)-1]==2&&bytes[sizeof(magic)]==0)return decode_profile_metadata_complete(bytes,l);
    ProfileMetadataDecoded out;if(!limits_valid(l)){out.code="InvalidProfileStoreCapacity";return out;}if(bytes.size()>l.max_file_bytes){out.code="ProfileMetadataCapacityExceeded";return out;}
    if(bytes.size()<sizeof(magic)-1+4+8+4+4+20+32){out.code="TruncatedProfileMetadata";out.physical_corruption=true;return out;}
    if(!std::equal(magic,magic+sizeof(magic)-1,bytes.begin())){out.code="UnsupportedProfileMetadata";return out;}
    const auto digest=profile_sha256(bytes.data(),bytes.size()-32);if(!std::equal(digest.begin(),digest.end(),bytes.end()-32)){out.code="ProfileMetadataDigestMismatch";out.physical_corruption=true;return out;}
    Reader r(bytes);r.take(sizeof(magic)-1);if(r.number(2)!=1||r.number(2)!=0){out.code="UnsupportedProfileMetadata";return out;}const auto generation=r.number(8);if(!generation||generation>std::uint64_t(std::numeric_limits<Integer>::max())){out.code="InvalidProfileMetadataGeneration";return out;}out.snapshot.generation=static_cast<Integer>(generation);out.snapshot.name_space=r.name_space();const auto count=r.number(4);if(!r.okay||!count||count>l.max_records||count>(bytes.size()-r.at)/30){out.code="InvalidProfileMetadataCount";return out;}
    // count 已受真实剩余字节和原有限记录数约束；关闭与文件容量无关的内存估算。
    out.snapshot.records.reserve(static_cast<std::size_t>(count));
    for(std::uint64_t i=0;i<count;++i){const auto length=r.number(8);if(!r.okay||!length||length>l.admission.max_capsule_bytes||length>bytes.size()-r.at){out.code="ProfileMetadataCapacityExceeded";return out;}const auto start=r.at;r.take(static_cast<std::size_t>(length));Bytes capsule(bytes.begin()+static_cast<std::ptrdiff_t>(start),bytes.begin()+static_cast<std::ptrdiff_t>(r.at));// 保留真实原输入与当前胶囊副本；非空外来 baseline 仍走原拒门。
        auto capsule_limits=l.admission;
        auto decoded=decode_slot_admission_capsule(capsule,capsule_limits);if(!decoded.ok){out.code=decoded.error.original.code;return out;}
        if(!decoded.value.baseline.empty()){out.code="UnsupportedProfileDeliveryProjection";return out;}
        AdmissionRecord record;record.capsule=std::move(decoded.value);const auto origin=r.number(1),record_generation=r.number(8),history=r.number(4);if(!r.okay||origin>1||record_generation>std::uint64_t(std::numeric_limits<Integer>::max())){out.code="InvalidAdmissionRecord";return out;}if(history||origin!=static_cast<unsigned>(AdmissionOrigin::local_created)){out.code="UnsupportedProfileDeliveryProjection";return out;}record.origin=static_cast<AdmissionOrigin>(origin);record.record_generation=static_cast<Integer>(record_generation);out.snapshot.records.push_back(std::move(record));}
    for(unsigned i=0;i<5;++i)if(r.number(4)!=0){out.code="UnsupportedProfileMetadataCollections";return out;}
    if(!r.okay||r.at!=bytes.size()-32){out.code="InvalidProfileMetadataLength";return out;}auto valid=snapshot_valid(out.snapshot,l);if(!valid.ok){out.code=valid.error.original.code;return out;}out.ok=true;return out;
}catch(const std::bad_alloc&){ProfileMetadataDecoded out;out.code="ProfileMetadataResourceExhausted";return out;}}
ProfileMetadataSelected select_profile_metadata(const SlotBankPair& banks,const ProfileNamespace& host,const ProfileStoreLimits& limits){
    ProfileMetadataSelected out;const bool a=banks.a.present,b=banks.b.present;if(!a&&!b){out.ok=true;out.not_found=true;out.snapshot.name_space=host;return out;}
    ProfileMetadataDecoded da,db;if(a)da=decode_profile_metadata(banks.a.bytes,limits);if(b)db=decode_profile_metadata(banks.b.bytes,limits);
    for(const auto* value:{a?&da:nullptr,b?&db:nullptr})if(value&&!value->ok&&!value->physical_corruption){out.code=value->code;return out;}
    if((!a||!da.ok)&&(!b||!db.ok)){out.code="ProfileMetadataNoValidGeneration";return out;}
    if(a&&da.ok&&b&&db.ok&&da.snapshot.generation==db.snapshot.generation&&banks.a.bytes!=banks.b.bytes){out.code="ProfileMetadataGenerationConflict";return out;}
    const bool choose_a=a&&da.ok&&(!b||!db.ok||da.snapshot.generation>=db.snapshot.generation);out.bank=choose_a?SlotBank::a:SlotBank::b;out.snapshot=choose_a?std::move(da.snapshot):std::move(db.snapshot);if(!same_profile_namespace(host,out.snapshot.name_space)){out.code="AdmissionNamespaceMismatch";return out;}out.ok=true;out.recovered=(a&&!da.ok)||(b&&!db.ok);return out;
}
Outcome<ProfileMetadataSnapshot> insert_local_creation_record(const ProfileMetadataSnapshot& before,const AdmissionRecord& record,const ProfileStoreLimits& limits,std::uint16_t version){try{
    if(!limits_valid(limits))return failure<ProfileMetadataSnapshot>("InvalidProfileStoreCapacity");
    auto valid=validate_admission_record(record,limits.admission);
    if(!valid.ok)return {false,{},valid.error};
    if(record.origin!=AdmissionOrigin::local_created||!record.capsule.baseline.empty()||!record.allowed_history.empty()||record.record_generation!=0)return failure<ProfileMetadataSnapshot>("OriginalLocalCreationReceiptRequired");
    if(!same_profile_namespace(before.name_space,record.capsule.name_space))return failure<ProfileMetadataSnapshot>("AdmissionNamespaceMismatch");
    for(const auto& r:before.records){if(r.capsule.admission_id==record.capsule.admission_id){if(same_admission_capsule(r.capsule,record.capsule)&&r.origin==record.origin&&r.record_generation==0&&r.allowed_history.empty())return {true,before,{}};return failure<ProfileMetadataSnapshot>("AdmissionRecordConflict");}if(r.capsule.source_world_id==record.capsule.source_world_id)return failure<ProfileMetadataSnapshot>("LocalWorldAlreadyAdmitted");}
    if(before.generation<0||before.generation==std::numeric_limits<Integer>::max()||before.records.size()>=limits.max_records)return failure<ProfileMetadataSnapshot>("ProfileMetadataCapacityExceeded");
    if(version!=1&&version!=2)return failure<ProfileMetadataSnapshot>("UnsupportedProfileMetadata");
    ProfileMetadataSnapshot next=before;next.wire_version=version;
    next.generation++;
    reserve_profile_rows(next.records,next.records.size()+1,next.owned_memory);(next.owned_memory).grow(0);next.records.push_back(record);
    std::sort(next.records.begin(),next.records.end(),[](const auto& a,const auto& b){return a.capsule.admission_id<b.capsule.admission_id;});
    if(version==2){auto valid=validate_profile_metadata_complete(next,limits);if(!valid.ok)return {false,{},valid.error};}else{auto encoded=encode_profile_metadata(next,limits);if(!encoded.ok)return {false,{},encoded.error};}
    return {true,std::move(next),{}};
}catch(const std::bad_alloc&){return failure<ProfileMetadataSnapshot>("ProfileMetadataResourceExhausted");}}
}
