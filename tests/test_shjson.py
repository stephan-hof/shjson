from shjson import basic_parse
import json
import unittest
import time
from cStringIO import StringIO

class TestSHJson(unittest.TestCase):
    def test_simple_example(self):
        simple = [
            None,
            True,
            False,
            1,
            1.1,
            111111111111111111111.111111111111,
            u'\xd6sterreich']

        ret = list(basic_parse(StringIO(json.dumps(simple))))
        ref = [
            ('start_array', None),
            ('null', None),
            ('boolean', True),
            ('boolean', False),
            ('number', 1L),
            ('number', 1.1),
            ('number', 1.1111111111111111e+20),
            ('string', '\xc3\x96sterreich'),
            ('end_array', None)
        ]
        self.assertEqual(ref, ret)

    def test_perf(self):
        cust = {}
        for i in xrange(160000):
            cust["id%i" % i] = {"xxxxx": '1234', 'bbbb': '452', "d": 'y'}
        stream = StringIO(json.dumps(cust))

        # should take around 0.17 seconds
        start = time.time()
        for x in basic_parse(stream): pass
        print time.time() - start
