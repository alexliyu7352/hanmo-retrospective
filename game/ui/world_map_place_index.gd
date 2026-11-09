## 只索引当前地图已经提供的作者地点；检索不查询冷世界或推导可通行性。
extends RefCounted
const PAGE_SIZE: int=32

## 按可见名称检索；限定集合来自密集地点的真实ID，空集合表示整张当前地图。
static func query(data: Dictionary, text: String, candidates: Array=[], offset: int=0) -> Dictionary:
	var matches: Array=[]
	var needle: String=text.strip_edges().to_lower()
	for id in data.get("settlements",{}):
		if not candidates.is_empty() and id not in candidates: continue
		var place: Dictionary=data.settlements[id]
		var label: String=data.get("texts",{}).get(place.get("name_key",""),id)
		if not needle.is_empty() and not needle in label.to_lower(): continue
		matches.append({"id":id,"label":label,"kind":place.get("kind","")})
	matches.sort_custom(func(a: Dictionary,b: Dictionary):
		return a.id<b.id if a.label==b.label else a.label<b.label)
	var start: int=clampi(offset,0,maxi(0,matches.size()-1))
	return {"rows":matches.slice(start,start+PAGE_SIZE),"total":matches.size(),"offset":start,"has_more":start+PAGE_SIZE<matches.size()}
