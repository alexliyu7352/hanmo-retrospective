## 真实双代存储、严格JSON边界和在途行动恢复；测试只使用独立目录，不触碰玩家存档。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Kernel = preload("res://core/world_kernel.gd")
const Codec = preload("res://core/save_codec.gd")
const P=preload("res://tests/policy_fixture.gd")
const F=preload("res://tests/companion_fixture.gd")
const Rules=preload("res://core/rules_migration.gd")
const Native = preload("res://storage/native_storage.gd")
var checks: int=0
var failed: int=0

## 在正式引擎初始化之后执行存储与解析回归。
func _init() -> void:
	call_deferred("_run")

## 统计可观察断言，失败明确输出；不能把提前返回算作成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 通过正式版本化命令提交世界变化，不直接修改内核状态。
func send(k, kind: String, params: Dictionary={}) -> Dictionary:
	var s: Dictionary=k.snapshot()
	return k.submit({"kind":kind,"params":params,"actor_id":s.player_actor_id,"request_seq":s.request_sequences[s.player_actor_id]+1,"expected_revision":s.revision})

## 数字沿原语法匹配当前偏移的合法前缀，Unicode前缀、坏后缀与重复键不能因性能优化放行。
func check_strict_numbers() -> void:
	for sample in [["0",0,TYPE_INT],["-0",0,TYPE_INT],["1.25",1.25,TYPE_FLOAT],["1e2",100,TYPE_INT],["-1.25e+2",-125,TYPE_INT],["1e-9999",0,TYPE_INT],["2147483647",2147483647,TYPE_INT],["-2147483647",-2147483647,TYPE_INT],["2147483648",2147483648.0,TYPE_FLOAT],["-2147483648",-2147483648.0,TYPE_FLOAT]]:
		var parsed: Dictionary=Strict.parse_text(sample[0])
		check(parsed.ok and parsed.value==sample[1] and typeof(parsed.value)==sample[2],"严格数字值与整数规范化保持一致："+sample[0])
	for sample in [["01","TrailingData"],["1.","TrailingData"],["1e","TrailingData"],["1e+","TrailingData"],["1.e2","TrailingData"],["+1","InvalidNumber"],[".1","InvalidNumber"],["--1","InvalidNumber"],["NaN","InvalidNumber"],["Infinity","InvalidNumber"],["1e309","NonFiniteNumber"],["0e9999","NonFiniteNumber"],["[1e+]","ExpectedComma"],["[01]","ExpectedComma"],["[1e309x]","ExpectedComma"]]:
		check(Strict.parse_text(sample[0]).get("code")==sample[1],"坏数字保留原有拒绝码与合法前缀边界："+sample[0])
	var unicode: Dictionary=Strict.parse_text('["😀汉é",12,-3.5e2,{"值":4}]')
	check(unicode.ok and unicode.value==["😀汉é",12,-350,{"值":4}],"Unicode字符之后的数字按字符偏移匹配，不误用UTF-8字节偏移")
	for invalid in ["+1","--1","١٢","１２","−1","x12",".2"]:
		check(Strict.parse_text('["😀汉",'+invalid+']').get("code")=="ExpectedComma","数字锚点不能跳过当前位置的非法字符再搜索后面的数字："+invalid)
	check(Strict.parse_text('{"a":1,"\\u0061":2}').get("code")=="DuplicateKey","ASCII转义同名键仍在Godot覆盖之前拒绝")
	check(Strict.parse_text('{"汉":1,"\\u6c49":2}').get("code")=="DuplicateKey","Unicode解码同名键仍明确拒绝")
	check(Strict.parse_text('"😀汉"',2).ok and Strict.parse_text('"😀汉"',1).get("code")=="StringTooLong","字符串限额仍按解码字符核验")
	check(not Strict.parse_text("[".repeat(34)+"0"+"]".repeat(34)).ok,"过深数组不能借数字快路径绕过深度限制")
	var tokens: PackedStringArray=[]
	for index in range(4096): tokens.append(str(index))
	var many: Dictionary=Strict.parse_text('{"名字":"😀汉","values":['+",".join(tokens)+']}')
	check(many.ok and many.value.values.size()==4096 and many.value.values[4095]==4095,"较大数字数组完整读取，不跳项、不截断或改变最终值")

## 保存真实受托行动、恢复后完成一次，再破坏隔离目录的新代文件以检验旧代恢复。
func _run() -> void:
	check_strict_numbers()
	check_policy_storage()
	var k=Kernel.new()
	check(k.new_game().ok,"new world")
	send(k,"move_local",{"place_id":"west.office"})
	send(k,"inspect",{"prop_id":"notice.grain"})
	check(send(k,"accept_offer",{"offer_id":"offer.grain","context":{"place_id":"west.office","provider_actor_id":"actor.chen","role_id":"role.office"}}).ok,"acceptance starts")
	var one: String=Codec.encode(k.snapshot(),1,"manual-1")
	check(not one.is_empty(),"migrated world can encode committed acceptance")
	if one.is_empty():
		print("STORAGE_CHECKS=",checks," FAILURES=",failed)
		quit(1)
		return
	var decoded: Dictionary=Codec.decode(one)
	check(decoded.ok,"decode exact content and engine")
	var restored=Kernel.new(); restored.new_game()
	check(decoded.ok and restored.restore(decoded.state).ok,"pending action restores")
	check(restored.advance_one_tick().ok and k.advance_one_tick().ok,"both real clocks advance")
	check(restored.snapshot()==k.snapshot(),"restored pending budget not paid twice")
	var state: Dictionary=restored.snapshot()
	check(state.wallets[ state.tasks.values()[0].budget_wallet_id ].balance==650,"single budget after restore")
	var directory: String=OS.get_user_data_dir().path_join("f02-native-"+str(Time.get_ticks_usec()))
	var store=Native.new(directory)
	check(store.put("manual-1",one,0).ok,"actual first file written and read back")
	var two: String=Codec.encode(state,2,"manual-1")
	check(store.put("manual-1",two,1).ok,"actual second file written and read back")
	check(not store.put("manual-1",two,1).ok,"stale generation refused")
	var file=FileAccess.open(directory.path_join("manual-1.A.hmsave"),FileAccess.WRITE)
	file.store_string("corrupted newest generation"); file.close()
	var recovered: Dictionary=store.get_slot("manual-1")
	check(recovered.ok and recovered.recovered and recovered.text==one,"corrupt newest recovers previous actual bytes")
	check(not store.get_slot("../outside").ok,"slot traversal refused")
	var outer: Dictionary=JSON.parse_string(one)
	outer.payload_sha256="00"
	check(not Codec.decode(JSON.stringify(outer)).ok,"damaged checksum refused")
	outer=JSON.parse_string(one); outer.ruleset_id="e0-1"
	check(not Codec.decode(JSON.stringify(outer)).ok,"legacy envelope not silently imported")
	outer=JSON.parse_string(one); outer.content_digest="wrong"
	check(not Codec.decode(JSON.stringify(outer)).ok,"different content digest refused")
	var invalid: Dictionary=state.duplicate(true); invalid.actors[invalid.player_actor_id].hp=3.5
	check(Codec.encode(invalid,3,"manual-1").is_empty(),"invalid live state cannot be saved")
	# 只清理由本测试创建的隔离文件，绝不触碰实际玩家槽位。
	for filename in DirAccess.get_files_at(directory): DirAccess.remove_absolute(directory.path_join(filename))
	DirAccess.remove_absolute(directory)
	print("STORAGE_CHECKS=",checks," FAILURES=",failed)
	quit(0 if failed==0 else 1)

## 真实f09封装先核验旧校验和，仅版本迁移；取消只能在下一正常提交生效。
func check_policy_storage() -> void:
	var k=P.queued(); var old: Dictionary=k.snapshot(); old.actors["actor.shen"].available=false
	check(k.restore(old).ok,"旧事件失效夹具仍可恢复而不清理")
	var envelope: Dictionary=JSON.parse_string(Codec.encode(old,1,"a04-migration",k.content()))
	old.ruleset_id="f09-military-1"; envelope.ruleset_id=old.ruleset_id
	for actor in old.actors.values(): actor.erase("appearance")
	envelope.payload_json=JSON.stringify(old,"",true,true); envelope.payload_length=envelope.payload_json.to_utf8_buffer().size(); envelope.payload_sha256=envelope.payload_json.sha256_text()
	var decoded: Dictionary=Codec.decode(JSON.stringify(envelope,"",true,true),k.content())
	var expected: Dictionary=old.duplicate(true); expected.ruleset_id=Rules.CURRENT
	for actor in expected.actors.values(): actor.appearance={"format_version":1,"generator_version":"legacy-portrait-v1","generation_seed":"not-sampled","base_features":{},"attire_style":"legacy","source":"legacy","parent_sources":[]}
	check(decoded.ok and decoded.state==expected,"真实f09封装只追加legacy和规则版本，不猜取消")
	if decoded.ok:
		check(k.restore(decoded.state).ok and k.snapshot().events["actor.player|event.letter"].status=="queued","restore不执行reconcile")
		check_process_resume(k,"before-cancellation")
		check(k.snapshot().events["actor.player|event.letter"].status=="cancelled","首次普通事务取消")
		check_process_resume(k,"after-cancellation")
	var pending=P.accepting(); check_process_resume(pending,"pending-acceptance","advance")
	check(pending.snapshot().wallets[pending.snapshot().tasks.values()[0].budget_wallet_id].balance==650,"独立进程待受领恢复恰好拨款650")
	var interrupted=P.accepting(); P.replace_officer(interrupted); check_process_resume(interrupted,"acceptance-interruption","advance")
	check(interrupted.snapshot().tasks.values()[0].status=="cancelled" and interrupted.snapshot().wallets["wallet.office"].balance==5000,"独立进程受领中断不拨款")
	check(F.send(interrupted,"accept_offer",P.params(interrupted)).ok,"跨进程中断后正式上下文可重办")
	check_process_resume(interrupted,"acceptance-retry","advance")
	check(interrupted.snapshot().wallets["wallet.office"].balance==4350,"跨进程重办只拨款一次")
	var alpha=P.fresh(true)
	check(F.send(alpha,"move_local",{"place_id":"west.estate"}).ok and F.send(alpha,"family_adopt",{"context":F.context(alpha,"role.estate_steward"),"display_name":"阿宁"}).ok and alpha.advance_one_tick().ok and alpha.advance_one_tick().ok,"真实两半日收养生成家庭权威")
	var source: Dictionary=alpha.snapshot()
	check(not source.family.children.is_empty() and not source.offices.is_empty() and not source.military_units.is_empty(),"兼容样本含真实非空家庭和军事权威")
	envelope=JSON.parse_string(Codec.encode(source,2,"a04-family-military",alpha.content()))
	source.ruleset_id="f09-military-1"; envelope.ruleset_id=source.ruleset_id
	for actor in source.actors.values(): actor.erase("appearance")
	envelope.payload_json=JSON.stringify(source,"",true,true); envelope.payload_length=envelope.payload_json.to_utf8_buffer().size(); envelope.payload_sha256=envelope.payload_json.sha256_text()
	expected=source.duplicate(true); expected.ruleset_id=Rules.CURRENT
	for actor in expected.actors.values(): actor.appearance={"format_version":1,"generator_version":"legacy-portrait-v1","generation_seed":"not-sampled","base_features":{},"attire_style":"legacy","source":"legacy","parent_sources":[]}
	decoded=Codec.decode(JSON.stringify(envelope,"",true,true),alpha.content())
	check(decoded.ok and decoded.state==expected,"真实家庭军务f09封装只追加legacy和版本，旧字段全部保持")
	var upgraded: Dictionary=Rules.upgrade(source)
	check(upgraded.ok and upgraded.state==expected,"f09分支先于旧军务拒绝且深复制全部家庭军务事实")
	if upgraded.ok: upgraded.state.family.children.clear(); check(source.family.children.size()==1,"迁移没有共享旧权威引用")
	var current: Dictionary=expected.duplicate(true); var same: Dictionary=Rules.upgrade(current)
	check(same.ok and same.state==current and not same.migrated,"f10幂等复制")
	source.ruleset_id="unknown"; check(not Rules.upgrade(source).ok,"未知版本仍拒绝")
	source.ruleset_id=Rules.FAMILY; check(Rules.upgrade(source).get("code")=="AmbiguousLegacyMilitary","真正旧版本非空军务仍拒绝")

## 保存字节交给另一个实际Godot进程，完整比较恢复、下一命令与重复序号。
func check_process_resume(k, label: String, kind: String="end_interaction") -> void:
	var folder: String=OS.get_environment("TMPDIR").path_join("a04-resume-"+label+"-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var command: Dictionary=P.command(k,kind)
	for row in [["input.hmsave",Codec.encode(k.snapshot(),0,label,k.content())],["command.json",JSON.stringify(command)]]:
		var file=FileAccess.open(folder.path_join(row[0]),FileAccess.WRITE); file.store_string(row[1]); file.close()
	var before: Dictionary=k.snapshot(); var reply: Dictionary=k.submit(command); var after: Dictionary=k.snapshot()
	var output: Array=[]
	var exit_code: int=OS.execute(OS.get_executable_path(),["--headless","--path",ProjectSettings.globalize_path("res://"),"--script","res://tests/policy_resume.gd","--","--input="+folder.path_join("input.hmsave"),"--command="+folder.path_join("command.json"),"--output="+folder.path_join("result.json")],output,true)
	check(exit_code==0,"独立进程完成："+label+" "+str(output))
	if exit_code!=0: return
	var decoded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(folder.path_join("result.json")))
	check(decoded.ok and decoded.value.before==before and decoded.value.reply==reply and decoded.value.after==after,"独立进程完整恢复与下一命令一致："+label)
	if decoded.ok: check(decoded.value.duplicate==reply and decoded.value.after_duplicate==after,"独立进程重复请求零二次结算："+label)
