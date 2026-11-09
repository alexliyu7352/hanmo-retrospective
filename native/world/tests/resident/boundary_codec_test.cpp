// 边界测试覆盖类型、插入序、损坏输入与域codec访客；不冒充完整世界验收。
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace hanmo::resident;
// 用最小无游戏规则对象证明visitor机械往返，不复制任意领域schema。
struct Fixture { Integer count=0; double scalar=0; bool ready=false; std::string id; std::vector<Integer> ids; };
// 域示例保字段顺序；真实schema由领域owner实现。
Status visit(CodecVisitor& codec, Fixture& value) {
    codec.field("count",value.count); codec.field("scalar",value.scalar);
    codec.field("ready",value.ready); codec.field("id",value.id);
    codec.sequence("ids",value.ids,[](CodecVisitor& c,Integer& x){ return c.field("item",x); });
    return codec.status();
}
int main() {
    auto a=parse_boundary_json(R"({"z":1,"a":1.0,"b":true,"n":null,"arr":["1",2]})");
    assert(a.ok); auto rendered=render_boundary_json(a.value); assert(rendered.ok);
    auto b=parse_boundary_json(rendered.value); assert(b.ok && compare_boundary(a.value,b.value).exact());
    auto ordered=parse_boundary_json(R"({"a":1.0,"z":1,"b":true,"n":null,"arr":["1",2]})");
    auto delta=compare_boundary(a.value,ordered.value); assert(delta.values_equal && !delta.dictionary_order_equal);
    const auto keys=capture_field_order(a.value); assert(keys.ok&&keys.value.front()=="z");
    assert(apply_field_order(ordered.value,keys.value).ok&&compare_boundary(a.value,ordered.value).exact());
    auto untouched=ordered.value; assert(!apply_field_order(ordered.value,{"z","z"}).ok&&compare_boundary(untouched,ordered.value).exact());
    assert(!apply_field_order(ordered.value,{"z","a","b","n","absent"}).ok&&compare_boundary(untouched,ordered.value).exact());
    auto typed=parse_boundary_json(R"({"z":1.0,"a":1.0,"b":true,"n":null,"arr":["1",2]})");
    assert(!compare_boundary(a.value,typed.value).values_equal);
    assert(!parse_boundary_json(R"({"x":1,"x":2})").ok);
    assert(!parse_boundary_json("9223372036854775808").ok);
    assert(!parse_boundary_json("1e999").ok);
    assert(!parse_boundary_json("-9223372036854775809").ok);
    assert(!parse_boundary_json("18446744073709551616").ok);
    Fixture original{INT64_MIN,-0.0,true,std::string("甲\0乙",7),{INT64_MAX,3,1}};
    std::vector<std::uint8_t> bytes; CodecVisitor writer(bytes); assert(visit(writer,original).ok && writer.finish().ok);
    const auto saved=bytes; Fixture restored; CodecVisitor reader(saved); assert(visit(reader,restored).ok && reader.finish().ok);
    assert(restored.count==original.count && std::signbit(restored.scalar));
    assert(restored.id==original.id && restored.ready && restored.ids==original.ids);
    bytes.push_back(0); CodecVisitor trailing(static_cast<const std::vector<std::uint8_t>&>(bytes));
    assert(visit(trailing,restored).ok && !trailing.finish().ok);
    for(std::size_t n=0;n<saved.size();++n) { const std::vector<std::uint8_t> short_input(saved.begin(),saved.begin()+n); CodecVisitor short_reader(short_input); Fixture target; visit(short_reader,target); assert(!short_reader.finish().ok); }
    // callback不经reject直接返回失败也必须传播；writer/reader均不假成功。
    const Status direct_error{false,{},{{"DomainDecodeRejected","domain.x"},ErrorKind::rule,{},{},{},{}}};
    std::vector<std::uint8_t> failed_bytes; CodecVisitor failed_writer(failed_bytes);
    assert(!failed_writer.object("domain",[&](CodecVisitor&){return direct_error;}).ok);
    assert(failed_writer.status().error.original.code=="DomainDecodeRejected");
    std::vector<std::uint8_t> object_bytes; CodecVisitor object_writer(object_bytes);
    assert(object_writer.object("domain",[](CodecVisitor& c){return c.status();}).ok);
    CodecVisitor failed_reader(static_cast<const std::vector<std::uint8_t>&>(object_bytes));
    assert(!failed_reader.object("domain",[&](CodecVisitor&){return direct_error;}).ok);
    assert(failed_reader.status().error.original.path=="domain.x");
    std::cout << "boundary codec checks passed\n";
}
