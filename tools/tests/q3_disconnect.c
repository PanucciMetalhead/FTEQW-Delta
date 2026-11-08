#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;
enum { false, true, ca_disconnected, ca_demostart, ca_connected, ca_onserver, ca_active, NA_INVALID = 0, Q3TEXTCMD_BACKUP = 64 };
struct ftenet_connections_s { int unused; };
static struct {
	int state, numClientCommands, lastClientCommandNum;
	struct { struct { int type; } remote_address; } netchan;
} ccs;
static int commands, packets;
static void CG_Stop(void) { assert(ccs.state == ca_disconnected); }
static void CLQ3_SendClientCommand(const char *command)
{
	assert(!strcmp(command, "disconnect") && ccs.state == ca_disconnected);
	commands++;
}
static void CLQ3_SendCmd(struct ftenet_connections_s *socket, void *cmd, unsigned sequence, double time)
{
	assert(socket && !cmd && !sequence && !time && commands);
	packets++;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
	struct ftenet_connections_s socket = {0};
	ccs.netchan.remote_address.type = 1;
	ccs.state = ca_active;
	CLQ3_Disconnect(&socket);
	assert(commands == 1 && packets == 3 && ccs.state == ca_disconnected);
	CLQ3_Disconnect(&socket);
	assert(commands == 1 && packets == 3); // cleanup is idempotent
	ccs.state = ca_connected;
	CLQ3_Disconnect(&socket); // cgame load failed before activation
	assert(commands == 2 && packets == 6);
	ccs.state = ca_active;
	CLQ3_Disconnect(NULL); // demos never transmit
	ccs.state = ca_demostart;
	CLQ3_Disconnect(&socket);
	ccs.state = ca_active;
	ccs.numClientCommands = Q3TEXTCMD_BACKUP - 1;
	CLQ3_Disconnect(&socket); // no recursive reliable overflow on error cleanup
	assert(commands == 2 && packets == 6);
	puts("Q3 disconnect: reliable close, failed loading, demos and overflow cleanup passed");
	return 0;
}
