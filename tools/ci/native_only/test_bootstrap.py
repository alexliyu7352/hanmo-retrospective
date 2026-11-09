"""本地小归档验证下载/解包拒绝合同；不联网、不安装、不执行编译器。"""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('native_bootstrap', Path(__file__).with_name('bootstrap.py'))
bootstrap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bootstrap)


class BootstrapTests(unittest.TestCase):
    """缓存只能提供已核原件，不能让未核的已解包CTest进入后续门。"""

    def setUp(self):
        """将Git配置和所有小夹具限制在本测试临时目录。"""
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source, self.cache = self.root / 'source', self.root / 'cache'
        (self.source / 'native').mkdir(parents=True)
        self.cache.mkdir()
        self.env = patch.dict(os.environ, {'GIT_CONFIG_GLOBAL': str(self.root / 'gitconfig')})
        self.env.start()
        self.addCleanup(self.env.stop)
        libraries = {}
        for name in ('flecs', 'json', 'picosha2'):
            directory = self.cache / 'deps' / name
            directory.mkdir(parents=True)
            (directory / 'header.h').write_bytes(name.encode())
            self.git(directory, 'init', '-q')
            self.git(directory, 'config', 'user.name', 'CI contract fixture')
            self.git(directory, 'config', 'user.email', 'ci-test@example.invalid')
            self.git(directory, 'add', '.')
            self.git(directory, 'commit', '-qm', 'fixture')
            libraries[name] = {'commit': self.git(directory, 'rev-parse', 'HEAD'),
                'repository': 'unused.invalid', 'files': {'header.h': hashlib.sha256(name.encode()).hexdigest()}}
        self.prefix = 'cmake-3.31.12-linux-x86_64'
        self.lock = {'libraries': libraries, 'toolchain': {'cmake': {
            'version': '3.31.12', 'archive_url': 'unused.invalid',
            'binary_sha256': hashlib.sha256(b'cmake contract fixture').hexdigest()}}}
        self.archive()

    def git(self, directory, *args):
        """只操作独占测试Git仓库。"""
        return subprocess.check_output(['git', '-C', str(directory)] + list(args), text=True).strip()

    def archive(self, unsafe=False):
        """构造小型解包夹具，其内容不被当作可运行CMake。"""
        path = self.cache / 'cmake.tar.gz'
        with tarfile.open(path, 'w:gz') as stream:
            members = {self.prefix + '/bin/cmake': b'cmake contract fixture',
                       self.prefix + '/bin/ctest': b'ctest contract fixture',
                       self.prefix + '/share/cmake/Modules/Test.cmake': b'module contract fixture'}
            if unsafe:
                members['../outside'] = b'not permitted'
            for name, data in members.items():
                info = tarfile.TarInfo(name)
                info.size = len(data)
                stream.addfile(info, io.BytesIO(data))
        self.lock['toolchain']['cmake']['archive_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        (self.source / 'native/dependencies.lock.json').write_text(json.dumps(self.lock))

    def test_fresh_verified_archive_materializes_complete_tool(self):
        """CTest和模块必须与CMake一起来自同一已核归档。"""
        bootstrap.bootstrap(self.source, self.cache)
        self.assertEqual((self.cache / self.prefix / 'bin/ctest').read_bytes(), b'ctest contract fixture')
        self.assertTrue((self.cache / 'locked-native-only.cmake').is_file())

    def test_existing_extracted_tree_is_rejected(self):
        """即便cmake本身未改，已有或被污染的ctest也不能被复用。"""
        bootstrap.bootstrap(self.source, self.cache)
        (self.cache / self.prefix / 'bin/ctest').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'extraction must be fresh'):
            bootstrap.bootstrap(self.source, self.cache)

    def test_corrupt_archive_rejected_before_extraction(self):
        """半份下载或错误缓存不能写为可用工具。"""
        (self.cache / 'cmake.tar.gz').write_bytes(b'partial download')
        with self.assertRaises(ValueError):
            bootstrap.bootstrap(self.source, self.cache)
        self.assertFalse((self.cache / self.prefix).exists())

    def test_traversal_member_rejected(self):
        """归档成员不能越出专属缓存目录。"""
        self.archive(unsafe=True)
        with self.assertRaisesRegex(ValueError, 'Unsafe CMake archive'):
            bootstrap.bootstrap(self.source, self.cache)
        self.assertFalse((self.root / 'outside').exists())

    def test_dirty_dependency_rejected(self):
        """已有Git目录不是验证完成的证据。"""
        (self.cache / 'deps/flecs/header.h').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            bootstrap.bootstrap(self.source, self.cache)


if __name__ == '__main__':
    unittest.main()
