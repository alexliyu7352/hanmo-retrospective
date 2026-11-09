"""纯临时字节验证缓存冻结；不调用ccache/GCC，不上传Space。"""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE=Path(__file__).resolve().parents[1]/'native/cache_snapshot.py'


class CacheSnapshotTests(unittest.TestCase):
    """只允许专用cache已稳定的普通entry，不把tmp/符号链接/凭证归档。"""
    def setUp(self):
        """每例小型临时cache布局，不是实际cache命中证据。"""
        self.assertTrue(MODULE.exists(),'cache snapshot not implemented')
        spec=importlib.util.spec_from_file_location('cache_snapshot',MODULE)
        self.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.m)
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.cache=self.root/'cache';(self.cache/'a/b').mkdir(parents=True);(self.cache/'tmp').mkdir()
        (self.cache/'a/b/entryR').write_bytes(b'entry bytes');(self.cache/'a/stats').write_bytes(b'mutable stats')
        (self.cache/'tmp/active').write_bytes(b'unfinished');(self.cache/'credentials.json').write_text('not to archive')

    def test_real_streaming_gzip_archive_accepts_pathlib_output(self):
        """实际Python tar流走完整freeze，不以仅分组单测冒已可写包。"""
        def fixed(name,value):
            path=self.root/name;path.write_text(json.dumps(value));return self.m.D.ident(path)
        config=self.root/'ccache.conf';config.write_text('hard_link = false\nfile_clone = false\nremote_storage = # explicit empty value\n')
        profile=fixed('profile.json',{'cache_dir':str(self.cache),'ccache':{'synthetic':True},'configuration':self.m.D.ident(config)})
        scheduler=self.root/'synthetic_memory.py';scheduler.write_text('def memory_snapshot(): return {"visible_available_bytes": 8589934592}\n')
        queue=self.root/'queue.lock';queue.touch()
        request={'kind':'QUIESCENT_N1_CCACHE_SNAPSHOT_REQUEST','cache_dir':str(self.cache),'output_directory':str(self.root/'snapshot'),'queue_lock':str(queue),'scheduler_terminal':fixed('terminal.json',{'status':'complete'}),'active_calls':fixed('active.json',[]),'profile':profile,'ccache_identity':{'synthetic':True},'scheduler':self.m.D.ident(scheduler),'previous_indexes':[],'chunk_bytes':64*1024**2}
        with queue.open('a') as lock:
            try: result=self.m.freeze(request,lock)
            except AttributeError as error:self.fail('real streaming tar writer rejected Path: '+str(error))
        self.assertEqual(result['new_entry_versions'],1);self.assertEqual(len(result['archives']),1)
        import tarfile
        with tarfile.open(result['archives'][0]['path'],'r:gz') as archive:
            self.assertEqual(archive.extractfile('cache/a/b/entryR').read(),b'entry bytes')

    def test_blocked_empty_writer_snapshot_and_active_rejection(self):
        """原N1 blocked是终态；必须同时active空，非空不能因blocked被绕过。"""
        def fixed(name,value):
            path=self.root/name;path.write_text(json.dumps(value));return self.m.D.ident(path)
        config=self.root/'ccache.conf';config.write_text('hard_link = false\nfile_clone = false\nremote_storage = # explicit empty value\n')
        profile=fixed('profile.json',{'cache_dir':str(self.cache),'ccache':{'synthetic':True},'configuration':self.m.D.ident(config)})
        scheduler=self.root/'synthetic_memory.py';scheduler.write_text('def memory_snapshot(): return {"visible_available_bytes": 8589934592}\n')
        queue=self.root/'queue.lock';queue.touch()
        request={'kind':'QUIESCENT_N1_CCACHE_SNAPSHOT_REQUEST','cache_dir':str(self.cache),'output_directory':str(self.root/'snapshot'),'queue_lock':str(queue),'scheduler_terminal':fixed('terminal.json',{'status':'blocked','pending':['real-not-run-source']}),'active_calls':fixed('active.json',[{'pid':123}]),'profile':profile,'ccache_identity':{'synthetic':True},'scheduler':self.m.D.ident(scheduler),'previous_indexes':[],'chunk_bytes':64*1024**2}
        with queue.open('a') as lock:
            with self.assertRaisesRegex(RuntimeError,'cache_writers_not_quiescent'):self.m.freeze(request,lock)
            self.assertFalse(Path(request['output_directory']).exists())
            request['active_calls']=fixed('empty-active.json',[])
            result=self.m.freeze(request,lock)
        self.assertEqual(result['new_entry_versions'],1)
        import tarfile
        with tarfile.open(result['archives'][0]['path'],'r:gz') as archive:
            self.assertEqual(archive.extractfile('cache/a/b/entryR').read(),b'entry bytes')

    def test_only_entry_subtrees(self):
        """不依赖未核文件后缀推断，保hex子树真实普通bytes且排stats/tmp。"""
        rows=self.m.scan_entries(self.cache)
        self.assertEqual([r['relative_path'] for r in rows],['a/b/entryR'])

    def test_symlink_rejected(self):
        """专用cache内部链接也不可读取外部路径。"""
        (self.cache/'a/b/escape').symlink_to(self.root/'other')
        with self.assertRaises(RuntimeError):self.m.scan_entries(self.cache)

    def test_content_version_dedup_not_path_only(self):
        """同key新bytes必须保存，旧remote同path同SHA才排重。"""
        rows=self.m.scan_entries(self.cache)
        self.assertEqual(self.m.unpreserved(rows,rows),[])
        (self.cache/'a/b/entryR').write_bytes(b'new version')
        self.assertEqual(len(self.m.unpreserved(self.m.scan_entries(self.cache),rows)),1)

    def test_chunks_bounded_and_large_entry_single(self):
        """不切坏单一entry；大entry独包，其他按字节界。"""
        rows=[{'bytes':4},{'bytes':4},{'bytes':20},{'bytes':3}]
        self.assertEqual([len(x) for x in self.m.chunk_entries(rows,8)],[2,1,1])


if __name__=='__main__':unittest.main()
