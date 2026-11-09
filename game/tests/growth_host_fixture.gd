## 成长页测试沿用正式主场景入口，兼容既有测试调用名称，不另造界面组合器。
extends "res://ui/main.gd"

## 由正式入口打开同一成长页，测试无需私有成长开关或替代存储界面。
func open_growth_fixture() -> void:
	_open_growth()
