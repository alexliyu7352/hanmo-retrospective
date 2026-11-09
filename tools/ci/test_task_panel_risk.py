"""三面板限定控件门的真实来源、缓存和原件边界；不运行替代引擎。"""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('task_panel_risk', HERE/'task_panel_risk.py')
risk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(risk)


class TaskPanelRisk(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def test_exact_successful_cache_request_only(self):
        result = risk.request(risk.CACHE_KEY)
        self.assertEqual(result['native_run_id'], 38365397879)
        self.assertEqual(result['native_source'], '0406e06686dcce90aefb1c43ebb2e6566949ab2f')
        for key in ('', risk.CACHE_KEY+'\n', risk.CACHE_KEY.replace('attempt-1', 'attempt-2'), risk.CACHE_KEY.replace('0406e', '0407e')):
            with self.subTest(key=key), self.assertRaises(ValueError):
                risk.request(key)

    def test_real_run_metadata_is_required(self):
        run = {'id':risk.RUN, 'run_attempt':1, 'head_sha':risk.NATIVE_SOURCE,
               'status':'completed', 'conclusion':'success',
               'repository':{'full_name':risk.REPOSITORY}, 'path':'.github/workflows/native-ubuntu2004.yml'}
        risk.validate_run(run)
        for key, value in [('id', 1), ('run_attempt', 2), ('head_sha','a'*40), ('status','in_progress'), ('conclusion','failure'), ('repository',{'full_name':'elsewhere/other'}), ('path','other.yml')]:
            changed = dict(run); changed[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):risk.validate_run(changed)

    def repository(self):
        root = self.root/'source'; root.mkdir()
        subprocess.run(['git','init','-q',str(root)],check=True)
        for path in risk.CLOSURE:
            target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(path+'\n')
        subprocess.run(['git','-C',str(root),'add','.'],check=True)
        subprocess.run(['git','-C',str(root),'-c','user.name=fixture','-c','user.email=fixture@local.invalid','commit','-qm','source fixture'],check=True)
        head=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
        return root,head

    def test_fixed_closure_matches_actual_git_blobs(self):
        root,head=self.repository()
        result=risk.source_identity(root,head)
        self.assertEqual(result['source_commit'],head)
        self.assertEqual(set(result['files']),set(risk.CLOSURE))
        with self.assertRaises(ValueError):risk.source_identity(root,'0'*40)
        path=root/risk.GD_FILES[0];path.write_text('changed')
        with self.assertRaises(ValueError):risk.source_identity(root,head)

    def test_missing_and_linked_source_rejected(self):
        root,head=self.repository();path=root/risk.GD_FILES[0];data=path.read_bytes();path.unlink()
        with self.assertRaises(ValueError):risk.source_identity(root,head)
        other=self.root/'other';other.write_bytes(data);path.symlink_to(other)
        with self.assertRaises(ValueError):risk.source_identity(root,head)

    def test_git_subprocess_cannot_fetch(self):
        with patch.object(risk.subprocess,'check_output',return_value=b'abc\n') as call:
            self.assertEqual(risk.git(self.root,'rev-parse','HEAD'),b'abc\n')
        self.assertEqual(call.call_args.kwargs['env']['GIT_ALLOW_PROTOCOL'],'')
        self.assertEqual(call.call_args.kwargs['env']['GIT_TERMINAL_PROMPT'],'0')

    def test_minimal_project_only_contains_four_gd(self):
        root,head=self.repository();identity=risk.source_identity(root,head)
        target=self.root/'project';risk.prepare_project(root,target,identity)
        files={str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
        self.assertEqual(files,{'project.godot'}|{p[len('game/'):] if p.startswith('game/') else p for p in risk.GD_FILES})
        self.assertNotIn('autoload',(target/'project.godot').read_text())
        with self.assertRaises(FileExistsError):risk.prepare_project(root,target,identity)

    def test_cache_engine_exact_hash_and_size(self):
        cache=self.root/'cache';cache.mkdir();engine=cache/risk.ENGINE_FILE;engine.write_bytes(b'fixture')
        lock={'editor_build':risk.ENGINE_VERSION,'archives':{'editor':{'entries':{risk.ENGINE_FILE:{'sha256':risk.ENGINE_SHA,'bytes':risk.ENGINE_BYTES}}}}}
        with patch.object(risk,'sha256',return_value=risk.ENGINE_SHA), patch.object(risk,'regular_size',return_value=risk.ENGINE_BYTES):
            self.assertEqual(risk.cached_engine(cache,lock),engine)
            lock['editor_build']='4.6.3'
            with self.assertRaises(ValueError):risk.cached_engine(cache,lock)
        lock['editor_build']=risk.ENGINE_VERSION
        with self.assertRaises(ValueError):risk.cached_engine(cache,lock)

    def test_success_requires_unique_marker_no_runtime_errors(self):
        risk.validate_output(0,'Godot Engine\n'+risk.PASS_MARKER+'\n')
        for code, log in [(1,risk.PASS_MARKER), (0,''), (0,risk.PASS_MARKER+'\n'+risk.PASS_MARKER), (0,'SCRIPT ERROR: bad\n'+risk.PASS_MARKER), (0,'ERROR: bad\n'+risk.PASS_MARKER)]:
            with self.subTest(code=code,log=log), self.assertRaises(ValueError):risk.validate_output(code,log)


if __name__=='__main__':unittest.main()
