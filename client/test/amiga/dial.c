/* throwaway spike: dial the game server through fujinet-nio-lib on KS1.3 */
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdint.h>
#include <string.h>
#include "fujinet-nio.h"

static void say(const char *s){ Write(Output(),(APTR)s,strlen(s)); }
static void hex(uint8_t b){ static const char d[]="0123456789ABCDEF"; char t[3]={d[b>>4],d[b&15],0}; say(t); }
static void num(unsigned v){ char t[12]; int i=11; t[i]=0; do{t[--i]='0'+v%10;v/=10;}while(v); say(t+i); }

int main(void){
  fn_handle_t h; uint8_t rc, buf[128], flags; uint16_t n, w; uint32_t off=0; int i, polls;
  uint8_t hello[3]={'I',3,0};
  say("dial: fn_init\n");
  rc=fn_init(); say("init rc="); num(rc); say("\n");
  rc=fn_tcp_open(&h,"127.0.0.1",6599); say("tcp_open rc="); num(rc); say("\n");
  if(rc) return 10;
  rc=fn_write(h,0,hello,3,&w); say("write rc="); num(rc); say(" w="); num(w); say("\n");
  for(polls=0; polls<40; polls++){
    n=0; flags=0;
    rc=fn_read(h,off,buf,sizeof buf,&n,&flags);
    if(rc==FN_ERR_NOT_READY || (rc==FN_OK && n==0)){ Delay(10); continue; }
    say("read rc="); num(rc); say(" n="); num(n); say(" flags="); num(flags); say(" :");
    for(i=0;i<n && i<24;i++){ say(" "); hex(buf[i]); }
    say("\n"); off+=n; if(rc) break;
  }
  fn_close(h); say("dial: done\n"); return 0; }
