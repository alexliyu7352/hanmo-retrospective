## 野战轴向六角几何：统一邻接、地形、路径与射界；所有查询只读，不移动军队或推进世界时钟。
extends RefCounted
const DIRECTIONS=[[1,0],[0,1],[-1,1],[-1,0],[0,-1],[1,-1]]
const TERRAINS=["plain","forest","hill","water","wall"]

## 验证有限六角地形与阵营入口，重复格和错误坐标不能被后写内容覆盖。
static func validate(board: Dictionary) -> Dictionary:
	if not board.get("cells") is Array or board.cells.is_empty() or board.cells.size()>1024: return _fail("InvalidHexCells","cells")
	var seen: Dictionary={}
	for index in range(board.cells.size()):
		var cell: Variant=board.cells[index]
		var location: String="cells/"+str(index)
		if not cell is Dictionary or not point([cell.get("q"),cell.get("r")]) or not cell.get("terrain") in TERRAINS: return _fail("InvalidHexCell",location)
		var key: String=key_of([cell.q,cell.r])
		if seen.has(key): return _fail("DuplicateHexCell",location)
		seen[key]=true
		for flag in ["blocks_movement","blocks_sight"]:
			if cell.has(flag) and not cell[flag] is bool: return _fail("InvalidHexFlag",location+"/"+flag)
		for number in ["height","cover"]:
			if cell.has(number) and not whole(cell[number],0,100): return _fail("InvalidHexTerrainValue",location+"/"+number)
	for kind in ["spawns","exits"]:
		if not board.get(kind) is Array: return _fail("InvalidHexEntries",kind)
		var positions: Dictionary={}; var sides: Array=[]
		for index in range(board[kind].size()):
			var entry: Variant=board[kind][index]
			var location: String=kind+"/"+str(index)
			if not entry is Dictionary or not whole(entry.get("side"),0,1) or not point(entry.get("position")) or not seen.has(key_of(entry.position)): return _fail("InvalidHexEntry",location)
			var key: String=key_of(entry.position)
			if kind=="spawns" and positions.has(key): return _fail("DuplicateHexSpawn",location)
			var cell: Dictionary=cell_at(board,entry.position)
			if cell.get("blocks_movement",cell.terrain=="wall"): return _fail("BlockedHexEntry",location)
			positions[key]=true
			if not entry.side in sides: sides.append(entry.side)
		if kind=="spawns" and sides.size()!=2: return _fail("MissingHexSpawnSide",kind)
	return {"ok":true}

## 接受有限整数轴向坐标，拒绝小数、字符串和不完整数组。
static func point(value: Variant) -> bool:
	return value is Array and value.size()==2 and whole(value[0],-1024,1024) and whole(value[1],-1024,1024)

## 六个方向从右方起顺时针排列，返回新数组以免调用方改坏共享方向表。
static func neighbors(position: Array) -> Array:
	var result: Array=[]
	for delta in DIRECTIONS: result.append([position[0]+delta[0],position[1]+delta[1]])
	return result

## 立方坐标距离对应真正六邻接步数，不使用方格曼哈顿距离。
static func distance(from: Array, to: Array) -> int:
	var dq: int=to[0]-from[0]; var dr: int=to[1]-from[1]
	return maxi(absi(dq),maxi(absi(dr),absi(dq+dr)))

## 选取最接近目标的六角方向；分界同距时按固定方向序选择，读档后方向不变。
static func direction(from: Array, to: Array) -> int:
	var best: int=0; var score: int=2147483647
	for index in range(6):
		var delta: Array=DIRECTIONS[index]
		var dq: int=(from[0]+delta[0])-to[0]; var dr: int=(from[1]+delta[1])-to[1]
		var candidate: int=dq*dq+dr*dr+(dq+dr)*(dq+dr)
		if candidate<score: best=index; score=candidate
	return best

## 读取已有地形格，场外返回空字典，不隐式补成平原。
static func cell_at(board: Dictionary, position: Array) -> Dictionary:
	for cell in board.get("cells",[]):
		if cell.q==position[0] and cell.r==position[1]: return cell
	return {}

## 地形通行由兵种代价和实际格阻挡共同决定，零代价表示不可通过而非免费移动。
static func movement_cost(board: Dictionary, position: Array, troop: Dictionary) -> int:
	var cell: Dictionary=cell_at(board,position)
	if cell.is_empty() or cell.get("blocks_movement",cell.terrain=="wall"): return -1
	var cost: Variant=troop.get("movement_costs",{}).get(cell.terrain,0)
	return int(cost) if whole(cost,1,24) else -1

## 地势和掩护供预览及实际命中共用；缺失格不是可射击的虚构位置。
static func height(board: Dictionary, position: Array) -> int:
	var cell: Dictionary=cell_at(board,position)
	return cell.get("height",1 if cell.get("terrain")=="hill" else 0)

## 以百分数表示当前地形掩护，内容可以明确覆盖各地形的初始取值。
static func cover(board: Dictionary, position: Array) -> int:
	var cell: Dictionary=cell_at(board,position)
	return cell.get("cover",{"forest":20,"hill":10,"wall":45}.get(cell.get("terrain"),0))

## 用整数线段采样检查完整射界，落在两格边界时两格都须通透，结果正反方向一致。
static func line_of_sight(board: Dictionary, from: Array, to: Array) -> bool:
	if not point(from) or not point(to) or cell_at(board,from).is_empty() or cell_at(board,to).is_empty(): return false
	var steps: int=distance(from,to)
	var sight_height: int=maxi(height(board,from),height(board,to))
	for index in range(1,steps):
		for position in _line_cells(from,to,index,steps):
			var cell: Dictionary=cell_at(board,position)
			if cell.is_empty() or cell.get("blocks_sight",cell.terrain in ["forest","wall"]) or height(board,position)>sight_height: return false
	return true

## 求最便宜的合法路线；占用格不可穿过，进入敌控制区后本轮停止继续扩展。
static func path(board: Dictionary, from: Array, to: Array, troop: Dictionary, budget: int, occupied: Array=[], stop_points: Array=[]) -> Dictionary:
	if not point(to): return _fail("InvalidHexDestination")
	var result: Dictionary=reachable(board,from,troop,budget,occupied,stop_points)
	if not result.ok: return result
	for entry in result.cells:
		if entry.position==to: return {"ok":true,"path":entry.path,"cost":entry.cost}
	return _fail("HexDestinationUnreachable")

## 确定顺序的有限 Dijkstra 同时生成预览移动圈与 AI 候选路径，不随机挑选同代价路线。
static func reachable(board: Dictionary, from: Array, troop: Dictionary, budget: int, occupied: Array=[], stop_points: Array=[]) -> Dictionary:
	if not point(from) or movement_cost(board,from,troop)<0 or budget<0 or budget>24: return _fail("InvalidHexMovement")
	var blocked: Dictionary={}; var stops: Dictionary={}
	for position in occupied:
		if not point(position): return _fail("InvalidHexOccupation")
		if position!=from: blocked[key_of(position)]=true
	for position in stop_points:
		if not point(position): return _fail("InvalidHexControlZone")
		stops[key_of(position)]=true
	var start: String=key_of(from)
	var paths: Dictionary={start:{"position":from.duplicate(),"path":[from.duplicate()],"cost":0}}
	var pending: Dictionary={start:0}; var finished: Dictionary={}
	while not pending.is_empty():
		var keys: Array=pending.keys(); keys.sort()
		var current: String=keys[0]
		for key in keys:
			if pending[key]<pending[current]: current=key
		pending.erase(current); finished[current]=true
		var entry: Dictionary=paths[current]
		if current!=start and stops.has(current): continue
		for position in neighbors(entry.position):
			var key: String=key_of(position)
			if blocked.has(key) or finished.has(key): continue
			var cost: int=movement_cost(board,position,troop)
			if cost<0 or entry.cost+cost>budget: continue
			if paths.has(key) and paths[key].cost<=entry.cost+cost: continue
			var route: Array=entry.path.duplicate(true); route.append(position)
			paths[key]={"position":position,"path":route,"cost":entry.cost+cost}; pending[key]=entry.cost+cost
	var result: Array=[]; var keys: Array=paths.keys(); keys.sort()
	for key in keys: result.append(paths[key])
	return {"ok":true,"cells":result}

## 整数比例线段采用所有最近立方格，边界不靠浮点微扰选择穿墙的一侧。
static func _line_cells(from: Array, to: Array, index: int, steps: int) -> Array:
	var q: int=from[0]*steps+(to[0]-from[0])*index
	var r: int=from[1]*steps+(to[1]-from[1])*index
	var floor_q: int=floori(float(q)/steps); var floor_r: int=floori(float(r)/steps)
	var result: Array=[]; var best: int=9223372036854775807
	for candidate_q in [floor_q,floor_q+1]:
		for candidate_r in [floor_r,floor_r+1]:
			var error_q: int=candidate_q*steps-q; var error_r: int=candidate_r*steps-r
			var error: int=error_q*error_q+error_r*error_r+(error_q+error_r)*(error_q+error_r)
			if error<best: best=error; result=[]
			if error==best: result.append([candidate_q,candidate_r])
	return result

## 内部键只用于查询索引，不能被当成独立世界位置。
static func key_of(position: Array) -> String:
	return str(position[0])+","+str(position[1])

## 内容整数必须落在当前模块有界范围内，布尔值不作为数字接受。
static func whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 统一返回失败字段，调用者不能把非法路径解释为原地成功。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
