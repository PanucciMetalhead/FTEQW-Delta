/* Actual connection-cookie packing; compare with a wide integer bit oracle. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t state = 0x80000000u;
static int values[2], next;
static int test_rand(void) { assert(next < 2); return values[next++]; }
#define rand test_rand
static unsigned int cookie(double realtime, int index)
{
    struct client { unsigned int realip_ping; } clients[32] = {{0}};
    struct { struct client *clients; } svs = {clients};
    struct client *newcl = &clients[index];
    /* ENGINE_COOKIE */
    return newcl->realip_ping;
}
static unsigned int realip_number(int challenge)
{
    struct {int challenge; unsigned int realip_num;} client = {challenge,0}, *host_client = &client;
    /* ENGINE_REALIP_NUMBER */
    return host_client->realip_num;
}
int main(void)
{
    const double times[] = {0,1.25,-17.125,123456.789};
    for (int i = 0; i < 10000; ++i)
    {
        state = state * 1664525u + 1013904223u;
        values[0] = state & 0x7fffffff;
        state = state * 1664525u + 1013904223u;
        values[1] = state & 0x7fffffff;
        int index = i % 32;
        double time = times[i % 4];
        unsigned int timebits;
        memcpy(&timebits,&time,sizeof(timebits));
        uint64_t random_bits = (uint64_t)values[0] ^ ((uint64_t)values[1] << 8) ^ timebits;
        uint32_t expected = (uint32_t)((random_bits & 0xffffffu) * 256) | (uint32_t)index;
        next = 0;
        assert(cookie(time,index) == expected && next == 2);
        int challenge = (int)state;
        uint32_t expected_num = (uint32_t)((uint64_t)(uint32_t)challenge * 65536 + values[0]);
        next = 0;
        assert(realip_number(challenge) == expected_num && next == 1);
    }
    puts("Connection cookie packing: 10,000 high random values preserve clock salt, client-index bits and real-IP numbers");
    return 0;
}
