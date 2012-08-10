shjson
======

Stream based JSON parsing with a Python C-Extension around YAJL.
This module provides a single function called 'basic_parse'.
This function returns a iterator of JSON events like http://pypi.python.org/pypi/ijson/ does.

Example:
```python
import cStringIO
import shjson
for event in shjson.basic_parse(cStringIO.StringIO('[1, "2", null, {"key": "value"}]')):
    print event

('start_array', None)
('number', 1L)
('string', '2')
('null', None)
('start_map', None)
('map_key', 'key')
('string', 'value')
('end_map', None)
('end_array', None)
```