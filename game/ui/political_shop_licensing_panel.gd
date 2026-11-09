## 本人须先到原柜台听取声明，再提交逐铺申请；控件不签发许可或改写通缉簿。
extends RefCounted

## 预读只消费有限页和原报价；任何异步依赖未齐都不发写命令。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_shop_licensing_enabled") or not kernel.political_shop_licensing_enabled(): return true
	var page: Dictionary=kernel.political_shop_licensing(place_id,int(host.get_meta("shop_licensing_cursor",0)))
	if _pending(page): return false
	if not page.get("ok",false) or not page.get("enabled",false): return true
	var context: Dictionary=kernel.command_context()
	if _pending(context): return false
	if page.get("source_identity",{})!=kernel.source_identity() or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id: return true
	var actions: Array=page.get("rows",[]).duplicate()
	if page.has("counter_action"): actions.append(page.counter_action)
	var ready=true
	for action in actions:
		if not action.get("available",false): continue
		var quote: Dictionary=kernel.preview(action.get("command",""),action.get("params",{}),context.get("actor_id",""))
		if _pending(quote): ready=false
	return ready

## 当前本人、地点和完整来源身份必须仍匹配；旧页面不展示远方查核或留下可用按钮。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page):
		host._text(parent,"正在核对本地办证柜台…",17);return
	if not page.get("ok",false):
		host._text(parent,reason(page.get("code","PoliticalShopJurisdictionUnavailable")),17);return
	if not page.get("enabled",false): return
	var identity: Dictionary=host.kernel.source_identity()
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or page.get("source_identity",{})!=identity or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id:
		host._text(parent,"办理来源已变化，请等当前页面刷新。",17);return
	if page.get("mode","")=="registry_unavailable":
		host._text(parent,reason(page.get("code","PoliticalShopRegistryContinuityRequired")),17);return
	if page.get("mode","")=="documents":
		host._text(parent,"本人取得的铺位许可",22)
		host._text(parent,"这里只能查阅当面取得的证面。后来是否有撤证或新的限制，仍须到柜台查核；开店时会重新核验。",16)
		for row in page.get("rows",[]):
			host._text(parent,row.get("premises_id","铺位"),19)
			host._text(parent,"铺位地点："+str(row.get("place_id","")),16)
			host._text(parent,"原签发人：%s（%s）"%[row.get("issuer_actor_id",""),row.get("issuer_office_id","")],16)
			host._text(parent,"第 %d 半日取得，证面记载至第 %d 半日。"%[int(row.get("issued_tick",0)),int(row.get("valid_through_tick",0))],17)
		_pagination(host,parent,page,identity,context,place_id)
		return
	if page.get("mode","")=="administration":
		host._text(parent,"本地司法办理",22)
		host._text(parent,"按当前公职逐项核对原册。每项处分仍须提交真实本人命令。",16)
		for row in page.get("rows",[]):
			host._regional_button(parent,row.get("label","办理"),row.get("command",""),row.get("subject_actor_id",""),row.get("params",{}),{},not row.get("available",false))
			if not row.get("available",false): host._text(parent,reason(row.get("code","")),16)
		_pagination(host,parent,page,identity,context,place_id)
		return
	host._text(parent,"私铺许可",22)
	if page.has("counter_action"):
		var counter: Dictionary=page.counter_action
		host._text(parent,"先到本地柜台说明要经营的铺位，由在职承办人当面查核。",17)
		host._regional_button(parent,"与柜台交谈",counter.get("command",""),page.get("court_id",""),counter.get("params",{}),{},not counter.get("available",false))
		if not counter.get("available",false): host._text(parent,reason(counter.get("code","")),16)
	for row in page.get("rows",[]):
		host._text(parent,row.get("premises_id","铺位"),19)
		host._text(parent,"铺位地点："+str(row.get("place_id","")),16)
		if row.has("permit_fact_id"):
			host._text(parent,"新开设申请可使用本次查核至第 %d 半日。"%int(row.get("valid_through_tick",0)),17)
			host._text(parent,"许可只用于该铺位的新开设；租约、房东同意和其他条件仍须另核。",16)
		else:
			host._regional_button(parent,"申请该铺位许可",row.get("command",""),row.get("premises_id",""),row.get("params",{}),{},not row.get("available",false))
		if not row.get("available",false) and row.get("code","")!="PoliticalShopPermitAlreadyValid": host._text(parent,reason(row.get("code","")),16)
	_pagination(host,parent,page,identity,context,place_id)

## 同一个有限游标适用于本人申请或持职行政视图；换人后旧页仍失效。
static func _pagination(host, parent: Control, page: Dictionary, identity: Dictionary, context: Dictionary, place_id: String) -> void:
	var next: int=page.get("next_cursor",-1)
	var records: bool=page.get("mode","") in ["administration","documents"]
	if next>=0: host._button(parent,"后续记录" if records else "后续铺位","shop_licensing_page","next",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,next))
	if page.get("cursor",0)>0: host._button(parent,"最前记录" if records else "最前铺位","shop_licensing_page","first",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标只影响当前来源的本地页；旧控件不能把另一个人物或地点带到后续页。
static func _page(host, identity: Dictionary, actor_id: String, place_id: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor_id: return
	var page: Dictionary=host.kernel.political_shop_licensing(place_id,int(host.get_meta("shop_licensing_cursor",0)))
	if _pending(page) or page.get("place_id","")!=place_id or page.get("source_identity",{})!=identity: return
	host.set_meta("shop_licensing_cursor",cursor);host._render()

## 异步标记属于原后端协议，不把空页当作没有通缉或没有许可。
static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

## 未知拒因保留原码；没有完整司法册时明确不能替本人宣布清白。
static func reason(code: String) -> String:
	return {"PoliticalShopRegistryContinuityRequired":"辖区控制已变化，原司法册尚无完整接续证明。", "PoliticalShopRegistryUnavailable":"本地尚无可核验的完整司法册。", "PoliticalShopApplicantWanted":"原司法册仍有本人未解除的通缉记录。", "PoliticalShopPermitAlreadyValid":"此铺位已有仍有效的许可。", "PoliticalShopLocalOfficerRequired":"承办人须实际在本地柜台。", "PoliticalShopOfficerUnavailable":"在职承办人当前无法办理。", "PoliticalShopAuthorityDenied":"该公职没有当前所需的专项办证权。", "PoliticalShopCounterDeclarationRequired":"请先听取原柜台承办人的查核声明。", "PoliticalShopPremisesMismatch":"该铺位不在此柜台明确管辖的范围内。", "PoliticalShopLocalQueryRequired":"只能在本人实际所在的柜台查询。"}.get(code,"暂不能办理："+code)
