// Fault injection against actual engine transport and netchan functions.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#define true 1
#define false 0
#define HAVE_PACKET 1
#define FTENET_ADDRTYPES 2
#define HAVE_IPV6 1
#define SERVERONLY 1
#define INVALID_SOCKET -1
#define MAX_ADR_SIZE 128
#define SZ_RAWBYTES 0
#define NCF_CLIENT 1
#define NCF_FRAGABLE 2
#define NCF_STUNAWARE 4
#define ANTISTUNBIAS 0x40000000
#define OLD_AVG 0.99
#define countof(a) (sizeof(a)/sizeof((a)[0]))
#define NET_EINTR EINTR
#define NET_ENOBUFS ENOBUFS
#define NET_EWOULDBLOCK EWOULDBLOCK
#define NET_EMSGSIZE EMSGSIZE
#define NET_ECONNRESET ECONNRESET
#define NET_ECONNABORTED ECONNABORTED
#define NET_ECONNREFUSED ECONNREFUSED
#define NET_EADDRNOTAVAIL EADDRNOTAVAIL
#define NET_EACCES EACCES
#define NET_ENETUNREACH ENETUNREACH
#define neterrno() errno
#define Con_Printf(...) (void)(prints++)
#define Con_TPrintf(...) (void)(prints++)
#define Con_DPrintf(...) (void)0
#define Q_memset memset
#define BigLong(x) __builtin_bswap32(x)
typedef unsigned char qbyte;
typedef int qboolean;
typedef enum {NETERR_SENT, NETERR_NOROUTE, NETERR_DISCONNECTED, NETERR_MTU, NETERR_CLOGGED} neterr_t;
enum {NA_INVALID, NA_IP, NA_IPV6};
enum {NP_DGRAM, NP_DTLS, NP_KEXLAN};
struct sockaddr_qstorage { char storage[128]; };
typedef struct {int type, prot, port; union {unsigned char ip[4];} address;} netadr_t;
typedef struct {float value; int ival;} cvar_t;
cvar_t showdrop, showpackets;
static int prints, destroys, frees;
static double realtime = 6;
typedef struct {unsigned char *data; int maxsize, cursize, currentbit, packing;} sizebuf_t;
static unsigned char net_message_buffer[4096];
static sizebuf_t net_message = {net_message_buffer,4096,0,0,0};
static netadr_t net_from = {NA_IP, NP_DGRAM, 1};
typedef struct dtlsfuncs_s {void (*DestroyContext)(void *); neterr_t (*Timeouts)(void *);} dtlsfuncs_t;
struct dtlspeer_s {struct dtlspeer_s *next, **link; const dtlsfuncs_t *funcs; void *dtlsstate; double timeout;};
typedef struct {struct dtlspeer_s *dtls;} ftenet_connections_t;
typedef struct {int thesocket, addrtype[2], islisten;} ftenet_generic_connection_t;
static ssize_t io_result;
static int io_error;
static ssize_t test_sendto(int s,const void *data,size_t n,int flags,const struct sockaddr *to,socklen_t len) {errno=io_error; return io_result;}
static ssize_t test_recvfrom(int s,void *data,size_t n,int flags,struct sockaddr *from,int *len) {errno=io_error;return io_result;}
#define sendto test_sendto
#define recvfrom test_recvfrom
static int NetadrToSockadr(netadr_t *a,struct sockaddr_qstorage *s) {return sizeof(*s);}
static void SockadrToNetadr(void *s,int n,netadr_t *a) {a->type=NA_IP;}
static char *NET_AdrToString(char *s,int n,netadr_t *a) {return s;}
static unsigned Sys_Milliseconds(void) {return realtime*1000;}
static void Z_Free(void *p) {frees++; free(p);}
static int NET_CompareAdr(netadr_t *a,netadr_t *b) {return a->port==b->port;}
static int NET_AddrIsReliable(netadr_t *a) {return 0;}
static double Sys_DoubleTime(void) {return realtime;}
static int msg_badread, net_drop;
static void MSG_BeginReading(sizebuf_t *b,int prim) {b->currentbit=0;msg_badread=0;}
static int MSG_GetReadCount(void) {return net_message.currentbit>>3;}
static int MSG_ReadLong(void) {int pos=MSG_GetReadCount();net_message.currentbit+=32;if(pos+4>net_message.cursize){msg_badread=1;return -1;}return (uint32_t)net_message.data[pos] | (uint32_t)net_message.data[pos+1]<<8 | (uint32_t)net_message.data[pos+2]<<16 | (uint32_t)net_message.data[pos+3]<<24;}
static int MSG_ReadShort(void) {int pos=MSG_GetReadCount();net_message.currentbit+=16;if(pos+2>net_message.cursize){msg_badread=1;return -1;}return net_message.data[pos] | net_message.data[pos+1]<<8;}
static void MSG_ReadSkip(int n) {net_message.currentbit+=n*8;}
typedef struct {netadr_t remote_address; int flags, qportsize, netprim, bytesin, incoming_sequence, outgoing_sequence, mtu_cur, mtu_probes, incoming_unreliable, in_fragment_length, drop_count, reliable_sequence, reliable_length, incoming_acknowledged, incoming_reliable_acknowledged, incoming_reliable_sequence; unsigned short sentsizes[64]; unsigned char in_fragment_buf[128]; double frame_latency, frame_rate, last_received;} netchan_t;

static unsigned connectgeneration;
static struct {char servername[64];} cls;
enum {CIM_Q2EONLY=2};
static struct {int trying,resolving,mode,numadr,nextadr;netadr_t adr[8];} connectinfo;
struct resolvectx_s {netadr_t adr[8];size_t found;int defaultport;unsigned generation;char servername[1];};
static int aborts;
static void CL_ConnectAbort(const char *fmt,...) {aborts++;connectinfo.trying=0;connectinfo.resolving=0;}
static int entropy_ok=1;
static qboolean Sys_RandomBytes(qbyte *data,int size) {memset(data,0x37,size);return entropy_ok;}

/* ENGINE_FUNCTIONS */
static void destroyed(void *p) {destroys++;}
static neterr_t timed(void *p) {return p?(neterr_t)(uintptr_t)p:NETERR_SENT;}
static const dtlsfuncs_t funcs={destroyed,timed};
static struct dtlspeer_s *peer(ftenet_connections_t *col,double timeout,int err) {struct dtlspeer_s *p=calloc(1,sizeof(*p));p->funcs=&funcs;p->dtlsstate=(void *)(uintptr_t)err;p->timeout=timeout;p->next=col->dtls;p->link=&col->dtls;if(p->next)p->next->link=&p->next;col->dtls=p;return p;}
static void packet(unsigned seq,unsigned ack,int offset,int len) {net_message.cursize=len;memset(net_message_buffer,0,sizeof(net_message_buffer));for(int i=0;i<4;i++){net_message_buffer[i]=seq>>(8*i);net_message_buffer[i+4]=ack>>(8*i);}net_message_buffer[8]=offset;net_message_buffer[9]=offset>>8;}
static struct resolvectx_s *resolved(unsigned generation,int found) {
 struct resolvectx_s *p=calloc(1,sizeof(*p)+32);strcpy(p->servername,"test");p->generation=generation;p->found=found;p->adr[0].port=123;return p;
}
int main(void) {
 srand(9);int first=rand(),second=rand(),third=rand();srand(9);assert(Netchan_GenerateChallenge()==0x37373737);assert(rand()==third);
 entropy_ok=0;srand(9);assert((unsigned)Netchan_GenerateChallenge()==(((unsigned)first<<16)^(unsigned)second));assert(rand()==third);
 strcpy(cls.servername,"test");connectgeneration=2;connectinfo.trying=connectinfo.resolving=1;
 int freed=frees;CL_ResolvedServer(resolved(1,1),0,0,0);assert(frees==freed+1&&connectinfo.resolving&&connectinfo.numadr==0);
 CL_ResolvedServer(resolved(2,1),0,0,0);assert(frees==freed+2&&!connectinfo.resolving&&connectinfo.numadr==1&&connectinfo.adr[0].port==123);
 connectinfo.resolving=1;CL_ResolvedServer(resolved(2,0),0,0,0);assert(aborts==1&&frees==freed+3);
 frees=0;

 assert(CL_ConnectRetryInterval(1,1)==1);assert(CL_ConnectRetryInterval(2,1)==2);assert(CL_ConnectRetryInterval(3,1)==4);assert(CL_ConnectRetryInterval(100,1)==5);assert(CL_ConnectRetryInterval(1,2)==0.25);
 ftenet_generic_connection_t con={1,{NA_IP,NA_IP},0}; netadr_t adr={NA_IP,NP_DGRAM,1};
 int errors[]={EWOULDBLOCK,EINTR,ENOBUFS,EMSGSIZE,ECONNREFUSED,EADDRNOTAVAIL,EACCES,ENETUNREACH,EIO};
 neterr_t results[]={NETERR_CLOGGED,NETERR_CLOGGED,NETERR_CLOGGED,NETERR_MTU,NETERR_DISCONNECTED,NETERR_NOROUTE,NETERR_DISCONNECTED,NETERR_NOROUTE,NETERR_NOROUTE};
 io_result=-1;
 for(unsigned i=0;i<countof(errors);i++){io_error=errors[i];assert(FTENET_Datagram_SendPacket(&con,12,"test",&adr)==results[i]);}
 io_result=12;assert(FTENET_Datagram_SendPacket(&con,12,"test",&adr)==NETERR_SENT);
 io_result=5;assert(FTENET_Datagram_SendPacket(&con,12,"test",&adr)==NETERR_MTU);
 io_result=-1;io_error=EMSGSIZE;prints=0;assert(!FTENET_Datagram_GetPacket(&con));assert(!FTENET_Datagram_GetPacket(&con));assert(prints==1);
 io_error=ECONNRESET;prints=0;assert(!FTENET_Datagram_GetPacket(&con));assert(!FTENET_Datagram_GetPacket(&con));assert(prints==1);
 ftenet_connections_t col={0};struct dtlspeer_s *tail=peer(&col,20,0);peer(&col,1,0);struct dtlspeer_s *head=peer(&col,20,0);
 NET_DTLS_Timeouts(&col);assert(head->next==tail);assert(tail->link==&head->next);assert(frees==1&&destroys==1);
 head->timeout=1;NET_DTLS_Timeouts(&col);assert(col.dtls==tail&&tail->link==&col.dtls);
 tail->dtlsstate=(void *)(uintptr_t)NETERR_DISCONNECTED;NET_DTLS_Timeouts(&col);assert(!col.dtls&&frees==3&&destroys==3);
 netchan_t ch={0};ch.remote_address=net_from;ch.flags=NCF_CLIENT|NCF_FRAGABLE;ch.outgoing_sequence=10;ch.mtu_cur=1200;
 for(int n=0;n<10;n++){packet(1,0,1,n);assert(!Netchan_Process(&ch));assert(ch.incoming_sequence==0&&ch.in_fragment_length==0&&ch.last_received==0);}
 ch.flags=NCF_FRAGABLE;ch.qportsize=2;for(int n=8;n<12;n++){packet(1,0,1,n);assert(!Netchan_Process(&ch));assert(ch.incoming_sequence==0&&ch.in_fragment_length==0);}
 ch.flags=NCF_CLIENT|NCF_FRAGABLE;packet(1,0,1,18);memcpy(net_message.data+10,"abcdefgh",8);assert(!Netchan_Process(&ch));assert(ch.in_fragment_length==8);
 packet(1,0,2,18);memcpy(net_message.data+10,"ijklmnop",8);assert(Netchan_Process(&ch));assert(net_message.cursize==16&&!memcmp(net_message.data,"abcdefghijklmnop",16));
 packet(1,0,0,10);assert(!Netchan_Process(&ch)); // duplicate
 packet(3,0,2,18);assert(!Netchan_Process(&ch)); // missing first fragment
 packet(4,0,0,10);assert(Netchan_Process(&ch)); // recover after loss
 for(int n=0;n<10000;n++){packet(5,0,65535,20);assert(!Netchan_Process(&ch));} // bounded malicious fragments
 puts("Socket faults, DNS cancellation, challenge entropy, DTLS cleanup, retries and fragments passed");
}
