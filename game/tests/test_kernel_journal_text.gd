## 原Main行旅手记的真实文字oracle与只读Kernel门；公共任务/问讯短链，纯query边界单独标明。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Notes=preload("res://ui/journey_notes.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const TimeRules=preload("res://core/base/time_rules.gd")
const OLD_MAIN_SHA="ae8eecfd9dcdb166020bd50a3dd3058ddec1443dbb57d6345db66b2099e3eea0"
const OLD_NOTES_SHA="296bae9afbd4e7ba3bdfe885dd7856d29b42f7b09d4376e90ebeca4ef9a122ff"
var checks: int=0
var failures: int=0
var content_path: String=""
var rows: Dictionary={}
var require_query: bool=false

## 只统计真正入口的snapshot调用；其余状态与命令都继承正式Kernel，不改视图算法。
class CountKernel extends Kernel:
	var snapshots: int=0
	## 计数后原样返回真实深快照，旧入口必须实际走过它。
	func snapshot() -> Dictionary:
		snapshots+=1;return super.snapshot()

## 真实Main不进树，避免无关_ready、音频与Storage请求；只执行原journal_text方法。
func _init() -> void:call_deferred("run")

## 每个观察都进入真实进程退出码，未知分支不假称已覆盖。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value

## 所有任务与消息来源都通过公共带版本命令产生。
func send(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	var result: Dictionary=k.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	check(result.ok,"真实命令 "+kind+" "+str(result));return result

## 读取真实岗位当前人物，UI草案不能捏造任务权限或问讯来源。
func context(k, role: String) -> Dictionary:return Interaction.for_role(k.snapshot(),k.content().data,"base::"+role)

## 控制器未_ready时其构造Node尚未成为孩子，显式释放这些测试对象，不调用存储或声音。
func dispose(main) -> void:
	main._ui_font_scale.free();main.journey_view.free();main.storage.free();main._ambience.music.free();main._ambience.effect.free();main._ambience.free();main.free()

## 真实Main输出与输入全态/RNG隔离；baseline留旧文字，迁位后才要求专用snapshot消失。
func journal(main, label: String) -> String:
	var k=main.kernel;var before: Dictionary=k.save_context();var data_before: Dictionary=main._data.duplicate(true)
	seed(184205);var expected_random: int=randi();seed(184205)
	k.snapshots=0;var text: String=main.journal_text();var count: int=k.snapshots
	check(count==(0 if require_query else 1),"真实Main只去掉手记专用snapshot "+label+" count="+str(count))
	check(k.save_context()==before and main._data==data_before and randi()==expected_random,"手记读取不改state/catalog/registry/输入或任何RNG "+label)
	if require_query and k.has_method("journal_text"):
		k.snapshots=0;check(k.call("journal_text")==text and k.snapshots==0 and k.save_context()==before,"真实Kernel薄入口与原Main文字一致且不取全快照 "+label)
	rows[label]=text;return text

## 初始未知→真实告示→受领→两批受托实物→完成；保持原lot顺序与旧中文格式。
func task_flow(main) -> bool:
	var k=main.kernel;var data: Dictionary=k.content().data
	var grain_title: String=Notes.name_of(data,data.offers["base::offer.grain"]);var letter_title: String=Notes.name_of(data,data.offers["base::offer.letter"])
	var text: String=journal(main,"unknown")
	check(not grain_title in text and not letter_title in text,"未知全局任务不能从作者定义泄漏")
	if not send(k,"move_local",{"place_id":"base::west.office"}).ok or not send(k,"inspect",{"prop_id":"base::notice.grain"}).ok:return false
	text=journal(main,"known")
	check(grain_title in text and not letter_title in text and k.snapshot().tasks.is_empty(),"实际阅读告示只显示已知未受领任务")
	var params: Dictionary={"offer_id":"base::offer.grain","context":context(k,"role.office")}
	if not send(k,"accept_offer",params).ok or not send(k,"advance").ok:return false
	text=journal(main,"accepted")
	check("正在履行" in text and "受托经费余额：650 钱" in text,"实际受领与拨款后才出现义务与公费")
	var task: Dictionary=k.snapshot().tasks.values()[0]
	if not send(k,"move_local",{"place_id":"base::west.street"}).ok or not send(k,"move_local",{"place_id":"base::west.grain"}).ok:return false
	for quantity in [6,4]:
		var trade: Dictionary={"shop_id":"base::shop.grain","item_id":"base::item.grain","side":"buy","quantity":quantity,"payer_wallet_id":task.budget_wallet_id,"context":context(k,"role.merchant")}
		if not send(k,"trade",trade).ok or not send(k,"advance").ok:return false
	text=journal(main,"entrusted_lots")
	var state: Dictionary=k.snapshot();var lots: Array=state.inventories[state.actors[state.player_actor_id].inventory_id].lots
	check(lots.size()==2 and lots[0].quantity==6 and lots[1].quantity==4 and lots[0].owner.kind=="task" and lots[1].owner.kind=="task","两次真实采购保留两批原受托顺序")
	check(text.find(" × 6 · 受托保管")>=0 and text.find(" × 6 · 受托保管")<text.find(" × 4 · 受托保管"),"原文本按实物lot顺序逐批显示，不重排或合并")
	if not send(k,"move_local",{"place_id":"base::west.street"}).ok or not send(k,"move_local",{"place_id":"base::west.office"}).ok:return false
	params.context=context(k,"role.office")
	if not send(k,"report_offer",params).ok or not send(k,"advance").ok:return false
	text=journal(main,"completed")
	check("已履约" in text and "已经兑现了 1 份承诺" in text and not "受托经费余额" in text,"真实报告完成后显示原履约总结而非活动预算")
	return true

## NPC真实社会事件先对本人未知，当面问讯后才显示本人消息和首个来源/学习日期。
func social_flow(main) -> bool:
	var k=main.kernel;var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	check(not state.social.events.is_empty() and state.social.memories.get(actor,{}).is_empty(),"远处已发生真实消息，但本人尚未知晓")
	check(not "近事与见闻" in journal(main,"social_unknown"),"原Main不自动揭示远方社会事件")
	if not send(k,"move_local",{"place_id":"base::west.street"}).ok or not send(k,"move_local",{"place_id":"base::west.tavern"}).ok or not send(k,"talk",{"role_id":"base::role.host"}).ok:return false
	var heard: Dictionary=send(k,"ask_news")
	if not heard.ok:return false
	state=k.snapshot();var memories: Dictionary=state.social.memories.get(actor,{})
	if not check(not heard.details.news_ids.is_empty() and not memories.is_empty(),"真实当面问讯确实新增本人知闻"):return false
	var text: String=journal(main,"social_heard")
	var memory: Dictionary=memories.values()[0];var person: String=Identity.definition(state,k.content().data,memory.source_actor_id).get("display_name",memory.source_actor_id)
	check(memory.source_actor_id=="base::actor.host" and person+"相告" in text and "第%d日"%[TimeRules.day_index(memory.learned_tick)+1] in text,"原消息文字保留真实相告者和获知日期")
	return true

## 仅纯query防御边界：未知memory kind与非task归属旧标签；不装入Kernel或宣称合法持久世界。
func projection_boundary(main) -> void:
	var input: Dictionary=main.kernel.snapshot();var data: Dictionary=main.kernel.content().data;var actor: String=input.player_actor_id
	var original: String=Notes.describe(input,data)
	input.facts["test.unknown"]={"kind":"test_unknown_kind","tick":input.tick}
	if not input.social.memories.has(actor):input.social.memories[actor]={}
	input.social.memories[actor]["test.unknown"]={"learned_tick":input.tick,"source_actor_id":actor}
	var before: Dictionary=input.duplicate(true)
	check(Notes.describe(input,data)==original and input==before,"未知kind只在纯query边界跳过，不泄漏额外文字或改输入")
	var real_lot: Dictionary={}
	for inventory in input.inventories.values():
		if inventory.owner.kind=="organization" and not inventory.lots.is_empty():real_lot=inventory.lots[0].duplicate(true);break
	check(not real_lot.is_empty(),"存在实际公库批次供旧显示边界描述")
	if real_lot.is_empty():return
	input.inventories[input.actors[actor].inventory_id].lots=[real_lot];before=input.duplicate(true)
	var text: String=Notes.describe(input,data)
	check("%s × %d · 私有"%[Notes.name_of(data,data.items[real_lot.item_id]),real_lot.quantity] in text and input==before,"保留原非task统一写私有的历史标签，不借迁位重定财产权")
	rows.projection_boundary=text

## 旧文件和旧真实输出首次冻结；后续只读oracle，不从新入口生成替代期望。
func oracle(args: PackedStringArray) -> void:
	for arg in args:
		if arg.begins_with("--record-oracle="):
			var path: String=arg.trim_prefix("--record-oracle=")
			if not check(FileAccess.get_file_as_string("res://ui/main.gd").sha256_text()==OLD_MAIN_SHA and FileAccess.get_file_as_string("res://ui/journey_notes.gd").sha256_text()==OLD_NOTES_SHA and not FileAccess.file_exists(path),"只从原ed27 Main/Notes字节首次冻结"):return
			var file=FileAccess.open(path,FileAccess.WRITE)
			if not check(file!=null,"可写独立手记oracle证据"):return
			file.store_string(JSON.stringify({"main_sha256":OLD_MAIN_SHA,"notes_sha256":OLD_NOTES_SHA,"texts":rows},"\t",true,true));file.close();return
		if arg.begins_with("--oracle="):
			var value: Variant=JSON.parse_string(FileAccess.get_file_as_string(arg.trim_prefix("--oracle=")))
			if not check(value is Dictionary and value.get("main_sha256")==OLD_MAIN_SHA and value.get("notes_sha256")==OLD_NOTES_SHA and value.get("texts") is Dictionary,"读取原ed27真实手记oracle"):return
			check(value.texts==rows,"原Main完整文字/顺序/知识边界逐字相等");return
	check(false,"明确record或只读oracle路径")

## 显式正式目录优先；默认pin缺失或坏形状转普通测试失败，不要求重建本地绝对路径。
func configure_content(args: PackedStringArray) -> bool:
	var explicit: bool=false
	for arg in args:
		if arg.begins_with("--content="):content_path=arg.trim_prefix("--content=");explicit=true
	if not explicit:
		var path: String="res://data/safe-departure-button-fixture.json"
		if not FileAccess.file_exists(path):check(false,"请用--content指定正式prepared目录");return false
		var parser=JSON.new()
		if parser.parse(FileAccess.get_file_as_string(path))!=OK or not parser.data is Dictionary or not parser.data.get("directory") is String:check(false,"默认内容pin格式无效");return false
		content_path="res://data/generated/"+parser.data.directory+"/catalog.hmcatalog.json"
	if content_path.is_empty() or not FileAccess.file_exists(content_path):check(false,"正式prepared目录不存在");return false
	return true

## 已发布正式作者输入复用；只调用真正Main的纯方法，不初始化UI或执行任何存储请求。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args();require_query="--require-kernel-query" in args
	if not configure_content(args):finish();return
	var k=CountKernel.new()
	if not check(k.new_game("base::scenario.alpha_life",content_path).ok,"真实Kernel以同正式作者开局"):finish();return
	var main=Main.new();main.kernel=k;main._data=k.content().data
	print("JOURNAL_TEXT_STORAGE={requests:none,Codec:false,storage_profile:none,user_data_dir:",OS.get_user_data_dir(),",unused_native_directory:",main.storage.native.directory,"}")
	if require_query:check(k.has_method("journal_text"),"Kernel提供journal_text只读入口")
	if task_flow(main) and social_flow(main):projection_boundary(main)
	oracle(args);dispose(main);finish()

## 只报告本手记迁位门，不宣称全任务、社会/持久化矩阵或完整Main体验。
func finish() -> void:
	print("KERNEL_JOURNAL_TEXT_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
