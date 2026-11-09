// 文书冻结来源只有固定字段；真正亲历与送达仍需领域验证，不从可解码推为可信。
#include "death_notice_source.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
#include "resident/storage/common_checkpoint.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident { namespace {
using S=WitnessedDeathNoticeSource;using V=BoundaryValue;
const std::vector<std::pair<const char*,StableId S::*>> texts={{"version",&S::version},{"scenario_id",&S::scenario_id},{"death_fact_id",&S::death_fact_id},{"decedent_actor_id",&S::decedent_actor_id},{"sender_actor_id",&S::sender_actor_id},{"recipient_actor_id",&S::recipient_actor_id},{"casualty_fact_id",&S::casualty_fact_id},{"battle_id",&S::battle_id},{"unit_id",&S::unit_id}};
const std::set<StableId> keys={"version","scenario_id","death_fact_id","decedent_actor_id","sender_actor_id","recipient_actor_id","casualty_fact_id","battle_id","unit_id","world_id","observed_tick","issued_tick"};
// 编码与解码共用严格形状门，不补字段、修重键或归并不同观察者。
Status shape(const S& s){if(s.version!="witnessed_death_v1"||s.observed_tick<0||s.issued_tick<s.observed_tick||s.sender_actor_id==s.recipient_actor_id||s.sender_actor_id==s.decedent_actor_id||s.recipient_actor_id==s.decedent_actor_id||std::all_of(s.world_id.begin(),s.world_id.end(),[](auto b){return b==0;}))return common_failure("InvalidDeathNoticeSource");for(const auto& p:texts)if((s.*p.second).empty()||(s.*p.second).size()>256)return common_failure("InvalidDeathNoticeSource",p.first);if(!s.field_order.empty()&&(s.field_order.size()!=keys.size()||std::set<StableId>(s.field_order.begin(),s.field_order.end())!=keys))return common_failure("BoundaryFieldOrderMismatch");return common_success();}
// 固定规范小写世界ID，拒绝截断、混合大小写或非十六进制字符。
WorldId world(const StableId& text){if(text.size()!=32)boundary::fail("InvalidWorldId","death_notice");auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};WorldId id{};for(std::size_t i=0;i<16;++i){int a=digit(text[i*2]),b=digit(text[i*2+1]);if(a<0||b<0)boundary::fail("InvalidWorldId","death_notice");id[i]=static_cast<std::uint8_t>(a*16+b);}return id;}
}
// 源只有标量；先显式拒重复键，再读任何字段，未知与嵌套对象都不能旁路。
Outcome<WitnessedDeathNoticeSource> decode_witnessed_death_notice_source(const V& value){try{std::set<StableId> seen;for(const auto& p:boundary::object(value,"death_notice"))if(!keys.count(p.first)||!seen.insert(p.first).second)boundary::fail("InvalidDeathNoticeSourceFields",p.first);if(seen!=keys)boundary::fail("InvalidDeathNoticeSourceFields","death_notice");S result;for(const auto& p:texts)result.*p.second=boundary::get_string(value,p.first,"");result.world_id=world(boundary::get_string(value,"world_id",""));result.observed_tick=boundary::get_int(value,"observed_tick","");result.issued_tick=boundary::get_int(value,"issued_tick","");result.field_order=boundary::order(value);auto valid=shape(result);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 输出同一完整形状；字段序只保留合法原序，不改变内容身份。
Status emit_witnessed_death_notice_source_fields(ProjectionWriter& out,const S& s){
    auto valid=shape(s);if(!valid.ok){valid.error.original.path="death_notice";return valid;}
    for(const auto& p:texts)out.field(p.first,s.*p.second);
    constexpr char digits[]="0123456789abcdef";std::array<char,32> id{};
    for(std::size_t i=0;i<s.world_id.size();++i){id[2*i]=digits[s.world_id[i]>>4];id[2*i+1]=digits[s.world_id[i]&15];}
    out.text("world_id",std::string_view(id.data(),id.size()));out.field("observed_tick",s.observed_tick);out.field("issued_tick",s.issued_tick);return out.order_fields(s.field_order);
}
BoundaryValue encode_witnessed_death_notice_source(const S& s){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& fields){return emit_witnessed_death_notice_source_fields(fields,s);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}

// 只供新的Task文书绑定段调用；旧Task/handoff段没有新增存在位或字段。
Status visit_witnessed_death_notice_source(CodecVisitor& c,S& s){for(const auto& p:texts)c.field(p.first,s.*p.second);std::vector<Integer> bytes;for(auto b:s.world_id)bytes.push_back(b);c.sequence("world_id",bytes,[](CodecVisitor& at,Integer& byte){return at.field("byte",byte);});if(c.reading()&&c.status().ok){if(bytes.size()!=16)return c.reject("InvalidWorldId");for(std::size_t i=0;i<16;++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidWorldId");s.world_id[i]=static_cast<std::uint8_t>(bytes[i]);}}c.field("observed_tick",s.observed_tick);c.field("issued_tick",s.issued_tick);common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto valid=shape(s);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
