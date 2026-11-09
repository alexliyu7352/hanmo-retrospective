## 精确新锁下同一真实NPC的城镇、路遇、简化战斗及v2存读身份门；tiny素材只证明节点合同。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Art=preload("res://ui/art_catalog.gd")
const Identity=preload("res://core/actor_identity.gd")
const Location=preload("res://core/world_location.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Battle=preload("res://ui/personal_battle_screen.gd")
const Codec=preload("res://core/save_codec.gd")
const Native=preload("res://storage/native_storage.gd")
var checks: int=0
var failures: int=0
var fixture: Dictionary={}
var receipts: Array=[]

## 资源及节点系统就绪后才开启已安装真实世界。
func _init() -> void:
	call_deferred("run")

## 失败进入退出码，不能用图片非空代替身份和状态一致。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
	return value

## 只等待真实节点布局和释放，不用等待代替世界advance。
func settle() -> void:
	await process_frame
	await process_frame

## 所有世界变化由当前真实版本与请求序号提交，NPC也采用同一个Kernel入口。
func send(main: Control, kind: String, params: Dictionary={}, actor: String="") -> bool:
	var state: Dictionary=main.kernel.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	var result: Dictionary=main.kernel.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	return check(result.get("ok",false),"正常命令 "+kind+" actor="+actor+" "+str(result.get("code","")))

## 从实际分层控件读取独立描述，保持同一World/Content，不在测试中造人物显示数据。
func portrait(main: Control, name: String, actor_id: String) -> Dictionary:
	var node=main.find_child(name,true,false)
	if not check(node!=null and node.has_method("describe"),"真实表现入口存在："+name): return {}
	var descriptor: Dictionary=node.describe()
	check(descriptor.get("actor_id")==actor_id,"控件绑定同一实际actor："+name)
	var image: Image=node.image_copy()
	check(image!=null and image.get_used_rect().has_area(),"既有tiny组件实际合成非空："+name)
	return descriptor

## 主身份、年龄阶段和当前服饰都从已投影的运行档案解析，战斗只切换其已登记身体方案。
func descriptor_ok(main: Control, descriptor: Dictionary, variant: Dictionary, view: String) -> void:
	var state: Dictionary=main.kernel.snapshot()
	var actor: String=variant.actor_id
	var expected_proportion: String="child" if int(variant.age)<=15 else "adult"
	check(descriptor.get("ok",false) and descriptor.get("degradation",["missing"]).is_empty(),"新锁表现解析无降级："+view)
	check(descriptor.get("actor_id")==actor and descriptor.get("identity_profile_id")==fixture.namespace+"::fixture.person","namespace/profile引用与真实actor一致："+view)
	check(descriptor.get("proportion")==expected_proportion and Identity.age_years(state,actor)==int(variant.age),"受控初态年龄正确，不继承77岁基础原型："+view)
	check(state.actors[actor].appearance.base_features==fixture.base_features and state.actors[actor].appearance.attire_style==variant.attire,"基础外貌与当前衣服保留在真实档案："+view)
	var garments: Array=descriptor.get("layers",[]).filter(func(layer): return layer.get("slot")=="garment")
	check(garments.size()==1 and garments[0].id==fixture.namespace+"::"+expected_proportion+".garment."+variant.attire+(".battle" if view=="battle" else ""),"服饰解析到当前年龄的确切已限定组件："+view)
	if view=="battle":
		var bindings: Variant=descriptor.get("selected_binding_id")
		check(descriptor.get("battle_style")=="simple" and bindings is Array and not bindings.is_empty() and bindings[0]==fixture.namespace+"::fixture.battle","简化战斗绑定链从同一身份登记方案开始")

## 浏览三次当前真实页面须保留完整state、内容、实例号、世界RNG及全局RNG。
func readonly_render(main: Control, label: String) -> void:
	var before: Dictionary=main.kernel.snapshot()
	var content: Dictionary=main.kernel.content()
	seed(9147); var expected: int=randi(); seed(9147)
	for index in range(3): main._render()
	await settle()
	check(main.kernel.snapshot()==before and main.kernel.content()==content,label+"浏览不改完整世界、请求序号和目录")
	check(randi()==expected,label+"浏览不消费全局RNG")

## 真实银行写出v2，冷Main从同一银行原件按元信息精确安装链恢复，期间不创建默认世界。
func save_restore(main: Control, variant: Dictionary, expected_body: Dictionary, expected_avatar: Dictionary) -> void:
	var context: Dictionary=main.kernel.save_context()
	var profile: String="locked-"+str(Time.get_ticks_usec())
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	var directory: String="user://profiles/"+profile+"/saves"
	var storage=Native.new(directory,context.catalog,context.registry)
	var text: String=Codec.encode(context.state,1,"manual-1",context.catalog,context.registry)
	var metadata: Dictionary=Codec.inspect_metadata(text)
	check(metadata.get("ok",false) and metadata.metadata.format_version==2 and metadata.metadata.pack_set_digest==variant.pack_set_digest,"真实v2封装携带本精确新锁")
	var written: Dictionary=storage.put("manual-1",text,0)
	if not check(written.get("ok",false),"实际Native完成写入并回读："+str(written.get("code",""))): return
	var read: Dictionary=storage.get_slot("manual-1")
	check(read.get("ok",false) and read.get("text")==text,"原生实际读取仍是同一代原字节")
	var path: String=storage.directory.path_join("manual-1."+read.suffix+".hmsave")
	var before_sha: String=FileAccess.get_sha256(path)
	var cold=Main.new(); root.add_child(cold); await settle()
	check(cold.kernel.snapshot().is_empty() and not cold._started,"冷Main未建立默认世界")
	var restored: Dictionary=cold.import_snapshot(path)
	if check(restored.get("ok",false),"冷Main沿实际v2元信息完整恢复："+str(restored.get("code",""))):
		await settle()
		check(cold.kernel.snapshot()==context.state and cold.kernel.content()==context.catalog,"恢复保留完整身份/appearance/RNG与精确目录")
		check(cold.kernel.content().pack_set_digest==variant.pack_set_digest,"多个同namespace版本并存仍选择原锁")
		var screen=cold.find_child("PersonalBattleScreen",true,false)
		if check(screen!=null,"冷恢复继续显示真实个人战节点"):
			check(screen.canvas.battle_descriptors[variant.actor_id]==expected_body and screen.canvas.identity_descriptors[variant.actor_id]==expected_avatar,"恢复后的战斗身体与身份头像描述逐字段相同")
		await readonly_render(cold,"冷恢复战斗")
	check(FileAccess.get_sha256(path)==before_sha and main.kernel.snapshot()==context.state,"恢复读取没有写银行或改变源世界")
	receipts.append({"variant":variant.id,"bank":path,"sha256":before_sha,"lock":variant.pack_set_digest})
	cold.queue_free(); await settle()

## 同一真实NPC先在城镇显示，再正常行旅反向相遇和持械战斗；两段推进各硬限四次。
func check_variant(variant: Dictionary, catalog: Dictionary) -> Dictionary:
	var main=Main.new(); root.add_child(main); await settle()
	var started: Dictionary=main.start_content(variant.scenario,catalog.catalog_path,{},variant.player_id)
	if not check(started.get("ok",false),"新锁正常开局及显式同一玩家选择："+str(started.get("code",""))):
		main.queue_free(); await settle(); return {}
	await settle()
	check(main.kernel.content().pack_set_digest==variant.pack_set_digest and main.kernel.snapshot().player_actor_id==variant.player_id,"开局锁和所选玩家身份明确")
	var npc: String=variant.actor_id
	var initial: Dictionary=main.kernel.snapshot()
	check(initial.actors[npc].birth_tick==-int(variant.age)*720 and initial.actors[npc].appearance.source=="fixed","合法初态建立受控birth_tick与固定外貌")
	var town: Dictionary=portrait(main,"ScenePersonPortrait",npc)
	descriptor_ok(main,town,variant,"half_body")
	await readonly_render(main,"城镇")
	if not send(main,"travel",{"destination":fixture.namespace+"::village.east"},npc):
		main.queue_free(); await settle(); return {}
	var arrived: bool=false
	for tick in range(int(fixture.advance_budget_per_leg)):
		if not send(main,"advance"): break
		if Location.resolve(main.kernel.snapshot(),npc).get("place_id")==fixture.namespace+"::east.entrance": arrived=true; break
	if not check(arrived,"NPC在四次正常advance预算内实际抵达东村"):
		main.queue_free(); await settle(); return {}
	if not send(main,"travel",{"destination":fixture.namespace+"::city.west"},npc) or not send(main,"travel",{"destination":fixture.namespace+"::village.east"}):
		main.queue_free(); await settle(); return {}
	var meeting: Dictionary={}
	for tick in range(int(fixture.advance_budget_per_leg)):
		if not send(main,"advance"): break
		meeting=Encounters.pending_for(main.kernel.snapshot(),variant.player_id)
		if not meeting.is_empty(): break
	if not check(not meeting.is_empty() and npc in meeting.leaders,"四次正常advance预算内由真实接触生成同人会面"):
		main.queue_free(); await settle(); return {}
	main._render(); await settle()
	var encounter: Dictionary=portrait(main,"EncounterPortrait",npc)
	descriptor_ok(main,encounter,variant,"half_body")
	check(encounter.identity_key==town.identity_key and encounter.identity_profile_id==town.identity_profile_id and encounter.layers==town.layers,"城镇与会面保留同一身份和当前衣服图层")
	await readonly_render(main,"会面")
	if not send(main,"encounter_fight",{"encounter_id":meeting.id}):
		main.queue_free(); await settle(); return {}
	main._render(); await settle()
	var state: Dictionary=main.kernel.snapshot()
	var screen=main.find_child("PersonalBattleScreen",true,false)
	if not check(screen!=null and not Battle.active(state).is_empty(),"真实encounter_fight建立个人战并进入现有屏幕"):
		main.queue_free(); await settle(); return {}
	var body: Dictionary=screen.canvas.battle_descriptors[npc]
	var avatar: Dictionary=screen.canvas.identity_descriptors[npc]
	descriptor_ok(main,body,variant,"battle")
	descriptor_ok(main,avatar,variant,"avatar")
	check(body.identity_key==town.identity_key and avatar.identity_key==town.identity_key and body.display_name==town.display_name,"三处与战斗头像均使用同一实际姓名和身份键")
	check(screen.canvas._portraits[npc]!=null and screen.canvas._identity_portraits[npc]!=null and screen.canvas._full_figures[npc],"已登记简化身体和身份头像都实际生成纹理")
	check(state.actors[npc].appearance==initial.actors[npc].appearance and Identity.age_years(state,npc)==int(variant.age),"有限正常移动及战斗不重抽外貌或伪增长年龄")
	await readonly_render(main,"个人战")
	await save_restore(main,variant,body,avatar)
	var result: Dictionary={"town":town,"body":body,"appearance":state.actors[npc].appearance,"age":Identity.age_years(state,npc)}
	main.queue_free(); await settle()
	return result

## 只跑两组有限新锁节点合同；自然长大、动态收养、OS视觉和旧V0整套均不运行。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/locked_appearance_isolated",false):
		printerr("FAIL: isolated locked-appearance fixture required"); quit(1); return
	var project: String=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	fixture=JSON.parse_string(FileAccess.get_file_as_string(project.path_join("fixture.json")))
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.get("ok",false) and installed.get("catalogs",[]).size()==2,"正式compose/stage的两代精确新锁均通过安装校验："+str(installed.get("code",""))):
		quit(1); return
	var results: Array=[]
	for variant in fixture.variants:
		var matches: Array=installed.catalogs.filter(func(c): return c.pack_set_digest==variant.pack_set_digest)
		if check(matches.size()==1,"按精确锁唯一选择版本："+variant.id): results.append(await check_variant(variant,matches[0]))
	if results.size()==2 and not results[0].is_empty() and not results[1].is_empty():
		check(results[0].town.identity_key==results[1].town.identity_key and results[0].appearance.base_features==results[1].appearance.base_features,"两组受控年龄/衣服保留同actor同基础外貌身份")
		check(results[0].age==24 and results[1].age==12 and results[0].town.proportion=="adult" and results[1].town.proportion=="child","年龄来自不同新锁的合法initial，未进行自然年龄推进")
		check(results[0].appearance.attire_style=="plain" and results[1].appearance.attire_style=="formal" and results[0].town.layers!=results[1].town.layers,"当前年龄和衣服各自解析到已登记组件")
	print("PACK_APPEARANCE_IDENTITY_CHECKS=",checks," FAILURES=",failures," SAVES=",receipts)
	print("TINY_NODE_CONTRACT=true; REAL_TRAVEL_ENCOUNTER_BATTLE_V2=true; OS_VISUAL=NOT_RUN; NATURAL_AGE=NOT_RUN")
	quit(0 if failures==0 else 1)
