## 编译可信阶段的显式依赖；执行只改调用者候选，不拥有世界事务或领域规则。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")

## 失败不泄露部分执行图，诊断细节只记录阶段或资源名称。
static func _fail(code: String, detail: String="") -> Dictionary:
	return {"ok":false,"code":code,"detail":detail}

## 阶段和所属模块采用非空协议名称，不隐式转换其他值。
static func _name(value: Variant) -> bool:
	return value is String and not value.is_empty()

## 资源只接受state/context及点分标识符；声明不是字段查询或写屏障。
static func _path(value: Variant) -> bool:
	if not value is String: return false
	var parts: PackedStringArray=value.split(".",true)
	if parts.is_empty() or not parts[0] in ["state","context"]: return false
	for index in range(1,parts.size()):
		var part: String=parts[index]
		if part.is_empty(): return false
		for offset in part.length():
			var character: int=part.unicode_at(offset)
			if not (character==95 or (character>=65 and character<=90) or (character>=97 and character<=122) or (offset>0 and character>=48 and character<=57)): return false
	return true

## 拷贝前检查完整形状，绝不调用run来试探签名或执行能力。
static func _valid(hook: Variant) -> bool:
	if not Values.fields(hook,{"id":TYPE_STRING,"owner":TYPE_STRING,"before":TYPE_ARRAY,"after":TYPE_ARRAY,"reads":TYPE_ARRAY,"writes":TYPE_ARRAY,"run":TYPE_CALLABLE}): return false
	if not _name(hook.id) or not _name(hook.owner) or not hook.run.is_valid(): return false
	for field in ["before","after"]:
		for id in hook[field]:
			if not _name(id): return false
	for field in ["reads","writes"]:
		for path in hook[field]:
			if not _path(path): return false
	return true

## 只按完整路径段判断祖先重叠，actors与actors_extra互不冲突。
static func _overlap(left: String, right: String) -> bool:
	return left==right or left.begins_with(right+".") or right.begins_with(left+".")

## 读读可并列，写写及任一方向的写读必须已有显式可达顺序。
static func _conflict(left: Dictionary, right: Dictionary) -> bool:
	for write in left.writes:
		for resource in right.reads+right.writes:
			if _overlap(write,resource): return true
	for write in right.writes:
		for resource in left.reads:
			if _overlap(write,resource): return true
	return false

## 显式图的传递可达性用于证明顺序，不从资源注释暗中增加边。
static func _reachable(edges: Dictionary, from: String, to: String) -> bool:
	var pending: Array=[from]; var visited: Dictionary={}
	while not pending.is_empty():
		var id: String=pending.pop_back()
		if visited.has(id): continue
		visited[id]=true
		for next in edges[id]:
			if next==to: return true
			pending.append(next)
	return false

## 先拒绝形状、未知边、环与无序冲突，最后仅对可交换的就绪项按ID稳定排序。
static func compile(hooks: Array) -> Dictionary:
	var entries: Dictionary={}; var edges: Dictionary={}; var indegree: Dictionary={}
	for hook in hooks:
		if not _valid(hook): return _fail("InvalidPhaseHook")
		if entries.has(hook.id): return _fail("DuplicatePhaseId",hook.id)
		entries[hook.id]=hook.duplicate(true); edges[hook.id]=[]; indegree[hook.id]=0
	for id in entries:
		for field in ["before","after"]:
			for other in entries[id][field]:
				if not entries.has(other): return _fail("UnknownPhaseDependency",other)
				var start: String=id if field=="before" else other
				var end: String=other if field=="before" else id
				if not end in edges[start]: edges[start].append(end); indegree[end]+=1
	var ordered: Array=[]
	while ordered.size()<entries.size():
		var ready: Array=[]
		for id in entries:
			if indegree[id]==0 and not id in ordered: ready.append(id)
		if ready.is_empty(): return _fail("PhaseDependencyCycle")
		ready.sort()
		var id: String=ready[0]; ordered.append(id)
		for next in edges[id]: indegree[next]-=1
	for left in range(ordered.size()):
		for right in range(left+1,ordered.size()):
			var a: String=ordered[left]; var b: String=ordered[right]
			if _conflict(entries[a],entries[b]) and not _reachable(edges,a,b) and not _reachable(edges,b,a): return _fail("AmbiguousPhaseOrder",a+":"+b)
	var compiled: Array=[]
	for id in ordered: compiled.append(entries[id])
	return {"ok":true,"format":"HM-WORLD-PHASES/1","order":ordered,"hooks":compiled}

## 只接受已经编译的非空计划；不重建描述、不引入领域或缓存启动时行动表。
static func advance(state: Dictionary, data: Dictionary, hold: Array, registry: Dictionary) -> Dictionary:
	var plan: Variant=registry.get("phase_plan")
	if not plan is Dictionary or plan.get("format")!="HM-WORLD-PHASES/1" or plan.get("ok")!=true or not plan.get("order") is Array or not plan.get("hooks") is Array or plan.hooks.is_empty() or plan.hooks.size()!=plan.order.size(): return _fail("MissingWorldPhaseGraph")
	for hook in plan.hooks:
		if not hook is Dictionary or not hook.get("run") is Callable or not hook.run.is_valid(): return _fail("MissingWorldPhaseGraph")
	var context: Dictionary={"from_tick":state.tick,"to_tick":state.tick+1,"hold":hold,"_actions":registry.get("actions",{}),"contacts":[],"military_contacts":[],"_military_executions":{},"outcomes":{},"family_completed":[],"military_returns":[],"suspended_actors":[],"journey_requests":[]}
	for hook in plan.hooks:
		var result: Dictionary=hook.run.call(state,data,context)
		if not result.ok: return result
	return {"ok":true,"details":{"completed_actions":context.outcomes,"suspended_actors":context.suspended_actors,"journey_requests":context.journey_requests}}
