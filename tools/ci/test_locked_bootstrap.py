"""仅核缓存字节拒绝门；不联网、不冒目标构建成功。"""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest


class LockedBytesTest(unittest.TestCase):
    def test_changed_cache_is_rejected(self):
        spec = importlib.util.spec_from_file_location('bootstrap', Path(__file__).with_name('bootstrap_locked_native.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'locked\n')
            expected = hashlib.sha256(path.read_bytes()).hexdigest()
            module.verify(path, expected)
            path.write_bytes(b'changed\n')
            with self.assertRaises(RuntimeError):
                module.verify(path, expected)
            self.assertEqual(path.read_bytes(), b'changed\n')


if __name__ == '__main__':
    unittest.main()
