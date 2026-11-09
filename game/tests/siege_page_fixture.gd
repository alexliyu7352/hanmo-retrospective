## 沿既有安全离城作者的真实任官/受令/编组/行军，到开围城即止；不造运行部队或战果。
extends RefCounted
const Interaction=preload("res://core/interaction_rules.gd")

## 固定准备目录的正式Catalog路径；不从当前工作目录猜未准备作者文件。
static func content_path() -> String:
	return ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")

## 真实多人物提交保持原请求序号和revision；失败直接返回，不补写缺失状态。
static func send(kernel,kind: String,params: Dictionary={},actor_id: String="") -> Dictionary:
	var context: Dictionary=kernel.command_context(actor_id)
	if not context.ok: return context
	return kernel.submit({"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})

## 只有真实无行动时先提交等待，以原公开命令推进半日。
static func advance(kernel) -> Dictionary:
	if kernel.preview("advance").get("code")=="NoCurrentAction":
		var waiting: Dictionary=send(kernel,"wait")
		if not waiting.ok: return waiting
	return send(kernel,"advance")

## 与既有prepare前缀相同，省略后续围困/谈判/离城/存读，用作页面共同真实来源。
static func enter(kernel) -> Dictionary:
	var opened: Dictionary=kernel.new_game("scenario.alpha_life",content_path())
	if not opened.ok: return opened
	var rows: Array=[[kernel.snapshot().player_actor_id,"a"],["actor.military.b.commander","b"],["actor.military.b2.commander","b2"]]
	for row in rows:
		var appointed: Dictionary=send(kernel,"office_accept",{"office_id":"office.military."+row[1],"context":Interaction.for_role(kernel.snapshot(),kernel.content().data,"role.military."+row[1])},row[0])
		if not appointed.ok: return appointed
	var advanced: Dictionary=advance(kernel)
	if not advanced.ok: return advanced
	var campaigns: Dictionary={}
	for row in rows:
		var accepted: Dictionary=send(kernel,"campaign_accept",{"template_id":"order.military."+row[1],"context":Interaction.for_role(kernel.snapshot(),kernel.content().data,"role.military."+row[1])},row[0])
		if not accepted.ok: return accepted
		campaigns[row[1]]=accepted.details.campaign_id
		var formed: Dictionary=send(kernel,"army_form",{"order_id":campaigns[row[1]],"unit_ids":["unit.military."+row[1]],"food_quantity":160 if row[1]=="a" else 0,"money":20},row[0])
		if not formed.ok: return formed
	advanced=advance(kernel)
	if not advanced.ok: return advanced
	for row in rows:
		var departed: Dictionary=send(kernel,"campaign_depart",{"campaign_id":campaigns[row[1]]},row[0])
		if not departed.ok: return departed
	advanced=advance(kernel)
	if not advanced.ok: return advanced
	var started: Dictionary=send(kernel,"siege_start",{"campaign_id":campaigns.a})
	if not started.ok: return started
	return {"ok":true,"siege_id":started.details.siege_id,"campaigns":campaigns}
