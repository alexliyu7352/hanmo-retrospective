## 受信代码观察器的有界候选派生；不提交世界、不加载作者脚本、不重演来源交易。
extends RefCounted
const Context=preload("res://core/runtime/transaction_context.gd")
const Values=preload("res://core/base/value_checks.gd")
const MAX_STEPS: int=64

## 失败必须由持有原世界的提交者丢弃整个候选，绝不在观察器内恢复快照。
static func _fail(code: String, detail: String="") -> Dictionary:
	return {"ok":false,"code":code,"detail":detail,"discard_candidate":true}

## 标识只能是明确非空字符串，不能把路径或其他值强制转换成身份。
static func _name(value: Variant) -> bool:
	return value is String and not value.is_empty()

## 所有观察器可写候选，必须有显式先后；只接受已有发行代码的三参Callable。
static func _valid(entry: Variant) -> bool:
	if not Values.fields(entry,{"id":TYPE_STRING,"owner":TYPE_STRING,"before":TYPE_ARRAY,"after":TYPE_ARRAY,"callback":TYPE_CALLABLE}): return false
	if not _name(entry.id) or not _name(entry.owner) or not entry.callback.is_valid() or entry.callback.get_argument_count()!=3: return false
	for field in ["before","after"]:
		for dependency in entry[field]:
			if not _name(dependency): return false
	return true

## 启动可调用的纯编译入口：无序、未知依赖与环全部拒绝，绝不试执行回调。
static func compile(descriptors: Array) -> Dictionary:
	var entries: Dictionary={}; var edges: Dictionary={}; var indegree: Dictionary={}
	for entry in descriptors:
		if not _valid(entry): return _fail("InvalidObserverDescriptor")
		if entries.has(entry.id): return _fail("DuplicateObserverId",entry.id)
		entries[entry.id]=entry.duplicate(true); edges[entry.id]=[]; indegree[entry.id]=0
	for id in entries:
		for field in ["before","after"]:
			for other in entries[id][field]:
				if not entries.has(other): return _fail("UnknownObserverDependency",other)
				var source: String=id if field=="before" else other
				var target: String=other if field=="before" else id
				if not target in edges[source]: edges[source].append(target); indegree[target]+=1
	var order: Array=[]; var ambiguous: bool=false
	while order.size()<entries.size():
		var ready: Array=[]
		for id in entries:
			if indegree[id]==0 and not id in order: ready.append(id)
		if ready.is_empty(): return _fail("ObserverDependencyCycle")
		if ready.size()>1: ambiguous=true
		# 排序只让错误诊断稳定，出现并列最终仍拒绝，不暗中选业务顺序。
		ready.sort()
		var id: String=ready[0]; order.append(id)
		for target in edges[id]: indegree[target]-=1
	if ambiguous: return _fail("AmbiguousObserverOrder")
	var observers: Array=[]
	for id in order: observers.append(entries[id])
	return {"ok":true,"format":"HM-DOMAIN-OBSERVERS/1","order":order,"observers":observers}

## 效果只携带普通值，拒绝对象/Callable与递归容器；不因此将临时引用变成持久事实。
static func _plain(value: Variant, depth: int=0) -> bool:
	if depth>32: return false
	match typeof(value):
		TYPE_NIL,TYPE_BOOL,TYPE_INT,TYPE_STRING: return true
		TYPE_FLOAT: return is_finite(value)
		TYPE_ARRAY:
			for child in value:
				if not _plain(child,depth+1): return false
			return true
		TYPE_DICTIONARY:
			for key in value:
				if not key is String or not _plain(value[key],depth+1): return false
			return true
	return false

## 返回载荷的数组只含普通字典；这里不推断引用可见性或复制领域事实。
static func _payload(result: Dictionary) -> bool:
	for field in ["effects","fact_refs"]:
		var rows: Variant=result.get(field,[])
		if not rows is Array: return false
		for row in rows:
			if not row is Dictionary or not _plain(row): return false
	return true

## 失败后封闭上下文，只转交领域错误码和文本诊断，不复制未提交效果或递归载荷。
static func _abort(context: Dictionary, result: Dictionary, observer_id: String) -> Dictionary:
	context.observer_status="failed"
	var failure: Dictionary={"ok":false,"code":result.code,"observer_id":observer_id,"discard_candidate":true}
	if result.get("detail") is String: failure.detail=result.detail
	return failure

## 依赖序执行一次来源的派生处理；回调得到隔离上下文，只以返回效果传给下游。
static func reconcile(candidate: Dictionary, data: Dictionary, context: Dictionary, registry: Dictionary) -> Dictionary:
	var valid: Dictionary=Context.validate(context)
	if not valid.ok: return _fail(valid.code)
	if context.observer_status=="failed": return _fail("ObserverContextFailed")
	if context.observer_status=="running": return _fail("ObserverReentry")
	if not registry.get("observers") is Dictionary: return _fail("MissingObserverRegistry")
	for id in registry.observers:
		if not registry.observers[id] is Dictionary or registry.observers[id].get("id")!=id: return _fail("InvalidObserverDescriptor")
	var plan: Dictionary=compile(registry.observers.values())
	if not plan.ok: return plan
	context.observer_status="running"
	for observer in plan.observers:
		# JSON元组不受标识中冒号或分隔符影响；只在此临时候选中去重。
		var key: String=JSON.stringify([context.source_command_key,observer.id])
		if context.processed_observers.has(key): continue
		if context.observer_steps>=MAX_STEPS: return _abort(context,_fail("ObserverStepLimit"),observer.id)
		var view: Dictionary=Context.snapshot(context); view.observer_id=observer.id
		context.observer_steps+=1
		var response: Variant=observer.callback.call(candidate,data,view)
		if not response is Dictionary or not response.get("ok") is bool: return _abort(context,_fail("InvalidObserverResult"),observer.id)
		if not response.ok:
			if not _name(response.get("code")): return _abort(context,_fail("InvalidObserverResult"),observer.id)
			return _abort(context,response,observer.id)
		if not _payload(response): return _abort(context,_fail("InvalidObserverResult"),observer.id)
		context.effects.append_array(response.get("effects",[]).duplicate(true))
		context.fact_refs.append_array(response.get("fact_refs",[]).duplicate(true))
		context.processed_observers[key]=true
	context.observer_status="reconciled"
	return {"ok":true,"observer_steps":context.observer_steps,"effects":context.effects.duplicate(true),"fact_refs":context.fact_refs.duplicate(true)}
