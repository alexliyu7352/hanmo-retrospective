// 真实随军行军bank逐叶只读核验；错误也保留原来源，不能为显示伪装玩家位置。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
// 文件与原生层错误保原代码，未准入世界不能继续取页。
template<class T>T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code+":"+value.error.original.path);return std::move(value.value);}
// 输入都是指定正式目录或原worker字节，不制造新角色与行动。
Bytes bytes(const char* path){std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("input missing");return Bytes(std::istreambuf_iterator<char>(stream),{});}
}
// 失败叶完整落盘，全部读完再给进程结果，便于区分首错与后继潜在阻点。
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("usage: military_service_journey_probe CATALOG BANK OUTPUT");auto raw=bytes(argv[1]);auto catalog=take(parse_boundary_json(std::string(raw.begin(),raw.end())));auto bank=bytes(argv[2]);auto session=create_resident_replay_session();auto installed=session->initialize_checkpoint(catalog,bank);if(!installed.ok)throw std::runtime_error(installed.error.original.code);auto before=take(session->checkpoint());BoundaryValue::Dict results;int errors=0;
 for(const auto* id:{"command_context","journey_location","journey_encounter","journey_travel","journey_route","journey_map_position"}){LiveOperation operation;operation.source=take(session->identity());operation.payload=boundary::obj({{"query_id",BoundaryValue{id}},{"args",BoundaryValue{BoundaryValue::Dict{}}}});auto reply=session->execute_live(operation);if(reply.ok)results.emplace_back(id,reply.value.result);else{++errors;results.emplace_back(id,boundary::obj({{"ok",BoundaryValue{false}},{"code",BoundaryValue{reply.error.original.code}},{"path",BoundaryValue{reply.error.original.path}}}));}}
 if(before!=take(session->checkpoint()))throw std::runtime_error("journey leaf mutated checkpoint");std::ofstream output(argv[3]);if(!output)throw std::runtime_error("output unavailable");output<<take(render_boundary_json(BoundaryValue{results}))<<'\n';output.close();if(!output)throw std::runtime_error("output failed");std::cout<<"MILITARY_SERVICE_JOURNEY_LEAVES=6 FAILURES="<<errors<<" ZERO_WRITE=true SOURCE=ORIGINAL_MARCHING_BANK\n";return errors?1:0;
 }catch(const std::exception& error){std::cerr<<"MILITARY_SERVICE_JOURNEY_FAIL "<<error.what()<<'\n';return 1;}}
