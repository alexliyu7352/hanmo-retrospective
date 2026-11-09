// 使用真实GD逐命令成功回执证明typed缓存和checkpoint无损，不用摘要代替完整details。
#include "resident/kernel/command_types.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <fstream>
#include <cassert>
#include <iostream>
#include <algorithm>
using namespace hanmo::resident;
// 仅读取固定oracle文本，命令规则仍由原GD独立产生。
BoundaryValue load(const char* path) {std::ifstream in(path);std::string bytes((std::istreambuf_iterator<char>(in)),{});auto parsed=parse_boundary_json(bytes);assert(parsed.ok);return std::move(parsed.value);}
// 人为改变每层Dictionary序但不改值，用来检验恢复codec不偷偷排序。
void reverse_dictionaries(BoundaryValue& value) {if(auto* d=value.as<BoundaryValue::Dict>()){std::reverse(d->begin(),d->end());for(auto& row:*d)reverse_dictionaries(row.second);}else if(auto* a=value.as<BoundaryValue::Array>())for(auto& row:*a)reverse_dictionaries(row);}
// cache值与其binary恢复后的全值/字典序都必须逐项一致。
void verify(const BoundaryValue& receipt) {
    auto decoded=decode_resident_receipt(receipt);assert(decoded.ok);
    auto encoded=encode_resident_receipt(decoded);assert(encoded.ok&&compare_boundary(receipt,encoded.value).exact());
    auto ptr=decoded.value;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_resident_receipt(writer,ptr).ok&&writer.finish().ok);
    CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));std::shared_ptr<const CommitResult> restored;
    assert(visit_resident_receipt(reader,restored).ok&&reader.finish().ok);auto again=encode_resident_receipt({true,restored,{}});
    assert(again.ok&&compare_boundary(receipt,again.value).exact());
}
// 实际青川1.5第16次命令的原回执；外层对白身份不妨碍唯一允许的切磋明细无损冷读。
void dialogue_duel() {
    auto actual=parse_boundary_json(R"({"ok":true,"kind":"dialogue_confirm","tick":2,"revision":16,"result_id":"result:27","details":{"battle_id":"battle:26"}})");assert(actual.ok);verify(actual.value);
    auto reordered=actual.value;reverse_dictionaries(reordered);verify(reordered);
    auto extra=actual.value;extra.find("details")->as<BoundaryValue::Dict>()->emplace_back("outcome",BoundaryValue{StableId{"draw"}});assert(!decode_resident_receipt(extra).ok);
    auto bad_type=actual.value;*bad_type.find("details")->find("battle_id")=BoundaryValue{Integer{26}};assert(!decode_resident_receipt(bad_type).ok);
    auto other=actual.value;*other.find("kind")=BoundaryValue{StableId{"dialogue_next"}};assert(!decode_resident_receipt(other).ok);
}
// 新问讯保留空消息与原消息ID；条件字段由已保存kind判别，不改旧回执的二进制布局。
// 显式旧宽松布局构造畸形冷字节，不能靠当前严格writer先拒绝来冒充reader覆盖。
void reject_news_binary(bool has_details,Integer tag) {
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);bool yes=true;StableId kind="ask_news",id="result:4";Integer tick=2,revision=3;std::vector<StableId> keys;
    writer.field("present",yes);writer.field("ok",yes);writer.field("kind",kind);writer.field("tick",tick);writer.field("revision",revision);writer.field("result_id",id);writer.field("has_details",has_details);
    if(has_details)writer.object("details",[&](auto& child){child.field("variant",tag);if(tag==1){StableId action="action:2";child.field("action_id",action);}return child.sequence("field_order",keys,[](auto& v,auto& key){return v.field("key",key);});});
    writer.sequence("field_order",keys,[](auto& v,auto& key){return v.field("key",key);});assert(writer.finish().ok);
    CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));std::shared_ptr<const CommitResult> value;assert(!visit_resident_receipt(reader,value).ok);
}
void regional_news() {
    for(const auto* details:{"{}","{\"action_id\":\"action:2\"}","{\"completed_actions\":{},\"suspended_actors\":[]}"}){auto bad=parse_boundary_json(std::string("{\"ok\":true,\"kind\":\"ask_news\",\"tick\":2,\"revision\":3,\"result_id\":\"result:4\",\"details\":")+details+"}");assert(bad.ok&&!decode_resident_receipt(bad.value).ok);}
    reject_news_binary(false,0);reject_news_binary(true,0);reject_news_binary(true,1);
    auto missing=std::make_shared<CommitResult>();missing->kind="ask_news";assert(!encode_resident_receipt({true,missing,{}}).ok);
    for(const auto* json:{R"({"ok":true,"kind":"ask_news","tick":2,"revision":3,"result_id":"result:4","details":{"news_ids":[]}})",R"({"ok":true,"kind":"ask_news","tick":2,"revision":3,"result_id":"result:4","details":{"news_ids":["message:3"]}})"}){auto value=parse_boundary_json(json);assert(value.ok);verify(value.value);reverse_dictionaries(value.value);verify(value.value);auto wrong=value.value;*wrong.find("kind")=BoundaryValue{StableId{"talk_actor"}};assert(!decode_resident_receipt(wrong).ok);auto extra=value.value;extra.find("details")->as<BoundaryValue::Dict>()->emplace_back("amount",BoundaryValue{Integer{3}});assert(!decode_resident_receipt(extra).ok);}
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--regional-news"){regional_news();std::cout<<"regional news receipt+checkpoint boundary passed\n";return 0;}
    if(argc==2&&std::string(argv[1])=="--dialogue-duel"){dialogue_duel();std::cout<<"dialogue-confirm duel receipt+checkpoint boundary passed\n";return 0;}
    dialogue_duel();
    assert(argc==3);auto index=load(argv[1]);int count=0;
    for(const auto& command:*index.find("commands")->as<BoundaryValue::Array>()){verify(*command.find("receipt"));auto reordered=*command.find("receipt");reverse_dictionaries(reordered);verify(reordered);++count;}
    auto recovery=load(argv[2]);
    for(const auto& c:*recovery.find("cases")->as<BoundaryValue::Array>())for(const auto& row:*c.find("trace")->as<BoundaryValue::Array>())if(const auto* result=row.find("result")){const auto* ok=result->find("ok");if(ok&&ok->as<bool>()&&*ok->as<bool>()){verify(*result);++count;}}
    auto repeated=parse_boundary_json(R"({"ok":true,"kind":"advance","tick":2,"revision":3,"result_id":"result:3","details":{"completed_actions":{"action:1":{"ok":true,"success":true,"already_completed":true,"details":{"office_id":"office.guard"}}},"suspended_actors":[]}})");assert(repeated.ok);verify(repeated.value);
    auto repeated_decoded=decode_resident_receipt(repeated.value);auto repeated_encoded=encode_resident_receipt(repeated_decoded);const auto& completion=*repeated_encoded.value.find("details")->find("completed_actions")->find("action:1");assert(!completion.find("code")&&completion.find("already_completed")&&!completion.find("details")->find("already_completed"));
    // 原100人command-008观测到的departure明细，用有限回执壳单测其嵌套顺序与二进制恢复。
    auto departure=parse_boundary_json(R"({"ok":true,"kind":"campaign_depart","tick":5,"revision":9,"result_id":"fixture:departure","details":{"campaign_id":"campaign:197","journey_request":{"army_id":"army:262","destination_settlement_id":"city.west"}}})");assert(departure.ok);verify(departure.value);reverse_dictionaries(departure.value);verify(departure.value);
    auto invalid_request=departure.value;invalid_request.find("details")->find("journey_request")->as<BoundaryValue::Dict>()->emplace_back("extra",BoundaryValue{true});assert(!decode_resident_receipt(invalid_request).ok);
    auto malformed=parse_boundary_json(R"({"ok":true,"kind":"advance","tick":0,"revision":1,"result_id":"r","details":{"completed_actions":{},"suspended_actors":[],"hidden":1}})");assert(!decode_resident_receipt(malformed.value).ok);
    std::cout<<"real oracle receipt+checkpoint boundaries passed: "<<count<<"\n";
}
