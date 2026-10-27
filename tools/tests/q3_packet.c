#include <assert.h>
#include <stdio.h>
enum { CP_QUAKE3=3, ca_connected=2, ca_active=4 };
typedef int cactive_t;
static struct { int state; struct { double last_received; } netchan; } cls;
static int net_message, incoming_state=ca_connected, activated;
static double realtime;
static cactive_t parse(int *message) { assert(message==&net_message); return incoming_state; }
static struct { struct { cactive_t (*ParseServerMessage)(int *); } cl; } plugin={{parse}}, *q3=&plugin;
static void CL_MakeActive(const char *game) { assert(game); activated++; }
static void packet(void)
{
	switch (CP_QUAKE3) {
	/* Q3_PACKET */
	}
}
int main(void)
{
	cls.state=ca_connected;
	for (int sec=1;sec<90;sec++) {
		realtime=sec;
		if (sec==10) incoming_state=ca_active;
		if (sec==30) incoming_state=ca_connected; // server redirects to a mod/download
		if (sec==70) incoming_state=ca_active;
		packet();
		assert(cls.netchan.last_received==realtime && cls.state==incoming_state);
	}
	assert(activated==2);
	puts("Q3 packets: timeout refreshed during loading and gameplay, activation transitions preserved");
	return 0;
}
