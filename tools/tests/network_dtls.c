// Real GnuTLS DTLS records through the engine's send/receive adapters.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#define true 1
#define false 0
#define Con_DPrintf(...) (void)0
#define safeswitch switch
#define safedefault default
#define qgnutls_dtls_cookie_verify gnutls_dtls_cookie_verify
#define qgnutls_dtls_cookie_send gnutls_dtls_cookie_send
#define qgnutls_dtls_prestate_set gnutls_dtls_prestate_set
static gnutls_datum_t cookie_key;
enum {NA_LOOPBACK,NA_IP,NA_IPV6,NA_IPX,NA_INVALID};
static struct {int type;union {char ip[4],ip6[16],ipx[10];} address;} net_from;
#define CON_ERROR ""
#define CON_WARNING ""
#define VFS_ERROR_EOF -1
#define VFS_ERROR_TRYLATER -2
typedef unsigned char qbyte;
typedef int qboolean;
typedef enum {NETERR_SENT,NETERR_NOROUTE,NETERR_DISCONNECTED,NETERR_MTU,NETERR_CLOGGED} neterr_t;
typedef struct {qbyte *data;int cursize,maxsize;} sizebuf_t;
typedef struct {gnutls_session_t session;int challenging,handshaking,pusherror;neterr_t datagramerror;void *cbctx;neterr_t(*cbpush)(void *,const qbyte *,size_t);qbyte *readdata;size_t readsize;gnutls_dtls_prestate_st prestate;} gnutlsfile_t;
#define qgnutls_transport_set_errno gnutls_transport_set_errno
#define qgnutls_record_send gnutls_record_send
#define qgnutls_record_recv gnutls_record_recv
#define qgnutls_record_discard_queued gnutls_record_discard_queued
#define qgnutls_dtls_get_data_mtu gnutls_dtls_get_data_mtu
#define qgnutls_dtls_get_timeout gnutls_dtls_get_timeout
#define qgnutls_error_is_fatal gnutls_error_is_fatal
static int SSL_DoHandshake(gnutlsfile_t *f) {int r=gnutls_handshake(f->session);if(r<0)return gnutls_error_is_fatal(r)?-1:0;f->handshaking=0;return 1;}
/* ENGINE_FUNCTIONS */
struct datagram {size_t size;qbyte data[65536];};
struct queue {struct datagram data[64];unsigned head,tail;};
struct transport {struct queue *incoming,*outgoing;neterr_t next_error;};
static neterr_t push(void *ctx,const qbyte *p,size_t n) {struct transport *t=ctx;if(t->next_error){int e=t->next_error;t->next_error=0;return e;}assert(t->outgoing->tail-t->outgoing->head<64);struct datagram *d=&t->outgoing->data[t->outgoing->tail++%64];d->size=n;memcpy(d->data,p,n);return NETERR_SENT;}
static ssize_t pull(gnutls_transport_ptr_t ctx,void *p,size_t n) {gnutlsfile_t *f=ctx;struct transport *t=f->cbctx;if(t->incoming->head==t->incoming->tail){gnutls_transport_set_errno(f->session,EAGAIN);return -1;}struct datagram *d=&t->incoming->data[t->incoming->head++%64];if(d->size>n){gnutls_transport_set_errno(f->session,EMSGSIZE);return -1;}memcpy(p,d->data,d->size);return d->size;}
static int pull_timeout(gnutls_transport_ptr_t ctx,unsigned ms) {gnutlsfile_t *f=ctx;struct transport *t=f->cbctx;return t->incoming->head!=t->incoming->tail;}
static int key(gnutls_session_t s,const char *username,gnutls_datum_t *k) {k->size=16;k->data=gnutls_malloc(16);memset(k->data,0x42,16);return strcmp(username,"test")?-1:0;}
int main(void) {
 assert(!gnutls_global_init());static struct queue a,b;struct transport ct={&a,&b,0},st={&b,&a,0};gnutlsfile_t c={0},s={0};
 gnutls_psk_client_credentials_t cc;gnutls_psk_server_credentials_t sc;
 assert(!gnutls_psk_allocate_client_credentials(&cc));assert(!gnutls_psk_allocate_server_credentials(&sc));gnutls_psk_set_server_credentials_function(sc,key);
 unsigned char raw[16];memset(raw,0x42,16);gnutls_datum_t k={raw,16};assert(!gnutls_psk_set_client_credentials(cc,"test",&k,GNUTLS_PSK_KEY_RAW));
 assert(!gnutls_init(&c.session,GNUTLS_CLIENT|GNUTLS_DATAGRAM|GNUTLS_NONBLOCK));assert(!gnutls_init(&s.session,GNUTLS_SERVER|GNUTLS_DATAGRAM|GNUTLS_NONBLOCK));
 c.cbctx=&ct;s.cbctx=&st;c.cbpush=s.cbpush=push;
 for(int i=0;i<2;i++){gnutlsfile_t *f=i?&s:&c;assert(!gnutls_priority_set_direct(f->session,"NORMAL:-KX-ALL:+PSK",NULL));gnutls_credentials_set(f->session,GNUTLS_CRD_PSK,i?(void *)sc:(void *)cc);gnutls_dtls_set_mtu(f->session,1200);gnutls_transport_set_ptr(f->session,f);gnutls_transport_set_push_function(f->session,DTLS_Push);gnutls_transport_set_pull_function(f->session,pull);gnutls_transport_set_pull_timeout_function(f->session,pull_timeout);f->handshaking=1;}
 for(int i=0;i<100&&(c.handshaking||s.handshaking);i++){assert(SSL_DoHandshake(&c)>=0);assert(SSL_DoHandshake(&s)>=0);}assert(!c.handshaking&&!s.handshaking);
 qbyte payload[2048],recv[2048];for(int i=0;i<2048;i++)payload[i]=(i*17)^91;
 unsigned mtu=gnutls_dtls_get_data_mtu(c.session);assert(mtu<1200&&mtu>1000);
 assert(GNUDTLS_Transmit(&c,payload,mtu+1)==NETERR_MTU);
 assert(GNUDTLS_Transmit(&c,payload,mtu)==NETERR_SENT);assert(gnutls_record_recv(s.session,recv,sizeof(recv))==mtu);assert(!memcmp(payload,recv,mtu));
 for(int err=NETERR_NOROUTE;err<=NETERR_CLOGGED;err++){
  if(err==NETERR_DISCONNECTED)continue;
  ct.next_error=err;assert(GNUDTLS_Transmit(&c,payload,200)==err);
  // A later packet must never flush the previous blocked encrypted record.
  assert(GNUDTLS_Transmit(&c,payload+300,100)==NETERR_SENT);assert(gnutls_record_recv(s.session,recv,sizeof(recv))==100);assert(!memcmp(payload+300,recv,100));
 }
 assert(GNUDTLS_Transmit(&c,payload,100)==NETERR_SENT);b.data[(b.tail-1)%64].data[b.data[(b.tail-1)%64].size-1]^=1;
 assert(gnutls_record_recv(s.session,recv,sizeof(recv))<0); // corrupt ciphertext rejected
 assert(GNUDTLS_Transmit(&c,payload+500,100)==NETERR_SENT);assert(gnutls_record_recv(s.session,recv,sizeof(recv))==100);assert(!memcmp(payload+500,recv,100));
 // Actual engine receive adapter with an exactly full output buffer and canary.
 gnutls_transport_set_pull_function(s.session,DTLS_Pull);
 assert(GNUDTLS_Transmit(&c,payload,100)==NETERR_SENT);struct datagram *d=&b.data[b.head++%64];// Input is staged separately.
 // Received stages the encrypted datagram itself, then decrypts in place.
 unsigned char in[2048];memcpy(in,d->data,d->size);sizebuf_t msg={in,(int)d->size,101};
 assert(GNUDTLS_Received(&s,&msg)==NETERR_SENT);assert(msg.cursize==100&&!memcmp(in,payload,100)&&in[100]==0);
 assert(GNUDTLS_Transmit(&c,payload,101)==NETERR_SENT);d=&b.data[b.head++%64];memcpy(in,d->data,d->size);msg.cursize=d->size;unsigned char canary=in[101];assert(GNUDTLS_Received(&s,&msg)==NETERR_CLOGGED);assert(msg.cursize==0&&in[101]==canary);
 assert(GNUDTLS_Transmit(&c,payload,99)==NETERR_SENT);d=&b.data[b.head++%64];memcpy(in,d->data,d->size);msg.cursize=d->size;assert(GNUDTLS_Received(&s,&msg)==NETERR_SENT);assert(msg.cursize==99&&!memcmp(in,payload,99));
 gnutls_deinit(c.session);gnutls_deinit(s.session);gnutls_psk_free_client_credentials(cc);gnutls_psk_free_server_credentials(sc);gnutls_global_deinit();
 puts("Real DTLS MTU, blocked-send fidelity, tampering rejection and receive bounds passed");
}
