## 普通技艺课程复用正式长行动生命周期；这里仅连接课时规则，不另造时钟或提交事务。
extends RefCounted
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")

## 只复制已核准的双方和课时，不提前收费、分配行动ID或奖励经验。
static func prepare(_state: Dictionary, _data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind!=Study.KIND or not offer.get("study") is Dictionary: return {"ok":false,"code":"InvalidStudyOffer"}
	return {"ok":true,"participants":[actor_id,offer.study.teacher_id],"duration":offer.duration,"params":params.duplicate(true)}

## 占用已建立时重新核对报价并预留课程费用；失败由共用候选事务回滚。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	return Study.begin(state,data,action,offer)

## 真正经过半日才记一课；环境中断返回正常失败结课，保证runtime释放占用。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, _context: Dictionary) -> Dictionary:
	return Study.step(state,data,action)

## 成功或来源失效均只结算已提供课时成长，重复完成不再次发放。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	return Study.finish(state,data,action,finish_tick)

## 读档验证复用同一条款及进度规则，不重放收费、课时和经验。
static func validate(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return Study.validate_action(state,data,action)

## 已认证的取消命令结束当前课程事实；调用方继续用原释放器解除同一候选中的占用和未用资金。
static func cancel(state: Dictionary, data: Dictionary, action: Dictionary, _reason: String="") -> Dictionary:
	return Study.cancel(state,data,action.actor_id,action.id)

## 恢复注册入口只重新取得原双方占用，不以start替代恢复或重新收费。
static func resume(state: Dictionary, data: Dictionary, action: Dictionary, params: Dictionary) -> Dictionary:
	var offer: Dictionary=Study.preview_control(state,data,action.actor_id,Study.RESUME_KIND,params)
	if not offer.ok: return offer
	return Study.apply_control(state,data,action.actor_id,Study.RESUME_KIND,params,offer)
