/* Run Linux callback dispatch on every host without requiring epoll syscalls. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned char qbyte;
typedef struct epollctx_s { void (*Polled)(struct epollctx_s *, unsigned int); } epollctx_t;
typedef struct { void (*ReadGamePacket)(void); } owner_t;
typedef struct ftenet_generic_connection_s {
    uint64_t prefix[3];
    epollctx_t epoll;
    int (*GetPacket)(struct ftenet_generic_connection_s *);
    int connum;
    owner_t *owner;
} ftenet_generic_connection_t;
typedef struct { int id; } ftenet_tcp_connection_t;
typedef struct {
    uint64_t prefix[5];
    epollctx_t epoll;
    ftenet_tcp_connection_t *con;
} ftenet_tcp_stream_t;
static struct { int connum; } net_from;
static ftenet_generic_connection_t *net_from_connection, *expected_udp;
static ftenet_tcp_stream_t *expected_tcp;
static unsigned remaining, delivered, flushed, killed;
static const char failure[] = "original stream failure";
static int fail_read;
/* ENGINE_CONTAINER_MACRO */
static int get_packet(ftenet_generic_connection_t *con)
{
    assert(con == expected_udp);
    if (!remaining) return 0;
    --remaining; return 1;
}
static void read_packet(void)
{
    assert(net_from_connection == expected_udp);
    assert(net_from.connum == expected_udp->connum);
    ++delivered;
}
static const char *FTENET_TCP_ReadStream(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st)
{
    assert(st == expected_tcp && con == expected_tcp->con);
    return fail_read ? failure : NULL;
}
static void FTENET_TCP_KillStream(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st, const char *err)
{
    assert(st == expected_tcp && con == expected_tcp->con && err == failure); ++killed;
}
static void FTENET_TCP_Flush(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st)
{
    assert(st == expected_tcp && con == expected_tcp->con); ++flushed;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    owner_t owner = {read_packet};
    ftenet_generic_connection_t udp[2] = {0};
    ftenet_tcp_connection_t connection[2] = {{1}, {2}};
    ftenet_tcp_stream_t tcp[2] = {0};
    for (unsigned i = 0; i < 2; ++i) {
        udp[i].GetPacket = get_packet; udp[i].owner = &owner; udp[i].connum = (int)i + 5;
        udp[i].epoll.Polled = FTENET_Datagram_Polled;
        tcp[i].con = &connection[i]; tcp[i].epoll.Polled = FTENET_TCP_Polled;
        expected_udp = &udp[i]; expected_tcp = &tcp[i];
        for (unsigned packets = 0; packets < 5; ++packets) {
            remaining = packets; delivered = 0;
            udp[i].epoll.Polled(&udp[i].epoll, 1);
            assert(!remaining && delivered == packets);
        }
        flushed = killed = 0; fail_read = 0;
        tcp[i].epoll.Polled(&tcp[i].epoll, 1);
        assert(flushed == 1 && !killed);
        fail_read = 1; tcp[i].epoll.Polled(&tcp[i].epoll, 1);
        assert(flushed == 1 && killed == 1);
    }
    puts("Epoll callbacks: embedded UDP/TCP context recovery, empty/multiple packets, routing and stream flush/error dispatch passed");
    return 0;
}
