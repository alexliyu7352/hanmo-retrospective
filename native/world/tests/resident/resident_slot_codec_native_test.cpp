// 原GD固定历史样本与物理拒绝边界的原生兼容门；不是世界语义或新封套/2门。
#include "resident_slot_codec.hpp"
#include <nlohmann/json.hpp>
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
using Json=nlohmann::json;
// 原resident_slot_codec_test.gd的固定字节，不能以新编码结果自生期望值。
const std::string legacy=R"({"format_version":1,"generation":7,"magic":"HM-RESIDENT-SAVE","payload_base64":"AQID","payload_length":3,"payload_sha256":"039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81","save_id":"slot:test"})";
// 只构造原JSON输入，不在测试中代替生产物理检查。
Bytes bytes(const std::string& text){return Bytes(text.begin(),text.end());}
// 拒绝必须匹配具名旧错误和正确的物理损坏分类。
void rejected(const Json& value,const char* code,bool physical=true){auto result=decode_resident_slot(bytes(value.dump()));assert(!result.ok&&result.code==code&&result.physical_corruption==physical);}
// 旧格式、同代冲突和未知错误均走新实际codec，不依赖Godot可用性假定。
int main(){
    auto encoded=encode_resident_slot(Bytes{1,2,3},7,"slot:test");assert(encoded.ok&&encoded.value==bytes(legacy));
    auto decoded=decode_resident_slot(bytes(legacy));assert(decoded.ok&&decoded.envelope.payload==Bytes({1,2,3})&&!decoded.envelope.continuation);
    auto header=Json::parse(legacy);header["generation"]=7.0;header["format_version"]=1.0;assert(decode_resident_slot(bytes(header.dump())).ok);
    header=Json::parse(legacy);header["generation"]=2147483648LL;rejected(header,"InvalidSaveHeader");
    auto contextual=encode_resident_slot(Bytes{1,2,3},8,"slot:test",Bytes{72,77,67,78,1,254,0});assert(contextual.ok);
    auto round=decode_resident_slot(contextual.value);assert(round.ok&&round.envelope.format_version==2&&round.envelope.continuation==Bytes({72,77,67,78,1,254,0}));
    header=Json::parse(contextual.value);auto bad=header;bad["continuation_payload_sha256"]=std::string(64,'0');rejected(bad,"ChecksumMismatch");
    bad=header;bad["continuation_sha256"]=std::string(64,'A');rejected(bad,"InvalidSaveMetadata");
    for(const auto* malformed:{"","SE1D\n","SE1D----","AA=A","AR=="}){bad=header;bad["continuation_base64"]=malformed;rejected(bad,"InvalidResidentBase64");}
    bad=header;bad["continuation_base64"]="SE1D";rejected(bad,"ChecksumMismatch");
    bad=header;bad["format_version"]=3;bad.erase("generation");rejected(bad,"UnsupportedSaveVersion",false);bad["magic"]="OTHER-SAVE";rejected(bad,"UnsupportedSaveFormat",false);
    bad=header;bad["extra"]=true;rejected(bad,"InvalidSaveHeader");bad=header;bad.erase("continuation_sha256");rejected(bad,"InvalidSaveHeader");
    for(const auto* field:{"continuation_length","continuation_sha256","continuation_payload_sha256","continuation_base64"}){bad=header;bad[field]=false;rejected(bad,"InvalidSaveHeader");}
    bad=header;bad["continuation_length"]=0;rejected(bad,"InvalidSaveMetadata");bad["continuation_length"]=2049;rejected(bad,"SaveSizeLimit",false);
    auto maximum=encode_resident_slot(Bytes{1,2,3},1,"slot:test",Bytes(2048,0));assert(maximum.ok&&decode_resident_slot(maximum.value).ok);
    assert(!encode_resident_slot(Bytes{1,2,3},1,"slot:test",Bytes(2049,0)).ok);
    auto trailing=decode_resident_slot(bytes(legacy+" []"));assert(!trailing.ok&&trailing.code=="TrailingData");
    auto duplicate=decode_resident_slot(bytes(legacy.substr(0,legacy.size()-1)+",\"generation\":7}"));assert(!duplicate.ok&&duplicate.code=="DuplicateKey");
    // 旧StrictJSON的容器标点覆写优先级保留；TooDeep仍不是可回退的物理损坏。
    for(const auto* malformed:{"{\"x\":-}","[bad]","{\"x\":1e999 x}","[1e999 x]"}){auto value=decode_resident_slot(bytes(malformed));assert(!value.ok&&value.code=="ExpectedComma"&&value.physical_corruption);}
    auto nonfinite=decode_resident_slot(bytes("{\"x\":1e999}"));assert(!nonfinite.ok&&nonfinite.code=="NonFiniteNumber"&&!nonfinite.physical_corruption);
    auto deep=decode_resident_slot(bytes(std::string(34,'[')+"0"+std::string(34,']')));assert(!deep.ok&&deep.code=="TooDeep"&&!deep.physical_corruption);
    const auto first=encode_resident_slot(Bytes{1,2,3},1,"manual-1").value,second=encode_resident_slot(Bytes{1,2,4},2,"manual-1").value;
    SlotBankPair pair;pair.a={true,first,0};pair.b={true,second,0};auto selected=select_resident_slot(pair,"manual-1");assert(selected.ok&&selected.bank==SlotBank::b&&selected.envelope.generation==2);
    pair.b.bytes=bytes("broken");selected=select_resident_slot(pair,"manual-1");assert(selected.ok&&selected.bank==SlotBank::a&&selected.recovered);
    bad=Json::parse(first);bad["format_version"]=9;pair.b.bytes=bytes(bad.dump());selected=select_resident_slot(pair,"manual-1");assert(!selected.ok&&selected.code=="UnsupportedSaveVersion");
    pair.b.bytes=encode_resident_slot(Bytes{4,5,6},1,"manual-1").value;selected=select_resident_slot(pair,"manual-1");assert(!selected.ok&&selected.code=="GenerationConflict");
    std::cout<<"resident old-slot codec exact historical bytes/continuation/selection checks passed; no P14 or world validation claim\n";
}
