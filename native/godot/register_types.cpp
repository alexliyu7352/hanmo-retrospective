// 注册只读桥及显式启用的常驻桥类型；注册本身不安装世界、不创建全局Kernel。
#include "hanmo_world_bridge.hpp"
#ifdef HANMO_RESIDENT_CORE_LINKED
#include "resident_bridge_runtime.hpp"
#endif
#include <godot_cpp/godot.hpp>

// 默认只注册旧只读类；resident仅随真实目标的显式宏出现，所有实例仍由调用者持有。
void initialize_hanmo_bridge(godot::ModuleInitializationLevel level) {
    if(level!=godot::MODULE_INITIALIZATION_LEVEL_SCENE)return;
    GDREGISTER_CLASS(HanmoWorldBridge);
#ifdef HANMO_RESIDENT_CORE_LINKED
    GDREGISTER_CLASS(HanmoResidentBridge);
#endif
}
// 无全局资源；每个RefCounted独立按RAII释放持有的只读实例。
void uninitialize_hanmo_bridge(godot::ModuleInitializationLevel) {}

// C ABI入口只配置官方绑定初始化器，不执行领域安装或世界加载。
extern "C" GDExtensionBool GDE_EXPORT hanmo_world_library_init(
    GDExtensionInterfaceGetProcAddress address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject object(address,library,initialization);
    object.register_initializer(initialize_hanmo_bridge);
    object.register_terminator(uninitialize_hanmo_bridge);
    object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return object.init();
}
