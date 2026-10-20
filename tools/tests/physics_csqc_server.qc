// Original SSQC side of the independent-client-world fixture.
#include "declarations.qh"
string(string a,string b,optional string c) strcat=#115;
float(float skel) skel_get_numbones=#265;
entity server_rig;
float server_skel,reported,doneat;
void(entity client,string cmd) clientcommand=#440;
void(float ok,string what) verify = {
    if(!ok) {dprint("CSQC_PHYSICS_FAIL SSQC: ");dprint(what);dprint("\n");error(what);}
};
void() worldspawn = {
    self.dimension_solid=1;self.dimension_hit=1;
    verify(physics_supported()==1,"server backend enabled");
    precache_model("progs/physics-rig.md5mesh");
    server_rig=spawn();setmodel(server_rig,"progs/physics-rig.md5mesh");
    setorigin(server_rig,'0 0 300');
    server_skel=skel_create(server_rig.modelindex);server_rig.skeletonindex=server_skel;
    verify(skel_build(server_skel,server_rig,server_rig.modelindex,0,0,0)>0,"server skeleton builds");
    verify(skel_ragupdate(server_rig,"doll progs/physics-rig.doll",0)==1,"server rig instantiates");
    server_rig.modelindex=0;
    if(cvar("csqc_fixture_shared")) localcmd(strcat("set csqc_fixture_server_skel ",ftos(server_skel),"\n"));
};
void(string cmd) SV_ParseClientCommand = {
    if(cmd=="csqc_fixture_map") {localcmd("map csqc\n");return;}
    if(cmd=="csqc_fixture_done") {doneat=time+1;return;}
    clientcommand(self,cmd);
};
void() StartFrame = {
    if(doneat && time>doneat) localcmd("quit force\n");
    verify(physics_supported()==1,"client operations retain server backend");
    verify(skel_get_numbones(server_skel)==3,"client operations retain server skeleton");
    if(cvar("csqc_fixture_shared") && cvar("csqc_fixture_client_skel")>0) {
        verify(skel_get_numbones(cvar("csqc_fixture_client_skel"))==0,"server cannot borrow client skeleton");
        if(!reported) {reported=1;dprint("SSQC_ISOLATION_OK\n");}
    }
    skel_ragupdate(server_rig,"",0);
};
void() PutClientInServer = {
    self.movetype=8;self.solid=0;self.health=100;
    setorigin(self,'0 -240 160');self.angles='10 90 0';self.fixangle=1;
};
void() info_player_start={};void() main={};
void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() ClientDisconnect={};void() SetNewParms={};void() SetChangeParms={};
