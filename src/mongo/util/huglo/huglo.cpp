#include "mongo/util/huglo/huglo.h"

#include <iostream>

namespace mongo {
namespace huglo {

constexpr uint64_t MYTRACE_BUFFER_SIZE = 16;

typedef enum { MYTRACE_EVENT_ENTER = 0, MYTRACE_EVENT_EXIT = 1 } mytrace_event_type;

typedef struct {
    long long timestamp_ns;
    uint64_t method_id;
    mytrace_event_type type;
    pid_t pid; /* Process ID */
    pid_t tid; /* Thread ID */
} mytrace_event_t;

/* The circular buffer and related state. */
mytrace_event_t buffer[MYTRACE_BUFFER_SIZE];
int buffer_pos = 0;
int buffer_wrapped = 0;

/* Global process ID captured at initialization */
static pid_t global_pid = 0;

/* Thread-local storage for thread IDs */
static __thread pid_t cached_tid = 0;

static char* escape_json_string(const char* input) {
    // Estimate worst-case size (every char needs escaping + quotes + null)
    size_t max_len = strlen(input) * 2 + 3;
    char* output = (char*)malloc(max_len);
    if (!output)
        return NULL;

    char* ptr = output;
    *ptr++ = '"';  // Start quote

    while (*input) {
        switch (*input) {
            case '"':
                *ptr++ = '\\';
                *ptr++ = '"';
                break;
            case '\\':
                *ptr++ = '\\';
                *ptr++ = '\\';
                break;
            // Add more escapes as needed (e.g., \n, \t, control chars)
            case '\n':
                *ptr++ = '\\';
                *ptr++ = 'n';
                break;
            default:
                *ptr++ = *input;
                break;
        }
        input++;
    }

    *ptr++ = '"';  // End quote
    *ptr = '\0';   // Null terminate
    return output;
}

/* Adds an entry to the circular buffer. */
void trace_add_entry(uint64_t fn, mytrace_event_type type) {
    mytrace_event_t* event = &buffer[buffer_pos];
    // struct timespec ts; // Use timespec for clock_gettime

    // TODO: Ain't gonna work on Mac
    // uint64_t t = __rdtscp(&aux); /* already emits RDTSCP */
    uint64_t t = 1;

    /* Lazy initialization of thread ID on first call per thread */
    if (cached_tid == 0) {
        cached_tid = 1;
    }

    /* Store the event data. */
    // event->timestamp_ns = (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
    event->timestamp_ns = t;
    event->method_id = fn;
    event->type = type;
    event->pid = global_pid;
    event->tid = cached_tid;

    /* Update the buffer position. */
    buffer_pos = (buffer_pos + 1) % MYTRACE_BUFFER_SIZE;
    if (buffer_pos == 0) {
        buffer_wrapped = 1;
    }
}

/* Records a function entry event. */
void* trace_func_entry(uint64_t fn) {
    trace_add_entry(fn, MYTRACE_EVENT_ENTER);
}

/* Records a function exit event. */
void* trace_func_exit(u_int64_t fn) {
    trace_add_entry(fn, MYTRACE_EVENT_EXIT);
}

/* Dumps the buffer to a file in Perfetto format. */
void trace_dump_buffer(const char* filename) {
    FILE* f = fopen(filename, "w");
    int i, count;


    if (!f) {
        return;
    }

    /* Write Perfetto JSON format header */
    fprintf(f, "{\n");
    fprintf(f, "  \"traceEvents\": [\n");

    count = buffer_wrapped ? MYTRACE_BUFFER_SIZE : buffer_pos;
    for (i = 0; i < count; i++) {
        int idx = buffer_wrapped ? (buffer_pos + i) % MYTRACE_BUFFER_SIZE : i;
        mytrace_event_t* event = &buffer[idx];
        // const char* method_name = rb_id2name(event->method_id);

        const char* phase =
            event->type == MYTRACE_EVENT_ENTER ? "B" : "E"; /* B for begin, E for end */

        /* Write trace event in Perfetto format with actual PID and TID */
        fprintf(f,
                "    {\"name\": \"%s\", \"cat\": \"ruby\", \"ph\": \"%s\", \"ts\": %lld, "
                "\"pid\": %d, \"tid\": %d}",
                "method_name",
                phase,
                event->timestamp_ns / 1000, /* Convert ns to μs for Perfetto */
                event->pid,
                event->tid);

        /* Add comma after every event except the last one */
        if (i < count - 1) {
            fprintf(f, ",");
        }
        fprintf(f, "\n");
    }

    /* Close the JSON structure */
    fprintf(f, "  ],\n");

    fprintf(f, "  \"otherData\": {\n");
    fprintf(f, "    \"version\": \"Ruby Tracer 1.0\"\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
}

void sayHello() {
    std::cout << "Huglo says hello" << std::endl;
    trace_func_entry(1);
    trace_func_exit(1);
    trace_dump_buffer("/tmp/trace");
}

}  // namespace huglo
}  // namespace mongo
