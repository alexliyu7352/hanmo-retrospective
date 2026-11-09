## 固定5×4军阵操练纯题面：三个抽象编组和四步布置不生成士卒、不执行战斗，也不修改真实训练。
## 自动成绩与表现上限复用纯基础；实例资格、首课前冻结时机及唯一训练消费由military owner负责。
extends RefCounted
const Common=preload("res://core/domains/cultivation/challenge_basis.gd")
const FORMAT="HM-DRILL-CHALLENGE/1"
const WIDTH=5
const HEIGHT=4
const MOVE_COUNT=4
const GROUP_KINDS=["foot","ranged","cavalry"]

## 公开合法格与固定四项评分均为显式数据；附四步100分见证是本实现约定，不宣称原文逐字规定。
static func validate_definition(definition: Variant, texts: Dictionary) -> Dictionary:
	if not Common.fields(definition,{"id":TYPE_STRING,"difficulty":TYPE_INT,"prompt_key":TYPE_STRING,"groups":TYPE_ARRAY,"objectives":TYPE_DICTIONARY,"solution":TYPE_ARRAY}) or not Common.symbol(definition.id,true) or not Common.whole(definition.difficulty,0,3) or not Common.text(texts,definition.prompt_key) or definition.groups.size()!=3 or definition.solution.size()!=MOVE_COUNT: return _fail("InvalidDrillDefinition")
	var groups: Dictionary={}; var kinds: Array=[]; var occupied: Array=[]
	for group in definition.groups:
		if not Common.fields(group,{"id":TYPE_STRING,"kind":TYPE_STRING,"text_key":TYPE_STRING,"origin":TYPE_ARRAY,"targets":TYPE_ARRAY}) or not Common.symbol(group.id) or groups.has(group.id) or not group.kind in GROUP_KINDS or group.kind in kinds or not Common.text(texts,group.text_key) or not _cell(group.origin) or group.origin in occupied or not _cells(group.targets): return _fail("InvalidDrillGroup")
		groups[group.id]=group; kinds.append(group.kind); occupied.append(group.origin)
	var objectives: Dictionary=_validate_objectives(definition.objectives,groups,texts)
	if not objectives.ok: return objectives
	var board: Dictionary=_origins(definition)
	var moves: Array=[]
	for solution in definition.solution:
		if not Common.fields(solution,{"group_id":TYPE_STRING,"target":TYPE_ARRAY}): return _fail("InvalidDrillSolution")
		var applied: Dictionary=_apply(definition,board,solution.group_id,solution.target)
		if not applied.ok: return _fail("InvalidDrillSolution")
		board=applied.board; moves.append(applied.move)
	if _raw_score(_objectives(definition,board,moves))!=100: return _fail("UnsolvedDrillTemplate")
	return {"ok":true,"maximum_raw_score":100}

## owner给定的来源、人物快照与seed全部保存；纯叶不抽取世界随机或认领真实部队。
static func create(definition: Dictionary, texts: Dictionary, binding: Dictionary, actor_snapshot: Dictionary, seed: String, mode: String) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if not _binding(binding) or not Common.snapshot(actor_snapshot) or not Common.seed(seed) or not mode in ["manual","automatic"]: return _fail("InvalidDrillStart")
	var state: Dictionary={"format":FORMAT,"definition_id":definition.id,"source_sha256":_source_digest(definition,texts),"binding":binding.duplicate(true),"actor_snapshot":actor_snapshot.duplicate(true),"seed":seed,"mode":mode,"phase":"active","moves":[],"pending":null,"result":{}}
	if mode=="automatic":
		state.phase="completed"; state.result=_result(definition,state,"automatic")
	return {"ok":true,"state":state}

## 选择一个公开允许的空目标格，只形成待确认值；不计算入已确认骑兵轨迹，也不改变原状态。
static func draft_move(definition: Dictionary, texts: Dictionary, state: Dictionary, expected_step: Variant, group_id: String, target: Variant) -> Dictionary:
	var checked: Dictionary=_active(definition,texts,state,expected_step)
	if not checked.ok: return checked
	var board: Dictionary=_replay(definition,state.moves).board
	var applied: Dictionary=_apply(definition,board,group_id,target)
	if not applied.ok: return applied
	var next: Dictionary=state.duplicate(true)
	next.pending={"group_id":group_id,"target":target.duplicate()}
	return {"ok":true,"state":next}

## 仅撤销匹配的未确认步；旧确认/撤销不能覆盖重新选择的另一个目标，不提供回改已确认轨迹入口。
static func cancel_pending(definition: Dictionary, texts: Dictionary, state: Dictionary, expected_step: Variant, expected_pending: Variant) -> Dictionary:
	var checked: Dictionary=_active(definition,texts,state,expected_step)
	if not checked.ok: return checked
	if not _pending(expected_pending) or state.pending!=expected_pending: return _fail("StaleDrillPending")
	var next: Dictionary=state.duplicate(true); next.pending=null
	return {"ok":true,"state":next}

## 确认只追加当前精确待选步，第四次即终态；目标碰撞/非法格不能借确认绕过公开移动合同。
static func confirm_move(definition: Dictionary, texts: Dictionary, state: Dictionary, expected_step: Variant, expected_pending: Variant) -> Dictionary:
	var checked: Dictionary=_active(definition,texts,state,expected_step)
	if not checked.ok: return checked
	if not _pending(expected_pending) or state.pending!=expected_pending: return _fail("StaleDrillPending")
	var applied: Dictionary=_apply(definition,_replay(definition,state.moves).board,state.pending.group_id,state.pending.target)
	if not applied.ok: return applied
	var next: Dictionary=state.duplicate(true)
	next.moves.append(applied.move); next.pending=null
	if next.moves.size()==MOVE_COUNT:
		next.phase="completed"; next.result=_result(definition,next,"manual")
	return {"ok":true,"state":next}

## 中途退出丢弃未确认步并按同实例自动成绩结束，已确认步保留；终态不能换模式重新评分。
static func exit_challenge(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	var next: Dictionary=state.duplicate(true)
	if next.phase=="active":
		next.phase="completed"; next.pending=null; next.result=_result(definition,next,"exited")
	return {"ok":true,"state":next}

## 显示全部公开合法格和地形目标，但不暴露作者解证据；三个编组只有抽象布置，无兵员和战损。
static func view(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase=="completed": return {"ok":true,"phase":"completed","result":state.result.duplicate(true)}
	var board: Dictionary=_replay(definition,state.moves).board
	var groups: Array=[]
	for group in definition.groups: groups.append({"id":group.id,"kind":group.kind,"text":texts[group.text_key],"position":board[group.id].duplicate(),"targets":group.targets.duplicate(true)})
	var objectives: Dictionary=definition.objectives.duplicate(true)
	for objective in objectives.values(): objective.text=texts[objective.text_key]; objective.erase("text_key")
	return {"ok":true,"phase":"active","width":WIDTH,"height":HEIGHT,"step":state.moves.size()+1,"prompt":texts[definition.prompt_key],"groups":groups,"objectives":objectives,"pending":state.pending.duplicate(true) if state.pending is Dictionary else null}

## 存读逐步重放同一抽象棋盘，拒绝非法轨迹、丢失已确认步、伪造分数或重开终态；不还原任何真实战场。
static func validate_state(definition: Dictionary, texts: Dictionary, state: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definition(definition,texts)
	if not checked.ok: return checked
	if state.size()!=11 or not Common.fields(_without_pending(state),{"format":TYPE_STRING,"definition_id":TYPE_STRING,"source_sha256":TYPE_STRING,"binding":TYPE_DICTIONARY,"actor_snapshot":TYPE_DICTIONARY,"seed":TYPE_STRING,"mode":TYPE_STRING,"phase":TYPE_STRING,"moves":TYPE_ARRAY,"result":TYPE_DICTIONARY}) or not state.has("pending") or state.format!=FORMAT or state.definition_id!=definition.id or not _binding(state.binding) or not Common.snapshot(state.actor_snapshot) or not Common.seed(state.seed) or not state.mode in ["manual","automatic"] or not state.phase in ["active","completed"]: return _fail("InvalidDrillState")
	if state.source_sha256!=_source_digest(definition,texts): return _fail("DrillSourceChanged")
	var played: Dictionary=_replay(definition,state.moves)
	if not played.ok: return played
	if state.phase=="active":
		if state.mode!="manual" or state.moves.size()>=MOVE_COUNT or not state.result.is_empty(): return _fail("InvalidDrillProgress")
		if state.pending!=null and (not _pending(state.pending) or not _apply(definition,played.board,state.pending.group_id,state.pending.target).ok): return _fail("InvalidDrillPending")
		return {"ok":true}
	if state.pending!=null: return _fail("InvalidDrillPending")
	var completion: Variant=state.result.get("completion")
	if not completion is String or not completion in ["manual","automatic","exited"]: return _fail("InvalidDrillResult")
	if completion=="manual" and (state.mode!="manual" or state.moves.size()!=MOVE_COUNT): return _fail("InvalidDrillResult")
	if completion=="automatic" and (state.mode!="automatic" or not state.moves.is_empty()): return _fail("InvalidDrillResult")
	if completion=="exited" and (state.mode!="manual" or state.moves.size()>=MOVE_COUNT): return _fail("InvalidDrillResult")
	if not Common.fields(state.result,{"completion":TYPE_STRING,"score":TYPE_INT,"raw_score":TYPE_INT if completion=="manual" else TYPE_NIL,"auto_score":TYPE_INT,"performance_bp":TYPE_INT,"objectives":TYPE_DICTIONARY if completion=="manual" else TYPE_NIL,"choices":TYPE_ARRAY}) or not _replay(definition,state.result.choices).ok: return _fail("InvalidDrillResult")
	if completion=="manual" and not Common.fields(state.result.objectives,{"supply":TYPE_BOOL,"passage":TYPE_BOOL,"screen":TYPE_BOOL,"cavalry":TYPE_BOOL}): return _fail("InvalidDrillResult")
	if state.result!=_result(definition,state,completion): return _fail("InvalidDrillResult")
	return {"ok":true}

## 只返回已有正常基线的有界整数修正候选；真实训练单位、人员和逐半日差额只能由原owner结算。
static func scaled_result(definition: Dictionary, texts: Dictionary, state: Dictionary, normal_value: Variant) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="completed" or not Common.whole(normal_value,0,1200): return _fail("InvalidDrillBaseline")
	@warning_ignore("integer_division")
	var amount: int=normal_value*state.result.performance_bp/10000
	return {"ok":true,"normal_value":normal_value,"value":amount,"performance_bp":state.result.performance_bp}

## 每项目标的角色和格对必须显式声明；不猜前后邻接、不套真实兵种移动力或引入任意表达式。
static func _validate_objectives(value: Dictionary, groups: Dictionary, texts: Dictionary) -> Dictionary:
	if not Common.fields(value,{"supply":TYPE_DICTIONARY,"passage":TYPE_DICTIONARY,"screen":TYPE_DICTIONARY,"cavalry":TYPE_DICTIONARY}): return _fail("InvalidDrillObjectives")
	var supply: Dictionary=value.supply; var passage: Dictionary=value.passage; var screen: Dictionary=value.screen; var cavalry: Dictionary=value.cavalry
	if not Common.fields(supply,{"group_id":TYPE_STRING,"point":TYPE_ARRAY,"guard_cells":TYPE_ARRAY,"text_key":TYPE_STRING}) or not _group_kind(groups,supply.group_id,"foot") or not _cell(supply.point) or not _cells(supply.guard_cells): return _fail("InvalidDrillSupplyGoal")
	if not Common.fields(passage,{"group_id":TYPE_STRING,"cells":TYPE_ARRAY,"text_key":TYPE_STRING}) or not groups.has(passage.group_id) or not _cells(passage.cells): return _fail("InvalidDrillPassageGoal")
	if not Common.fields(screen,{"front_group_id":TYPE_STRING,"ranged_group_id":TYPE_STRING,"pairs":TYPE_ARRAY,"text_key":TYPE_STRING}) or not _group_kind(groups,screen.front_group_id,"foot") or not _group_kind(groups,screen.ranged_group_id,"ranged") or screen.pairs.is_empty() or screen.pairs.size()>400: return _fail("InvalidDrillScreenGoal")
	var seen: Array=[]
	for pair in screen.pairs:
		if not Common.fields(pair,{"front":TYPE_ARRAY,"rear":TYPE_ARRAY}) or not _cell(pair.front) or not _cell(pair.rear) or pair.front==pair.rear or pair in seen: return _fail("InvalidDrillScreenGoal")
		seen.append(pair)
	if not Common.fields(cavalry,{"group_id":TYPE_STRING,"bad_cells":TYPE_ARRAY,"text_key":TYPE_STRING}) or not _group_kind(groups,cavalry.group_id,"cavalry") or not _cells(cavalry.bad_cells): return _fail("InvalidDrillCavalryGoal")
	for objective in value.values():
		if not Common.text(texts,objective.text_key): return _fail("MissingDrillObjectiveText")
	return {"ok":true}

## 只在作者公开目标格间布置，不允许原地充数或重叠占格；不模拟实际路径、距离或战场攻击。
static func _apply(definition: Dictionary, board: Dictionary, group_id: String, target: Variant) -> Dictionary:
	if not _cell(target) or not board.has(group_id): return _fail("InvalidDrillTarget")
	var group: Dictionary={}
	for row in definition.groups:
		if row.id==group_id: group=row
	if not target in group.targets or target==board[group_id]: return _fail("DrillTargetNotAllowed")
	for other in board:
		if other!=group_id and board[other]==target: return _fail("DrillCellOccupied")
	var next: Dictionary=board.duplicate(true); next[group_id]=target.duplicate()
	return {"ok":true,"board":next,"move":{"group_id":group_id,"from":board[group_id].duplicate(),"target":target.duplicate()}}

## 已确认轨迹重放同一合法性函数，from字段必须正好等于此前位置，防止存档瞬移或删步续分。
static func _replay(definition: Dictionary, moves: Array) -> Dictionary:
	if moves.size()>MOVE_COUNT: return _fail("InvalidDrillMoves")
	var board: Dictionary=_origins(definition)
	for move in moves:
		if not Common.fields(move,{"group_id":TYPE_STRING,"from":TYPE_ARRAY,"target":TYPE_ARRAY}) or not _cell(move.from) or not _cell(move.target): return _fail("InvalidDrillMove")
		var applied: Dictionary=_apply(definition,board,move.group_id,move.target)
		if not applied.ok or applied.move!=move: return _fail("InvalidDrillMove")
		board=applied.board
	return {"ok":true,"board":board}

## 前三项按终盘公开格/格对评估；骑兵全程已确认目标（含起点）不入显式劣地，撤销草稿不计。
static func _objectives(definition: Dictionary, board: Dictionary, moves: Array) -> Dictionary:
	var goals: Dictionary=definition.objectives
	var screened: bool=false
	for pair in goals.screen.pairs:
		if board[goals.screen.front_group_id]==pair.front and board[goals.screen.ranged_group_id]==pair.rear: screened=true
	var cavalry_safe: bool=not _origins(definition)[goals.cavalry.group_id] in goals.cavalry.bad_cells
	for move in moves:
		if move.group_id==goals.cavalry.group_id and move.target in goals.cavalry.bad_cells: cavalry_safe=false
	return {"supply":board[goals.supply.group_id] in goals.supply.guard_cells,"passage":board[goals.passage.group_id] in goals.passage.cells,"screen":screened,"cavalry":cavalry_safe}

## 四项各25、无额外基础分；统一自动兜底和120%上限不会把布置题当战场胜负。
static func _result(definition: Dictionary, state: Dictionary, completion: String) -> Dictionary:
	var automatic: int=Common.automatic_score(state.actor_snapshot,definition.difficulty)
	var raw: Variant=null; var goals: Variant=null; var score: int=automatic
	if completion=="manual":
		goals=_objectives(definition,_replay(definition,state.moves).board,state.moves)
		raw=_raw_score(goals); score=maxi(raw,automatic-10)
	return {"completion":completion,"score":score,"raw_score":raw,"auto_score":automatic,"performance_bp":Common.performance_bp(score,automatic),"objectives":goals,"choices":state.moves.duplicate(true)}

## 评分只累加四个固定布尔，不接受作者任意分值或额外奖励键。
static func _raw_score(objectives: Dictionary) -> int:
	var total: int=0
	for reached in objectives.values():
		if reached: total+=25
	return total

## 当前步号防止旧确认当作下一步；完整终态只能查看，不能再次起草。
static func _active(definition: Dictionary, texts: Dictionary, state: Dictionary, step: Variant) -> Dictionary:
	var checked: Dictionary=validate_state(definition,texts,state)
	if not checked.ok: return checked
	if state.phase!="active": return _fail("DrillAlreadyCompleted")
	if not Common.whole(step,1,MOVE_COUNT) or step!=state.moves.size()+1: return _fail("StaleDrillStep")
	return {"ok":true}

## 固定棋盘所有坐标均为真实整数，浮点、额外维度、负值和越界拒绝。
static func _cell(value: Variant) -> bool:
	return value is Array and value.size()==2 and Common.whole(value[0],0,WIDTH-1) and Common.whole(value[1],0,HEIGHT-1)

## 公开格集有界且不重复，不能用任意地形脚本或外部地图替代。
static func _cells(value: Array) -> bool:
	if value.is_empty() or value.size()>WIDTH*HEIGHT: return false
	var seen: Array=[]
	for cell in value:
		if not _cell(cell) or cell in seen: return false
		seen.append(cell)
	return true

## 编组起点从同一题面派生，不保存第二份可互相矛盾的位置真值。
static func _origins(definition: Dictionary) -> Dictionary:
	var board: Dictionary={}
	for group in definition.groups: board[group.id]=group.origin.duplicate()
	return board

## 固定目标角色只指向已声明抽象编组，不把其ID解释成真实unit_id。
static func _group_kind(groups: Dictionary, id: String, kind: String) -> bool:
	return groups.has(id) and groups[id].kind==kind

## 未确认值只有组和目标，不携带步数、评分、兵员或世界写入。
static func _pending(value: Variant) -> bool:
	return Common.fields(value,{"group_id":TYPE_STRING,"target":TYPE_ARRAY}) and _cell(value.target)

## 可空pending单独检查，其余固定字段仍完整且精确，不放宽未知字段。
static func _without_pending(state: Dictionary) -> Dictionary:
	var result: Dictionary=state.duplicate(); result.erase("pending")
	return result

## 来源摘要包含解证据及全部实际公开文本，修改题面不能沿用原步序分数。
static func _source_digest(definition: Dictionary, texts: Dictionary) -> String:
	var selected: Dictionary={definition.prompt_key:texts[definition.prompt_key]}
	for group in definition.groups: selected[group.text_key]=texts[group.text_key]
	for objective in definition.objectives.values(): selected[objective.text_key]=texts[objective.text_key]
	return JSON.stringify({"definition":definition,"texts":selected},"",true).sha256_text()

## owner来源ID只作精确绑定，不读取任意路径，也不代替真实人物或部队授权。
static func _binding(value: Dictionary) -> bool:
	return Common.fields(value,{"actor_id":TYPE_STRING,"source_id":TYPE_STRING}) and Common.symbol(value.actor_id,true) and not value.source_id.is_empty() and value.source_id.length()<=160 and not "\n" in value.source_id and not "\r" in value.source_id

## 失败保留稳定诊断，不提供默认满分或虚构完成。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
