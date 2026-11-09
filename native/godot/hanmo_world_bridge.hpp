// 显式离线profile的Godot字节边界；一个RefCounted至多拥有一个同库只读实例。
#pragma once
#include "hanmo/inventory_projection.hpp"
#include "hanmo/legacy_projection.hpp"
#include "hanmo/travel_schedule.hpp"
#include "hanmo/military_schedule.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <memory>
#include <variant>
#include <type_traits>

// 不接收Kernel或组件指针，不提供世界推进、重装、存档或写回接口。
class HanmoWorldBridge : public godot::RefCounted {
    GDCLASS(HanmoWorldBridge, godot::RefCounted)
    // 判别联合仅有一个活跃实例；未安装是monostate，不另存loaded或可变profile标记。
    using V1Runtime=std::unique_ptr<hanmo::InventoryProjection>;
    using LegacyRuntime=std::unique_ptr<hanmo::ProjectionStore>;
    using Runtime=std::variant<std::monostate,V1Runtime,LegacyRuntime>;
    static_assert(std::is_nothrow_move_assignable_v<Runtime>, "Runtime publication must not throw");
    Runtime runtime_;
protected:
    // 仅注册同步显式profile方法，保留Variant类型门以拒绝隐式字节转换。
    static void _bind_methods();
public:
    // 合法字节交原一次安装；失败后仍可重试，成功后原实例不可替换。
    godot::Dictionary install_artifact(const godot::Variant& bytes);
    // 整批查询成功才构造独立返回值，不把半批结果交给脚本。
    godot::Dictionary query_batch(const godot::Variant& bytes) const;
    // 新入口只接legacy工件，不能经兼容store误收旧v1；失败仍可重试。
    godot::Dictionary install_legacy_artifact(const godot::Variant& bytes);
    // 关联查询转换成Godot纯值DTO，任意深层失败不返回部分结果。
    godot::Dictionary query_actor_commitments(const godot::Variant& bytes) const;
    // 同步纯行程提案不要求安装，不读取或改变runtime_，失败不返回部分批次。
    godot::Dictionary propose_travel_schedule(const godot::Variant& bytes) const;
    // 同步军政一步纯提案；无安装前置，完整转换成功前不返回任何子数组。
    godot::Dictionary propose_military_schedule(const godot::Variant& bytes) const;
    // 每次从实际原生实例复制审计值，不暴露内部容器别名。
    godot::Dictionary audit() const;
};
