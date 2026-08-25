/* The network, two ways -- both borrowed from the machines that did it first.
 *
 * The Meatloaf rule (the C64's Meatloaf cartridge): a URL is a filename.
 * Any name beginning http:// or https:// that reaches the filesystem device
 * ($D300) for reading -- LOAD, TYPE, CP, RUN, EhBASIC's LOAD, BBC BASIC's
 * LOAD on the Tube -- is fetched and served like a file. No ROM change:
 * the ROM passes names through untouched and the host does the fetching.
 *
 * The N: device (FujiNet's network device, as the Atari and Apple saw it),
 * at $D900, for programs that want a live connection: four channels, each
 * a URL opened for reading and writing.
 *   $D900 W: command          $D901 R: status (0 ok, 1 not found / refused, 2 i/o error,
 *                                          3 bad command, 4 closed by the peer, 5 name too long, 6 not fitted)
 *   $D902 RW: channel 0-3     $D904-$D907 NAMEPTR 28-bit -> URL, NUL-terminated
 *   $D908-$D90B ADDR 28-bit   $D90C-$D90F LEN  bytes requested; updated to bytes done
 *   $D910-$D913 SIZE          after OPEN: the content length, $FFFFFFFF if unknown; after STATUS: bytes waiting
 *   commands: 1 OPEN  (tcp://host:port a connection; http://, https:// a GET whose body is then READ)
 *             2 READ  (up to LEN bytes to ADDR, what has arrived so far; LEN = done, 0 = nothing yet)
 *             3 WRITE (LEN bytes from ADDR; tcp only)
 *             4 CLOSE
 *             5 STATUS (SIZE = bytes waiting; status 4 once the peer has closed and the bytes are gone)
 *             6 GET   (the Meatloaf rule for programs: fetch the whole URL into ADDR, at most LEN; LEN = done, SIZE = total)
 * Reads never block the machine: a program polls, as it would a UART.
 * The desktop host does TCP with sockets and HTTP(S) with curl; the Pi is
 * not fitted yet (status 6; Circle has a TCP/IP stack and an HTTP client,
 * which is the way in). */
#ifndef K4510_NET_H
#define K4510_NET_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int     net_is_url(const char *name);                      /* http:// or https://, any case */
int     net_fetch_file(const char *url, char *path, size_t max);   /* to a fresh temp file; 0 = ok, the caller unlinks */
void    net_reset(void);
uint8_t net_read(uint8_t reg);
void    net_write(uint8_t reg, uint8_t v);
#ifdef __cplusplus
}
#endif
#endif
