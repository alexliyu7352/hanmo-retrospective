## 个人战已结算事件的纯UI时钟；仅持有限值副本，不持Kernel、规则对象、RNG或命令入口。
extends RefCounted

const PHASES: Array[String]=["prepare","move","strike","impact","state","settle"]
const BINDING_KEYS: Array[String]=["kernel_id","load_generation","actor_id","world_id","execution_epoch"]
const SNAPSHOT_VERSION: int=1
const MAX_DURATION: float=30.0
var _clip_id: String=""
var _battle_id: String=""
var _binding: Dictionary={}
var _signature: String=""
var _phases: Array=[]
var _total: float=0.0
var _elapsed: float=0.0
var _paused: bool=false
var _speed: float=1.0
var _complete: bool=true
var _reason: String="unconfigured"

## 接受有限已结算片段；同片段重配续放，过期或损坏恢复立即终态而不阻塞下一轮输入。
func configure(clip: Dictionary,resume: Dictionary={}) -> Dictionary:
	var previous: Dictionary=snapshot()
	var saved: Dictionary=resume
	if saved.is_empty() and not _clip_id.is_empty() and clip.get("id")==_clip_id: saved=previous
	_clip_id=""; _battle_id=""; _binding={}; _signature=""; _phases=[]
	_total=0.0; _elapsed=0.0; _paused=false; _speed=1.0; _complete=true; _reason="invalid_clip"
	if not _read_clip(clip): return frame()
	if not clip.get("ok",false) or not clip.get("playable",false):
		_finish("not_playable"); return frame()
	if not saved.is_empty():
		if not _restore(saved): return frame()
		if _same_identity(previous):
			if previous.complete: _finish("")
			elif _elapsed<float(previous.elapsed): _elapsed=float(previous.elapsed)
	elif _total>0.0:
		_complete=false; _reason=""
	else:
		_finish("empty_timeline")
	return frame()

## 只推进有限非负UI秒数；坏delta即使暂停也立即收尾，零delta保持当前阶段。
func advance(delta: float) -> Dictionary:
	if not is_finite(delta) or delta<0.0:
		_finish("invalid_delta"); return frame()
	if _complete or _paused or delta==0.0: return frame()
	if delta>=(_total-_elapsed)/_speed:
		_finish("")
	else:
		_elapsed+=delta*_speed
	return frame()

## 返回当前阶段的深副本；同阶段事件同时公开，终态清空瞬态事件而保稳定实际结果。
func frame() -> Dictionary:
	var result: Dictionary={"active":not _complete,"complete":_complete,"phase_kind":"settle","phase_progress":1.0,"events":[],"elapsed":_elapsed,"paused":_paused,"speed":_speed,"clip_id":_clip_id,"phase_index":_phases.size(),"total_duration":_total,"reason":_reason}
	if _complete: return result
	var start: float=0.0
	for index in _phases.size():
		var phase: Dictionary=_phases[index]; var duration: float=phase.duration
		if duration>0.0 and _elapsed<start+duration:
			result.phase_kind=phase.kind; result.phase_progress=clampf((_elapsed-start)/duration,0.0,1.0)
			result.events=phase.events.duplicate(true); result.phase_index=index
			return result
		start+=duration
	# 浮点末位误差也回到稳定终态，绝不留下找不到阶段的active帧。
	_finish(""); return frame()

## 暂停只影响UI时间；完成之后不再保留暂停状态以免宿主误认仍在播放。
func set_paused(value: bool) -> Dictionary:
	_paused=value and not _complete
	return frame()

## 仅接受界面承诺的1x或2x；拒绝其他值时保留最后有效速度而不产生零速挂起。
func set_speed(value: float) -> Dictionary:
	if is_finite(value) and value in [1.0,2.0]: _speed=value
	return frame()

## 跳过只抵达已结算片段末尾，没有推进回合、关闭战果或发送命令的能力。
func skip() -> Dictionary:
	_finish("")
	return frame()

## 来源失效时只保完成标记；同片段再次绑定不会复活旧动画。
func invalidate() -> Dictionary:
	_finish("invalidated")
	return frame()

## 宿主可保存的仅是UI身份与进度；没有事件、演员、完整世界或任何对象引用。
func snapshot() -> Dictionary:
	return {"version":SNAPSHOT_VERSION,"clip_id":_clip_id,"battle_id":_battle_id,"binding":_binding.duplicate(true),"signature":_signature,"elapsed":_elapsed,"paused":_paused,"speed":_speed,"complete":_complete}

## 验有限来源与阶段后只复制时序值；演员只参与恢复签名，永不交回规则或持有引用。
func _read_clip(clip: Dictionary) -> bool:
	if not clip.get("id") is String or not clip.get("battle_id") is String: return false
	_clip_id=clip.id; _battle_id=clip.battle_id
	if _clip_id.is_empty() or _battle_id.is_empty(): return false
	if not clip.get("ok") is bool or not clip.get("playable") is bool: return false
	if not clip.get("binding") is Dictionary or not _valid_binding(clip.binding): return false
	for key in BINDING_KEYS: _binding[key]=clip.binding[key]
	if not clip.get("phases") is Array or not clip.get("actors",{}) is Dictionary: return false
	if not _pure_values(clip.actors if clip.has("actors") else {},0): return false
	var phases: Array=clip.phases
	if phases.size()>PHASES.size(): return false
	var previous_index: int=-1
	var accepted: Array=[]; var duration: float=0.0
	for candidate in phases:
		if not candidate is Dictionary: return false
		var index: int=PHASES.find(candidate.get("kind",""))
		if index<0 or index<=previous_index: return false
		previous_index=index
		if not _number(candidate.get("duration")): return false
		var seconds: float=float(candidate.duration)
		if seconds<0.0 or seconds>MAX_DURATION: return false
		if not candidate.get("events") is Array or not _pure_values(candidate.events,0): return false
		for event in candidate.events:
			if not event is Dictionary: return false
		accepted.append({"kind":PHASES[index],"duration":seconds,"events":candidate.events.duplicate(true)})
		duration+=seconds
		if duration>MAX_DURATION: return false
	_phases=accepted; _total=duration
	_signature=var_to_bytes({"phases":_phases,"actors":clip.get("actors",{})}).hex_encode().sha256_text()
	return true

## 精确比对五项来源和整段签名；不借用同名战斗、旧load代数或不同时间线恢复。
func _restore(saved: Dictionary) -> bool:
	if not _same_identity(saved):
		_finish("resume_identity_mismatch"); return false
	if saved.get("version")!=SNAPSHOT_VERSION or not _number(saved.get("elapsed")) or not _number(saved.get("speed")) or not saved.get("paused") is bool or not saved.get("complete") is bool:
		_finish("invalid_resume"); return false
	var elapsed: float=float(saved.elapsed); var speed: float=float(saved.speed)
	if elapsed<0.0 or elapsed>_total or not speed in [1.0,2.0]:
		_finish("invalid_resume"); return false
	_speed=speed; _elapsed=elapsed; _complete=saved.complete or elapsed>=_total
	_paused=saved.paused and not _complete; _reason=""
	if _complete: _finish("")
	return true

## 身份与内容签名同时相同才可合并UI进度，避免同对象旧快照倒退或复活完成片段。
func _same_identity(saved: Dictionary) -> bool:
	return saved.get("clip_id")==_clip_id and saved.get("battle_id")==_battle_id and saved.get("binding")==_binding and saved.get("signature")==_signature

## 实例ID保留Godot有符号非零整数；GD后端空world_id与零execution_epoch仍是有效绑定。
func _valid_binding(binding: Dictionary) -> bool:
	for key in ["kernel_id","load_generation","execution_epoch"]:
		if not binding.get(key) is int: return false
	if binding.kernel_id==0 or binding.load_generation<0 or binding.execution_epoch<0: return false
	return binding.get("actor_id") is String and not binding.actor_id.is_empty() and binding.get("world_id") is String

## 只接受有限数值，避免NaN比较穿过进度和恢复检查。
func _number(value: Variant) -> bool:
	return (value is int or value is float) and is_finite(float(value))

## 深度和大小有界地拒绝Object、Callable与循环容器；所有进入时钟的事件都是纯值。
func _pure_values(value: Variant,depth: int) -> bool:
	if depth>16: return false
	match typeof(value):
		TYPE_NIL,TYPE_BOOL,TYPE_INT,TYPE_STRING: return true
		TYPE_FLOAT: return is_finite(value)
		TYPE_ARRAY:
			if value.size()>4096: return false
			for item in value:
				if not _pure_values(item,depth+1): return false
			return true
		TYPE_DICTIONARY:
			if value.size()>4096: return false
			for key in value:
				if not key is String or not _pure_values(value[key],depth+1): return false
			return true
	return false

## 所有失败、跳过和自然完成共享同一稳定终态，只修改本对象的UI元数据。
func _finish(reason: String) -> void:
	_elapsed=_total; _complete=true; _paused=false; _reason=reason
