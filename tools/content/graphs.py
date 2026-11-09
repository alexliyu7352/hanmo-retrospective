"""Bounded topology and dialogue checks; static reachability is not a gameplay proof."""


def reachable(start, edges):
    """Return graph reachability with an explicit worklist, avoiding recursion limits."""
    seen = set()
    todo = [start]
    while todo:
        node = todo.pop()
        if node in seen:
            continue
        seen.add(node)
        todo.extend(edges.get(node, []))
    return seen


def reverse_graph(edges):
    """Reverse directed edges to check return and end reachability."""
    result = {key: [] for key in edges}
    for origin, targets in edges.items():
        for target in targets:
            result.setdefault(target, []).append(origin)
    return result


def has_cycle(edges):
    """Detect a cycle with Kahn elimination, not a recursion-prone depth-first traversal."""
    indegree = {node: 0 for node in edges}
    for targets in edges.values():
        for target in targets:
            indegree[target] += 1
    ready = [key for key, degree in indegree.items() if degree == 0]
    removed = 0
    while ready:
        node = ready.pop(); removed += 1
        for target in edges[node]:
            indegree[target] -= 1
            if indegree[target] == 0:
                ready.append(target)
    return removed != len(edges)


def node_targets(node):
    """Enumerate control-flow edges from a validated dialogue node."""
    kind = node['kind']
    if kind == 'say': return [node['next']]
    if kind == 'choice': return [o['next'] for o in node['options']]
    if kind == 'branch': return [node['if_true'], node['if_false']]
    if kind == 'action': return [node['on_success'], node['on_failure']]
    return []


def check_graphs(pack, index):
    """Validate separate local-place, world-road and dialogue graph contracts."""
    issues = []
    place_edges = {p['id']:p['passages'] for p in pack['places']}
    for number, place in enumerate(pack['places']):
        for target in place['passages']:
            if index['places'][target]['settlement_id'] != place['settlement_id']:
                issues.append(('CROSS_SETTLEMENT_PASSAGE', ('places', number, 'passages'), 'Intercity travel requires a world route'))
        current = place['id']; visited = set()
        while current is not None:
            if current in visited:
                issues.append(('PLACE_PARENT', ('places', number, 'parent_place'), 'Parent hierarchy contains a cycle'))
                break
            visited.add(current)
            parent = index['places'][current]['parent_place']
            if parent is not None and index['places'][parent]['settlement_id'] != place['settlement_id']:
                issues.append(('PLACE_PARENT', ('places', number, 'parent_place'), 'Parent belongs to another settlement'))
                break
            current = parent
    for number, settlement in enumerate(pack['settlements']):
        entry = settlement['entry_place']
        if index['places'][entry]['settlement_id'] != settlement['id']:
            issues.append(('SETTLEMENT_ENTRY', ('settlements', number, 'entry_place'), 'Entry belongs to another settlement'))
            continue
        owned = {p['id'] for p in pack['places'] if p['settlement_id'] == settlement['id']}
        local = {k:[v for v in place_edges[k] if v in owned] for k in owned}
        if not owned.issubset(reachable(entry, local)):
            issues.append(('PLACE_REACHABILITY', ('settlements', number), 'A place cannot be reached from the settlement entry'))
        if not owned.issubset(reachable(entry, reverse_graph(local))):
            issues.append(('PLACE_RETURN', ('settlements', number), 'A place has no route back to the settlement entry'))
    roads = {s['id']:[] for s in pack['settlements']}
    for number, route in enumerate(pack['routes']):
        if route['origin'] == route['destination']:
            issues.append(('SELF_ROUTE', ('routes', number), 'A world route must lead to another settlement'))
        roads[route['origin']].append(route['destination'])
    for number, scenario in enumerate(pack['scenarios']):
        actor = index['actors'][scenario['player_actor']]
        start = index['places'][actor['initial_place']]['settlement_id']
        if not set(roads).issubset(reachable(start, roads)):
            issues.append(('WORLD_REACHABILITY', ('scenarios', number), 'Some settlements are unreachable from this start'))
        if not set(roads).issubset(reachable(start, reverse_graph(roads))):
            issues.append(('WORLD_RETURN', ('scenarios', number), 'Some settlements have no return route'))
    for number, dialogue in enumerate(pack['dialogues']):
        issues.extend(check_dialogue(dialogue, number))
    return issues


def check_dialogue(dialogue, number):
    """Find broken links, unreachable nodes, dead ends and non-yielding branch cycles."""
    path = ('dialogues', number); issues = []
    nodes = {n['id']:n for n in dialogue['nodes']}
    if len(nodes) != len(dialogue['nodes']):
        return [('DUPLICATE_NODE', path + ('nodes',), 'Dialogue node IDs must be unique locally')]
    edges = {n['id']:node_targets(n) for n in dialogue['nodes']}
    if dialogue['entry'] not in nodes:
        issues.append(('DIALOGUE_TARGET', path + ('entry',), 'Dialogue entry node is missing'))
    for n, node in enumerate(dialogue['nodes']):
        for target in node_targets(node):
            if target not in nodes:
                issues.append(('DIALOGUE_TARGET', path + ('nodes', n), 'Unknown local node: ' + target))
        if node['kind'] == 'choice':
            ids = [x['id'] for x in node['options']]
            if len(set(ids)) != len(ids):
                issues.append(('DUPLICATE_OPTION', path + ('nodes', n), 'Option IDs must be unique within a choice'))
            if not any(not o['conditions'] for o in node['options']):
                issues.append(('CHOICE_ESCAPE', path + ('nodes', n), 'A choice needs an unconditional option'))
    if any(i[0] == 'DIALOGUE_TARGET' for i in issues):
        return issues
    if set(nodes) - reachable(dialogue['entry'], edges):
        issues.append(('DIALOGUE_UNREACHABLE', path, 'There are nodes unreachable from the entry'))
    reversed_edges = reverse_graph(edges)
    can_end = set()
    for node in nodes.values():
        if node['kind'] == 'end':
            can_end.update(reachable(node['id'], reversed_edges))
    if set(nodes) - can_end:
        issues.append(('DIALOGUE_NO_END', path, 'Some nodes have no structural route to an end'))
    # Discovery is the only current action that neither prompts nor advances time.
    # Other action nodes must yield for player confirmation or a domain result.
    automatic_ids = {nid for nid, node in nodes.items()
                     if node['kind'] == 'branch'
                     or (node['kind'] == 'action' and node['command']['kind'] == 'discover_offer')}
    automatic = {nid:[target for target in edges[nid] if target in automatic_ids]
                 for nid in automatic_ids}
    if has_cycle(automatic):
        issues.append(('AUTOMATIC_CYCLE', path, 'An automatic branch/discovery loop can run without returning player control'))
    return issues
