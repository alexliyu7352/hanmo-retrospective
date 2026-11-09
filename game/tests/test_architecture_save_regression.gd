## 冻结真实围城存读的已暴露数组次序缺陷；严格红灯不能被排序数组或删除事实掩盖。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Trace=preload("res://tests/architecture_trace.gd")
var checks: int=0
var failures: int=0

## 延后执行真实命令回放，夹具只冻结输入和来源，不重算世界。
func _init() -> void:
	call_deferred("run_checks")

## 保留每个明确断言的失败标签，现状缺陷必须以非零退出显露。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 两个实际Kernel接受完全相同命令，仅一方在真实围城等待点经Codec重启。
func run_checks() -> void:
	var path: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/architecture/regressions/siege_fact_array_order.json")
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not check(parsed.ok,"读取已绑定原提交的实际命令夹具"): finish(); return
	var input: Dictionary=parsed.value; var original=Kernel.new(); var resumed=Kernel.new()
	for kernel in [original,resumed]:
		if not check(kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"实际Alpha围城开局"): finish(); return
	for index in range(input.commands.size()):
		var command: Dictionary=input.commands[index]
		var first: Dictionary=original.submit(command)
		var second: Dictionary=resumed.submit(command)
		if not check(first.ok and second==first,"同输入同实际回执："+str(index)): finish(); return
		if index+1==input.restore_after_command:
			var saved: Dictionary=Codec.decode(Codec.encode(resumed.snapshot(),0,"siege-array-regression",resumed.content()),resumed.content())
			if not check(saved.ok and resumed.restore(saved.state).ok and Trace.normalize_state(original.snapshot())==Trace.normalize_state(resumed.snapshot()),"稳定围城节点完整保存恢复"): finish(); return
		if index==input.difference_command_index:
			var first_state: Dictionary=original.snapshot(); var second_state: Dictionary=resumed.snapshot()
			print("SIEGE_SAVE_ARRAY_ORIGINAL=",first_state.facts["siege:185:17:inner"].unit_ids)
			print("SIEGE_SAVE_ARRAY_RESTORED=",second_state.facts["siege:185:17:inner"].unit_ids)
			check(Trace.normalize_state(first_state)==Trace.normalize_state(second_state),"完整存读后下一命令轨迹一致，永久事实数组不可排序掩盖")
	finish()

## 严格失败汇总保留缺陷门；本任务不修改生产规则来强行变绿。
func finish() -> void:
	print("ARCHITECTURE_SAVE_REGRESSION_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
