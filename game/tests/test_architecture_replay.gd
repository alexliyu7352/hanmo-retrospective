## 验证无损规范化与真实Kernel失败零副作用，未实现合同必须明确红灯。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0

## 引擎准备完成后执行，缺测试辅助实现是断言失败而非解析错误。
func _init() -> void:
	call_deferred("run_checks")

## 记录实际断言，退出码不允许吞掉失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 全快照比较保留所有缓存和随机流，不把无副作用缩成单一钱包断言。
func run_checks() -> void:
	if not ResourceLoader.exists("res://tests/architecture_trace.gd"):
		check(false,"缺少无损规范化和实际Kernel轨迹合同")
		finish(); return
	var trace=load("res://tests/architecture_trace.gd")
	var artifact_contract: bool=trace.has_method("write_artifact") and trace.has_method("output_error")
	check(artifact_contract,"缺少持久读回及中断证据不可覆盖合同")
	if artifact_contract:
		var folder: String=OS.get_environment("TMPDIR").path_join("a01-artifact-test-"+str(Time.get_ticks_usec()))
		check(trace.output_error(folder,"").is_empty(),"新独立输出目录可用")
		var path: String=folder.path_join("partial.hmsave")
		check(trace.write_artifact(path,"原完整字节") and FileAccess.get_file_as_string(path)=="原完整字节","原子写出后实际持久字节读回")
		check(not trace.write_artifact(path,"覆盖") and FileAccess.get_file_as_string(path)=="原完整字节","中断产物禁止覆盖且原字节保留")
		check(not trace.output_error(folder,"").is_empty(),"只有partial没有final仍拒绝旧目录")
		check(not trace.write_artifact(folder.path_join("missing/partial.json"),"{}"),"缺失父目录写入明确失败")
		var log_folder: String=folder+"-collector"
		DirAccess.make_dir_recursive_absolute(log_folder)
		var log=FileAccess.open(log_folder.path_join("run.log"),FileAccess.WRITE); log.store_string("current attempt"); log.close()
		check(trace.output_error(log_folder,"run.log").is_empty() and not trace.output_error(log_folder,"").is_empty(),"仅明确当前采集日志可在启动前存在")
	var sample: Dictionary={"z":["b","a"],"a":{"rng":"18446744073709551615","id":"actor:07","balance":800}}
	var old: Dictionary=sample.duplicate(true)
	var normalized: Dictionary=trace.normalize_state(sample)
	check(trace.state_digest(sample)==JSON.stringify(normalized,"",true,true).sha256_text(),"引擎直接摘要等价显式递归规范化")
	check(normalized==old and sample==old and normalized.z==["b","a"],"字典规范化保留数组、RNG、ID和金额且不修改输入")
	for field in ["balance","rng","array"]:
		var changed: Dictionary=old.duplicate(true)
		if field=="balance": changed.a.balance+=1
		if field=="rng": changed.a.rng="18446744073709551614"
		if field=="array": changed.z.reverse()
		check(trace.state_digest(old)!=trace.state_digest(changed),"故意修改必须被检出："+field)
	var k=Kernel.new()
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实Alpha开局")
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	var invalid: Dictionary={"kind":"unknown_baseline_command","params":{},"actor_id":actor,"request_seq":1,"expected_revision":before.revision}
	check(not k.submit(invalid).ok and k.snapshot()==before and k.snapshot().rng==before.rng,"注入失败命令保留完整快照与RNG")
	invalid.kind="wait"; invalid.expected_revision=-1
	check(k.submit(invalid).get("code")=="StalePreview" and k.snapshot()==before,"过时revision失败零副作用")
	invalid.expected_revision=before.revision
	var result: Dictionary=k.submit(invalid); var applied: Dictionary=k.snapshot()
	check(result.ok and k.submit(invalid)==result and k.snapshot()==applied,"重复request_seq同结果且完整快照不变")
	var decoded: Dictionary=Codec.decode(Codec.encode(applied,0,"architecture-replay",k.content()),k.content())
	check(decoded.ok and k.restore(decoded.state).ok and trace.normalize_state(applied)==trace.normalize_state(k.snapshot()),"完整存读轨迹一致")
	finish()

## 统一汇总用于唯一验证入口的断言计数。
func finish() -> void:
	print("ARCHITECTURE_REPLAY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
