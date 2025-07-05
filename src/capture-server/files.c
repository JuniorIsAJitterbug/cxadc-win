/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 * Copyright (C) 2024 namazso <admin@namazso.eu>
 */

#include "files.h"

#include "common.h"
#include "network.h"

#include <ctype.h>
#include <time.h>

#include "audio.h"
#include "ringbuffer.h"
#include "version.h"

#define BUFFER_READ_SIZE    (65536 * 32)

servefile_fn file_root;
servefile_fn file_version;
servefile_fn file_cxadc;
servefile_fn file_linear;
servefile_fn file_start;
servefile_fn file_stop;
servefile_fn file_stats;

struct served_file SERVED_FILES[] = {
  {"/", "Content-Type: text/html; charset=utf-8\r\n", file_root},
  {"/version", "Content-Type: text/plain; charset=utf-8\r\n", file_version},
  {"/cxadc", "Content-Disposition: attachment\r\n", file_cxadc},
  {"/linear", "Content-Disposition: attachment\r\n", file_linear},
  {"/start", "Content-Type: text/json; charset=utf-8\r\n", file_start},
  {"/stop", "Content-Type: text/json; charset=utf-8\r\n", file_stop},
  {"/stats", "Content-Type: text/json; charset=utf-8\r\n", file_stats},
  {NULL}
};

enum capture_state {
    State_Idle = 0,
    State_Starting,
    State_Running,
    State_Stopping,

    State_Failed
};

const char* capture_state_to_str(enum capture_state state) {
    const char* NAMES[] = { "Idle", "Starting", "Running", "Stopping", "Failed" };
    return NAMES[(int)state];
}

struct rb_threads {
    _Atomic(thrd_t) reader; // This is special and not protected by cap_state
    _Atomic(bool) is_reader_alive;

    thrd_t writer;
    _Atomic(bool) is_writer_alive;
};

struct cxadc_state {
    int fd;
    char name[16];
    thrd_t writer_thread;
    ringbuffer_t ring_buffer;
    struct rb_threads rb_threads;

    // This is special and not protected by cap_state
    _Atomic(thrd_t) reader_thread;
};

struct linear_state {
    struct audio_device_info dev_info;

    thrd_t writer_thread;
    ringbuffer_t ring_buffer;

    // This is special and not protected by cap_state
    _Atomic(thrd_t) reader_thread;

    // Some initialization happens in writer thread on win32
    // Used to signal state
    enum capture_state writer_thread_state;
};

struct {
    _Atomic(enum capture_state)cap_state;
    struct cxadc_state cxadc[256];
    size_t cxadc_count;
    _Atomic(size_t) overflow_counter;

    bool linear_enabled;
    struct linear_state linear;
} g_state;

int cxadc_writer_thread(void* id);
int linear_writer_thread(void*);

static ssize_t timespec_to_nanos(const struct timespec* ts) {
    return (ssize_t)ts->tv_nsec + (ssize_t)ts->tv_sec * 1000000000;
}

static void urldecode2(char* dst, const char* src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = (char)(16 * a + b);
            src += 3;
        }
        else if (*src == '+') {
            *dst++ = ' ';
            src++;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void file_start(int fd, int argc, char** argv) {
    enum capture_state expected = State_Idle;
    if (!atomic_compare_exchange_strong(&g_state.cap_state, &expected, State_Starting)) {
        _socket_printf(fd, "{\"state\": \"%s\"}", capture_state_to_str(expected));
        return;
    }

    char errstr[256];
    memset(errstr, 0, sizeof(errstr));

    unsigned cxadc_array[256];
    unsigned cxadc_count = 0;

    char linear_name[64];
    g_state.linear_enabled = false;
    strcpy(linear_name, AUDIO_DEFAULT_NAME);
    g_state.linear.dev_info.format = AUDIO_DEFAULT_FORMAT;

    for (int i = 0; i < argc; ++i) {
        unsigned num;
        if (1 == sscanf(argv[i], "cxadc%u", &num)) {
            if (cxadc_count < sizeof(cxadc_array) / sizeof(*cxadc_array)) {
                unsigned idx = cxadc_count++;
                cxadc_array[idx] = num;
                g_state.cxadc[idx].fd = -1;
                sprintf(g_state.cxadc[idx].name, "cxadc%u", num);
            }
            continue;
        }

        if (0 == strncmp(argv[i], "linear", 6)) {
            g_state.linear_enabled = true;
            continue;
        }

        char urlencoded[64];
        if (1 == sscanf(argv[i], "lname=%63s", urlencoded)) {
            urldecode2(linear_name, urlencoded);
            continue;
        }
        if (1 == sscanf(argv[i], "lformat=%63s", urlencoded)) {
            audio_set_format_value(&g_state.linear.dev_info, urlencoded);
            continue;
        }
        unsigned int rate = 0;
        if (1 == sscanf(argv[i], "lrate=%u", &rate) && rate >= 22050 && rate <= 384000) {
            g_state.linear.dev_info.rate = rate;
            continue;
        }
        unsigned int channels = 0;
        if (1 == sscanf(argv[i], "lchannels=%u", &channels) && channels >= 1 && channels <= 16) {
            g_state.linear.dev_info.channels = channels;
            continue;
        }
    }

    g_state.overflow_counter = 0;

    for (size_t i = 0; i < cxadc_count; ++i) {
        if (rb_init(&g_state.cxadc[i].ring_buffer, g_state.cxadc[i].name, 1 << 30) != 0) {
            snprintf(errstr, sizeof(errstr) - 1, "failed to allocate ringbuffer: %s", _get_error());
            goto error;
        }
    }

    struct timespec time1;
    if (timespec_get(&time1, TIME_UTC) == 0) {
        goto error;
    }

    if (g_state.linear_enabled) {
        if (audio_device_init(&g_state.linear.dev_info, linear_name) != 0) {
            snprintf(errstr, sizeof(errstr) - 1, "failed to get audio device info: %s", _get_error());
            goto error;
        }

        size_t sample_size = audio_get_sample_size(&g_state.linear.dev_info);

        if (rb_init(&g_state.linear.ring_buffer, "linear", (2 << 20) * sample_size) != 0) {
            snprintf(errstr, sizeof(errstr) - 1, "failed to allocate ringbuffer: %s", _get_error());
            goto error;
        }
    }

    int err = 0;

    struct timespec time2;
    if (timespec_get(&time2, TIME_UTC) == 0) {
        goto error;
    }

    for (size_t i = 0; i < cxadc_count; ++i) {
        char cxadc_name[32];
        sprintf(cxadc_name, CX_DEVICE_PATH "%d", cxadc_array[i]);
        int cxadc_fd = _file_open(cxadc_name, FILE_OPEN_FLAGS);
        if (cxadc_fd < 0) {
            snprintf(errstr, sizeof(errstr) - 1, "cannot open cxadc: %s", _file_get_error());
            goto error;
        }
        g_state.cxadc[i].fd = cxadc_fd;
    }

    g_state.cxadc_count = cxadc_count;

    struct timespec time3;
    if (timespec_get(&time3, TIME_UTC) == 0) {
        goto error;
    }

    const size_t linear_ns = timespec_to_nanos(&time2) - timespec_to_nanos(&time1);
    const ssize_t cxadc_ns = timespec_to_nanos(&time3) - timespec_to_nanos(&time2);

    for (size_t i = 0; i < cxadc_count; ++i) {
        thrd_t thread_id;
        if ((err = thrd_create(&thread_id, cxadc_writer_thread, (void*)i) != 0)) {
            snprintf(errstr, sizeof(errstr) - 1, "can't create cxadc writer thread: %s", _get_error());
            goto error;
        }
        g_state.cxadc[i].writer_thread = thread_id;
    }

    if (g_state.linear_enabled) {
        thrd_t thread_id;
        g_state.linear.writer_thread_state = State_Starting;

        if ((err = thrd_create(&thread_id, linear_writer_thread, NULL) != 0)) {
            snprintf(errstr, sizeof(errstr) - 1, "can't create linear writer thread: %s", _get_error());
            goto error;
        }
        g_state.linear.writer_thread = thread_id;
    }

    g_state.cap_state = State_Running;

    if (g_state.linear_enabled) {
        // wait for writer thread to set state
        while (g_state.linear.writer_thread_state == State_Starting)
            _sleep_ms(1);

        if (g_state.linear.writer_thread_state == State_Failed) {
            snprintf(errstr, sizeof(errstr) - 1, "audio capture failed, check server log");
            goto error;
        }
    }

    _socket_printf(
        fd,
        "{"
        "\"state\": \"%s\","
        "\"cxadc_ns\": %ld,"
        "\"linear_ns\": %ld,"
        "\"linear_rate\": %u,"
        "\"linear_channels\": %u,"
        "\"linear_format\": \"%s\""
        "}",
        capture_state_to_str(State_Running),
        cxadc_ns,
        linear_ns,
        g_state.linear.dev_info.rate,
        g_state.linear.dev_info.channels,
        audio_get_format_name(&g_state.linear.dev_info)
    );
    return;

error:
    g_state.cap_state = State_Failed;
    if (_thread_is_init(g_state.linear.writer_thread)) {
        thrd_join(g_state.linear.writer_thread, NULL);
        g_state.linear.writer_thread = (thrd_t){ 0 };
    }

    audio_device_deinit(&g_state.linear.dev_info);

    for (size_t i = 0; i < cxadc_count; ++i) {
        struct cxadc_state* cxadc = &g_state.cxadc[i];
        if (_thread_is_init(cxadc->writer_thread)) {
            thrd_join(cxadc->writer_thread, NULL);
            cxadc->writer_thread = (thrd_t){ 0 };
        }
    }

    for (size_t i = 0; i < cxadc_count; ++i) {
        struct cxadc_state* cxadc = &g_state.cxadc[i];
        if (cxadc->fd != -1) {
            _file_close(cxadc->fd);
            cxadc->fd = -1;
        }
        rb_close(&cxadc->ring_buffer);
    }

    _socket_printf(fd, "{\"state\": \"%s\", \"fail_reason\": \"%s\"}", capture_state_to_str(State_Failed), errstr);
    g_state.cap_state = State_Idle;
}

int cxadc_writer_thread(void* id) {
    while (g_state.cap_state == State_Starting)
        _sleep_ms(1);

    if (g_state.cap_state == State_Failed)
        return -1;

    ringbuffer_t* rb = &g_state.cxadc[(size_t)id].ring_buffer;
    const int fd = g_state.cxadc[(size_t)id].fd;

    while (g_state.cap_state != State_Stopping) {
        void* ptr = rb_write_ptr(rb, BUFFER_READ_SIZE);
        if (ptr == NULL) {
            ++g_state.overflow_counter;
            fprintf(stderr, "ringbuffer full, may be dropping samples!!! THIS IS BAD!\n");
            _sleep_ms(1);
            continue;
        }
        ssize_t count = read(fd, ptr, BUFFER_READ_SIZE);
        if (count == 0) {
            _sleep_us(1);
            continue;
        }
        if (count < 0) {
            fprintf(stderr, "read failed\n");
            break;
        }

        rb_write_finished(rb, count);
    }
    _file_close(fd);
    return 0;
}

int linear_writer_thread(void* arg) {
    (void)arg;

    while (g_state.cap_state == State_Starting)
        _sleep_ms(1);

    if (g_state.cap_state == State_Failed) {
        g_state.linear.writer_thread_state = State_Failed;
        return -1;
    }

    int ret = 0;
    struct audio_capture_state state = { 0 };

    if ((ret = audio_capture_init(&state, &g_state.linear.dev_info)) != 0) {
        goto exit;
    }

    if ((ret = audio_capture_start(&state)) != 0) {
        goto exit;
    }

    // throw away first buffer as it's not always full
    audio_capture_fill_buffer(&state, NULL, audio_capture_get_next_frame_count(&state));

    g_state.linear.writer_thread_state = State_Running;
    ringbuffer_t* rb = &g_state.linear.ring_buffer;

    while (g_state.cap_state != State_Stopping) {
        ssize_t frame_count = audio_capture_get_next_frame_count(&state);

        if (frame_count == 0) {
            continue;
        }

        if (frame_count == -1) {
            goto exit;
        }

        size_t buffer_sz = audio_get_sample_size(state.dev_info) * frame_count;

        void* ptr = rb_write_ptr(rb, buffer_sz);

        if (ptr == NULL) {
            ++g_state.overflow_counter;
            fprintf(stderr, "ringbuffer full, may be dropping samples!!! THIS IS BAD!\n");
            _sleep_ms(1);
            continue;
        }

        ssize_t count = audio_capture_fill_buffer(&state, ptr, frame_count);

        if (count < 0) {
            fprintf(stderr, "audio_capture_fill_buffer failed\n");
            break;
        }

        rb_write_finished(rb, count);
    }

exit:
    // if we're here but the main thread is running, likely err
    if (g_state.cap_state == State_Running) {
        g_state.linear.writer_thread_state = State_Failed;
    }
    else {
        g_state.linear.writer_thread_state = State_Stopping;
    }

    audio_capture_stop(&state);
    audio_capture_deinit(&state);
    audio_device_deinit(&g_state.linear.dev_info);
    return 0;
}

void file_stop(int fd, int argc, char** argv) {
    (void)argc;
    (void)argv;
    enum capture_state expected = State_Running;
    if (!atomic_compare_exchange_strong(&g_state.cap_state, &expected, State_Stopping)) {
        _socket_printf(fd, "{\"state\": \"%s\"}", capture_state_to_str(expected));
        return;
    }

    for (size_t i = 0; i < g_state.cxadc_count; ++i)
        thrd_join(g_state.cxadc[i].writer_thread, NULL);

    if (g_state.linear_enabled) {
        thrd_join(g_state.linear.writer_thread, NULL);

        while (_thread_is_init(g_state.linear.reader_thread))
            _sleep_ms(100);
    }

    for (size_t i = 0; i < g_state.cxadc_count; ++i) {
        while (_thread_is_init(g_state.cxadc[i].reader_thread))
            _sleep_ms(100);

        rb_close(&g_state.cxadc[i].ring_buffer);

        g_state.cxadc[i].writer_thread = (thrd_t){ 0 };
        g_state.cxadc[i].reader_thread = (thrd_t){ 0 };
    }

    if (g_state.linear_enabled) {
        rb_close(&g_state.linear.ring_buffer);
        g_state.linear.writer_thread = (thrd_t){ 0 };
        g_state.linear.reader_thread = (thrd_t){ 0 };
    }

    g_state.cap_state = State_Idle;

    _socket_printf(fd, "{\"state\": \"%s\", \"overflows\": %ld}", capture_state_to_str(State_Idle), g_state.overflow_counter);
}

void file_root(int fd, int argc, char** argv) {
    (void)argc;
    (void)argv;
    _socket_printf(fd, "Hello World!\n");
}

void file_version(int fd, int argc, char** argv) {
    (void)argc;
    (void)argv;
    _socket_printf(fd, "%s\n", CXADC_VHS_SERVER_VERSION);
}

void pump_ringbuffer_to_fd(int fd, ringbuffer_t* rb, _Atomic(thrd_t) *pt) {
    thrd_t expected = { 0 };
    if (!atomic_compare_exchange_strong(pt, &expected, thrd_current())) {
        return;
    }
    while (g_state.cap_state != State_Running && g_state.cap_state != State_Stopping)
        _sleep_us(1);

    while (g_state.cap_state == State_Running || g_state.cap_state == State_Stopping) {
        size_t read_size = rb->tail - rb->head;

        if (g_state.cap_state == State_Stopping) {
            // flush data
            if (read_size == 0) {
                break;
            }
        }

        const void* ptr = rb_read_ptr(rb, read_size);
        if (ptr == NULL) {
            if (g_state.cap_state == State_Stopping)
                break;
            _sleep_us(1);
            continue;
        }

        ssize_t count = _socket_write(fd, ptr, read_size);
        if (count < 0) {
            fprintf(stderr, "write failed: %s\n", _get_network_error());
            break;
        }

        if (count == 0) {
            _sleep_us(1);
            continue;
        }
        rb_read_finished(rb, count);
    }

    *pt = (thrd_t){ 0 };
}

void file_cxadc(int fd, int argc, char** argv) {
    if (argc != 1)
        return;
    unsigned id;
    if (1 != sscanf(argv[0], "%u", &id) || id >= 256)
        return;
    pump_ringbuffer_to_fd(fd, &g_state.cxadc[id].ring_buffer, &g_state.cxadc[id].reader_thread);
}

void file_linear(int fd, int argc, char** argv) {
    (void)argc;
    (void)argv;
    pump_ringbuffer_to_fd(fd, &g_state.linear.ring_buffer, &g_state.linear.reader_thread);
}

void file_stats(int fd, int argc, char** argv) {
    const enum capture_state state = g_state.cap_state;
    if (state != State_Running) {
        _socket_printf(fd, "{\"state\":\"%s\"}", capture_state_to_str(state));
    }
    else {
        _socket_printf(
            fd,
            "{\"state\":\"%s\",\"overflows\":%zu,",
            capture_state_to_str(state),
            g_state.overflow_counter
        );

        if (g_state.linear_enabled) {
            const size_t linear_read = atomic_load(&g_state.linear.ring_buffer.total_read);
            const size_t linear_written = atomic_load(&g_state.linear.ring_buffer.total_write);
            const size_t linear_difference = linear_written - linear_read;

            _socket_printf(
                fd,
                "\"linear\":{\"read\":%zu,\"written\":%zu,\"difference\":%zu,\"difference_pct\":%zu},",
                linear_read,
                linear_written,
                linear_difference,
                linear_difference * 100 / g_state.linear.ring_buffer.buffer_size
            );
        }

        _socket_printf(fd, "\"cxadc\":[");

        for (size_t i = 0; i < g_state.cxadc_count; ++i) {
            const size_t read = atomic_load(&g_state.cxadc[i].ring_buffer.total_read);
            const size_t written = atomic_load(&g_state.cxadc[i].ring_buffer.total_write);
            const size_t difference = written - read;

            if (i != 0)
                _socket_printf(fd, ",");
            _socket_printf(
                fd,
                "{\"read\":%zu,\"written\":%zu,\"difference\":%zu,\"difference_pct\":%zu}",
                read,
                written,
                difference,
                difference * 100 / g_state.cxadc[i].ring_buffer.buffer_size
            );
        }
        _socket_printf(fd, "]}");
    }
    (void)fd;
    (void)argc;
    (void)argv;
}
