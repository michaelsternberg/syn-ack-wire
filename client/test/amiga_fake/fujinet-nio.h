/* The part of fujinet-nio-lib's API that src/amiga/net_fn.c uses, for the host tests (test/amiga_net_test.c): same
 * names, types and meanings as the library's include/fujinet-nio.h; the test supplies the bodies. */
#ifndef FAKE_FUJINET_NIO_H
#define FAKE_FUJINET_NIO_H
#include <stdint.h>
#define FN_OK            0x00
#define FN_ERR_NOT_READY 0x04
#define FN_ERR_IO        0x05
#define FN_ERR_TRANSPORT 0x10
#define FN_READ_EOF      0x01
#define FN_READ_AVAILABLE 0x08
typedef uint16_t fn_handle_t;
uint8_t fn_init(void);
uint8_t fn_tcp_open(fn_handle_t *handle, const char *host, uint16_t port);
uint8_t fn_write(fn_handle_t handle, uint32_t offset, const uint8_t *data, uint16_t len, uint16_t *written);
uint8_t fn_read(fn_handle_t handle, uint32_t offset, uint8_t *buf, uint16_t max_len, uint16_t *bytes_read, uint8_t *flags);
uint8_t fn_close(fn_handle_t handle);
#endif
