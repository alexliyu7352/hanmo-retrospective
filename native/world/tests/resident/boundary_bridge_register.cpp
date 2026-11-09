// 隔离测试专用注册口；正式插件注册归中央，不借此宣称已合入游戏Main。
#include "resident_bridge_runtime.hpp"
#include <godot_cpp/godot.hpp>
// 只注册受测类，不安装世界或触发任何自动推进。
void initialize_resident_test(godot::ModuleInitializationLevel level) {if(level==godot::MODULE_INITIALIZATION_LEVEL_SCENE) GDREGISTER_CLASS(HanmoResidentBridge);}
// 无测试全局世界，RefCounted生命周期负责关闭worker。
void uninitialize_resident_test(godot::ModuleInitializationLevel) {}
// 专用入口只用于protocol smoke；真实core链接由独立构建宏明确控制。
extern "C" GDExtensionBool GDE_EXPORT hanmo_resident_test_init(GDExtensionInterfaceGetProcAddress address,GDExtensionClassLibraryPtr library,GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject object(address,library,initialization);
    object.register_initializer(initialize_resident_test); object.register_terminator(uninitialize_resident_test); object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE); return object.init();
}
