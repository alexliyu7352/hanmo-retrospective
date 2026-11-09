## 说话者画像单人物门面：复用旧真实caller纯输入oracle并核正式Kernel；不扩Host/姓名查询或资源验收。
extends "res://tests/test_character_input_callers.gd"
const Kernel=preload("res://core/world_kernel.gd")
const CharacterInputQuery=preload("res://core/views/character_input.gd")
const CONVERSATION_ORACLE="9bc43900328f3dc3bbc1c162d1aef2f8516b6ecf1d9cb44d805434de34c79bcd"
var paths_reported: bool=false

## 只计真实snapshot调用；不提供假的character_input实现，缺方法必须作为红门。
class SnapshotKernel extends "res://core/world_kernel.gd":
	var snapshot_calls: int=0
	## 完整快照继续委托原实现，计数只说明画像专用复制是否已被移除。
	func snapshot() -> Dictionary:
		snapshot_calls+=1; return super.snapshot()

## 复用旧caller明确的最小输入，真实Main/Kernel方法运行；不声称该纯查询fixture通过世界Schema。
func observed_main(f: Dictionary):
	var main=main_for(f)
	var kernel=SnapshotKernel.new(); kernel._state=f.state; kernel._catalog={"data":f.data}; kernel._registry={"commands":{}}
	main.kernel=kernel
	return main

## 记录实际Composer输出、缓存和纹理失败；次数单独断言，不混进原外观语义oracle。
func conversation_case(kind: String,capturing: bool) -> Dictionary:
	var f: Dictionary=fixture()
	var view: Dictionary={"phase":"say","speaker":"actor:7","context":{"provider_actor_id":"npc"},"text":"只测试实际说话者"}
	if kind=="provider": view.erase("speaker")
	if kind=="missing_binding": f.data.actors.npc.presentation_profile_id="absent"
	if kind=="unknown_age": f.state.actors["actor:7"].erase("birth_tick")
	if kind=="bad_profile": f.state.actors["actor:7"].appearance={}
	var before: Dictionary=f.duplicate(true); var main=observed_main(f)
	if not paths_reported:
		print("CHARACTER_QUERY_PATHS ",JSON.stringify({"user_data_dir":OS.get_user_data_dir(),"computed_native_directory":main.storage.native.directory,"native_operations":0}))
		paths_reported=true
	var stage=Control.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	Character.clear_cache(); registry.reads.clear(); main.kernel.snapshot_calls=0
	Screen.conversation(main,stage,view)
	var used: int=main.kernel.snapshot_calls
	if not capturing: check(used==1,"只移除speaker画像快照，姓名标题仍留一次 "+kind+"/"+str(used))
	check(f==before and main.kernel._state==before.state and not main.is_inside_tree() and not main.storage._busy,"实际caller只读且Main离树 "+kind)
	var portrait: Control=stage.find_child("SpeakerPortrait",true,false)
	var values: Array=cached_descriptors()
	if kind!="bad_profile": check(values.size()==1 and values[0].actor_id==("npc" if kind=="provider" else "actor:7"),"原实际speaker/回退provider保持 "+kind)
	else: check(values.is_empty(),"坏外观先拒绝，不伪造缓存画像")
	var captions: Array=[]
	for label in main.main_column.find_children("*","Label",true,false): captions.append(label.text)
	var output: Dictionary={"result":portrait.get_meta("render_result").duplicate(true),"descriptors":values,"reads":registry.reads.duplicate(true),"captions":captions}
	check(output.result.get("code")==("InvalidRenderDescriptor" if kind=="bad_profile" else "MissingRenderTexture"),"原缺图/坏外观诊断仍明确 "+kind)
	stage.free(); main.free()
	return output

## 正式tiny开局的门面与原叶同值且不拿snapshot，输出变更不能回写世界或下一查询。
func kernel_queries() -> void:
	var kernel=SnapshotKernel.new()
	var available: bool=kernel.has_method("character_input")
	check(available,"实际Kernel公开单人物入口存在")
	if not available: return
	check(kernel.call("character_input","actor.player")=={"ok":false,"code":"InvalidPresentationActor"},"空世界仍沿原叶错误，不增NoWorld首错")
	var started: Dictionary=kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json")
	check(started.ok,"真实tiny Kernel开局验证门面")
	if not started.ok: return
	var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	kernel.snapshot_calls=0; seed(904); var random_value: int=randi(); seed(904)
	for id in ["actor.player","actor.chen","missing",""]:
		var expected: Dictionary=CharacterInputQuery.query(state,data,id)
		var value: Dictionary=kernel.call("character_input",id)
		check(value==expected,"门面逐值复用原单人物叶 "+id)
		if value.ok:
			var keys: Array=value.keys(); keys.sort()
			check(keys==["actor_view","appearance_view","ok"],"不返回第二state/data "+id)
			value.actor_view.display_name="forged"; value.appearance_view.base_features.injected=true
			check(kernel.call("character_input",id)==expected,"返回值与后续查询深隔离 "+id)
	check(kernel.snapshot_calls==0 and kernel._state==state and kernel._catalog.data==data and randi()==random_value,"纯门面不复制全世界/写状态/消耗全局随机")
	var f: Dictionary=fixture(); kernel._state=f.state; kernel._catalog={"data":f.data}
	f.state.actors.npc.prototype_id="missing"; f.state.actors.npc.appearance=false
	check(kernel.call("character_input","npc")=={"ok":false,"code":"InvalidPresentationActor"},"纯查询坏定义先于坏外观")
	f=fixture(); kernel._state=f.state; kernel._catalog={"data":f.data}; f.state.actors.npc.appearance=false
	check(kernel.call("character_input","npc")=={"ok":false,"code":"InvalidAppearanceProfile"},"纯查询显式坏外观不改legacy")

## capture只供旧caller首次冻结，正常门固定比较摘要，不允许新实现重新录oracle。
func _run() -> void:
	root.size=Vector2i(1280,720)
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string(MANIFEST)).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var outputs: Dictionary={}
	for kind in ["explicit_speaker","provider","missing_binding","unknown_age","bad_profile"]: outputs[kind]=conversation_case(kind,not capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE)
		file.store_string(JSON.stringify({"caller_sha256":FileAccess.get_sha256("res://ui/screen_composer.gd"),"kernel_sha256":FileAccess.get_sha256("res://core/world_kernel.gd"),"outputs":outputs,"sha256":digest(outputs)},"\t",true,true)+"\n"); file.close()
		print("CONVERSATION_OLD_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==CONVERSATION_ORACLE,"真实旧caller所有描述/缓存/错误/姓名逐值oracle保持")
		kernel_queries()
	Art._registry=previous; Character.clear_cache()
	print("KERNEL_CHARACTER_INPUT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
