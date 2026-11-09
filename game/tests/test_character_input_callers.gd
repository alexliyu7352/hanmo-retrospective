## 真实Host/Composer有限调用门；Main离树、专用输入无完整世界验收，缺图原样失败而不伪造GUI。
extends SceneTree
const Host=preload("res://ui/scene_host.gd")
const Screen=preload("res://ui/screen_composer.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Character=preload("res://ui/character_presentation.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Strict=preload("res://core/strict_json.gd")
const MANIFEST="res://../tests/fixtures/character_presentation/manifest.json"
const ORACLE_SHA="50bc8ad5f410223c66819a259883e81da68c6800ce794b7a591d0a0fbe1fa3f4"
## 从上面8b45原oracle仅扣除两隐藏按钮字段、256x320缓存和两次纹理请求，非重录新实现。
const VISIBLE_ORACLE_SHA="1ca1952e4821440e1a5a2e74a003d43c2301599b5fb613e6b8b54bef92edb838"
var checks: int=0
var failures: int=0
var registry: RefCounted

## 只读原manifest，明确记录资源请求并返回缺图；不能把描述对照算作资源通过。
class DescriptorRegistry extends RefCounted:
	var manifest: Dictionary={}
	var reads: Array=[]
	## 深复制真实清单以隔离本门对运行资源的引用。
	func _init(value: Dictionary) -> void: manifest=value.duplicate(true)
	## 与正式注册表相同返回独立描述，不读取磁盘图片。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 缺图按实际能力返回null，保留生产控件的MissingRenderTexture路径。
	func texture(group: String, id: String) -> Texture2D:
		reads.append([group,id]); return null

## 引擎就绪后运行，Main本体永远不加入树以免触发Native或启动流程。
func _init() -> void: call_deferred("_run")

## 所有失败进入退出码，不以引擎退出0掩盖断言失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 专用caller输入仅有实际读取字段；不冒称通过完整世界schema或new_game。
func fixture() -> Dictionary:
	var location: Dictionary={"kind":"Settlement","settlement_id":"town","place_id":"room"}
	var state: Dictionary={"tick":0,"revision":7,"player_actor_id":"player","actors":{},"roles":{},"actions":{},"interactions":{},"events":{},"encounters":{},"rng":{"fixture":"unchanged"}}
	var data: Dictionary={"actors":{},"roles":{},"texts":{},"props":{},"estates":{},"dialogues":{},"required_capabilities":[],"settlements":{"town":{"id":"town","entry_place":"room"}},"places":{}}
	for place in ["room","elsewhere"]: data.places[place]={"id":place,"settlement_id":"town","parent_place":null,"passages":[],"name_key":place}
	for id in ["player","npc","actor:7","hidden","dead"]:
		state.actors[id]={"id":id,"location":location.duplicate(true),"alive":id!="dead","available":true,"action_id":"","birth_tick":-24*720,"wallet_id":"private.wallet","skills":{"private":999}}
		data.actors[id]={"id":id,"name_key":"name."+id,"portrait_kind":"adult.dedicated.a","presentation_profile_id":"fixture.person"}
		data.texts["name."+id]="人物"+id
	state.actors.hidden.location.place_id="elsewhere"
	state.actors.hidden.appearance={"private":"must not project"}
	state.actors.dead.appearance=false
	state.actors.npc.appearance=Appearance.create("npc","unused",{"kind":"fixed","generator_version":"appearance-v1","attire_style":"plain","base_features":{"face_shape":"round","eyes":"calm","brows":"straight","nose":"plain","mouth":"plain","hair":"short","beard":"none","skin_palette":"warm","build":"slender"}}).profile
	state.actors["actor:7"].prototype_id="npc"; state.actors["actor:7"].display_name="后来的人"
	state.actors["actor:7"].appearance=state.actors.npc.appearance.duplicate(true)
	state.actors["actor:7"].appearance.source="inherited"; state.actors["actor:7"].appearance.generation_seed="private.child.seed"
	state.actors["actor:7"].appearance.parent_sources=[{"feature":"face_shape","source":"parent","actor_id":"private.parent","value":"round","generator_version":"appearance-v1","generation_seed":"private.parent.seed"}]
	data.actors.erase("actor:7")
	for pair in [["role.a","npc"],["role.b","npc"],["role.hidden","hidden"],["role.dead","dead"]]:
		data.roles[pair[0]]={"id":pair[0],"name_key":pair[0],"place_id":"room","services":[]}
		state.roles[pair[0]]={"actor_id":pair[1],"enabled":true}
	return {"state":state,"data":data}

## 真Main只作为方法宿主，空闲构造节点统一附属以便释放；不触发_ready。
func main_for(f: Dictionary):
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main._data=f.data; main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry={"commands":{}}
	return main

## 摘要绑定完整结果；不在常规运行更新旧oracle。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 稳定收集实际resolver缓存，额外隐藏按钮描述会使范围oracle不匹配。
func cached_descriptors() -> Array:
	var values: Array=[]
	var keys: Array=Character._descriptors.keys(); keys.sort()
	for key in keys: values.append(Character._descriptors[key].duplicate(true))
	return values

## 通过真实Host配置保留早退和错误位置，故意使用声明缺模板避开不存在的绘景资源。
func host_case(kind: String) -> Dictionary:
	var f: Dictionary=fixture(); var place: String="room"
	if kind!="unmapped": f.data.places.room.scene_template_id="missing.template"
	if kind=="unknown": place="elsewhere"
	if kind=="bad_visible": f.state.actors.npc.appearance={}
	if kind=="empty_mapping": f.data.places.room.scene_template_id=""
	var before: Dictionary=f.duplicate(true)
	Character.clear_cache(); registry.reads.clear()
	var host=Host.new(); host.embedded_list=false
	var result: Dictionary=host.configure(f.state,f.data,place,"interior",Callable())
	var projection: Dictionary=host.describe_projection()
	var public_people: Array=[]
	for person in projection.get("people",[]): public_people.append(person.duplicate(true))
	check(f==before,"Host查询不改原输入 "+kind)
	check(registry.reads.is_empty(),"Host短路/缺模板不读图片 "+kind)
	if kind=="unmapped": check(result=={"ok":true,"mode":"legacy"} and not public_people[0].has("render_descriptor") and Character._descriptors.is_empty(),"无模板先返回旧场景，不解析外观")
	elif kind=="unknown": check(result.get("code")=="SceneKnowledgeUnavailable" and public_people.is_empty() and Character._descriptors.is_empty(),"未知内景先拒绝，不读取远方坏外观")
	elif kind=="empty_mapping": check(result.get("code")=="InvalidSceneTemplateMapping" and Character._descriptors.is_empty(),"坏空映射先拒绝，不改成无模板fallback")
	else:
		check(result.get("code")=="MissingSceneTemplate" and public_people.size()==3,"声明缺模板保留原诊断和三个在场人物")
		for person in public_people:
			check(not person.actor_id in ["hidden","dead"] and person.has("render_descriptor"),"仅实际可见人在原位置解析 "+person.actor_id)
	var output: Dictionary={"code":result.get("code",""),"mode":result.mode,"people":public_people,"cache":cached_descriptors(),"reads":registry.reads.duplicate(true)}
	host.free()
	return output

## 真实Composer名单、悬停绑定和对白入口；节点仅用于调用检查，纹理缺失保留为失败。
func composer_case(kind: String) -> Dictionary:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true); var main=main_for(f)
	var stage=Control.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	Character.clear_cache(); registry.reads.clear()
	if kind=="conversation": Screen.conversation(main,stage,{"phase":"say","speaker":"actor:7","context":{"provider_actor_id":"npc"},"text":"只测试实际说话者"})
	else: Screen.inhabitants(main,stage,main.kernel.place_sidebar(),f.data.places.room,{"phase":"say" if kind=="inactive" else "scene"})
	var buttons: Array=[]
	for button in stage.find_children("*","Button",true,false):
		if button.get_meta("command_kind","")!="talk": continue
		var focus: Dictionary={}
		for connection in button.mouse_entered.get_connections():
			var args: Array=connection.callable.get_bound_arguments()
			if args.size()==4 and args[2] is Dictionary: focus=args[2].duplicate(true)
		buttons.append({"role_id":button.get_meta("target_id"),"old_main_descriptor":button.render_descriptor.duplicate(true),"focus":focus,"show_portrait":button.show_portrait})
	var portraits: Array=[]
	for name in ["ScenePersonPortrait","SpeakerPortrait"]:
		var portrait: Node=stage.find_child(name,true,false)
		if portrait!=null: portraits.append({"name":name,"result":portrait.get_meta("render_result").duplicate(true)})
	check(f==before and main.kernel.snapshot()==before.state and not main.is_inside_tree() and not main.storage._busy,"真实Composer离树Main调用只读 "+kind)
	if kind=="inhabitants":
		check(buttons.size()==2 and buttons[0].role_id=="role.a" and buttons[1].role_id=="role.b","只列同地点活人，两岗位保留")
		for row in buttons: check(row.focus.actor_id=="npc" and row.old_main_descriptor.is_empty() and not row.show_portrait,"隐藏按钮没有旧画像，焦点身份保持")
	elif kind=="inactive": check(buttons.is_empty() and portraits.is_empty() and Character._descriptors.is_empty(),"非scene名单在人物解析前短路")
	else: check(cached_descriptors().size()==1 and cached_descriptors()[0].actor_id=="actor:7","对白使用实际speaker而不是provider")
	for portrait in portraits: check(portrait.result.get("code")=="MissingRenderTexture","缺图原样诊断，不伪造画像成功")
	var output: Dictionary={"buttons":buttons,"portraits":portraits,"cache":cached_descriptors(),"reads":registry.reads.duplicate(true)}
	stage.free(); main.free()
	return output

## 仅删去已单独断言为空的旧按钮字段，其他caller输出继续比较原oracle的机械范围变换。
func visible_outputs(outputs: Dictionary) -> Dictionary:
	var visible: Dictionary=outputs.duplicate(true)
	for button in visible.composer_inhabitants.buttons: button.erase("old_main_descriptor")
	return visible

## 公共查询只返回指定人物的11项公开信息；返回副本不能改原件或下次查询。
func public_query_checks(query) -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true)
	seed(904); var expected_random: int=randi(); seed(904)
	for id in ["player","npc","actor:7"]:
		var value: Dictionary=query.query(f.state,f.data,id)
		check(value.ok,"可见单人物查询 "+id)
		if not value.ok: continue
		var actor_keys: Array=value.actor_view.keys(); actor_keys.sort()
		var appearance_keys: Array=value.appearance_view.keys(); appearance_keys.sort()
		var result_keys: Array=value.keys(); result_keys.sort()
		check(result_keys==["actor_view","appearance_view","ok"],"查询不夹带世界或私密原件 "+id)
		check(actor_keys==["actor_id","age_years","display_name","portrait_kind","presentation_profile_id","visibility_version"] and appearance_keys==["attire_style","base_features","format_version","generator_version","legacy"],"公开键白名单 "+id)
		check(value.actor_view.actor_id==id and value.actor_view.visibility_version==7 and not "private" in JSON.stringify(value),"实际身份/版本且不泄露私密字段 "+id)
		check(Art.new().call("character_input",value,"half_body",{})==Art.character(id,f.state,f.data,"half_body"),"新薄适配与旧入口完整描述一致 "+id)
		value.appearance_view.base_features.injected="changed"; value.actor_view.display_name="changed"
		check(f==before and query.query(f.state,f.data,id).actor_view.display_name!="changed","返回值深隔离 "+id)
	check(query.query(f.state,f.data,"missing")=={"ok":false,"code":"InvalidPresentationActor"},"不存在人物不投影替身")
	f.state.actors.npc.prototype_id="missing"; f.state.actors.npc.appearance=false
	check(query.query(f.state,f.data,"npc")=={"ok":false,"code":"InvalidPresentationActor"},"缺失真实定义先拒绝")
	f=fixture(); f.state.actors.npc.appearance=false
	check(query.query(f.state,f.data,"npc")=={"ok":false,"code":"InvalidAppearanceProfile"},"显式坏外观不伪装legacy")
	f=fixture(); f.state.actors.npc.erase("birth_tick")
	check(query.query(f.state,f.data,"npc").actor_view.age_years==-1,"未知年龄沿原-1，不编成年年龄")
	check(Art.new().call("character_input",query.query(f.state,f.data,"npc"),"half_body",{})==Art.character("npc",f.state,f.data,"half_body"),"未知年龄原fallback及cache不变")
	f=fixture(); f.state.actors.hidden=false; f.data.actors.hidden=false
	check(query.query(f.state,f.data,"npc").ok,"单人物query不扫描无关坏人物")
	f=fixture(); f.state.actors.hidden.erase("appearance")
	check(query.query(f.state,f.data,"hidden").ok,"查询本身没有隐藏人物授权，须依赖实际caller筛选")
	var failure: Dictionary={"ok":false,"code":"InvalidAppearanceProfile"}
	check(Art.new().call("character_input",failure,"half_body",{})==failure,"薄适配返回原错误")
	check(randi()==expected_random,"查询与描述不消费全局RNG")

## 先记录旧真实caller，再检查新接口；capture模式只供首次冻结，普通门绑定固定摘要。
func _run() -> void:
	root.size=Vector2i(1280,720)
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string(MANIFEST)).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	var outputs: Dictionary={}
	for kind in ["mapped","bad_visible","unmapped","unknown","empty_mapping"]: outputs["host_"+kind]=host_case(kind)
	for kind in ["inhabitants","inactive","conversation"]: outputs["composer_"+kind]=composer_case(kind)
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"outputs":outputs,"sha256":digest(outputs)},"\t",true,true)+"\n"); file.close()
		print("CHARACTER_CALLER_ORACLE=",digest(outputs))
	else:
		check(digest(visible_outputs(outputs))==VISIBLE_ORACLE_SHA,"8b45原caller oracle仅扣除批准删除的隐藏按钮开销")
		var exists: bool=FileAccess.file_exists("res://core/views/character_input.gd")
		check(exists,"有限单人物query入口存在")
		check(Art.new().has_method("character_input"),"Art公开输入薄适配存在")
		if exists and Art.new().has_method("character_input"): public_query_checks(load("res://core/views/character_input.gd"))
	Art._registry=previous; Character.clear_cache()
	print("CHARACTER_INPUT_CALLERS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
