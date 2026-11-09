#!/usr/bin/env python3
"""仅小数据验证迁移/预算/摘要边界；不会调用 compiler、linker 或游戏执行档。"""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.dont_write_bytecode = True


def module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


entry = module('run_selected_incremental')
scheduler = module('n1_compile_scheduler')
gate = module('verify_estate_pilot')


class PilotToolChecks(unittest.TestCase):
    """合成微型路径用于反例，不把它们冒作实际 CMake/RSS 或世界守门证据。"""
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='hanmo-pilot-tools-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.old = self.root / 'old'
        self.new = self.root / 'new'
        self.old.mkdir()
        self.new.mkdir()
        self.old_source = self.old / 'a.cpp'
        self.new_source = self.new / 'a.cpp'
        self.old_source.write_text('old source\n')
        self.new_source.write_text('new source\n')
        self.old_row = dict(source=str(self.old_source), object=str(self.root / 'old.o'),
                            source_sha256=entry.sha(self.old_source), cwd=str(self.old),
                            command=['c++', '-c', str(self.old_source), '-o', str(self.root / 'old.o'),
                                     '-MF', str(self.root / 'old-proof.d')])
        self.row = dict(source=str(self.new_source), object=str(self.root / 'next.o'), cwd=str(self.new),
                        source_sha256=entry.sha(self.new_source),
                        command=['c++', '-O2', '-ffunction-sections', '-c', str(self.new_source), '-o', str(self.root / 'next.o')])
        baseline = self.root / 'baseline.json'
        baseline.write_text(json.dumps(dict(files=[dict(path='a.cpp', frozen_source=str(self.old_source),
                                                        sha256=entry.sha(self.old_source))])))
        receipt = self.root / 'worktree.json'
        receipt.write_text(json.dumps(dict(path=str(self.new), ordinary_independent_files_no_hardlinks=True,
                                           commit='a'*40, tree='b'*40)))
        self.request = dict(schema_version=1, reason='isolated_worktree_full_rebuild',
                            old_source_root=str(self.old), new_source_root=str(self.new),
                            new_baseline_commit='a'*40, new_baseline_tree='b'*40,
                            old_baseline_reference=entry.identity(baseline), new_worktree_receipt=entry.identity(receipt))
        self.request_path = self.root / 'relocation.json'
        self.request_path.write_text(json.dumps(self.request))

    def verify(self, queue=None, selected=None):
        return entry.verify_source_relocation(self.request_path, queue or [self.row],
                    [self.row] if selected is None else selected,
                    [self.old_row], [self.old_row], [self.old_row])

    def test_migration_is_explicit_one_to_one_and_full_rebuild(self):
        original = deepcopy(self.old_row)
        relocation = self.verify()
        self.assertEqual(relocation['history_sources'], {str(self.new_source): str(self.old_source)})
        self.assertFalse(relocation['cache_reuse_allowed'])
        plan = entry.make_plan([self.row], [self.row], {}, {}, [self.old_row],
                               scheduler.command_key, True, relocation)
        self.assertEqual(plan['compile_count'], 1)
        self.assertEqual(plan['reused'], [])
        self.assertEqual(self.old_row, original)
        self.assertEqual(plan['rebuild'][0]['source'], str(self.new_source))

    def test_selection_drop_and_outside_source_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'logical resident selection'):
            self.verify(selected=[])
        with self.assertRaisesRegex(RuntimeError, 'outside declared root'):
            entry.logical_sources([dict(source=str(self.old / '..' / 'escape.cpp'))], self.old)
        with self.assertRaisesRegex(RuntimeError, 'Duplicate logical source'):
            entry.logical_sources([self.old_row, self.old_row], self.old)

    def test_old_include_or_bad_baseline_refused(self):
        bad = deepcopy(self.row)
        bad['command'].append('-I' + str(self.old))
        with self.assertRaisesRegex(RuntimeError, 'old source tree'):
            self.verify(queue=[bad])
        self.old_source.write_text('unapproved changed old baseline\n')
        with self.assertRaisesRegex(RuntimeError, 'baseline/source evidence differs'):
            self.verify()

    def test_migration_does_not_authorize_old_object_or_mf_overwrite(self):
        bad = deepcopy(self.row)
        bad['object'] = self.old_row['object']
        with self.assertRaisesRegex(RuntimeError, 'cannot overwrite an old object/MF/log'):
            self.verify(queue=[bad], selected=[bad])
        bad['object'] = str(self.root / 'old-proof')
        with self.assertRaisesRegex(RuntimeError, 'cannot overwrite an old object/MF/log'):
            self.verify(queue=[bad], selected=[bad])

    def test_following_worktree_preserves_the_same_external_flecs(self):
        # 真迁移口的有限边界：旧新队列同一外锁 C TU，仍禁止任意外根与旧对象覆盖。
        root = self.root / 'flecs'
        (root / 'distr').mkdir(parents=True)
        source = root / 'distr/flecs.c'
        source.write_text('/* synthetic locked dependency */\n')
        lock = self.new / 'native/dependencies.lock.json'
        lock.parent.mkdir()
        lock.write_text(json.dumps({'libraries': {'flecs': {'acquisition_state': 'verified',
                         'files': {'distr/flecs.c': entry.sha(source)}}}}))
        self.request['locked_flecs_source'] = dict(root=str(root), source=str(source),
                        sha256=entry.sha(source), dependency_lock=entry.identity(lock))
        self.request_path.write_text(json.dumps(self.request))
        old = dict(source=str(source), object=str(self.root / 'old-flecs.o'), cwd=str(self.old),
                   source_sha256=entry.sha(source), dependency_file=str(self.root / 'old-flecs.d'),
                   command=['cc', '-O2', '-c', str(source), '-o', str(self.root / 'old-flecs.o')])
        new = dict(old, object=str(self.root / 'new-flecs.o'), cwd=str(self.new),
                   command=['cc', '-O2', '-c', str(source), '-o', str(self.root / 'new-flecs.o')])
        original = deepcopy(old)
        def verify(prior=old, result=old, current=new):
            return entry.verify_source_relocation(self.request_path,
                       [self.row, current], [self.row], [self.old_row, prior],
                       [self.old_row], [self.old_row, result])
        result = verify()
        self.assertEqual(result['history_sources'], {str(self.new_source): str(self.old_source)})
        self.assertEqual(result['locked_flecs_source']['source'], str(source))
        self.assertFalse(result['cache_reuse_allowed'])
        self.assertEqual(old, original)
        with self.assertRaisesRegex(RuntimeError, 'only the same locked Flecs'):
            verify(prior=dict(old, source=str(root / 'other.c')))
        with self.assertRaisesRegex(RuntimeError, 'only the same locked Flecs'):
            verify(prior=dict(old, source_sha256='0' * 64))
        with self.assertRaisesRegex(RuntimeError, 'successful source evidence differs'):
            verify(result=dict(old, source_sha256='0' * 64))
        with self.assertRaisesRegex(RuntimeError, 'compile flags changed'):
            verify(current=dict(new, command=new['command'] + ['-O3']))
        with self.assertRaisesRegex(RuntimeError, 'cannot overwrite an old object/MF/log'):
            verify(current=dict(new, object=old['object']))

    def test_external_source_requires_exact_original_flecs_lock(self):
        root = self.root / 'flecs'
        (root / 'distr').mkdir(parents=True)
        source = root / 'distr/flecs.c'
        source.write_text('/* synthetic locked dependency */\n')
        external = dict(source=str(source), object=str(self.root / 'flecs.o'), cwd=str(self.new),
                        source_sha256=entry.sha(source),
                        command=['cc', '-O2', '-c', str(source), '-o', str(self.root / 'flecs.o')])
        with self.assertRaisesRegex(RuntimeError, 'explicit Flecs dependency lock'):
            self.verify(queue=[self.row, external])
        lock = self.new / 'native/dependencies.lock.json'
        lock.parent.mkdir()
        lock.write_text(json.dumps({'libraries': {'flecs': {'acquisition_state': 'verified',
                         'files': {'distr/flecs.c': entry.sha(source)}}}}))
        self.request['locked_flecs_source'] = dict(root=str(root), source=str(source),
                        sha256=entry.sha(source), dependency_lock=entry.identity(lock))
        self.request_path.write_text(json.dumps(self.request))
        result = self.verify(queue=[self.row, external])
        self.assertEqual(result['locked_flecs_source']['source'], str(source))
        self.assertNotIn(str(source), result['history_sources'])
        extra = dict(external, source=str(root / 'other.c'))
        with self.assertRaisesRegex(RuntimeError, 'single locked Flecs'):
            self.verify(queue=[self.row, external, extra])
        source.write_text('changed dependency\n')
        with self.assertRaisesRegex(RuntimeError, 'source bytes differ'):
            self.verify(queue=[self.row, external])

    def test_rss_alias_preserves_original_evidence_and_margin(self):
        timing = self.root / 'time.txt'
        timing.write_text('Maximum resident set size (kbytes): 100000\n')
        original = dict(self.old_row, exit_code=0, time_log=str(timing), maximum_resident_set_kib=100000,
                        frozen_manifest_sha256='1'*64,
                        original_command=['c++', '-O0', '-c', str(self.old_source), '-o', str(self.root / 'old.o')])
        history = [original]
        before = deepcopy(history)
        policy = dict(unknown_peak_bytes=2*1024**3)
        unknown = scheduler.budget_for(self.row, history, policy, '2'*64)
        self.assertFalse(unknown['known'])
        actual = scheduler.budget_for(self.row, history, policy, '2'*64, str(self.old_source))
        self.assertTrue(actual['known'])
        self.assertEqual(actual['bytes'], (100000*1024*3+1)//2 + 128*1024**2)
        self.assertTrue(actual['evidence'][0]['mismatch']['source_path'])
        self.assertEqual(actual['evidence'][0]['actual_source'], str(self.new_source))
        self.assertEqual(actual['evidence'][0]['historical_source'], str(self.old_source))
        self.assertEqual(history, before)
        failed = dict(original, exit_code=137)
        lower = scheduler.budget_for(self.row, [failed], policy, '2'*64, str(self.old_source))
        self.assertFalse(lower['known'])
        self.assertTrue(lower['exclusive'])
        self.assertTrue(lower['failed_peak_lower_bounds'])

    def test_digest_keeps_values_but_not_object_format(self):
        first, second = self.root / 'a.json', self.root / 'b.json'
        first.write_text('{"world_id":"same","rows":[1,2],"tick":3}')
        second.write_text('{\n  "tick": 3, "rows": [1, 2], "world_id": "same"\n}')
        self.assertNotEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(gate.state_digest(first), gate.state_digest(second))
        second.write_text('{"world_id":"other","rows":[1,2],"tick":3}')
        self.assertNotEqual(gate.state_digest(first), gate.state_digest(second))
        second.write_text('{"tick":3,"tick":4}')
        with self.assertRaisesRegex(RuntimeError, 'Duplicate state key'):
            gate.state_digest(second)

    def test_one_gigabyte_disk_floor_is_reserved_before_dispatch(self):
        floor = 1_000_000_000
        self.assertEqual(scheduler.disk_admission(floor + 300, 100, 200, floor),
                         (True, floor + 300))
        self.assertFalse(scheduler.disk_admission(floor + 299, 100, 200, floor)[0])
        self.assertFalse(scheduler.disk_admission(floor - 1, 0, 0, floor)[0])
        with self.assertRaisesRegex(RuntimeError, 'at least 1000000000'):
            scheduler.disk_admission(floor, 0, 0, 50 * 1024 ** 2)

    def test_bounded_attempt_only_for_known_exclusive_nonfitting_history(self):
        gib = 1024 ** 3
        policy = dict(soft_budget_bytes=5*gib, allow_bounded_heavy_attempt=True)
        snapshot = dict(visible_available_bytes=8*gib, visible_limit_bytes=None,
                        cgroup_limit_status='unknown_no_mount')
        heavy = dict(bytes=8*gib, known=True, exclusive=True)
        self.assertFalse(scheduler.admission(heavy, [], snapshot, policy)[0])
        limit = scheduler.bounded_heavy_limit(heavy, [], snapshot, policy)
        self.assertEqual(limit['as_limit_bytes'], 8*gib-scheduler.RESERVE-
                         scheduler.BOUNDED_DRIVER_RESERVE-scheduler.BOUNDED_MONITOR_GUARD)
        self.assertEqual(limit['original_estimate_bytes'], 8*gib)
        self.assertFalse(limit['group_hard_limit'])
        self.assertFalse(limit['automatic_retry'])
        self.assertEqual(limit['cgroup_limit_status'], 'unknown_no_mount')
        for changed, active, chosen_policy, snap in (
                (dict(heavy, known=False), [], policy, snapshot),
                (dict(heavy, exclusive=False), [], policy, snapshot),
                (heavy, [heavy], policy, snapshot),
                (heavy, [], dict(policy, allow_bounded_heavy_attempt=False), snapshot),
                (dict(heavy, bytes=3*gib), [], policy, snapshot),
                (heavy, [], policy, dict(snapshot, visible_available_bytes=4*gib))):
            self.assertIsNone(scheduler.bounded_heavy_limit(changed, active, snap, chosen_policy))
        tight = dict(snapshot, visible_available_bytes=5*gib, visible_limit_bytes=6*gib,
                     cgroup_limit_status='visible')
        self.assertLess(scheduler.bounded_heavy_limit(heavy, [], tight, policy)['as_limit_bytes'],
                        limit['as_limit_bytes'])

    def standing_policy(self):
        # 合成授权只测试有限输入形状，不是生产授权/实测内存，也不启动编译器。
        source = self.new / 'native/world/src/resident/kernel/world_codec.cpp'
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text('synthetic world codec\n')
        authorization = self.root / 'standing-authorization.json'
        authorization.write_text(json.dumps(dict(user_slack_ts='1793290848.945159',
            standing_rule_recorded=True, user_text='synthetic policy-shape fixture',
            physical_stop_available_bytes=scheduler.GIB,
            as_formula='actual_MemTotal_bytes - 2147483648')))
        proof = dict(path=str(authorization), sha256=entry.sha(authorization))
        policy = dict(soft_budget_bytes=5*scheduler.GIB, allow_bounded_heavy_attempt=True,
                      timeout_seconds=1800, standing_exclusive_memory_rule=dict(
            rule_id='user_20261029_memtotal_minus_2gib_available_1gib',
            world_codec_source=str(source), authorization=proof))
        headers = [dict(path=str(source), sha256=entry.sha(source)), proof]
        return source, policy, headers

    def test_standing_authority_is_narrow_and_frozen(self):
        source, policy, headers = self.standing_policy()
        scheduler.validate_standing_rule(policy, headers)
        for changed in (
                dict(policy, timeout_seconds=600),
                dict(policy, standing_exclusive_memory_rule=dict(
                    policy['standing_exclusive_memory_rule'], physical_floor=0)),
                dict(policy, standing_exclusive_memory_rule=dict(
                    policy['standing_exclusive_memory_rule'], world_codec_source=str(self.new_source)))):
            with self.assertRaises(RuntimeError):
                scheduler.validate_standing_rule(changed, headers)
        with self.assertRaisesRegex(RuntimeError, 'actual frozen inputs'):
            scheduler.validate_standing_rule(policy, headers[:1])
        source.write_text('changed after freezing\n')
        with self.assertRaisesRegex(RuntimeError, 'actual frozen inputs'):
            scheduler.validate_standing_rule(policy, headers)

    def test_standing_world_codec_never_uses_light_or_fit_shortcut(self):
        source, policy, _ = self.standing_policy()
        g = scheduler.GIB
        snapshot = dict(host_total_bytes=10*g, mem_available_bytes=8*g,
                        visible_available_bytes=8*g, visible_limit_bytes=None,
                        cgroup_limit_status='unknown_no_mount')
        row = dict(self.row, source=str(source))
        for original in (dict(bytes=g, exclusive=False, known=True),
                         dict(bytes=3*g, exclusive=True, known=True),
                         dict(bytes=2*g, exclusive=True, known=False)):
            before = deepcopy(original)
            ok, reason, limit, effective = scheduler.dispatch_admission(row, original, [], snapshot, policy)
            self.assertTrue(ok)
            self.assertEqual(reason, 'standing_world_codec')
            self.assertTrue(effective['exclusive'])
            self.assertEqual(limit['as_limit_bytes'], 8*g)
            self.assertEqual(limit['physical_stop_available_bytes'], g)
            self.assertFalse(limit['group_hard_limit'])
            self.assertFalse(limit['automatic_retry'])
            self.assertEqual(original, before)
            self.assertFalse(scheduler.admission(dict(bytes=g, exclusive=False), [effective], snapshot, policy)[0])
            self.assertFalse(scheduler.dispatch_admission(row, original, [dict(bytes=g, exclusive=False)], snapshot, policy)[0])
        same_name = dict(row, source=str(self.new / 'other/world_codec.cpp'))
        self.assertIsNone(scheduler.dispatch_admission(same_name, dict(bytes=g, exclusive=False, known=True), [], snapshot, policy)[2])

    def test_standing_other_tu_requires_known_nonfitting_heavy(self):
        _, policy, _ = self.standing_policy()
        g = scheduler.GIB
        snapshot = dict(host_total_bytes=10*g, mem_available_bytes=8*g,
                        visible_available_bytes=8*g, visible_limit_bytes=None,
                        cgroup_limit_status='unknown_no_mount')
        heavy = dict(bytes=9*g, exclusive=True, known=True)
        ok, reason, limit, _ = scheduler.dispatch_admission(self.row, heavy, [], snapshot, policy)
        self.assertTrue(ok)
        self.assertEqual(reason, 'standing_nonfitting_heavy')
        self.assertEqual(limit['original_estimate_bytes'], 9*g)
        for budget in (dict(heavy, known=False), dict(heavy, bytes=3*g),
                       dict(bytes=g, exclusive=False, known=True)):
            self.assertIsNone(scheduler.dispatch_admission(self.row, budget, [], snapshot, policy)[2])
        no_standing = dict(policy)
        del no_standing['standing_exclusive_memory_rule']
        for budget in (dict(bytes=g, exclusive=False, known=True), dict(bytes=3*g, exclusive=True, known=True)):
            actual = scheduler.dispatch_admission(self.row, budget, [], snapshot, no_standing)
            self.assertEqual(actual[:2], scheduler.admission(budget, [], snapshot, no_standing))
            self.assertIsNone(actual[2])

    def test_standing_rechecks_actual_memory_and_preserves_other_guards(self):
        source, policy, _ = self.standing_policy()
        g = scheduler.GIB
        snapshot = dict(host_total_bytes=10*g, mem_available_bytes=8*g,
                        visible_available_bytes=8*g, visible_limit_bytes=None,
                        cgroup_limit_status='unknown_no_mount')
        budget = dict(bytes=g, exclusive=False, known=True)
        row = dict(self.row, source=str(source))
        self.assertTrue(scheduler.dispatch_admission(row, budget, [], snapshot, policy)[0])
        later = dict(snapshot, mem_available_bytes=g-1, visible_available_bytes=g-1)
        self.assertFalse(scheduler.dispatch_admission(row, budget, [], later, policy)[0])
        limit = scheduler.dispatch_admission(row, budget, [], snapshot, policy)[2]
        group = dict(members=[dict(pid=1)], rss_bytes=2*g, errors=[])
        self.assertTrue(scheduler.limited_memory_guard(limit, later, group))
        self.assertFalse(scheduler.limited_memory_guard(limit, dict(snapshot, visible_available_bytes=g), group))
        default = dict(limit)
        del default['physical_stop_available_bytes']
        self.assertTrue(scheduler.limited_memory_guard(default, dict(snapshot, visible_available_bytes=2*g), group))
        self.assertFalse(scheduler.limited_memory_guard(limit, dict(snapshot, visible_available_bytes=2*g), group))
        self.assertTrue(scheduler.limited_memory_guard(limit, snapshot, dict(group, members=[])))
        self.assertTrue(scheduler.limited_memory_guard(limit, snapshot, dict(group, rss_bytes=limit['group_rss_stop_bytes']+1)))
        tight_cgroup = dict(snapshot, visible_available_bytes=g-1, visible_limit_bytes=2*g, cgroup_limit_status='visible')
        self.assertFalse(scheduler.dispatch_admission(row, budget, [], tight_cgroup, policy)[0])
        import ast, inspect
        body = ast.parse(inspect.getsource(scheduler.compile_queue))
        calls = [x for x in ast.walk(body) if isinstance(x, ast.Call) and isinstance(x.func, ast.Name)
                 and x.func.id == 'dispatch_admission']
        self.assertEqual(len(calls), 2, 'candidate and after-hash dispatch must use the same finite policy')

    def test_wait_diagnostics_are_bounded_across_pid_churn(self):
        records = {}
        for number in range(10000):
            first = scheduler.observe_wait(records, 'real-source.cpp', 'exclusive', 0,
                dict(at=str(number), active_count=number % 8, visible_available_bytes=number))
            self.assertEqual(first, number == 0)
        self.assertEqual(len(records), 1)
        record = next(iter(records.values()))
        self.assertEqual(record['suppressed_observations'], 9999)
        self.assertEqual(record['first_at'], '0')
        self.assertEqual(record['last_at'], '9999')
        self.assertTrue(scheduler.observe_wait(records, 'real-source.cpp', 'exclusive', 1,
                                              dict(at='retry-budget')))
        self.assertEqual(len(records), 2)

    def test_child_limit_does_not_run_in_test_parent_and_exit_race_is_safe(self):
        # 限额调用全 mock，不改变本测试进程或任何其他进程的资源。
        with patch.object(scheduler.resource, 'setrlimit') as setlimit:
            scheduler.child_address_limit(5*1024**3)
        self.assertEqual(setlimit.call_args_list[0].args,
                         (scheduler.resource.RLIMIT_AS, (5*1024**3, 5*1024**3)))
        self.assertEqual(setlimit.call_args_list[1].args, (scheduler.resource.RLIMIT_CORE, (0, 0)))
        proc = Mock(pid=123)
        proc.poll.return_value = None
        alive, empty = dict(members=[dict(pid=124)], errors=[]), dict(members=[], errors=[])
        with patch.object(scheduler.os, 'killpg', side_effect=ProcessLookupError) as kill, \
                patch.object(scheduler, 'owned_group_memory', side_effect=[alive, empty]):
            scheduler.stop_owned(proc)
        kill.assert_called_once_with(123, scheduler.signal.SIGTERM)
        proc.wait.assert_called_once_with(timeout=5)
        proc.poll.return_value = 0
        with patch.object(scheduler.os, 'killpg') as kill, \
                patch.object(scheduler, 'owned_group_memory', return_value=empty):
            scheduler.stop_owned(proc)
        kill.assert_not_called()

    def test_exited_leader_does_not_hide_its_remaining_children(self):
        proc = Mock(pid=123)
        proc.poll.return_value = 0
        alive, empty = dict(members=[dict(pid=124)], errors=[]), dict(members=[], errors=[])
        with patch.object(scheduler, 'owned_group_memory', side_effect=[alive, alive, empty]), \
                patch.object(scheduler.time, 'monotonic', side_effect=[0, 6, 6]), \
                patch.object(scheduler.time, 'sleep'), patch.object(scheduler.os, 'killpg') as kill:
            scheduler.stop_owned(proc)
        self.assertEqual([x.args for x in kill.call_args_list],
                         [(123, scheduler.signal.SIGTERM), (123, scheduler.signal.SIGKILL)])
        proc.wait.assert_called_once_with(timeout=5)
        with patch.object(scheduler, 'owned_group_memory', return_value=alive), \
                patch.object(scheduler.time, 'monotonic', side_effect=[0, 6, 6, 12]), \
                patch.object(scheduler.time, 'sleep'), patch.object(scheduler.os, 'killpg'):
            with self.assertRaisesRegex(RuntimeError, 'no further dispatch'):
                scheduler.stop_owned(proc)

    def test_partial_repair_reuses_only_actual_unaffected_success(self):
        prior = self.root / 'terminal'
        (prior / 'compile').mkdir(parents=True)
        header = self.new / 'actual.hpp'
        header.write_text('old header\n')
        dep = prior / 'actual.d'
        dep.write_text(f"next.o: {self.new_source} {header}\n")
        obj = Path(self.row['object'])
        obj.write_bytes(b'original successful object')
        actual = dict(self.row, exit_code=0, atomic_cache_update=True,
            object_sha256=entry.sha(obj), dependency_file=str(dep), dependency_sha256=entry.sha(dep))
        metadata = {
            'queue': {'queue': [self.row]}, 'selected': {'selected': [self.row]},
            'frozen_manifest': {'files': [entry.identity(self.new_source), entry.identity(header)]},
            'toolchain_manifest': {'files': []}}
        batch = {}
        for key, value in metadata.items():
            path = prior / (key + '.json')
            path.write_text(json.dumps(value))
            batch[key], batch[key + '_sha256'] = str(path), entry.sha(path)
        (prior / 'batch-inputs.json').write_text(json.dumps(batch))
        (prior / 'compile/result.json').write_text(json.dumps(dict(status='failed', ended_at='real-terminal')))
        active = prior / 'compile/active-calls.json'
        active.write_text('[]')
        (prior / 'compile-partial-results.json').write_text(json.dumps([actual]))
        headers = entry.manifest_files(batch['frozen_manifest'], False)
        def plan():
            return entry.repair_plan(prior, [self.row], [self.row], headers,
                self.new / 'next-inputs/frozen.json', batch['toolchain_manifest_sha256'], scheduler.command_key)[0]
        self.assertEqual(plan()['reused'], [self.row['source']])
        header.write_text('fixed header\n')
        headers[str(header)] = entry.sha(header)
        self.assertEqual(plan()['compile_count'], 1)
        self.assertEqual(plan()['reused'], [])
        active.write_text('[{"pid":123}]')
        with self.assertRaisesRegex(RuntimeError, 'drained terminal'):
            plan()

    def test_keep_going_classifies_only_ordinary_compile_errors(self):
        self.assertTrue(scheduler.ordinary_compile_failure(1, {}, 'error: missing declaration', False))
        # AS 包装存在不把真正语法诊断误作资源耗尽，但限额触发绝不能继续。
        self.assertTrue(scheduler.ordinary_compile_failure(1, {'resource_limit_wrapper': {'as_limit_bytes': 1}},
                                                          'error: no matching function', False))
        for code, result, output, resource_evidence in (
                (0, {}, '', False), (-9, {}, '', False), (137, {}, '', False),
                (1, {}, 'cannot allocate memory', True),
                (1, {}, 'internal compiler error: assertion', False),
                (1, {'timeout_seconds': 10}, 'error: declaration', False),
                (1, {'bounded_memory_guard_stop': True}, '', False),
                (1, {'disk_low_watermark_stop': True}, '', False),
                (1, {'owned_group_remaining_after_leader_exit': True}, '', False)):
            self.assertFalse(scheduler.ordinary_compile_failure(code, result, output, resource_evidence))


    def pack_policy(self):
        return dict(soft_budget_bytes=5*scheduler.GIB, unknown_peak_bytes=5*scheduler.GIB,
                    measured_rss_packing={'rule_id': 'user_20261029_verified_rss_packing'},
                    allow_bounded_heavy_attempt=True)

    def pack_snapshot(self, available=8):
        return dict(host_total_bytes=10*scheduler.GIB, visible_limit_bytes=None,
                    visible_available_bytes=int(available*scheduler.GIB),
                    mem_available_bytes=int(available*scheduler.GIB), cgroup_limit_status='unknown_no_mount')

    def test_measured_known_heavy_can_pack_without_rewriting_history(self):
        g=scheduler.GIB
        original=dict(bytes=4*g, known=True, exclusive=True, measured_peak_bytes=2*g,
                      failed_peak_lower_bounds=[], evidence=[{'original': 'untouched'}])
        before=deepcopy(original); gate=scheduler.packing_budget(original)
        self.assertFalse(gate['exclusive']);self.assertEqual(gate['bytes'],2*g)
        active=dict(gate,observed_group_rss_bytes=g)
        self.assertTrue(scheduler.packed_admission(gate,[active],self.pack_snapshot(),self.pack_policy())[0])
        self.assertEqual(original,before);self.assertEqual(gate['original_padded_bytes'],4*g)

    def test_packing_total_cgroup_worker_and_exclusive_caps(self):
        g=scheduler.GIB; b=dict(bytes=4*g,exclusive=False);a=dict(b,observed_group_rss_bytes=4*g)
        snap=self.pack_snapshot(20);snap['host_total_bytes']=9*g
        self.assertEqual(scheduler.packed_admission(b,[a],snap,self.pack_policy()),(False,'packing_total_cap'))
        snap['host_total_bytes']=20*g;snap['visible_limit_bytes']=9*g
        self.assertEqual(scheduler.packed_admission(b,[a],snap,self.pack_policy()),(False,'packing_total_cap'))
        self.assertEqual(scheduler.packed_admission(b,[a]*8,self.pack_snapshot(40),self.pack_policy()),(False,'worker_limit'))
        self.assertEqual(scheduler.packed_admission(dict(b,exclusive=True),[a],snap,self.pack_policy()),(False,'exclusive'))

    def test_packing_headroom_uses_current_rss_and_driver_reserve(self):
        g=scheduler.GIB;b=dict(bytes=2*g,exclusive=False)
        a=dict(b,observed_group_rss_bytes=2*g,bounded_observed_peak_group_rss=100*g)
        self.assertTrue(scheduler.packed_admission(b,[a],self.pack_snapshot(5),self.pack_policy())[0])
        a['observed_group_rss_bytes']=0
        self.assertEqual(scheduler.packed_admission(b,[a],self.pack_snapshot(5),self.pack_policy()),(False,'available_memory'))
        with self.assertRaisesRegex(RuntimeError,'Current owned RSS'):
            scheduler.packed_admission(b,[b],self.pack_snapshot(),self.pack_policy())
        # 两个工位各保 driver；不能只比较原单进程峰和。
        a['observed_group_rss_bytes']=2*g
        self.assertFalse(scheduler.packed_admission(b,[a],self.pack_snapshot(4.5),self.pack_policy())[0])

    def test_packing_unknown_and_failed_lower_bound_remain_exclusive(self):
        b=dict(bytes=3*scheduler.GIB,known=False,exclusive=True)
        self.assertTrue(scheduler.packing_budget(b)['exclusive'])
        b.update(known=True,measured_peak_bytes=scheduler.GIB,failed_peak_lower_bounds=[{'peak_bytes':4*scheduler.GIB}])
        self.assertTrue(scheduler.packing_budget(b)['exclusive'])
        self.assertEqual(scheduler.packing_budget(b)['bytes'],b['bytes'])

    def test_packing_world_codec_still_uses_only_its_exclusive_rule(self):
        g=scheduler.GIB;policy=self.pack_policy();policy['standing_exclusive_memory_rule']={'world_codec_source':str(self.new_source),'authorization':{'path':'proof','sha256':'a'*64}}
        budget=dict(bytes=4*g,exclusive=True,known=True,measured_peak_bytes=2*g,failed_peak_lower_bounds=[])
        result=scheduler.dispatch_admission(self.row,budget,[],self.pack_snapshot(),policy)
        self.assertTrue(result[0]);self.assertTrue(result[3]['exclusive']);self.assertEqual(result[2]['physical_stop_available_bytes'],g)
        self.assertFalse(scheduler.dispatch_admission(self.row,budget,[dict(bytes=g,exclusive=False)],self.pack_snapshot(),policy)[0])
        ordinary=dict(self.row,source=str(self.new/'ordinary.cpp'))
        result=scheduler.dispatch_admission(ordinary,budget,[],self.pack_snapshot(),policy)
        self.assertTrue(result[0]);self.assertIsNone(result[2]);self.assertFalse(result[3]['exclusive'])

    def test_packing_stable_rss_time_original_order(self):
        b=dict(bytes=2,known=True,measured_peak_bytes=1,measured_elapsed_seconds=5)
        rows=[(4,dict(b,measured_elapsed_seconds=2)),(3,dict(b,measured_elapsed_seconds=2)),(1,b),(0,dict(b,known=False))]
        rows.sort(key=lambda p:scheduler.packing_order_key(*p))
        self.assertEqual([x[0] for x in rows],[3,4,1,0])

    def test_pack_physical_guard_and_actual_owned_group_required(self):
        active={3:{'proc':Mock()}};active[3]['proc'].poll.return_value=None
        groups={3:dict(members=[{'pid':3}],errors=[],rss_bytes=1)}
        snap=self.pack_snapshot(2.25)
        self.assertFalse(scheduler.packed_memory_guard(snap,groups,active))
        snap['visible_available_bytes']-=1
        self.assertTrue(scheduler.packed_memory_guard(snap,groups,active))
        groups[3]['members']=[]
        self.assertTrue(scheduler.packed_memory_guard(self.pack_snapshot(),groups,active))
        self.assertFalse(scheduler.ordinary_compile_failure(1,{'packed_memory_guard_stop':True},'error: declaration',False))

    def test_pack_stop_does_not_rewrite_already_drained_success(self):
        done=Mock();done.poll.return_value=0
        live=Mock();live.poll.return_value=None
        items={1:dict(proc=done,result={}),2:dict(proc=live,result={})}
        groups={1:dict(members=[],errors=[]),2:dict(members=[{'pid':2}],errors=[])}
        with patch.object(scheduler,'stop_owned') as stop:
            scheduler.stop_packed_active(items,groups,{'reason':'test'})
            stop.assert_called_once_with(live)
        self.assertEqual(items[1]['result'],{})
        self.assertIn('packed_memory_guard_stop',items[2]['result'])
        self.assertEqual(scheduler.gnu_elapsed_seconds('Elapsed (wall clock) time (h:mm:ss or m:ss): 1:02.50'),62.5)
        self.assertEqual(scheduler.gnu_elapsed_seconds('Elapsed (wall clock) time (h:mm:ss or m:ss): 1:02:03.25'),3723.25)
        self.assertIsNone(scheduler.gnu_elapsed_seconds('Elapsed (wall clock) time (h:mm:ss or m:ss): NaN:01'))

    def test_packing_keeps_actual_and_alias_gnu_time_origins_distinct(self):
        log=self.root/'rss.txt';log.write_text('Maximum resident set size (kbytes): 100000\n')
        old=dict(self.old_row,exit_code=0,time_log=str(log),maximum_resident_set_kib=100000,frozen_manifest_sha256='1'*64,seconds=7)
        actual=dict(self.row,exit_code=0,time_log=str(log),maximum_resident_set_kib=100000,frozen_manifest_sha256='2'*64,seconds=3)
        result=scheduler.budget_for(self.row,[old,actual],self.pack_policy(),'2'*64,str(self.old_source))
        self.assertEqual({x['historical_source'] for x in result['evidence']},{str(self.old_source),str(self.new_source)})
        self.assertEqual(result['measured_elapsed_seconds'],7)

    def test_named_guard_125_keeps_measured_lower_bound_and_exclusive(self):
        timing=self.root/'success-time.txt';timing.write_text('Maximum resident set size (kbytes): 100000\n')
        success=dict(self.row,exit_code=0,time_log=str(timing),maximum_resident_set_kib=100000,frozen_manifest_sha256='2'*64)
        for flag in ('packed_memory_guard_stop','bounded_memory_guard_stop','owned_group_remaining_after_leader_exit'):
            failed=dict(success,exit_code=125,**{flag:{'actual':'recorded stop'}})
            result=scheduler.budget_for(self.row,[success,failed],self.pack_policy(),'2'*64)
            self.assertEqual(len(result['resource_stop_evidence']),1)
            self.assertEqual(result['failed_peak_lower_bounds'][0]['peak_bytes'],100000*1024)
            self.assertTrue(scheduler.packing_budget(result)['exclusive'])
        bare=scheduler.budget_for(self.row,[success,dict(success,exit_code=125)],self.pack_policy(),'2'*64)
        self.assertEqual(bare['failed_peak_lower_bounds'],[]);self.assertEqual(bare['resource_stop_evidence'],[])
        self.assertFalse(scheduler.packing_budget(bare)['exclusive'])

    def test_guard_without_complete_gnu_time_stays_isolated_without_fake_peak(self):
        timing=self.root/'success-time.txt';timing.write_text('Maximum resident set size (kbytes): 100000\n')
        success=dict(self.row,exit_code=0,time_log=str(timing),maximum_resident_set_kib=100000,frozen_manifest_sha256='2'*64)
        failed=dict(success,exit_code=125,time_log=str(self.root/'not-written.time'),maximum_resident_set_kib=None,packed_memory_guard_stop={'actual':'recorded stop'})
        result=scheduler.budget_for(self.row,[success,failed],self.pack_policy(),'2'*64)
        self.assertTrue(result['known']);self.assertEqual(result['failed_peak_lower_bounds'],[])
        self.assertEqual(len(result['resource_stop_evidence']),1)
        self.assertTrue(scheduler.packing_budget(result)['exclusive'])
        self.assertGreaterEqual(result['bytes'],self.pack_policy()['unknown_peak_bytes'])
        unknown=scheduler.budget_for(self.row,[failed],self.pack_policy(),'2'*64)
        self.assertFalse(unknown['known']);self.assertNotIn('measured_peak_bytes',unknown)
        self.assertEqual(unknown['failed_peak_lower_bounds'],[])

    def test_packing_tool_and_user_proof_is_checked_without_cpp_manifest_mutation(self):
        source=self.new/'native/world/src/resident/kernel/world_codec.cpp';source.parent.mkdir(parents=True);source.write_text('same source')
        standing=self.root/'standing.json';standing.write_text(json.dumps(dict(user_slack_ts='1793290848.945159',standing_rule_recorded=True,user_text='authorized',physical_stop_available_bytes=scheduler.GIB,as_formula='actual_MemTotal_bytes - 2147483648')))
        packing=self.root/'packing.json';packing.write_text(json.dumps(dict(user_slack_ts='1793302239.976639',user_text='authorized',packing_basis='verified_previous_success_GNU_time_max_RSS',cap_formula='actual_MemTotal_bytes - 2147483648')))
        identity=lambda p:dict(path=str(Path(p).resolve()),sha256=entry.sha(p))
        policy=self.pack_policy();policy.update(max_workers=8,reserve_bytes=scheduler.RESERVE,timeout_seconds=1800,keep_going_compile_errors=True)
        policy['measured_rss_packing']['authorization']=identity(packing)
        policy['standing_exclusive_memory_rule']=dict(rule_id='user_20261029_memtotal_minus_2gib_available_1gib',world_codec_source=str(source),authorization=identity(standing))
        policy['execution_tool_lock']=dict(scheduler=identity(scheduler.__file__),runner=identity(entry.__file__))
        original=[identity(source)];before=deepcopy(original)
        headers=scheduler.runtime_tool_inputs(policy,original)
        scheduler.validate_packing_rule(policy,headers);scheduler.validate_standing_rule(policy,headers)
        self.assertEqual(original,before);self.assertEqual(len(headers),5)
        packing.write_text('{}')
        with self.assertRaisesRegex(RuntimeError,'drift'):
            scheduler.runtime_tool_inputs(policy,original)


if __name__ == '__main__':
    unittest.main()
