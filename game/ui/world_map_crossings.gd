## 从当前作者地形提取真实关渡地块；不把同旗标的异地设施平均到不存在的位置。
extends RefCounted

## 同kind/flag的四邻接地块为一处，代表点必须落在其中一个原地块的整数范围内。
static func build(map: Dictionary) -> Dictionary:
	var cell_size: int=map.get("cell_size",0)
	var width: int=map.get("width",0); var height: int=map.get("height",0)
	if cell_size<=0 or width<=0 or height<=0 or width%cell_size!=0 or height%cell_size!=0 or not map.get("cells") is Array: return {"ok":false,"code":"InvalidCrossingGeometry"}
	var columns: int=width/cell_size; var rows: int=height/cell_size
	if map.cells.size()!=columns*rows: return {"ok":false,"code":"InvalidCrossingGeometry"}
	var pending: Dictionary={}
	for index in range(map.cells.size()):
		if not map.cells[index] is Dictionary: return {"ok":false,"code":"InvalidCrossingSource"}
		var cell: Dictionary=map.cells[index]
		if cell.has("crossing") and cell.has("blocked_flag"):
			if cell.crossing not in ["ford","bridge","pass"] or not cell.blocked_flag is String or cell.blocked_flag.is_empty(): return {"ok":false,"code":"InvalidCrossingSource"}
			pending[index]={"kind":cell.crossing,"flag_id":cell.blocked_flag}
	var result: Array=[]
	for start in pending.keys():
		if not pending.has(start): continue
		var source: Dictionary=pending[start]; pending.erase(start)
		var component: Array=[start]; var cursor: int=0
		while cursor<component.size():
			var at: int=component[cursor]; cursor+=1
			var position:=Vector2i(at%columns,at/columns)
			for delta in [Vector2i.LEFT,Vector2i.RIGHT,Vector2i.UP,Vector2i.DOWN]:
				var next: Vector2i=position+delta
				if next.x<0 or next.x>=columns or next.y<0 or next.y>=rows: continue
				var index: int=next.y*columns+next.x
				if pending.get(index)!={"kind":source.kind,"flag_id":source.flag_id}: continue
				pending.erase(index); component.append(index)
		component.sort()
		var center:=Vector2.ZERO
		for index in component: center+=Vector2(index%columns,index/columns)
		center/=component.size()
		var chosen: int=component[0]; var distance: float=INF
		for index in component:
			var candidate: float=Vector2(index%columns,index/columns).distance_squared_to(center)
			if candidate<distance: chosen=index; distance=candidate
		var point: Array=[(chosen%columns)*cell_size+cell_size/2,(chosen/columns)*cell_size+cell_size/2]
		result.append({"key":str(start),"kind":source.kind,"flag_id":source.flag_id,"point":point,"cells":component})
	return {"ok":true,"rows":result}
