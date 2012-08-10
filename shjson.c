#include "Python.h"
#include <yajl/yajl_parse.h>

#include "deque.c"

/* forward declaration */
static PyObject *event_iterator_new(PyObject *, yajl_handle, deque_t*);
static PyTypeObject EventIteratorType;

static PyObject *read_args;

static PyObject *null_event;
static PyObject *start_map_event;
static PyObject *end_map_event;
static PyObject *start_array_event;
static PyObject *end_array_event;
static PyObject *bool_true_event;
static PyObject *bool_false_event;

static PyObject *string_event;
static PyObject *map_key_event;


#define PUSH_BACK(q, i) deque_push_back((deque_t*)q, i)

static int
convert_null(void *queued_events)
{
    Py_INCREF(null_event);
    return PUSH_BACK(queued_events, null_event);
}

static int
convert_boolean(void *queued_events, int boolean)
{
    if (boolean == 1) {
        Py_INCREF(bool_true_event);
        return PUSH_BACK(queued_events, bool_true_event);
    }
    else if (boolean == 0) {
        Py_INCREF(bool_false_event);
        return PUSH_BACK(queued_events, bool_false_event);
    }
    return 0;
}

static int
convert_integer(void *queued_events, long val)
{
    PyObject *item;
    if ((item = Py_BuildValue("(sl)", "integer", val)) == NULL)
        return 0;
    return PUSH_BACK(queued_events, item);
}

static int
convert_double(void *queued_events, double val)
{
    PyObject *item;
    if ((item = Py_BuildValue("(sd)", "double", val)) == NULL)
        return 0;
    return PUSH_BACK(queued_events, item);
}

static int
convert_number(void *queued_events, const char *val, unsigned int len)
{
    PyObject *number_str;
    PyObject *number;

    if ((number_str = PyString_FromStringAndSize(val, len)) == NULL)
        return 0;

    number = PyLong_FromString(PyString_AS_STRING(number_str), NULL, 10);
    if (number == NULL) {
        if (PyErr_ExceptionMatches(PyExc_ValueError)) {
            PyErr_Clear();
            number = PyFloat_FromString(number_str, NULL);
        }
    }
    Py_DECREF(number_str);
    if (number == NULL)
        return 0;

    PyObject *item = Py_BuildValue("(sO)", "number", number);
    Py_DECREF(number);
    if (item == NULL)
        return 0;

    return PUSH_BACK(queued_events, item);
}

static int
convert_string(void *queued_events, const unsigned char *val, unsigned int len)
{
    PyObject *string_obj = PyString_FromStringAndSize((const char*)val, len);
    if (string_obj == NULL)
        return 0;

    PyObject *item = PyTuple_Pack(2, string_event, string_obj);
    Py_DECREF(string_obj);

    if (item == NULL)
        return 0;

    return PUSH_BACK(queued_events, item);
}

static int
convert_start_map(void *queued_events)
{
    Py_INCREF(start_map_event);
    return PUSH_BACK(queued_events, start_map_event);
}

static int
convert_map_key(void *queued_events, const unsigned char *val, unsigned int len)
{
    PyObject *map_key= PyString_FromStringAndSize((const char*)val, len);
    if (map_key== NULL)
        return 0;

    PyObject *item = PyTuple_Pack(2, map_key_event, map_key);
    Py_DECREF(map_key);

    if (item == NULL)
        return 0;

    return PUSH_BACK(queued_events, item);
}

static int
convert_end_map(void *queued_events)
{

    Py_INCREF(end_map_event);
    return PUSH_BACK(queued_events, end_map_event);
}

static int
convert_start_array(void *queued_events)
{
    Py_INCREF(start_array_event);
    return PUSH_BACK(queued_events, start_array_event);
}

static int
convert_end_array(void *queued_events)
{
    Py_INCREF(end_array_event);
    return PUSH_BACK(queued_events, end_array_event);
}

static yajl_callbacks callbacks = {
    convert_null,
    convert_boolean,
    convert_integer,
    convert_double,
    convert_number,
    convert_string,
    convert_start_map,
    convert_map_key,
    convert_end_map,
    convert_start_array,
    convert_end_array,
};

static PyObject *
basic_parse(PyObject *module, PyObject *args)
{

    PyObject *file_handler;
    if (!PyArg_ParseTuple(args, "O", &file_handler))
        return NULL;

    PyObject *read_method = PyObject_GetAttrString(file_handler, "read");
    if (read_method == NULL)
        return NULL;

    deque_t *queued_events = deque_create();

    yajl_parser_config cfg = {0, 0};
    yajl_handle parser = yajl_alloc(&callbacks, &cfg, NULL, (void*) queued_events);

    return event_iterator_new(read_method, parser, queued_events);
}

static PyMethodDef ijson_methods[] = {
    {"basic_parse", basic_parse, METH_VARARGS, ""},
    /* Sentinel */
    {NULL, NULL, 0, NULL}
};

typedef struct {
    PyObject_HEAD;
    yajl_handle parser;
    PyObject *read_method;
    deque_t *queued_events;
    unsigned short eof;
} event_iterator;

static PyObject*
event_iterator_new(
        PyObject *read_method,
        yajl_handle parser,
        deque_t *queued_events)
{
    event_iterator *self;
    self = (event_iterator*) (&EventIteratorType)->tp_alloc(&EventIteratorType, 0);
    self->read_method = read_method;
    self->parser = parser;
    self->queued_events = queued_events;
    self->eof = 0;
    return (PyObject*) self;
}

static void
event_iterator_dealloc(event_iterator *self)
{
    yajl_free(self->parser);
    Py_DECREF(self->read_method);
    deque_free(self->queued_events);
    self->ob_type->tp_free((PyObject*) self);
}

static int
read_and_feed(event_iterator *self) {
    PyObject *data;
    yajl_status status;
    const unsigned char *err;

    /* Read until we have something in the queue */
    while (deque_is_emtpy(self->queued_events) && (self->eof == 0)) {
        data = PyObject_Call(self->read_method, read_args, NULL);

        if (data == NULL)
            return -1;

        if (PyString_CheckExact(data) == 0) {
            PyErr_Format(PyExc_ValueError, "read does not return plain string");
            Py_DECREF(data);
            return -1;
        }

        if (PyString_GET_SIZE(data) == 0) {
            self->eof = 1;
            status = yajl_parse_complete(self->parser);
            if (status == yajl_status_insufficient_data) {
                PyErr_Format(PyExc_Exception, "Parser expects more data");
                Py_DECREF(data);
                return -1;
            }
        }
        else {
            status = yajl_parse(
                        self->parser,
                        (const unsigned char *) PyString_AS_STRING(data),
                        PyString_GET_SIZE(data));
        }

        if (status == yajl_status_error) {
            err = yajl_get_error(
                        self->parser,
                        1,
                        (const unsigned char*) PyString_AS_STRING(data),
                        PyString_GET_SIZE(data));

            PyErr_SetString(PyExc_Exception, (char*) err);
            yajl_free_error(self->parser, (unsigned char*) err);
            Py_DECREF(data);
            return -1;
        }
        Py_DECREF(data);
    }
    return 0;
}

static PyObject*
event_iterator_iternext(event_iterator *self)
{

    if (!deque_is_emtpy(self->queued_events))
        return deque_pop_front(self->queued_events);

    if (self->eof == 1)
        return NULL;

    if (read_and_feed(self) == -1)
        return NULL;

    if (!deque_is_emtpy(self->queued_events))
        return deque_pop_front(self->queued_events);

    return NULL;
}

static PyTypeObject EventIteratorType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "EventIterator",             /*tp_name*/
    sizeof(event_iterator),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)event_iterator_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Iterator of JSON events",  /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    PyObject_SelfIter,          /* tp_iter */
    (iternextfunc)event_iterator_iternext,  /* tp_iternext */
    0,              /* tp_methods */
    0,              /* tp_members */
    0,              /* tp_getset */
    0,              /* tp_base */
    0,              /* tp_dict */
    0,              /* tp_descr_get */
    0,              /* tp_descr_set */
    0,              /* tp_dictoffset */
    0,              /* tp_init */
    0,              /* tp_alloc */
    0,              /* tp_new */
};

PyMODINIT_FUNC
initshjson(void)
{
    /* Init some statics so we dont have to build them all the time */
    read_args = Py_BuildValue("(i)", 32 * 1024);
    if (read_args == NULL)
        return;

    null_event = Py_BuildValue("(sO)", "null", Py_None);
    if (null_event == NULL)
        return;

    start_map_event = Py_BuildValue("(sO)", "start_map", Py_None);
    if (start_map_event == NULL)
        return;

    end_map_event = Py_BuildValue("(sO)", "end_map", Py_None);
    if (end_map_event == NULL)
        return;

    start_array_event = Py_BuildValue("(sO)", "start_array", Py_None);
    if (start_array_event == NULL)
        return;

    end_array_event = Py_BuildValue("(sO)", "end_array", Py_None);
    if (end_array_event == NULL)
        return;

    bool_true_event = Py_BuildValue("(sO)", "boolean", Py_True);
    if (bool_true_event == NULL)
        return;

    bool_false_event = Py_BuildValue("(sO)", "boolean", Py_False);
    if (bool_false_event == NULL)
        return;

    string_event = PyString_FromString("string");
    if (string_event == NULL)
        return;

    map_key_event = PyString_FromString("map_key");
    if (map_key_event == NULL)
        return;

    PyObject *m;
    m = Py_InitModule3("shjson", ijson_methods, "shjson.so");
    if (m==NULL)
        return;

    if (PyType_Ready(&EventIteratorType) < 0)
        return;
}
