## 真实货单议价功能块：冻结原货单，复用唯一报价实例与交易确认，不把课程成绩或界面折扣写进钱货。
extends RefCounted
const Session=preload("res://ui/activity_session.gd")
const Challenge=preload("res://ui/challenge_screen.gd")

## 原Main货单仍拥有商品/数量/付款选择；每次挂载只读取这一份准确请求与已有议价事实。
static func render(host, parent: Control, trade_request: Dictionary) -> Control:
	var box=VBoxContainer.new(); box.name="TradeBargaining"; parent.add_child(box); var session=Session.create(host,box)
	var trade: Dictionary=trade_request.duplicate(true)
	trade.erase("quoted_total"); trade.erase("market_revision")
	var actor: String=host.kernel.command_context().actor_id
	var prepared: Dictionary=host.kernel.preview("trade_bargain_open",{"trade":trade,"mode":"manual"},actor)
	if not prepared.ok:
		host._text(box,"议价暂不可用："+Session.reason(host,prepared),15); return box
	var source: Dictionary=host.kernel.trade_bargain_source(actor,prepared.basis.basis_sha256)
	if source.get("ok",true)==false:
		host._text(box,"正在读取这份货单的议价记录…" if source.get("code")=="NativeAsyncRequired" else Session.reason(host,source),16)
		return box
	if source["duplicate"]: host._text(box,"同一货单存在重复议价记录，暂不能操作。",16); return box
	var quote_id: String=source.quote_id
	if quote_id.is_empty():
		host._text(box,"议价只影响这份真实货单；关系优惠按原报价，不另叠加。",16)
		session.command(box,"就此货单亲自议价","trade_bargain_open",{"trade":trade,"mode":"manual"}).name="BargainOpenManual"
		session.command(box,"采用自动议价","trade_bargain_open",{"trade":trade,"mode":"automatic"}).name="BargainOpenAutomatic"
		return box
	box.set_meta("bargain_quote_id",quote_id)
	Challenge.render(host,box,"trade",quote_id,actor)
	var current: Dictionary=host.kernel.trade_bargain(actor,quote_id)
	if not current.ok or current.get("consumed",false) or current.get("phase")!="completed": return box
	var confirmation: Dictionary={"bargain_quote_id":quote_id}
	var quote: Dictionary=host.kernel.preview("trade",confirmation,actor)
	if not quote.ok:
		host._text(box,"这份货单不能成交："+Session.reason(host,quote),16); return box
	confirmation.quoted_total=quote.total; confirmation.market_revision=quote.market_revision
	host._text(box,"议价后合计%d钱 · 正式折扣%.2f%% · 交割与装货耗时半日"%[quote.total,float(quote.discount_bp)/100.0],18).name="BargainConfirmedTotal"
	session.command(box,"按这份议价金额确认交易","trade",confirmation).name="BargainConfirmTrade"
	return box
