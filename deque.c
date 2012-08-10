#include "Python.h"
#include <stdlib.h>

/* I know this is a waste of memory, but implementing a memory efficient double
 * ended queue algorithm cannot be done in 50 lines of code.
 */
#define MAX_SIZE (32*1024)

typedef struct {
    PyObject** array;
    unsigned int append_pos;
    unsigned int pop_pos;
    unsigned int current_size;

} deque_t;

static deque_t*
deque_create(void) {
    deque_t *queue = malloc(sizeof(deque_t));

    queue->array = malloc(sizeof(PyObject*) * MAX_SIZE);

    queue->append_pos = 0;
    queue->pop_pos = 0;
    queue->current_size = 0;

    return queue;
}

static void
deque_free(deque_t *queue) {
    free(queue->array);
    free(queue);
}

static int
deque_push_back(deque_t *self, PyObject *item) {
    if (self->current_size == MAX_SIZE) {
        PyErr_SetString(PyExc_Exception, "Queue is full");
        return 0;
    }

    if (self->append_pos == MAX_SIZE) {
        self->append_pos = 0;
    }
    self->current_size++;
    self->array[self->append_pos++] = item;
    return 1;
}

static PyObject *
deque_pop_front(deque_t *self) {
    if (self->pop_pos == MAX_SIZE) {
        self->pop_pos = 0;
    }
    self->current_size--;
    return self->array[self->pop_pos++];
}

inline static int
deque_is_emtpy(deque_t *self) {
    return self->current_size == 0;
}

inline static unsigned int
deque_size(deque_t *self) {
    return self->current_size;
}
