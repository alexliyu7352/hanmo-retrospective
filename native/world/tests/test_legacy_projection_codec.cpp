// 生产legacy解码的小golden与失败边界；手写类型夹具不冒称真实Schema来源。
#include "legacy_projection_codec.hpp"
#include "typed_wire.hpp"
#include "inventory_core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
using J=nlohmann::json;
using namespace hanmo;
using namespace hanmo::detail;
namespace {
int checks=0,failures=0;
// 每条断言进入退出码，异常和部分候选不能当通过。
void check(bool ok,const std::string& label) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; } }
// 文件只读，测试不重写输入或原oracle。
std::string read(const std::filesystem::path& path) { std::ifstream stream(path,std::ios::binary); if(!stream) throw std::runtime_error("missing golden: "+path.string()); return {std::istreambuf_iterator<char>(stream),{}}; }
// 手写源转换到严格标签；不把String数字或bool作为整数。
J tagged(const J& v) {
    if(v.is_null()) return {{"t","null"}};
    if(v.is_boolean()) return {{"t","bool"},{"v",v}};
    if(v.is_number_integer() || v.is_number_unsigned()) return {{"t","int"},{"v",v.dump()}};
    if(v.is_string()) return {{"t","string"},{"v",v}};
    if(v.is_array()) { J rows=J::array(); for(const auto& x:v) rows.push_back(tagged(x)); return {{"t","array"},{"v",rows}}; }
    if(v.is_object()) { J rows=J::object(); for(auto it=v.begin();it!=v.end();++it) rows[it.key()]=tagged(it.value()); return {{"t","dict"},{"v",rows}}; }
    throw std::runtime_error("golden unsupported value");
}
// 两层原字节摘要分别绑定，不以规范化后的外层字节代替输入SHA。
std::string artifact(const J& plain) { auto payload=tagged(plain).dump(); return J{{"format","HM-NATIVE-LEGACY-ARTIFACT/1"},{"binding_kind","artifact"},{"payload_sha256",sha256(payload)},{"payload_json",payload}}.dump(); }
// 每个错误都保证返回空候选，后续合法调用不受污染。
void reject(const std::string& bytes,const std::string& code,const std::string& label) {
    auto result=decode_legacy_artifact(bytes);
    check(!result.ok && result.error.code==code,label+" code="+result.error.code+" path="+result.error.path);
    check(result.value.actors.empty() && result.value.wallets.empty() && result.value.inventories.empty() && result.value.activities.empty(),"拒绝不泄漏半份候选 "+label);
}
// 可选缺失、原型目录、弱历史引用及完整原值保全均从生产解码结果读取。
void golden(const J& p) {
    const auto bytes=artifact(p); auto decoded=decode_legacy_artifact(bytes);
    check(decoded.ok,"golden可解码 "+decoded.error.code+decoded.error.path); if(!decoded.ok)return;
    auto& d=decoded.value;
    check(d.input_sha256==sha256(bytes) && d.payload_sha256==sha256(tagged(p).dump()),"工件与payload各自精确绑定");
    check(d.actors.size()==2 && d.wallets.size()==3 && d.inventories.size()==2 && d.activities.size()==1 && d.offices.size()==2,"全部五类记录存在，不从v1补假实体");
    check(!d.actors[0].birth_tick && !d.actors[0].created_tick && !d.actors[0].prototype_id,"原缺生日/创建/原型保持optional缺失");
    check(d.actors[1].id==d.facts[0].id && d.actors[1].prototype_id=="prototype","Actor与创建Fact同ID分域且目录原型不要求runtime存在");
    check(d.organizations[0].leader_actor=="unactivated.leader","组织领袖引用目录未激活人物");
    check(d.wallets[0].reserved.at("action:1")==0,"零金额预留不删除");
    check(d.activities[0].participants==std::vector<std::string>({"actor:6","a"}) && d.activities[0].start_tick==2147483647,"参与者原序及原合法start上界，不加start<=tick");
    check(d.offices[1].appointment_id=="missing-historical-fact" && !d.offices[1].enabled,"disabled历史弱引用保留");
    check(d.offices[0].appointment_id=="action:appointment" && d.activities[0].id!="action:appointment","任命Fact可在Action已删后强引用");
    check(encode_value(inventory_record(d.inventories[0]))==tagged(p["inventories"][0]),"唯一库存重组保lots原序、owner额外键与inventory extras");
    check(d.actors[0].extras.count("attributes") && d.actors[0].extras.count("extra") && !d.actors[0].extras.count("id"),"Actor known/extras不重叠");
    check(encode_value(Value{d.activities[0].params})==tagged(p["activities"][0]["params"]),"完整params类型和语义数组保持");
    check(encode_value(d.coverage)==tagged(p["coverage"]),"coverage原值含deferred保留");
    d.actors.clear(); check(decode_legacy_artifact(bytes).value.actors.size()==2,"解码结果值拥有，调用者修改不污染后续");
}
// 每组只损坏一个已声明字段；不把未覆盖领域的完整规则塞入codec。
void malformed(const J& p) {
    auto q=p; q["actors"][0]["hp"]=true; reject(artifact(q),"InvalidProjectionValue","bool不是hp整数");
    q=p;q["actors"][0]["birth_tick"]=nullptr;reject(artifact(q),"InvalidProjectionValue","显式null生日不归一成缺失");
    q=p;q["actors"][0]["hp"]=101;reject(artifact(q),"ProjectionRange","hp仍为旧0到100");
    q=p;q["actors"].push_back(q["actors"][0]);reject(artifact(q),"DuplicateActorId","Actor域内重复拒绝");
    q=p;q["actors"][0]["wallet_id"]="absent";reject(artifact(q),"DanglingProjectionReference","人物资产强引用");
    q=p;q["wallets"][0]["owner"]["extra"]=1;reject(artifact(q),"ActorAssetOwnership","本人主资产完整owner精确相等");
    q=p;q["wallets"][0]["reserved"]["action:1"]=101;reject(artifact(q),"OverReservedMoney","预留总和不超余额");
    q=p;q["wallets"][0]["reserved"]={{"absent",0}};reject(artifact(q),"DanglingProjectionReference","零预留仍需真Action");
    q=p;q["activities"][0]["participants"]=J::array({"a","a"});reject(artifact(q),"DuplicateParticipant","不排序消除重复参与者");
    q=p;q["actors"][1]["action_id"]="";reject(artifact(q),"InvalidProjectionOccupation","占用反向一致");
    q=p;q["activities"][0]["remaining_ticks"]=0;reject(artifact(q),"ProjectionRange","零余时不是运行行动");
    q=p;q["organizations"][0].erase("leader_actor");reject(artifact(q),"MissingProjectionField","目录组织必需leader不补空缺值");
    q=p;q["organizations"][0]["leader_actor"]="runtime-only";reject(artifact(q),"DanglingProjectionReference","组织领袖查catalog域");
    q=p;q["offices"][0]["state"]["appointment_id"]="absent";reject(artifact(q),"DanglingProjectionReference","enabled必须有Fact");
    q=p;q["facts"][2]["record"]["success"]=false;reject(artifact(q),"InvalidOfficeAppointment","enabled任命必须成功");
    q=p;q["offices"][1]["state"]["holder_actor_id"]="";reject(artifact(q),"InvalidOfficeHistory","disabled历史空值配对");
    q=p;q["facts"][0]["record"]["source"]="absent";reject(artifact(q),"DanglingProjectionReference","动态创建来源Fact强引用");
    q=p;q["facts"][0]["record"]["prototype_id"]="a";reject(artifact(q),"InvalidActorCreationReference","创建事实与Actor原型绑定");
    q=p;q["actors"][1]["location"]["party_id"]="absent";reject(artifact(q),"DanglingProjectionReference","Following只核原party引用");
    q=p;q["inventories"][0]["capacity"]=14;reject(artifact(q),"OverCapacity","新profile仍用唯一库存承重门");
    q=p;q["inventories"][0]["lots"][0]["reserved_by"]="absent";reject(artifact(q),"DanglingReservation","唯一库存强预留检查");
    q=p;q["coverage"]["records"][0]["extras"].push_back("id");reject(artifact(q),"InvalidProjectionCoverage","known/extras不得重叠");
    q=p;q["coverage"]["records"].erase(q["coverage"]["records"].begin());reject(artifact(q),"InvalidProjectionCoverage","coverage不能漏记录");
    auto wrapper=J::parse(artifact(p));wrapper["format"]="HM-NATIVE-INVENTORY-ARTIFACT/1";reject(wrapper.dump(),"UnsupportedProjectionProfile","旧v1不能伪造五表");
    wrapper["format"]=true;reject(wrapper.dump(),"UnsupportedProjectionProfile","format错类型保原profile首错");
    wrapper=J::parse(artifact(p));wrapper["payload_sha256"]=std::string(64,'0');reject(wrapper.dump(),"PayloadDigestMismatch","payload精确SHA");
    auto payload=tagged(p);payload["v"]["actors"]["v"][0]["v"]["hp"]["v"]="01";
    auto raw=payload.dump();wrapper["payload_json"]=raw;wrapper["payload_sha256"]=sha256(raw);reject(wrapper.dump(),"InvalidIntegerEncoding","整数编码不接受前导零");
    payload=tagged(p);payload["v"]["actors"]["v"][0]["v"]["extra"]={{"t","float"},{"v",1.5}};
    raw=payload.dump();wrapper["payload_json"]=raw;wrapper["payload_sha256"]=sha256(raw);reject(wrapper.dump(),"UnsupportedProjectionValue","未知Variant不清洗");
    reject("{\"format\":\"x\",\"for\\u006dat\":\"y\"}","DuplicateJsonKey","转义后重复键拒绝");
    check(decode_legacy_artifact(artifact(p)).ok,"所有坏输入之后合法解码仍独立");
}
// query数组保输入序，actor顺序交store排序；未知Actor延后整批存在性检查。
void queries(const J& p) {
    const auto binding=sha256(artifact(p)); J input={{"format","HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1"},{"input_sha256",binding},{"queries",J::array({{{"query_id","second"},{"actor_ids",J::array({"actor:6","a"})}},{{"query_id","empty"},{"actor_ids",J::array()}}})}};
    auto result=decode_actor_queries(input.dump(),binding);
    check(result.ok && result.value.size()==2 && result.value[0].query_id=="second" && result.value[0].actor_ids==std::vector<std::string>({"actor:6","a"}) && result.value[1].actor_ids.empty(),"query原序与空actor列表合法");
    auto bad=input;bad["queries"][1]["query_id"]="second";result=decode_actor_queries(bad.dump(),binding);check(!result.ok && result.value.empty() && result.error.code=="DuplicateQueryId","重复batch ID不回部分候选");
    bad=input;bad["queries"][1]["actor_ids"]=J::array({"a","a"});result=decode_actor_queries(bad.dump(),binding);check(!result.ok && result.value.empty() && result.error.code=="DuplicateQueryActor","重复Actor不静默去重");
    bad=input;bad["input_sha256"]=std::string(64,'0');result=decode_actor_queries(bad.dump(),binding);check(!result.ok && result.error.code=="SourceMismatch","查询绑定全工件原字节");
    bad=input;bad["queries"][0]["actor_ids"]=J::array({"unknown"});check(decode_actor_queries(bad.dump(),binding).ok,"未知人物只在store执行前整批拒绝");
}
// A导出目录只读送入同一生产入口；只声明解码接缝，不冒称已跑Flecs关联查询。
void exported(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> cases;
    for(const auto& entry:std::filesystem::directory_iterator(root)) if(entry.is_directory() && std::filesystem::exists(entry.path()/"artifact.json")) cases.push_back(entry.path());
    std::sort(cases.begin(),cases.end());check(!cases.empty(),"明确真实导出目录存在工件");
    for(const auto& path:cases) {
        const auto bytes=read(path/"artifact.json");auto decoded=decode_legacy_artifact(bytes);
        check(decoded.ok,"A工件解码 "+path.filename().string()+" "+decoded.error.code+decoded.error.path);
        if(!decoded.ok)continue;
        const auto raw=read(path/"queries.json");auto queries=decode_actor_queries(raw,decoded.value.input_sha256);
        check(queries.ok,"A查询绑定 "+path.filename().string()+" "+queries.error.code+queries.error.path);
        check(sha256(read(path/"artifact.json"))==decoded.value.input_sha256,"A原件仍为同字节 "+path.filename().string());
        std::cout<<"LEGACY_ARTIFACT="<<path.filename().string()<<" SHA="<<decoded.value.input_sha256<<" ACTORS="<<decoded.value.actors.size()<<" ACTIVITIES="<<decoded.value.activities.size()<<'\n';
    }
}
} // 纯协议夹具不生成旧世界或游戏结果。

// 默认从源码旁golden读取，也接受明确路径；返回断言失败或异常的非零码。
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--artifacts") exported(argv[2]);
        else {
            const auto path=argc>1?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"legacy_projection_codec/legacy_golden.json";
            const auto p=J::parse(read(path)); golden(p);malformed(p);queries(p);
        }
    } catch(const std::exception& e) { check(false,std::string("unexpected exception: ")+e.what()); }
    std::cout<<"LEGACY_CODEC_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
