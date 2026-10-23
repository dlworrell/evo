#!/usr/bin/env python3
"""Synthetic-only differential tests against hashlib.sha256."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).with_name('stream_hash.c')

class TestStreamingSHA256(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.work.name) / 'stream_hash'
        flags = subprocess.check_output(['pkg-config','--cflags','--libs','openssl'],text=True).split()
        compiler = os.environ.get('CC', 'cc')
        extra = os.environ.get('ARCHIVE_AUDIT_CFLAGS', '').split()
        subprocess.run([compiler,'-std=c17','-O2','-Wall','-Wextra',
                        '-Wpedantic','-Werror',*extra,str(SOURCE),
                        '-o',str(cls.exe),*flags],check=True)

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def check_data(self, content: bytes, chunk_bytes=4096):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp)/'example.mbox'
            file.write_bytes(content)
            for mode in ('file','pipe'):
                opts = [str(self.exe), str(file) if mode=='file' else '-', str(chunk_bytes)]
                args = {'input':content} if mode=='pipe' else {}
                p = subprocess.run(opts, capture_output=True,**args)
                self.assertEqual(p.returncode,0,p.stderr.decode())
                rows = [json.loads(x) for x in p.stdout.decode().splitlines()]
                self.assertEqual(rows[0]['schema'],'evo.archive-audit.stream-sha256.v1')
                blocks = [content[i:i+chunk_bytes] for i in range(0,len(content),chunk_bytes)]
                self.assertEqual(len(rows),2+len(blocks))
                for i,block in enumerate(blocks):
                    row = rows[i+1]
                    self.assertEqual(row['kind'],'chunk')
                    self.assertEqual(row['offset'],i*chunk_bytes)
                    self.assertEqual(row['length'],len(block))
                    self.assertEqual(row['sha256'],hashlib.sha256(block).hexdigest())
                final=rows[-1]
                self.assertTrue(final['complete'])
                self.assertEqual(final['kind'],'file')
                self.assertEqual(final['length'],len(content))
                self.assertEqual(final['chunks'],len(blocks))
                self.assertEqual(final['sha256'],hashlib.sha256(content).hexdigest())

    def test_empty(self): self.check_data(b'')
    def test_one_byte(self): self.check_data(b'X')
    def test_boundary(self):
        for n in [4095,4096,4097,8192,8193]:
            with self.subTest(n=n): self.check_data(os.urandom(n))
    def test_multi_megabyte(self): self.check_data(os.urandom(3_400_000))
    def test_invalid_chunks(self):
        for n in ['0','4095','67108865','2k','-100']:
            p=subprocess.run([str(self.exe),'-',n],input=b'',capture_output=True)
            self.assertNotEqual(p.returncode,0)

if __name__=='__main__': unittest.main()
