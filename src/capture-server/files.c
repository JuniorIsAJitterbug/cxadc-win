#include "files.h"

#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>

#include <ctype.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "version.h"

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

struct atomic_ringbuffer {
  uint8_t* buf;
  size_t buf_size;
  _Atomic size_t written;
  _Atomic size_t read;
};

bool atomic_ringbuffer_init(struct atomic_ringbuffer* ctx, size_t buf_size) {
  static const int FLAGS = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE;

  void* buf = MAP_FAILED;

#ifdef MAP_HUGE_SHIFT
  static const size_t ONE_GB = (1u << 30);
  static const size_t TWO_MB = (2u << 20);
  if (buf_size % ONE_GB == 0 && buf_size > ONE_GB)
    buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, FLAGS | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0);
  if (MAP_FAILED == buf && buf_size % TWO_MB == 0 && buf_size > TWO_MB)
    buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, FLAGS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT), -1, 0);
#endif
  if (MAP_FAILED == buf)
    buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, FLAGS, -1, 0);

  if (MAP_FAILED == buf) {
    return false;
  }

  volatile uint8_t test = *(volatile uint8_t*)buf;
  (void)test;

  ctx->buf_size = buf_size;
  ctx->read = 0;
  ctx->written = 0;
  ctx->buf = (uint8_t*)buf;
  return true;
}

void atomic_ringbuffer_free(struct atomic_ringbuffer* ctx) {
  if (ctx->buf)
    munmap(ctx->buf, ctx->buf_size);
  ctx->buf = NULL;
}

uint8_t* atomic_ringbuffer_get_write_ptr(struct atomic_ringbuffer* ctx) {
  return ctx->buf + (ctx->written % ctx->buf_size);
}

size_t atomic_ringbuffer_get_write_size(struct atomic_ringbuffer* ctx) {
  size_t buf_size = ctx->buf_size;
  size_t written = ctx->written;
  size_t read = ctx->read;
  size_t till_end = buf_size - (written % buf_size);
  size_t till_read = read + buf_size - written;
  return till_end < till_read ? till_end : till_read;
}

void atomic_ringbuffer_advance_written(struct atomic_ringbuffer* ctx, size_t count) {
  ctx->written += count;
}

uint8_t* atomic_ringbuffer_get_read_ptr(struct atomic_ringbuffer* ctx) {
  return ctx->buf + (ctx->read % ctx->buf_size);
}

size_t atomic_ringbuffer_get_read_size(struct atomic_ringbuffer* ctx) {
  size_t buf_size = ctx->buf_size;
  size_t written = ctx->written;
  size_t read = ctx->read;
  size_t till_end = buf_size - (read % buf_size);
  size_t till_written = written - read;
  return till_end < till_written ? till_end : till_written;
}

void atomic_ringbuffer_advance_read(struct atomic_ringbuffer* ctx, size_t count) {
  ctx->read += count;
}

// this is only usable for stats, do not rely on being correct
void atomic_ringbuffer_get_stats(struct atomic_ringbuffer* ctx, size_t* read, size_t* written, size_t* difference) {
  // we read `read` first, so that we never get negative results

  size_t _read = atomic_load(&ctx->read);
  size_t _written = atomic_load(&ctx->written);
  size_t _difference = _written - _read;
  if (_difference > ctx->buf_size)
    _difference = ctx->buf_size;
  if (read)
    *read = _read;
  if (written)
    *written = _written;
  if (difference)
    *difference = _difference;
}

enum capture_state {
  State_Idle = 0,
  State_Starting,
  State_Running,
  State_Stopping,

  State_Failed
};

const char* capture_state_to_str(enum capture_state state) {
  const char* NAMES[] = {"Idle", "Starting", "Running", "Stopping", "Failed"};
  return NAMES[(int)state];
}

struct cxadc_state {
  int fd;
  pthread_t writer_thread;
  struct atomic_ringbuffer ring_buffer;

  // This is special and not protected by cap_state
  _Atomic pthread_t reader_thread;
};

struct {
  _Atomic enum capture_state cap_state;
  struct cxadc_state cxadc[256];
  size_t cxadc_count;
  _Atomic size_t overflow_counter;

  struct {
    snd_pcm_t* handle;
    pthread_t writer_thread;
    struct atomic_ringbuffer ring_buffer;

    // This is special and not protected by cap_state
    _Atomic pthread_t reader_thread;
  } linear;

} g_state;

void* cxadc_writer_thread(void* id);
void* linear_writer_thread(void*);

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
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

void file_start(int fd, int argc, char** argv) {
  static const int MODE = SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_FORMAT | SND_PCM_NO_SOFTVOL;

  enum capture_state expected = State_Idle;
  if (!atomic_compare_exchange_strong(&g_state.cap_state, &expected, State_Starting)) {
    dprintf(fd, "{\"state\": \"%s\"}", capture_state_to_str(expected));
    return;
  }

  char errstr[256];
  memset(errstr, 0, sizeof(errstr));

  unsigned cxadc_array[256];
  unsigned cxadc_count = 0;

  char linear_name[64];
  strcpy(linear_name, "hw:CARD=CXADCADCClockGe");
  unsigned int linear_rate = 0;
  unsigned int linear_channels = 0;
  snd_pcm_format_t linear_format = SND_PCM_FORMAT_UNKNOWN;
  snd_pcm_t* handle = NULL;

  for (int i = 0; i < argc; ++i) {
    unsigned num;
    if (1 == sscanf(argv[i], "cxadc%u", &num)) {
      if (cxadc_count < sizeof(cxadc_array) / sizeof(*cxadc_array)) {
        unsigned idx = cxadc_count++;
        cxadc_array[idx] = num;
        g_state.cxadc[idx].fd = -1;
      }
      continue;
    }
    char urlencoded[64];
    if (1 == sscanf(argv[i], "lname=%63s", urlencoded)) {
      urldecode2(linear_name, urlencoded);
      continue;
    }
    if (1 == sscanf(argv[i], "lformat=%63s", urlencoded)) {
      linear_format = snd_pcm_format_value(urlencoded);
      continue;
    }
    unsigned int rate = 0;
    if (1 == sscanf(argv[i], "lrate=%u", &rate) && rate >= 22050 && rate <= 384000) {
      linear_rate = rate;
      continue;
    }
    unsigned int channels = 0;
    if (1 == sscanf(argv[i], "lchannels=%u", &channels) && channels >= 1 && channels <= 16) {
      linear_channels = channels;
      continue;
    }
  }

  g_state.overflow_counter = 0;

  for (size_t i = 0; i < cxadc_count; ++i) {
    if (!atomic_ringbuffer_init(&g_state.cxadc[i].ring_buffer, 1 << 30)) {
      snprintf(errstr, sizeof(errstr) - 1, "failed to allocate ringbuffer: %s", sys_errlist[errno]);
      goto error;
    }
  }

  int err = 0;

  if ((err = snd_pcm_open(&handle, linear_name, SND_PCM_STREAM_CAPTURE, MODE)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot open ALSA device: %s", snd_strerror(err));
    goto error;
  }

  snd_pcm_hw_params_t* hw_params = NULL;
  snd_pcm_hw_params_alloca(&hw_params);

  if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot initialize hardware parameter structure: %s", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot set access type: %s", snd_strerror(err));
    goto error;
  }

  if (linear_rate) {
set_rate:
    if ((err = snd_pcm_hw_params_set_rate(handle, hw_params, linear_rate, 0)) < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot set sample rate: %s", snd_strerror(err));
      goto error;
    }
  } else {
    if (snd_pcm_hw_params_get_rate(hw_params, &linear_rate, 0) < 0) {
      if ((err = snd_pcm_hw_params_get_rate_max(hw_params, &linear_rate, 0)) < 0) {
        snprintf(errstr, sizeof(errstr) - 1, "cannot get sample rate: %s", snd_strerror(err));
        goto error;
      } else {
        goto set_rate;
      }
    }
  }

  if (linear_channels) {
    if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, linear_channels)) < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot set channel count: %s", snd_strerror(err));
      goto error;
    }
  } else {
    if ((err = snd_pcm_hw_params_get_channels(hw_params, &linear_channels)) < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot get channel count: %s", snd_strerror(err));
      goto error;
    }
  }

  if (linear_format != SND_PCM_FORMAT_UNKNOWN) {
    if ((err = snd_pcm_hw_params_set_format(handle, hw_params, linear_format)) < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot set sample format: %s", snd_strerror(err));
      goto error;
    }
  } else {
    if ((err = snd_pcm_hw_params_get_format(hw_params, &linear_format)) < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot get sample format: %s", snd_strerror(err));
      goto error;
    }
  }

  ssize_t format_size;
  if ((format_size = snd_pcm_format_size(linear_format, 1)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot get format size: %s", snd_strerror(err));
    goto error;
  }

  size_t sample_size = linear_channels * format_size;
  if (!atomic_ringbuffer_init(&g_state.linear.ring_buffer, (2 << 20) * sample_size)) {
    snprintf(errstr, sizeof(errstr) - 1, "failed to allocate ringbuffer: %s", sys_errlist[errno]);
    goto error;
  }

  struct timespec time1;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time1);

  if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot set hw parameters: %s", snd_strerror(err));
    goto error;
  }

  snd_pcm_sw_params_t* sw_params = NULL;
  snd_pcm_sw_params_alloca(&sw_params);

  if ((err = snd_pcm_sw_params_current(handle, sw_params)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot query sw parameters: %s", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_sw_params_set_tstamp_mode(handle, sw_params, SND_PCM_TSTAMP_ENABLE)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot set tstamp mode: %s", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_sw_params_set_tstamp_type(handle, sw_params, SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot set tstamp type: %s", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_sw_params(handle, sw_params)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot set sw parameters: %s", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_prepare(handle)) < 0) {
    snprintf(errstr, sizeof(errstr) - 1, "cannot prepare audio interface for use: %s", snd_strerror(err));
    goto error;
  }

  snd_pcm_start(handle);

  struct timespec time2;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time2);

  for (size_t i = 0; i < cxadc_count; ++i) {
    char cxadc_name[32];
    sprintf(cxadc_name, "/dev/cxadc%u", cxadc_array[i]);
    const int cxadc_fd = open(cxadc_name, O_NONBLOCK);
    if (cxadc_fd < 0) {
      snprintf(errstr, sizeof(errstr) - 1, "cannot open cxadc: %s", sys_errlist[errno]);
      goto error;
    }
    g_state.cxadc[i].fd = cxadc_fd;
  }

  g_state.cxadc_count = cxadc_count;

  struct timespec time3;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time3);

  const long linear_ns = timespec_to_nanos(&time2) - timespec_to_nanos(&time1);
  const long cxadc_ns = timespec_to_nanos(&time3) - timespec_to_nanos(&time2);

  g_state.linear.handle = handle;

  for (size_t i = 0; i < cxadc_count; ++i) {
    pthread_t thread_id;
    if ((err = pthread_create(&thread_id, NULL, cxadc_writer_thread, (void*)i) != 0)) {
      snprintf(errstr, sizeof(errstr) - 1, "can't create cxadc writer thread: %s", sys_errlist[err]);
      goto error;
    }
    g_state.cxadc[i].writer_thread = thread_id;
  }

  pthread_t thread_id;
  if ((err = pthread_create(&thread_id, NULL, linear_writer_thread, NULL) != 0)) {
    snprintf(errstr, sizeof(errstr) - 1, "can't create linear writer thread: %s", sys_errlist[err]);
    goto error;
  }
  g_state.linear.writer_thread = thread_id;

  g_state.cap_state = State_Running;
  dprintf(
    fd,
    "{"
    "\"state\": \"%s\","
    "\"linear_ns\": %ld,"
    "\"cxadc_ns\": %ld,"
    "\"linear_rate\": %u,"
    "\"linear_channels\": %u,"
    "\"linear_format\": \"%s\""
    "}",
    capture_state_to_str(State_Running),
    linear_ns,
    cxadc_ns,
    linear_rate,
    linear_channels,
    snd_pcm_format_name(linear_format)
  );
  return;

error:
  g_state.cap_state = State_Failed;

  if (g_state.linear.writer_thread) {
    pthread_join(g_state.linear.writer_thread, NULL);
    g_state.linear.writer_thread = 0;
  }

  for (size_t i = 0; i < cxadc_count; ++i) {
    struct cxadc_state* cxadc = &g_state.cxadc[i];
    if (cxadc->writer_thread) {
      pthread_join(cxadc->writer_thread, NULL);
      cxadc->writer_thread = 0;
    }
  }

  if (handle)
    snd_pcm_close(handle);

  for (size_t i = 0; i < cxadc_count; ++i) {
    struct cxadc_state* cxadc = &g_state.cxadc[i];
    if (cxadc->fd != -1) {
      close(cxadc->fd);
      cxadc->fd = -1;
    }
    atomic_ringbuffer_free(&cxadc->ring_buffer);
  }

  dprintf(fd, "{\"state\": \"%s\", \"fail_reason\": \"%s\"}", capture_state_to_str(State_Failed), errstr);
  g_state.cap_state = State_Idle;
}

void* cxadc_writer_thread(void* id) {
  while (g_state.cap_state == State_Starting)
    usleep(1000);

  if (g_state.cap_state == State_Failed)
    return NULL;

  struct atomic_ringbuffer* buf = &g_state.cxadc[(size_t)id].ring_buffer;
  const int fd = g_state.cxadc[(size_t)id].fd;

  while (g_state.cap_state != State_Stopping) {
    void* ptr = atomic_ringbuffer_get_write_ptr(buf);
    size_t len = atomic_ringbuffer_get_write_size(buf);
    if (len == 0) {
      ++g_state.overflow_counter;
      fprintf(stderr, "ringbuffer full, may be dropping samples!!! THIS IS BAD!\n");
      usleep(1000);
      continue;
    }
    ssize_t count = read(fd, ptr, len);
    if (count == 0) {
      usleep(1);
      continue;
    }
    if (count < 0) {
      fprintf(stderr, "read failed\n");
      break;
    }

    atomic_ringbuffer_advance_written(buf, count);
  }
  close(fd);
  return NULL;
}

void* linear_writer_thread(void* arg) {
  (void)arg;

  while (g_state.cap_state == State_Starting)
    usleep(1000);

  if (g_state.cap_state == State_Failed)
    return NULL;

  struct atomic_ringbuffer* buf = &g_state.linear.ring_buffer;
  snd_pcm_t* handle = g_state.linear.handle;

  while (g_state.cap_state != State_Stopping) {
    void* ptr = atomic_ringbuffer_get_write_ptr(buf);
    size_t len = atomic_ringbuffer_get_write_size(buf);
    size_t len_samples = snd_pcm_bytes_to_frames(handle, (ssize_t)len);
    if (len_samples == 0) {
      ++g_state.overflow_counter;
      fprintf(stderr, "ringbuffer full, may be dropping samples!!! THIS IS BAD!\n");
      usleep(1000);
      continue;
    }
    long count = snd_pcm_readi(handle, ptr, len_samples);
    if (count == 0 || count == -EAGAIN) {
      usleep(1);
      continue;
    }
    if (count < 0) {
      fprintf(stderr, "snd_pcm_readi failed: %s\n", snd_strerror((int)count));
      break;
    }

    atomic_ringbuffer_advance_written(buf, snd_pcm_frames_to_bytes(handle, count));
  }
  snd_pcm_drop(handle);
  snd_pcm_close(handle);
  return NULL;
}

void file_stop(int fd, int argc, char** argv) {
  (void)argc;
  (void)argv;
  enum capture_state expected = State_Running;
  if (!atomic_compare_exchange_strong(&g_state.cap_state, &expected, State_Stopping)) {
    dprintf(fd, "{\"state\": \"%s\"}", capture_state_to_str(expected));
    return;
  }

  for (size_t i = 0; i < g_state.cxadc_count; ++i)
    pthread_join(g_state.cxadc[i].writer_thread, NULL);

  pthread_join(g_state.linear.writer_thread, NULL);

  while (g_state.linear.reader_thread)
    usleep(100000);

  for (size_t i = 0; i < g_state.cxadc_count; ++i) {
    while (g_state.cxadc[i].reader_thread)
      usleep(100000);
    atomic_ringbuffer_free(&g_state.cxadc[i].ring_buffer);
    g_state.cxadc[i].writer_thread = 0;
    g_state.cxadc[i].reader_thread = 0;
  }

  atomic_ringbuffer_free(&g_state.linear.ring_buffer);
  g_state.linear.writer_thread = 0;
  g_state.linear.reader_thread = 0;

  g_state.cap_state = State_Idle;

  dprintf(fd, "{\"state\": \"%s\", \"overflows\": %ld}", capture_state_to_str(State_Idle), g_state.overflow_counter);
}

void file_root(int fd, int argc, char** argv) {
  (void)argc;
  (void)argv;
  dprintf(fd, "Hello World!\n");
}

void file_version(int fd, int argc, char** argv) {
  (void)argc;
  (void)argv;
  dprintf(fd, "%s\n", CXADC_VHS_SERVER_VERSION);
}

void pump_ringbuffer_to_fd(int fd, struct atomic_ringbuffer* buf, _Atomic pthread_t* pt) {
  pthread_t expected = 0;
  if (!atomic_compare_exchange_strong(pt, &expected, pthread_self())) {
    return;
  }
  while (g_state.cap_state != State_Running && g_state.cap_state != State_Stopping)
    usleep(1);

  while (g_state.cap_state == State_Running || g_state.cap_state == State_Stopping) {
    void* ptr = atomic_ringbuffer_get_read_ptr(buf);
    size_t len = atomic_ringbuffer_get_read_size(buf);
    if (len == 0) {
      if (g_state.cap_state == State_Stopping)
        break;
      usleep(1);
      continue;
    }
    ssize_t count = write(fd, ptr, len);
    if (count == 0) {
      usleep(1);
      continue;
    }
    if (count < 0) {
      fprintf(stderr, "write failed: %s\n", sys_errlist[errno]);
      break;
    }

    atomic_ringbuffer_advance_read(buf, count);
  }

  *pt = 0;
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
    dprintf(fd, "{\"state\":\"%s\"}", capture_state_to_str(state));
  } else {
    size_t linear_read, linear_written, linear_difference;
    atomic_ringbuffer_get_stats(&g_state.linear.ring_buffer, &linear_read, &linear_written, &linear_difference);
    dprintf(
      fd,
      "{\"state\":\"%s\",\"overflows\":%zu,\"linear\":{\"read\":%zu,\"written\":%zu,\"difference\":%zu,\"difference_pct\":%zu},\"cxadc\":[",
      capture_state_to_str(state),
      g_state.overflow_counter,
      linear_read,
      linear_written,
      linear_difference,
      linear_difference * 100 / g_state.linear.ring_buffer.buf_size
    );
    for (size_t i = 0; i < g_state.cxadc_count; ++i) {
      size_t read, written, difference;
      atomic_ringbuffer_get_stats(&g_state.cxadc[i].ring_buffer, &read, &written, &difference);
      if (i != 0)
        dprintf(fd, ",");
      dprintf(
        fd,
        "{\"read\":%zu,\"written\":%zu,\"difference\":%zu,\"difference_pct\":%zu}",
        read,
        written,
        difference,
        difference * 100 / g_state.cxadc[i].ring_buffer.buf_size
      );
    }
    dprintf(fd, "]}");
  }
  (void)fd;
  (void)argc;
  (void)argv;
}
