// P1薄Godot驱动只有一个core worker；不接Main或物理槽，不创建另一套规则世界。
#pragma once
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident_bridge_live_jobs.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>

class HanmoResidentBridge : public godot::RefCounted {
    GDCLASS(HanmoResidentBridge,godot::RefCounted)
protected:
    static void _bind_methods();
public:
    ~HanmoResidentBridge() override;
    // 仅初始化接受完整catalog/state字节；每个实例只成功安装一次。
    godot::Dictionary install(const godot::Variant& catalog_json,const godot::Variant& state_json);
    // 新动态profile直接从目录构造；恢复直接解码原银行，均不先建立默认世界。
    godot::Dictionary install_native(const godot::Variant& catalog_json,const godot::Variant& request_json);
    godot::Dictionary install_checkpoint(const godot::Variant& catalog_json,const godot::Variant& checkpoint,const godot::Variant& continuation=godot::Variant());
    // 无worker/世界的冷探查只返回选择身份，不能安装或修改银行。
    godot::Dictionary inspect_checkpoint_selection(const godot::Variant& checkpoint);
    // 单个小命令DTO深复制后交worker；执行中再次提交明确Busy，无隐式排队/提前推进。
    godot::Dictionary enqueue_command(const godot::Variant& command);
    // oracle诊断入口保JSON原数值标签，避免Godot JSON解析把整数先变float。
    godot::Dictionary enqueue_command_json(const godot::Variant& command_json);
    // 按完整世界代次和操作号读取缓存，始终不join或导出世界。
    godot::Dictionary poll_command(const godot::Variant& reference);
    // 显式诊断导出要求worker空闲，只用于计时外oracle，不是poll的副作用。
    godot::Dictionary debug_export_state();
    // P2闭合操作复用唯一worker；source始终来自调用者，poll只读有限完成缓存。
    godot::Dictionary enqueue_live_operation(const godot::Variant& operation);
    godot::Dictionary poll_live_operation(const godot::Variant& reference);
    // 阅读撤权只改transport门，当前事务不被中断，解除暂停不发新许可。
    godot::Dictionary set_reading_paused(const godot::Variant& reference,bool paused);
private:
    std::unique_ptr<hanmo::resident::ResidentReplaySession> session_;
    std::thread worker_; std::mutex mutex_; std::condition_variable wake_;
    bool stopping_=false,running_=false,running_live_=false;
    hanmo::resident::ResidentLiveJobs live_jobs_;
    std::optional<hanmo::resident::BoundaryValue> command_;
    std::optional<hanmo::resident::Outcome<hanmo::resident::ReplayCommandResult>> result_;
    hanmo::resident::WorldIdentity identity_;
    hanmo::resident::Integer operation_id_=0,execute_usec_=0,input_dto_usec_=0;
    godot::Dictionary cached_;
    godot::Dictionary enqueue_value(hanmo::resident::BoundaryValue&&,hanmo::resident::Integer input_usec);
    void work();
};
