## 驻军训练接共用action生命周期的薄适配；注册/阶段与军务互斥由中央唯一接线。
extends RefCounted
const Training=preload("res://core/domains/military/training_rules.gd")

## 只复制已核准的实际参与者和固定服务时长，不分配ID或预留公共资金。
static func prepare(_state: Dictionary, _data: Dictionary, _actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind!=Training.KIND: return {"ok":false,"code":"UnsupportedAction"}
	return {"ok":true,"participants":offer.participants.duplicate(),"duration":offer.duration,"params":params.duplicate(true)}

## 共用runtime先建立人物占用，再让唯一训练叶重验并预留该部和费用。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	return Training.begin(state,data,action,offer)

## 沿现actions_step中的finish_tick，必须在真实军事供给之后才可结算一课。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, context: Dictionary) -> Dictionary:
	return Training.step(state,data,action,context.finish_tick)

## 只封账已提供份额，时钟和人物释放继续由原共用runtime处理。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	return Training.finish(state,data,action,finish_tick)

## 读档活动校验复用训练唯一事实和预留，不重建课程或重复支付。
static func validate(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return Training.validate_action(state,data,action)

## 正式取消callback仅调用本人的同一训练结束器，人物/行动由根层原release释放。
static func cancel(state: Dictionary, data: Dictionary, action: Dictionary, _reason: String) -> Dictionary:
	return Training.cancel(state,data,action.actor_id,action.id)
