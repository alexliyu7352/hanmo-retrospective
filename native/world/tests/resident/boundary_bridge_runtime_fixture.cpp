// 仅Godot边界测试用echo session，不含游戏规则；绝不链接正式world或计入P1性能数据。
#include "resident/kernel/boundary_dispatch.hpp"
#include <chrono>
#include <thread>
namespace hanmo::resident {
namespace {
// 固定身份和回显验证typed DTO/线程缓存；这里的计数不代表世界advance。
class EchoSession final:public ResidentReplaySession {
    WorldIdentity id_; BoundaryValue initial_;
public:
    Status initialize(const BoundaryValue&,const BoundaryValue& state) override {initial_=state;id_.world_id[0]=9;id_.execution_epoch=4;return {true,{},{}};}
    Outcome<ReplayCommandResult> execute(const BoundaryValue& command) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return {true,{command,{BoundaryValue::Dict{{"ok",{true}},{"scope",{std::string("protocol_echo_only")}}}},{}},{}};
    }
    // 仅验证二进制transport：原样回显高位/零字节，绝不冒称真实checkpoint或restore。
    Outcome<LiveOperationResult> execute_live(const LiveOperation& operation) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        LiveOperationResult result{operation.source,id_,{BoundaryValue::Dict{{"ok",{true}},{"scope",{std::string("protocol_echo_only")}},{"previous_present",{operation.previous_checkpoint_bytes.has_value()}},{"previous_size",{static_cast<Integer>(operation.previous_checkpoint_bytes?operation.previous_checkpoint_bytes->size():0)}}}}};
        if(operation.kind==LiveOperationKind::checkpoint||operation.kind==LiveOperationKind::restore)result.checkpoint_bytes=operation.checkpoint_bytes?*operation.checkpoint_bytes:Bytes{0,255,128,13};
        if(auto large=operation.payload.find("large_output");large&&large->as<bool>()&&*large->as<bool>())result.checkpoint_bytes=Bytes(live_checkpoint_byte_limit+1);
        return {true,std::move(result),{}};
    }
    Outcome<BoundaryValue> export_state() const override {return {true,initial_,{}};}
    Outcome<WorldIdentity> identity() const override {return {true,id_,{}};}
    Outcome<Bytes> checkpoint() override {return {false,{},{{"ProtocolFixtureNoCheckpoint","test"},ErrorKind::unsupported_capability,{},{},{},{}}};}
    Status restore(const Bytes&) override {return {false,{},{{"ProtocolFixtureNoRestore","test"},ErrorKind::unsupported_capability,{},{},{},{}}};}
};
}
// 测试目标独占factory，正式目标只能链接中央真实factory。
std::unique_ptr<ResidentReplaySession> create_resident_replay_session() {return std::make_unique<EchoSession>();}
}
