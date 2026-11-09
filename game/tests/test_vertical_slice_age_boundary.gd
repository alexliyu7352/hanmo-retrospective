## 合法15岁作者初态经正常wait跨16岁边界；只允许外部硬限的一次有界720半日验证。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Art = preload("res://ui/art_catalog.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Identity = preload("res://core/actor_identity.gd")
const Codec = preload("res://core/save_codec.gd")
const F = preload("res://tests/companion_fixture.gd")
var checks: int = 0
var failures: Array = []
var commands: Array = []
var output: String
var phases: Array = []

## 推进只在显式准备工程下运行，避免默认启动长测试。
func _init() -> void:
	call_deferred("run_checks")

## 为硬限与进度日志记录真实单调时间和当前阶段。
func phase(label: String) -> void:
	var item: Dictionary = {"phase": label, "elapsed_ms": Time.get_ticks_msec()}
	phases.append(item)
	print("V05_AGE_PHASE ", JSON.stringify(item))

## 失败有明确标签，不能用最后打印或截图覆盖失败。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)
	return value

## 只读查询年龄比例并保存生产控件的实际像素，完整状态必须保持不变。
func render(kernel, name: String) -> Dictionary:
	var state: Dictionary = kernel.snapshot()
	var descriptor: Dictionary = Art.character(state.player_actor_id, state, kernel.content().data, "half_body", {"resolution": [512, 768]})
	if not check(descriptor.ok and descriptor.degradation.is_empty(), "年龄代表无降级 " + name): return {}
	var control = Portrait.new()
	if check(control.configure(descriptor, Art.catalog()).ok, "生产年龄控件合成 " + name):
		check(control.image_copy().save_png(output.path_join(name + ".png")) == OK, "保存年龄像素 " + name)
	control.free()
	check(kernel.snapshot() == state, "渲染不写年龄档案世界RNG " + name)
	return descriptor

## 每一步wait提交与半日结算都同未渲染核心全字段对照，不省略正常规则或改写出生日期。
func run_checks() -> void:
	phase("begin")
	output = OS.get_environment("HANMO_AGE_OUTPUT")
	var project: String = OS.get_environment("HANMO_REAL_ART_PROJECT")
	if not check(not output.is_empty() and not project.is_empty(), "显式年龄工程输出"):
		finish(); return
	DirAccess.make_dir_recursive_absolute(output)
	var k = Kernel.new()
	var control = Kernel.new()
	var catalog: String = project.path_join("prepared/age_boundary.hmcatalog.json")
	if not check(k.new_game("scenario.procurement", catalog).ok and control.new_game("scenario.procurement", catalog).ok and Art.ready_result().ok, "两核心正常作者prepare创建"):
		finish(); return
	var initial: Dictionary = k.snapshot()
	var actor: String = initial.player_actor_id
	var appearance: Dictionary = initial.actors[actor].appearance.duplicate(true)
	check(initial.tick == 0 and initial.actors[actor].birth_tick == -15 * 720 and Identity.age_years(initial, actor) == 15, "合法整数15岁作者初态")
	var child: Dictionary = render(k, "age15-child")
	var boundary: Dictionary = {}
	phase("wait_begin")
	for index in range(720):
		var before: Dictionary = k.snapshot()
		var command: Dictionary = {"kind": "wait", "params": {}, "actor_id": actor, "request_seq": before.request_sequences[actor] + 1, "expected_revision": before.revision}
		var result: Dictionary = k.submit(command)
		var other: Dictionary = control.submit(command)
		if not check(result.ok and result == other, "正常wait回执对照 " + str(index)):
			finish(); return
		var stepped: Dictionary = k.advance_one_tick()
		var other_step: Dictionary = control.advance_one_tick()
		commands.append({"command": command, "result": result, "advance_result": stepped})
		if not check(stepped.ok and stepped == other_step and k.snapshot() == control.snapshot(), "半日完整状态与RNG对照 " + str(index)):
			finish(); return
		if index == 718:
			boundary = k.snapshot()
			check(Identity.age_years(boundary, actor) == 15, "第719半日仍15岁")
		if (index + 1) % 180 == 0: phase("wait_" + str(index + 1))
	phase("wait_end")
	var final_state: Dictionary = k.snapshot()
	var adult: Dictionary = render(k, "age16-adult")
	check(final_state.tick == 720 and Identity.age_years(final_state, actor) == 16, "第720半日自然到16岁")
	check(final_state.actors[actor].appearance == appearance and boundary.actors[actor].appearance == appearance, "跨界全部基础外貌与来源种子不重抽")
	check(child.proportion == "child" and adult.proportion == "adult" and child.body_basis != adult.body_basis, "年龄切换独立儿童成人比例")
	check(child.identity_key == adult.identity_key and child.layers[0].id != adult.layers[0].id and child.cache_key != adult.cache_key, "同身份切正确变体与描述缓存")
	var encoded: String = Codec.encode(final_state, 1, "age-boundary", k.content())
	var decoded: Dictionary = Codec.decode(encoded, k.content())
	check(decoded.ok and decoded.state == final_state, "完整自然跨界存档按同内容正常解码")
	var save = FileAccess.open(output.path_join("age16.hmsave"), FileAccess.WRITE)
	if check(save != null, "持久保存自然跨界原始字节"):
		save.store_string(encoded); save.close()
	phase("complete")
	var report = FileAccess.open(output.path_join("age-boundary.json"), FileAccess.WRITE)
	if check(report != null, "年龄回执可写"):
		report.store_string(JSON.stringify({"format": "HM-V05-NATURAL-AGE-BOUNDARY/1", "checks": checks, "failures": failures, "phases": phases, "initial_state": initial, "before_boundary_state": boundary, "final_state": final_state, "child": child, "adult": adult, "commands": commands, "scope": "one normal author procurement world, 720 wait ticks, full unrendered control comparison; no OS input"}, "\t"))
		report.close()
	finish()

## 外部仍需核对进程未超240秒、明确标记与非零检查，超时不得自动重跑。
func finish() -> void:
	print("V05_AGE_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
