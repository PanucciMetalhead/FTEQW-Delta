/* Actual QuakeWorld sequence/reliable header fields. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static void headers(int outgoing, int incoming, int send_reliable, int ack_reliable,
                    unsigned int *first, unsigned int *second)
{
    struct {int outgoing_sequence, incoming_sequence, incoming_reliable_sequence;} channel = {outgoing,incoming,ack_reliable}, *chan = &channel;
    unsigned int w1, w2;
    /* ENGINE_HEADERS */
    *first = w1; *second = w2;
}
int main(void)
{
    uint32_t state = 0x80000000u;
    for (int i = 0; i < 100000; ++i)
    {
        state = state * 1664525u + 1013904223u;
        int outgoing = state & 0x7fffffffu;
        state = state * 1664525u + 1013904223u;
        int incoming = state & 0x7fffffffu;
        for (int send = 0; send < 2; ++send)
        for (int ack = 0; ack < 2; ++ack)
        {
            unsigned int first, second;
            headers(outgoing,incoming,send,ack,&first,&second);
            assert(first == (uint32_t)outgoing + (send ? 0x80000000u : 0));
            assert(second == (uint32_t)incoming + (ack ? 0x80000000u : 0));
            assert((first >> 31) == (unsigned int)send && (second >> 31) == (unsigned int)ack);
        }
    }
    puts("Reliable packet headers: 100,000 sequences preserve all reliable/ack flag combinations");
    return 0;
}
