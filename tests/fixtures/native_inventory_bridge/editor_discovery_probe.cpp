// 未解决editor首自动发现崩溃的独立最小复现；无Hanmo、Flecs、字段或领域方法。
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>

// 只切换是否注册一个空类，用来区分官方绑定初始化与editor新类发现路径。
class ProbeRef : public godot::RefCounted {
    GDCLASS(ProbeRef,godot::RefCounted)
protected:
    // 无自有方法，崩溃不依赖库存转换或自有参数绑定。
    static void _bind_methods() {}
};
// NO_PROBE_CLASS构建作为同一SDK/绑定的无类对照。
void initialize_probe(godot::ModuleInitializationLevel level) {
#ifndef NO_PROBE_CLASS
    if(level==godot::MODULE_INITIALIZATION_LEVEL_SCENE) GDREGISTER_CLASS(ProbeRef);
#else
    (void)level;
#endif
}
// 不创建任何原生领域实例或持久资源。
void terminate_probe(godot::ModuleInitializationLevel) {}
// 仅沿官方初始化合同；入口名复用模板，便于同命令对照两份独立工程。
extern "C" GDExtensionBool GDE_EXPORT hanmo_world_library_init(
    GDExtensionInterfaceGetProcAddress address,GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject object(address,library,initialization);
    object.register_initializer(initialize_probe); object.register_terminator(terminate_probe);
    object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return object.init();
}
