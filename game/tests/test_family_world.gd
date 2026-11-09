## 正常收养、逐半日生活与合法退隐贯通同一Kernel；不改出生日期、不伪造继任事实或玩家身份。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Identity=preload("res://core/actor_identity.gd")
const Codec=preload("res://core/save_codec.gd")
const Schema=preload("res://core/state_schema.gd")
var checks: int=0
var failures: int=0
var capture_dir: String=""

## 延迟到正式引擎初始化后执行持续推进，测试存档仍由独立profile隔离。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="): capture_dir=arg.trim_prefix("--capture-dir=")
	call_deferred("run_checks")

## 记录领域含义与具体失败，禁止以最后状态掩盖中途无法推进。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 只记录大阶段起止和已有计数，不读取世界快照或改变推进、断言与存读输入。
func phase(name: String) -> void:
	print("FAMILY_PHASE ",name," elapsed_ms=",Time.get_ticks_msec()," checks=",checks," failures=",failures)

## 在正式目录开启同一个Alpha世界，所有后续变化都经普通命令提交。
func fresh():
	var k=Kernel.new()
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正常创建带年龄与家庭规则的集成世界")
	return k

## 正常完成指定半日，同时记录壁钟以观察长期事实增长的实际成本。
func advance(k, ticks: int) -> bool:
	var began: int=Time.get_ticks_msec()
	phase("advance_begin:"+str(ticks))
	for index in range(ticks):
		var result: Dictionary=k.advance_one_tick()
		if not result.ok: phase("advance_failed:"+str(index)); printerr("FAMILY_ADVANCE_FAILED step=",index," result=",result); return false
		if (index+1)%180==0: print("FAMILY_LONG_RUN halfdays=",index+1," elapsed_ms=",Time.get_ticks_msec()-began)
	phase("advance_end:"+str(ticks))
	return true

## 通过正式字节存档恢复另一内核，下一半日必须得到完全相同的世界与一次性消费。
func replay(k) -> bool:
	phase("replay_codec_begin")
	var saved: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"family-world",k.content()),k.content())
	phase("replay_codec_end")
	if not saved.ok: printerr("FAMILY_DECODE: ",saved); return false
	var other=Kernel.new()
	if not other.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok or not other.restore(saved.state).ok: return false
	return k.advance_one_tick().ok and other.advance_one_tick().ok and k.snapshot()==other.snapshot()

## 收养与家庭休整共用庄园私粮，子女成年以后正式继任并继续外出，旧公库和前代知识保持边界。
func run_checks() -> void:
	phase("world_begin")
	var k=fresh()
	if failures: finish(); return
	var state: Dictionary=k.snapshot(); var parent: String=state.player_actor_id
	check(Identity.age_years(state,parent)==42,"人物年龄来自作者初始年龄和唯一日期")
	var tampered: Dictionary=state.duplicate(true); tampered.actors[parent].birth_tick-=720
	check(not Schema.validate(tampered,k.content()).ok,"不能修改初始出生日期冒充退休资格")
	check(F.send(k,"move_local",{"place_id":"west.estate"}).ok,"实际进入自有庄园")
	var context: Dictionary=F.context(k,"role.estate_steward")
	var quote: Dictionary=k.preview("family_adopt",{"context":context,"display_name":"阿宁"})
	check(quote.ok and quote.duration==2 and quote.cost==40,"收养由真实管事当面办理，冻结两半日与实际费用："+str(quote))
	var before: Dictionary=k.snapshot()
	check(F.send(k,"family_adopt",{"context":context,"display_name":"阿宁"}).ok,"开始手续而非直接生成继任人物")
	state=k.snapshot()
	check(state.actors.size()==before.actors.size() and state.wallets["wallet.player"].balance==800 and state.wallets["wallet.player"].reserved.size()==1,"手续期间仅保留费用，没有提前创建人物或付钱")
	check(replay(k),"收养中途字节读档后同一半日得到相同状态")
	check(advance(k,1),"统一时钟真正完成收养")
	state=k.snapshot()
	if state.family.children.size()!=1: check(false,"收养须生成且仅生成一位子女："+str(state.family)); finish(); return
	var child: String=state.family.children.keys()[0]
	check(child.begins_with("actor:") and not k.content().data.actors.has(child) and state.actors.size()==before.actors.size()+1,"孩子具有新动态ID，初始人物完整保留")
	check(Identity.age_years(state,child)==16 and state.actors[child].created_tick==2 and state.tick==2,"创建与出生日期精确对应正常工期")
	check(state.wallets["wallet.player"].balance==760 and state.wallets["wallet.steward"].balance==40 and state.wallets["wallet.player"].reserved.is_empty(),"实际完成只支付一次手续费用")
	check(state.actors[child].combat.learned.is_empty() and state.knowledge[child].is_empty() and state.wallets[state.actors[child].wallet_id].balance==0,"动态子女没有复制前代武学、任务信息或钱包")
	check(k.preview("family_designate",{"heir_actor_id":child}).get("code")=="FamilyHeirNotAdult","未成年不能指定继任")
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":180}).ok,"在自己实际庄园开始一季休整")
	check(advance(k,2) and replay(k),"休整同样支持稳定半日存档和后续重放")
	state=k.snapshot(); var rest_id: String=state.actors[parent].action_id
	check(state.facts[rest_id].consumed==6 and state.facts[rest_id].last_mouths.size()==2 and state.estates["estate.riverside"].last_tick==state.tick and state.social.last_tick==state.tick,"父子三半日吃六份真实私粮，庄务与NPC社会使用同一日期")
	check(F.send(k,"rest_cancel").ok and k.snapshot().tick==state.tick and k.snapshot().facts[rest_id].stop_reason=="cancelled","随时结束长期休整，不加日期、不退已吃私粮")
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":720}).ok and advance(k,720),"正常生活一年而非改存档日期")
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":717}).ok and advance(k,717),"再经实际717半日，补足收养后的两年成长")
	if failures: finish(); return
	state=k.snapshot()
	phase("adult_assertions_begin")
	check(state.tick==1442 and Identity.age_years(state,child)==18 and state.actors[parent].action_id.is_empty(),"子女在同一持久世界经过两年正式成年")
	phase("adult_assertions_end")
	capture(k,"adult-before-designation")
	phase("designation_begin")
	check(F.send(k,"family_designate",{"heir_actor_id":child}).ok,"本人当面指定自己的成年子女")
	phase("designation_end")
	var old: Dictionary=k.snapshot(); var public_wallet: Dictionary=old.wallets["wallet.office"].duplicate(true)
	phase("retirement_begin")
	var retired: Dictionary=F.send(k,"family_retire")
	phase("retirement_end")
	check(retired.ok,"正常退隐移交控制权："+str(retired))
	state=k.snapshot()
	check(state.player_actor_id==child and state.tick==old.tick and state.family.successions.size()==1 and state.actors[parent].alive,"继任保留日期、前代真人与完整永久接续来源")
	check(state.estates["estate.riverside"].owner_actor_id==child and state.wallets["wallet.estate"].owner.id==child and state.inventories["inventory.estate"].owner.id==child,"庄园与独立私钱私粮合法接续")
	check(state.wallets[state.actors[child].wallet_id].balance==old.wallets["wallet.player"].balance and state.wallets["wallet.player"].balance==0 and state.wallets["wallet.office"]==public_wallet,"随身私钱移交，公库保持完整")
	phase("succession_codec_begin")
	var saved: Dictionary=Codec.decode(Codec.encode(state,1,"family-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"合法继任后在正式Schema和字节存档中恢复")
	phase("succession_codec_end")
	capture(k,"after-succession")
	phase("post_succession_life_begin")
	check(F.send(k,"estate_withdraw",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":1}).ok,"继任者能凭新的合法所有权实际领粮")
	check(F.send(k,"move_local",{"place_id":"west.street"}).ok and F.send(k,"wait").ok and advance(k,1),"继任后仍可离庄生活，世界没有重新开始或强制结局")
	phase("post_succession_life_end")
	finish()

## 可选保存正式长跑产生的稳定档供真实图形导入复验；文件不是修改字段后的测试替身。
func capture(k, name: String) -> void:
	if capture_dir.is_empty(): return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	var text: String=Codec.encode(k.snapshot(),0,name,k.content())
	var file=FileAccess.open(capture_dir.path_join(name+".hmsave"),FileAccess.WRITE)
	check(file!=null and not text.is_empty(),"保存正常长跑产生的图形复验档："+name)
	if file!=null: file.store_string(text); file.close()

## 只汇报世界规则与长期运行证据，实际图形、发行重启和Web另行验收。
func finish() -> void:
	phase("world_end")
	print("FAMILY_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
