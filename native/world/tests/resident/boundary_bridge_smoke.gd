# 隔离Godot真实加载仅验证未链接core时安全关闭，不冒充行旅规则/性能通过。
extends SceneTree

# 每个错误都断言明确原因；没有core时禁止返回安装成功或假ready回执。
func _initialize() -> void:
	if not ClassDB.class_exists("HanmoResidentBridge"):
		push_error("resident bridge class is absent")
		quit(1)
		return
	var bridge: Object = ClassDB.instantiate("HanmoResidentBridge")
	assert(bridge.install(PackedByteArray(), PackedByteArray()).get("code") == "UnsupportedNativeCapability")
	assert(bridge.enqueue_command({}).get("code") == "ResidentNotInstalled")
	assert(bridge.poll_command({}).get("code") == "ResidentNotInstalled")
	assert(bridge.debug_export_state().get("code") == "ResidentNotInstalled")
	print("Resident thin Bridge negative-capability smoke passed; no gameplay/performance acceptance")
	quit(0)
