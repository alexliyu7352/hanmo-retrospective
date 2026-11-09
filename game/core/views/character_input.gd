## 单人物公开表现输入：只投影调用者已判可见的指定人物，不枚举世界或授予可见权限。
extends RefCounted
const Identity=preload("res://core/actor_identity.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")

## 复用真实身份、年龄和外观验证；缺外观沿旧legacy，错误不伪造档案或泄露私密来源。
static func query(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary: return {"ok":false,"code":"InvalidPresentationActor"}
	var definition: Dictionary=Identity.definition(state,data,actor_id)
	if definition.is_empty(): return {"ok":false,"code":"InvalidPresentationActor"}
	var projected: Dictionary=Appearance.project_visible(actor.get("appearance",Appearance.legacy()))
	if not projected.ok: return projected
	var actor_view: Dictionary={"actor_id":actor_id,"display_name":definition.display_name,"age_years":Identity.age_years(state,actor_id),"presentation_profile_id":definition.get("presentation_profile_id",""),"portrait_kind":definition.get("portrait_kind",""),"visibility_version":state.get("revision",0)}
	return {"ok":true,"actor_view":actor_view,"appearance_view":projected.appearance_view}
