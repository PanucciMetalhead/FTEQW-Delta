// Data-free tests of the production C boundary and real Jolt simulation.
#include "plugins/jolt/jolt_api.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <initializer_list>
#ifdef FTE_TEST_ODE
#include <ode/ode.h>
#endif
static void require(bool ok,const char *message) {if (!ok) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static fte_jolt_desc box(float z,int motion=FTE_JOLT_DYNAMIC) {
    fte_jolt_desc d{};d.shape=FTE_JOLT_BOX;d.motion=motion;
    d.size[0]=d.size[1]=d.size[2]=32;d.mass=2;d.friction=.5f;d.gravity_scale=1;
    d.solid_mask=d.hit_mask=1;d.state.position[2]=z;d.state.rotation[3]=1;return d;
}
static fte_jolt_world *world(unsigned max=256) {auto *w=fte_jolt_create(max,120,8);require(w,"create world");return w;}
static fte_jolt_body add(fte_jolt_world *w,const fte_jolt_desc &d) {
    auto b=fte_jolt_add(w,&d);if (!b) std::fprintf(stderr,"%s\n",fte_jolt_error(w));require(b!=0,"create body");return b;
}
static fte_jolt_state state(fte_jolt_world *w,fte_jolt_body b) {fte_jolt_state s{};require(fte_jolt_get(w,b,&s)!=0,"get state");return s;}
static void advance(fte_jolt_world *w,int frames,double dt=1.0/60,double gravity=800) {
    for (int i=0;i<frames;i++) require(fte_jolt_step(w,dt,gravity)!=0,fte_jolt_error(w));
}
static void floor(fte_jolt_world *w) {auto d=box(-16,FTE_JOLT_STATIC);d.size[0]=d.size[1]=4096;d.entity=1;add(w,d);}
static double falling(int frames,double dt) {
    auto *w=world();auto b=add(w,box(1024));advance(w,frames,dt);double z=state(w,b).position[2];fte_jolt_destroy(w);return z;
}
static void primitives() {
    auto *w=world();floor(w);
    for (int shape=FTE_JOLT_BOX;shape<=FTE_JOLT_CYLINDER_Z;shape++) {
        auto d=box(128);d.shape=shape;d.state.position[0]=float(shape*96);d.entity=10+shape;
        if (shape>=FTE_JOLT_CAPSULE_X) {
            d.size[0]=d.size[1]=d.size[2]=24;
            int axis=(shape-(shape<=FTE_JOLT_CAPSULE_Z?FTE_JOLT_CAPSULE_X:FTE_JOLT_CYLINDER_X));d.size[axis]=48;
        }
        auto b=add(w,d);advance(w,180);auto s=state(w,b);
        require(std::isfinite(s.position[2]) && s.position[2]>8 && s.position[2]<27,"primitive lands on floor");
        require(std::fabs(s.velocity[2])<2,"primitive settles");
    }
    bool contact=false;
    for (size_t i=0;i<fte_jolt_contacts(w);i++) {fte_jolt_contact c{};require(fte_jolt_get_contact(w,i,&c)!=0,"read deferred contact");contact=true;}
    (void)contact; // Sleeping bodies need not generate another persisted contact.
    fte_jolt_destroy(w);
}
static void mesh_and_ccd() {
    auto *w=world();fte_jolt_desc d=box(0,FTE_JOLT_STATIC);d.shape=FTE_JOLT_MESH;
    const float vertices[]={-512,-512,0,512,-512,0,512,512,0,-512,512,0};
    const int indices[]={0,1,2,0,2,3};d.vertices=vertices;d.indices=indices;d.vertex_count=4;d.triangle_count=2;d.entity=1;
    add(w,d);auto b=add(w,box(100));advance(w,120);require(std::fabs(state(w,b).position[2]-16)<1,"static triangle mesh collision");
    auto wall=box(64,FTE_JOLT_STATIC);wall.size[0]=1;wall.size[1]=512;wall.size[2]=128;wall.state.position[0]=64;add(w,wall);
    auto projectile=box(64);projectile.shape=FTE_JOLT_SPHERE;projectile.size[0]=projectile.size[1]=projectile.size[2]=8;
    projectile.state.position[1]=100;projectile.state.velocity[0]=10000;projectile.gravity_scale=0;
    auto shot=add(w,projectile);advance(w,3);require(state(w,shot).position[0]<64,"CCD stops thin-wall tunnelling");
    fte_jolt_destroy(w);
}
static void lifecycle_forces_and_filters() {
    auto *w=world();auto b=add(w,box(1024));auto s=state(w,b);float f[]={240,0,0},impulse[]={64,0,0},torque[]={20,0,0};
    require(fte_jolt_force(w,b,f,s.position)!=0,"queue force");advance(w,1,1.0/60,0);s=state(w,b);require(std::fabs(s.velocity[0]-2)<.001,"force units and mass across substeps");
    require(fte_jolt_impulse(w,b,impulse)!=0,"impulse");require(std::fabs(state(w,b).velocity[0]-34)<.001,"impulse units and mass");
    require(fte_jolt_torque(w,b,torque)!=0,"torque");advance(w,1,1.0/120,0);require(std::fabs(state(w,b).spin[0]-20.0/(2.0*32*32/6)/120)<1e-6,"torque units and inertia produce expected rotation");
    require(fte_jolt_enable(w,b,0)!=0,"sleep body");auto asleep=state(w,b);advance(w,10);require(state(w,b).position[2]==asleep.position[2],"disabled body stays asleep");
    require(fte_jolt_enable(w,b,1)!=0,"wake body");advance(w,10);require(state(w,b).position[2]<asleep.position[2],"awake body falls");
    require(fte_jolt_remove(w,b)!=0 && !fte_jolt_get(w,b,&s),"stale handle rejected");
    auto other=add(w,box(1024));require(other!=b && !fte_jolt_remove(w,b),"new body cannot alias stale handle");
    auto *second=world();require(!fte_jolt_get(second,other,&s),"cross-world handle rejected");fte_jolt_destroy(second);fte_jolt_destroy(w);
    for (int mode=0;mode<2;mode++) {
        w=world();floor(w);auto d=box(128);d.entity=2;if (!mode) d.owner=1;else d.hit_mask=2;
        b=add(w,d);advance(w,90);require(state(w,b).position[2]<-100,"owner/dimension pair filter excludes collision");fte_jolt_destroy(w);
    }
    w=world();floor(w);auto d=box(40);d.entity=2;b=add(w,d);bool found=false;
    for (int i=0;i<60;i++) {advance(w,1);for (size_t j=0;j<fte_jolt_contacts(w);j++) {
        fte_jolt_contact c{};require(fte_jolt_get_contact(w,j,&c)!=0,"contact read");require(std::fabs(c.normal[2])>.9,"contact normal Z-up");found=true;
    }}require(found,"contacts delivered after simulation");fte_jolt_destroy(w);
}
static void validation_and_timestep() {
    require(!fte_jolt_create(0,120,8) && !fte_jolt_create(1,0,8),"invalid world limits");
    auto *w=world(1);auto d=box(100);auto b=add(w,d);
    require(!fte_jolt_add(w,&d),"body capacity enforced");require(fte_jolt_remove(w,b)!=0,"remove at capacity");
    d.mass=-1;require(!fte_jolt_add(w,&d),"negative mass rejected");d=box(100);d.size[0]=0;require(!fte_jolt_add(w,&d),"zero dimensions rejected");
    d=box(100);d.state.position[0]=std::numeric_limits<float>::quiet_NaN();require(!fte_jolt_add(w,&d),"nonfinite input rejected");
    require(!fte_jolt_step(w,-1,800) && !fte_jolt_step(w,1,std::numeric_limits<double>::infinity()),"invalid timestep/gravity rejected");
    require(fte_jolt_step(w,1,800)!=0,"long frame bounded");fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);
    require(stats.steps==8 && stats.dropped_time>.9,"catch-up work bounded and dropped time reported");fte_jolt_destroy(w);
    require(std::fabs(falling(30,1.0/60)-924)<2,"freefall matches analytic gravity in Quake units");
    require(std::fabs(falling(60,1.0/60)-falling(144,1.0/144))<.001,"fixed simulation independent of render frequency");
    float f[]={0,1,0},l[]={-1,0,0},u[]={0,0,1},q[4],a[3],c[3],e[3];require(fte_jolt_quat(f,l,u,q)!=0,"basis quaternion");fte_jolt_axes(q,a,c,e);
    require(std::fabs(a[1]-1)<1e-5 && std::fabs(c[0]+1)<1e-5 && std::fabs(e[2]-1)<1e-5,"orientation round trip");
    for (int i=0;i<32;i++) {w=world();add(w,box(1024));advance(w,2);fte_jolt_destroy(w);}
}
static void contacts_and_materials() {
    auto *w=world();floor(w);
    fte_jolt_body stack[4];
    for (int i=0;i<4;i++) {auto d=box(20+i*34);d.entity=10+i;stack[i]=add(w,d);}
    advance(w,240);
    for (int i=0;i<4;i++) require(std::fabs(state(w,stack[i]).position[2]-(16+i*32))<2,"body-body stack settles without penetration");
    require(fte_jolt_filter(w,stack[3],0,2,2)!=0,"change dimension mask after contacts are cached");
    advance(w,120);require(state(w,stack[3]).position[2]<-100,"filter changes invalidate cached contacts");
    fte_jolt_destroy(w);
    w=world();floor(w);auto d=box(128);d.shape=FTE_JOLT_SPHERE;d.restitution=1;
    auto b=add(w,d);bool hit=false,rebound=false;
    for (int i=0;i<90;i++) {advance(w,1);auto s=state(w,b);if(s.position[2]<20)hit=true;if(hit&&s.position[2]>80&&s.velocity[2]>0)rebound=true;}
    require(rebound,"restitution produces a rebound");fte_jolt_destroy(w);
    w=world();floor(w);d=box(16);d.friction=1;d.state.velocity[0]=128;b=add(w,d);advance(w,120);
    require(std::fabs(state(w,b).velocity[0])<2,"friction arrests sliding on the floor");fte_jolt_destroy(w);
    w=world();d=box(1024);b=add(w,d);float force[]={240,0,0},point[]={0,0,1024};
    require(fte_jolt_force(w,b,force,point)!=0,"queue force before a partial step");advance(w,1,.001,0);
    require(state(w,b).velocity[0]==0,"partial timestep does not advance simulation");advance(w,1,1.0/120,0);
    require(std::fabs(state(w,b).velocity[0]-1)<.001,"queued force survives a partial timestep");fte_jolt_destroy(w);
    w=world();d=box(0,FTE_JOLT_STATIC);d.shape=FTE_JOLT_MESH;
    const float vertices[]={0,0,0,32,0,0,0,32,0};const int invalid[]={0,1,3},valid[]={0,1,2};
    d.vertices=vertices;d.vertex_count=3;d.indices=invalid;d.triangle_count=1;
    require(!fte_jolt_add(w,&d),"out-of-bounds mesh index rejected");
    d.indices=valid;d.motion=FTE_JOLT_DYNAMIC;require(!fte_jolt_add(w,&d),"dynamic triangle mesh rejected explicitly");
    fte_jolt_destroy(w);
}
static void constraints() {
    auto *w=world();auto b=add(w,box(128));fte_jolt_joint_desc j{};
    j.type=FTE_JOLT_FIXED;j.body2=b;
    auto fixed=fte_jolt_joint_add(w,&j);require(fixed!=0,"fixed joint to world");
    advance(w,120);require(std::fabs(state(w,b).position[2]-128)<.01,"fixed world joint resists gravity");
    fte_jolt_state frame{};require(fte_jolt_joint_frame(w,fixed,&frame)!=0&&std::isfinite(frame.position[2]),"constraint frame query");
    require(fte_jolt_joint_enable(w,fixed,0)!=0,"disable fixed joint");advance(w,30);
    require(state(w,b).position[2]<100,"disabled joint releases body");
    require(fte_jolt_joint_remove(w,fixed)!=0&&!fte_jolt_joint_enable(w,fixed,1),"stale joint handle rejected");
    fte_jolt_destroy(w);
    w=world();b=add(w,box(128));j={};j.type=FTE_JOLT_POINT;j.body2=b;j.anchor[2]=256;
    auto point=fte_jolt_joint_add(w,&j);require(point!=0,"point joint to world");float kick[]={128,0,0};fte_jolt_impulse(w,b,kick);
    advance(w,120);auto s=state(w,b);
    require(std::fabs(std::sqrt(s.position[0]*s.position[0]+s.position[1]*s.position[1]+(s.position[2]-256)*(s.position[2]-256))-128)<1,"point joint preserves pendulum radius");
    fte_jolt_destroy(w);
    w=world();b=add(w,box(128));j={};j.type=FTE_JOLT_HINGE;j.body2=b;j.anchor[2]=128;j.axis[2]=1;
    j.lower=-.5f;j.upper=.5f;j.max_force=100000;j.velocity=3;
    auto hinge=fte_jolt_joint_add(w,&j);require(hinge!=0,"limited motorized hinge");advance(w,120,1.0/60,0);s=state(w,b);
    float angle=2*std::atan2(s.rotation[2],s.rotation[3]);
    require(std::fabs(angle)>.2&&std::fabs(angle)<.55,"hinge motor respects angular limit");
    require(std::fabs(s.rotation[0])<.001&&std::fabs(s.rotation[1])<.001,"hinge locks other rotational axes");
    require(fte_jolt_remove(w,b)!=0&&!fte_jolt_joint_frame(w,hinge,&frame),"body removal removes dependent constraint");
    fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);require(!stats.joints,"removed body's joints leave no records");fte_jolt_destroy(w);
    w=world();b=add(w,box(128));j={};j.type=FTE_JOLT_SLIDER;j.body2=b;j.axis[0]=1;j.lower=-32;j.upper=32;j.max_force=10000;j.velocity=64;
    auto slider=fte_jolt_joint_add(w,&j);require(slider!=0,"limited motorized slider");advance(w,120,1.0/60,0);s=state(w,b);
    require(std::fabs(s.position[0])>20&&std::fabs(s.position[0])<33,"slider motor respects translation limit in Quake units");
    require(std::fabs(s.position[1])<.01&&std::fabs(s.position[2]-128)<.01,"slider locks transverse movement");
    auto *other=world();require(!fte_jolt_joint_enable(other,slider,1),"cross-world joint handle rejected");fte_jolt_destroy(other);
    j.body1=b;require(!fte_jolt_joint_add(w,&j),"self joint rejected");j.body1=0;j.axis[0]=0;require(!fte_jolt_joint_add(w,&j),"zero joint axis rejected");
    j.axis[0]=1;j.lower=1;require(!fte_jolt_joint_add(w,&j),"joint limits excluding zero rejected");fte_jolt_destroy(w);
    w=world();auto a=add(w,box(128));b=add(w,box(128));j={};j.type=FTE_JOLT_FIXED;j.body1=a;j.body2=b;
    fixed=fte_jolt_joint_add(w,&j);require(fixed!=0,"connected bodies joint");advance(w,2,1.0/60,0);
    require(!fte_jolt_contacts(w),"connected overlapping bodies do not collide");fte_jolt_joint_enable(w,fixed,0);advance(w,2,1.0/60,0);
    require(!fte_jolt_contacts(w),"disabled joint retains connected collision exclusion");fte_jolt_joint_remove(w,fixed);advance(w,1,1.0/60,0);
    require(fte_jolt_contacts(w)>0,"removing last joint restores body pair collision");fte_jolt_destroy(w);
    for(int n=0;n<12;n++) {
        w=world();a=0;
        for(int i=0;i<8;i++) {b=add(w,box(1024-i*40));j={};j.type=FTE_JOLT_POINT;j.body1=a;j.body2=b;j.anchor[2]=1044-i*40;require(fte_jolt_joint_add(w,&j)!=0,"multi-body rig joint");a=b;}
        advance(w,2);fte_jolt_destroy(w);
    }
    std::puts("JOLT_CONSTRAINTS_OK: world/body joints, limits, motors, collision exclusion, frames and teardown");
}
// For initial axes (Y, X), q = Ry(first) * Rx(second). Compute the actual
// joint coordinates independently of the production adapter and Jolt helpers.
static void dual_angles(const fte_jolt_state &s,float &first,float &second) {
    float n=std::hypot(s.rotation[0],s.rotation[3]);
    require(n>.001f,"two-axis pose stays away from singular swing");
    float x=s.rotation[0]/n,w=s.rotation[3]/n;
    first=2*std::atan2(s.rotation[1]*w-s.rotation[2]*x,n);
    second=2*std::atan2(x,w);
}
static void dual_axis_constraints() {
    for(int type : {FTE_JOLT_UNIVERSAL,FTE_JOLT_HINGE2}) {
        auto *w=world();auto d=box(128);auto b=add(w,d);
        fte_jolt_joint_desc j{};j.type=type;j.body2=b;j.anchor[2]=128;
        j.axis[1]=1;j.axis2[0]=1;j.lower=-.35f;j.upper=.65f;j.lower2=-.7f;j.upper2=.45f;
        j.max_force=j.max_force2=100000;j.velocity=1.5f;j.velocity2=-2;
        auto joint=fte_jolt_joint_add(w,&j);require(joint!=0,"create two-axis motorized joint");
        for(int frame=0;frame<120;frame++) {
            advance(w,1,1.0/60,0);auto s=state(w,b);float a,c;dual_angles(s,a,c);
            require(a>-.4f&&a<.7f&&c>-.75f&&c<.5f,"both angular limits hold throughout motion");
            float x[3],y[3],z[3];fte_jolt_axes(s.rotation,x,y,z);
            require(std::fabs(x[1])<.015f,"universal axes remain perpendicular");
            require(std::fabs(s.position[2]-128)<.05f,"two-axis joint holds its anchor");
        }
        float a,c;dual_angles(state(w,b),a,c);
        require(a>.5f&&c<-.55f,"independent two-axis motors reach opposite stops");
        require(fte_jolt_joint_enable(w,joint,0)!=0,"disable two-axis joint");advance(w,30);
        require(state(w,b).position[2]<100,"disabled two-axis joint releases body");
        require(fte_jolt_remove(w,b)!=0&&!fte_jolt_joint_enable(w,joint,1),"remove dependent two-axis joint");
        fte_jolt_destroy(w);
    }
    // Soft angular stops are independent, with no hard core limit masking them.
    auto *w=world();auto b=add(w,box(128));fte_jolt_joint_desc j{};
    j.type=FTE_JOLT_UNIVERSAL;j.body2=b;j.anchor[2]=128;j.axis[1]=1;j.axis2[0]=1;
    j.lower=j.lower2=-.25f;j.upper=j.upper2=.25f;
    j.stiffness=j.stiffness2=100000;j.damping=j.damping2=20000;
    j.max_force=j.max_force2=500;j.velocity=1;j.velocity2=-1;
    require(fte_jolt_joint_add(w,&j)!=0,"soft two-axis stops");advance(w,180,1.0/60,0);
    float a,c;dual_angles(state(w,b),a,c);
    require(a>.24f&&a<.35f&&c<-.24f&&c>-.35f,"soft stops on both axes resist motor torque");
    fte_jolt_destroy(w);
    // Suspension is along body1's first axis. Check displacement in Quake units:
    // mass * gravity / stiffness = 2 * 800 / 2000 = 0.8 units.
    w=world();b=add(w,box(128));j={};j.type=FTE_JOLT_HINGE2;j.body2=b;j.anchor[2]=128;
    j.axis[2]=1;j.axis2[0]=1;j.suspension_stiffness=2000;j.suspension_damping=150;
    require(fte_jolt_joint_add(w,&j)!=0,"hinge2 suspension");advance(w,180);
    require(std::fabs(state(w,b).position[2]-127.2f)<.1f,"hinge2 suspension units and equilibrium");
    fte_jolt_destroy(w);
    w=world();b=add(w,box(128));j={};j.type=FTE_JOLT_UNIVERSAL;j.body1=b;
    j.axis[1]=1;j.axis2[0]=1;j.lower=j.lower2=-.5f;j.upper=j.upper2=.5f;
    require(fte_jolt_joint_add(w,&j)!=0,"two-axis joint with second side world-anchored");
    auto bad=j;bad.axis2[0]=0;bad.axis2[1]=1;require(!fte_jolt_joint_add(w,&bad),"parallel axes rejected");
    bad=j;bad.axis2[0]=2;require(!fte_jolt_joint_add(w,&bad),"nonunit second axis rejected");
    bad=j;bad.velocity2=std::numeric_limits<float>::infinity();require(!fte_jolt_joint_add(w,&bad),"nonfinite second-axis settings rejected");
    bad=j;bad.lower2=.1f;require(!fte_jolt_joint_add(w,&bad),"second-axis range must contain reference pose");
    advance(w,5);fte_jolt_destroy(w);
    std::puts("JOLT_DUAL_AXIS_OK: universal/hinge2 frames, two-axis motors/stops, suspension and validation");
}
static fte_jolt_joint spring(fte_jolt_world *w,fte_jolt_body b,float k,float damping) {
    fte_jolt_joint_desc j{};j.type=FTE_JOLT_HINGE2;j.body2=b;j.anchor[2]=128;
    j.axis[2]=1;j.axis2[0]=1;j.suspension_stiffness=k;j.suspension_damping=damping;
    auto token=fte_jolt_joint_add(w,&j);require(token!=0,"create suspension joint");return token;
}
static double spring_energy(float damping) {
    auto *w=world();auto b=add(w,box(128));spring(w,b,2000,damping);
    auto s=state(w,b);s.velocity[2]=32;require(fte_jolt_set(w,b,&s)!=0,"kick suspension");
    advance(w,15,1.0/60,0);s=state(w,b);
    double energy=s.velocity[2]*s.velocity[2]+1000*(s.position[2]-128)*(s.position[2]-128);
    fte_jolt_destroy(w);return energy;
}
static void suspension_controls() {
    auto *w=world();auto b=add(w,box(128));auto joint=spring(w,b,2000,150);
    advance(w,180);require(std::fabs(state(w,b).position[2]-127.2f)<.05f,"spring equilibrium before edits");
    fte_jolt_enable(w,b,0);auto before=state(w,b);
    require(fte_jolt_joint_suspension(w,joint,4000,150)!=0,"edit suspension in place");
    require(state(w,b).position[2]==before.position[2],"spring edit does not teleport body");
    fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);require(stats.active==1,"spring edit wakes its body");
    advance(w,120);require(std::fabs(state(w,b).position[2]-127.6f)<.05f,"stiffer spring preserves original rest anchor");
    require(fte_jolt_joint_suspension(w,joint,4000,60)!=0,"damping-only edit");advance(w,60);
    require(std::fabs(state(w,b).position[2]-127.6f)<.05f,"damping edit preserves static equilibrium");
    require(!fte_jolt_joint_suspension(w,joint,-1,0),"negative stiffness rejected");
    require(!fte_jolt_joint_suspension(w,joint,0,1),"damping without spring rejected");
    require(!fte_jolt_joint_suspension(w,joint,100,-1),"negative damping rejected");
    require(!fte_jolt_joint_suspension(w,joint,std::numeric_limits<float>::infinity(),0),"infinite stiffness rejected");
    require(!fte_jolt_joint_suspension(w,joint,100,std::numeric_limits<float>::quiet_NaN()),"NaN damping rejected");
    advance(w,60);require(std::fabs(state(w,b).position[2]-127.6f)<.05f,"rejected edits retain last valid spring");
    auto *other=world();require(!fte_jolt_joint_suspension(other,joint,2000,150),"cross-world spring handle rejected");fte_jolt_destroy(other);
    fte_jolt_joint_desc wrong{};wrong.type=FTE_JOLT_POINT;wrong.body2=b;wrong.anchor[2]=128;
    auto point=fte_jolt_joint_add(w,&wrong);require(point!=0,"point joint for type validation");
    require(!fte_jolt_joint_suspension(w,point,2000,150),"suspension update rejects point joint");fte_jolt_joint_remove(w,point);
    wrong.suspension_stiffness=2000;require(!fte_jolt_joint_add(w,&wrong),"suspension creation rejects non-hinge2 joint");
    require(fte_jolt_joint_suspension(w,joint,0,0)!=0,"restore rigid anchor");advance(w,60);
    require(std::fabs(state(w,b).position[2]-128)<.01f,"rigid edit restores the original rest position");
    fte_jolt_joint_enable(w,joint,0);require(fte_jolt_joint_suspension(w,joint,2000,150)!=0,"edit disabled spring");advance(w,30);
    require(state(w,b).position[2]<100,"editing a disabled spring does not enable it");
    fte_jolt_remove(w,b);require(!fte_jolt_joint_suspension(w,joint,2000,150),"removed spring handle rejected");fte_jolt_destroy(w);
    // Rotated frame: suspension along X, with a steady force rather than gravity.
    w=world();b=add(w,box(128));fte_jolt_joint_desc j{};j.type=FTE_JOLT_HINGE2;j.body2=b;j.anchor[2]=128;
    j.axis[0]=1;j.axis2[2]=1;j.suspension_stiffness=2000;j.suspension_damping=150;
    joint=fte_jolt_joint_add(w,&j);require(joint!=0,"sideways suspension");
    const float force[]={-1600,0,0};
    for(int i=0;i<180;i++) {auto s=state(w,b);fte_jolt_force(w,b,force,s.position);advance(w,1,1.0/60,0);}
    auto s=state(w,b);require(std::fabs(s.position[0]+.8f)<.05f && std::fabs(s.position[2]-128)<.01f,"suspension follows its axis rather than world Z");
    fte_jolt_destroy(w);
    double undamped=spring_energy(0),damped=spring_energy(150);
    require(undamped>1 && damped<undamped*.1,"damping dissipates suspension energy");
    std::puts("JOLT_SUSPENSION_OK: rest-preserving edits, equilibrium, damping, rotated frames, validation and lifecycle");
}

static fte_jolt_state target(fte_jolt_world *w, fte_jolt_body b, float x,float y,float z,float yaw=0) {
    auto s=state(w,b);s.position[0]=x;s.position[1]=y;s.position[2]=z;
    s.rotation[0]=s.rotation[1]=0;s.rotation[2]=std::sin(yaw/2);s.rotation[3]=std::cos(yaw/2);return s;
}
static void move_to(fte_jolt_world *w,fte_jolt_body b,const fte_jolt_state &s,double dt=1.0/60,double gravity=800) {
    require(fte_jolt_move(w,b,&s)!=0,"queue kinematic target");advance(w,1,dt,gravity);
}
static void kinematic_motion() {
    // Lift a fully sleeping rider, then carry it horizontally using contact friction.
    auto *w=world();auto deck=box(64,FTE_JOLT_KINEMATIC);deck.size[0]=deck.size[1]=192;deck.size[2]=16;deck.friction=1;
    auto lift=add(w,deck);auto rider=box(88);rider.friction=1;auto b=add(w,rider);advance(w,180);
    fte_jolt_enable(w,b,0);
    for(int i=1;i<=60;i++) move_to(w,lift,target(w,lift,0,0,64+i));
    require(std::fabs(state(w,b).position[2]-148)<1,"lift wakes and carries sleeping rider");
    for(int i=1;i<=120;i++) move_to(w,lift,target(w,lift,i*.5f,0,124));
    require(state(w,b).position[0]>50,"horizontal platform carries by friction");
    auto stop=state(w,lift);advance(w,60);auto stopped=state(w,lift);
    require(std::fabs(stopped.position[0]-stop.position[0])<.001 && stopped.velocity[0]==0,"arrived platform does not drift without a new target");
    require(fte_jolt_remove(w,lift)!=0,"remove loaded platform");advance(w,30);
    require(state(w,b).position[2]<120,"rider falls when its platform is removed");fte_jolt_destroy(w);
    // A moving thin wall must hit a prop even when neither endpoint overlaps it.
    w=world();deck=box(64,FTE_JOLT_KINEMATIC);deck.size[0]=1;deck.size[1]=128;deck.size[2]=128;deck.state.position[0]=-80;
    lift=add(w,deck);rider=box(64);rider.size[0]=rider.size[1]=rider.size[2]=8;rider.gravity_scale=0;
    b=add(w,rider);fte_jolt_enable(w,b,0);move_to(w,lift,target(w,lift,80,0,64),1.0/60,0);
    require(state(w,b).position[0]>60,"fast thin pusher sweeps a sleeping small prop");
    fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);
    require(stats.collision_steps>stats.steps && stats.collision_steps<=stats.steps*64,"kinematic sweep sampling is adaptive and bounded");
    require(std::fabs(state(w,lift).position[0]-80)<.001,"fast platform reaches authoritative target");fte_jolt_destroy(w);
    // Rotation creates tangential contact velocity; it cannot be treated as a teleport.
    w=world();deck=box(64,FTE_JOLT_KINEMATIC);deck.size[0]=128;deck.size[1]=8;deck.size[2]=32;
    lift=add(w,deck);rider=box(64);rider.size[0]=rider.size[1]=rider.size[2]=12;rider.state.position[0]=40;rider.state.position[1]=20;rider.gravity_scale=0;
    b=add(w,rider);
    for(int i=1;i<=60;i++) move_to(w,lift,target(w,lift,0,0,64,i*.02f),1.0/60,0);
    require(state(w,b).position[1]>35,"rotating geometry transfers tangential motion");fte_jolt_destroy(w);
    w=world();lift=add(w,deck);rider=box(64);rider.size[0]=rider.size[1]=rider.size[2]=12;
    rider.state.position[0]=40;rider.state.position[1]=40;rider.gravity_scale=0;b=add(w,rider);
    move_to(w,lift,target(w,lift,0,0,64,1.5707963f),1.0/60,0);
    auto crossed=state(w,b);
    require(std::hypot(crossed.position[0]-40,crossed.position[1]-40)>5,"fast rotating bar hits a prop outside both endpoint poses");
    fte_jolt_get_stats(w,&stats);require(stats.collision_steps>stats.steps,"rotation also requests intermediate collision samples");
    fte_jolt_destroy(w);
    // Kinematic triangle soup (a BSP surface) has infinite mass and needs no inertia.
    w=world();deck=box(0,FTE_JOLT_KINEMATIC);deck.shape=FTE_JOLT_MESH;
    const float v[]={-128,-128,0,128,-128,0,128,128,0,-128,128,0};const int t[]={0,1,2,0,2,3};
    deck.vertices=v;deck.vertex_count=4;deck.indices=t;deck.triangle_count=2;
    lift=add(w,deck);require(!fte_jolt_motion(w,lift,FTE_JOLT_DYNAMIC),"mesh cannot become dynamic");
    b=add(w,box(16));advance(w,120);
    for(int i=1;i<=60;i++) move_to(w,lift,target(w,lift,0,0,i*.5f));
    require(std::fabs(state(w,b).position[2]-46)<1,"moving mesh lift carries a prop");
    // Partial calls coalesce targets without losing movement or advancing early.
    auto before=state(w,lift),next=before;next.position[2]+=1;
    require(fte_jolt_move(w,lift,&next)!=0,"partial target");advance(w,1,.001);
    require(state(w,lift).position[2]==before.position[2],"no kinematic movement before fixed tick");
    next.position[2]+=1;require(fte_jolt_move(w,lift,&next)!=0,"latest partial target replaces earlier sample");advance(w,1,1.0/120);
    require(std::fabs(state(w,lift).position[2]-next.position[2])<.001,"pending target survives partial step");
    auto invalid=next;invalid.position[0]=std::numeric_limits<float>::infinity();
    require(!fte_jolt_move(w,lift,&invalid) && !fte_jolt_move(w,b,&next),"invalid target and dynamic body rejected");
    auto *other=world();require(!fte_jolt_move(other,lift,&next),"cross-world target rejected");fte_jolt_destroy(other);
    next.position[2]+=100;require(fte_jolt_move(w,lift,&next)!=0,"queue canceled target");
    require(fte_jolt_set(w,lift,&before)!=0,"explicit teleport cancels target");advance(w,1);
    require(std::fabs(state(w,lift).position[2]-before.position[2])<.001,"canceled target is not replayed");
    fte_jolt_remove(w,lift);require(!fte_jolt_move(w,lift,&next),"removed mover handle rejected");fte_jolt_destroy(w);
    // Identical linear trajectories at different engine rates end at the same pose.
    float endpoints[2];int rates[]={60,144};
    for(int k=0;k<2;k++) {w=world();deck=box(0,FTE_JOLT_KINEMATIC);lift=add(w,deck);
        for(int i=1;i<=rates[k];i++)move_to(w,lift,target(w,lift,120.0f*i/rates[k],0,0),1.0/rates[k],0);
        endpoints[k]=state(w,lift).position[0];fte_jolt_destroy(w);}
    require(std::fabs(endpoints[0]-120)<.001 && std::fabs(endpoints[1]-120)<.001,"60/144 Hz target arrival agrees");
    std::puts("JOLT_KINEMATIC_OK: lift, friction, rotation, mesh, bounded sweep, partial ticks and lifecycle");
}
static std::vector<float> hull_box(float x,float y,float z,float sx,float sy,float sz) {
    std::vector<float> points;
    for(int i=0;i<8;i++) {points.push_back(x+(i&1?sx:-sx)*.5f);points.push_back(y+(i&2?sy:-sy)*.5f);points.push_back(z+(i&4?sz:-sz)*.5f);}
    return points;
}
static std::string hull_text(const std::vector<float> &points) {
    std::string result="hull {\n";
    for(size_t i=0;i<points.size();i+=3)
        result+=std::to_string(points[i])+" "+std::to_string(points[i+1])+" "+std::to_string(points[i+2])+"\n";
    return result+"}\n";
}
static fte_jolt_desc convex_desc(const std::vector<float> &points,float z=128) {
    auto d=box(z);d.shape=FTE_JOLT_CONVEX;d.vertices=points.data();d.vertex_count=points.size()/3;return d;
}
static fte_jolt_desc compound_desc(const std::string &recipe,float z=128) {
    auto d=box(z);d.shape=FTE_JOLT_COMPOUND;d.compound_text=recipe.data();d.compound_length=recipe.size();return d;
}
static void convex_and_compound() {
    auto points=hull_box(0,0,0,32,32,32);
    auto *w=world();floor(w);fte_jolt_body stack[3];
    for(int i=0;i<3;i++)stack[i]=add(w,convex_desc(points,18+34*i));
    advance(w,240);
    for(int i=0;i<3;i++)require(std::fabs(state(w,stack[i]).position[2]-(16+32*i))<2,"convex stack settles");
    fte_jolt_destroy(w);
    // Analytic inertia of a cuboid and a force applied at its offset COM.
    points=hull_box(72,0,0,64,16,32);w=world();auto b=add(w,convex_desc(points));
    auto initial=state(w,b);float force[]={240,0,0},center[]={72,0,128},torque[]={20,0,0};
    require(fte_jolt_force(w,b,force,center)!=0,"force at convex COM");advance(w,1,1.0/120,0);
    auto s=state(w,b);require(std::fabs(s.velocity[0]-1)<.001 && std::fabs(s.spin[2])<1e-6,"hull mass and offset COM use correct units");
    require(fte_jolt_torque(w,b,torque)!=0,"convex torque");advance(w,1,1.0/120,0);s=state(w,b);
    require(std::fabs(s.spin[0]-20.0/(2.0*(16*16+32*32)/12)/120)<1e-5,"convex inertia matches cuboid");
    fte_jolt_destroy(w);
    // Compound inertia combines piece volumes and the parallel-axis theorem.
    std::string recipe="ftejolt 1\n// Original equal-volume pieces\n"+hull_text(hull_box(8,0,0,16,16,16))+hull_text(hull_box(40,0,0,16,16,16));
    float spins[2];
    for(int scale=1;scale<=2;scale++) {
        w=world();auto d=compound_desc(recipe);d.shape_scale=float(scale);b=add(w,d);
        float turn[]={0,0,20};require(fte_jolt_torque(w,b,turn)!=0,"compound torque");advance(w,1,1.0/120,0);s=state(w,b);spins[scale-1]=s.spin[2];
        float forward[3],left[3],up[3];fte_jolt_axes(s.rotation,forward,left,up);
        require(std::fabs(s.position[0]+forward[0]*24*scale-24*scale)<.001 && std::fabs(s.position[1]+forward[1]*24*scale)<.001,"compound rotates about volume-weighted COM without origin jump");
        fte_jolt_destroy(w);
    }
    require(std::fabs(spins[0]-20.0/(2.0*(16*16+16*16)/12+2.0*16*16)/120)<1e-6 && std::fabs(spins[0]-4*spins[1])<1e-6,"compound inertia and scale follow analytic units");
    // A U-frame preserves its opening; one convex hull intentionally fills it.
    auto left=hull_box(-24,0,0,8,16,64),right=hull_box(24,0,0,8,16,64),top=hull_box(0,0,28,56,16,8);
    recipe="ftejolt 1\n"+hull_text(left)+hull_text(right)+hull_text(top);
    points=left;points.insert(points.end(),right.begin(),right.end());points.insert(points.end(),top.begin(),top.end());
    for(int mode=0;mode<3;mode++) {
        w=world();auto frame=mode==0?convex_desc(points):compound_desc(recipe);frame.mass=100;auto body=add(w,frame);
        auto shot=box(128);shot.shape=FTE_JOLT_SPHERE;shot.size[0]=shot.size[1]=shot.size[2]=8;
        shot.state.position[0]=mode==2?24:0;shot.state.position[1]=-100;shot.state.velocity[1]=2000;shot.gravity_scale=0;
        b=add(w,shot);advance(w,10,1.0/120,0);s=state(w,b);
        if(mode==1)require(s.position[1]>50,"CCD projectile passes through compound opening");
        else require(s.position[1]<0,"CCD projectile strikes filled hull or solid compound piece");
        if(mode==2)require(state(w,body).velocity[1]>0,"impact transfers momentum to dynamic compound");
        fte_jolt_destroy(w);
    }
    // Exercise compounds on a moving platform and repeated native shape lifetimes.
    for(int iteration=0;iteration<12;iteration++) {
        w=world();auto deck=box(64,FTE_JOLT_KINEMATIC);deck.size[0]=deck.size[1]=128;deck.size[2]=16;
        auto lift=add(w,deck);b=add(w,compound_desc(recipe,104));advance(w,120);
        for(int i=1;i<=30;i++)move_to(w,lift,target(w,lift,0,0,64+i));
        require(state(w,b).position[2]>130,"lift carries dynamic compound");
        require(fte_jolt_remove(w,b)!=0,"compound removal releases children");fte_jolt_destroy(w);
    }
    // Reject malformed input before any body is registered.
    w=world();auto bad=convex_desc(points);bad.vertex_count=3;require(!fte_jolt_add(w,&bad),"too few hull points rejected");
    bad.vertex_count=4097;require(!fte_jolt_add(w,&bad),"hull point budget enforced before reading");
    auto planar=hull_box(0,0,0,32,32,0);bad=convex_desc(planar);require(!fte_jolt_add(w,&bad),"flat hull rejected");
    for(size_t i=0;i<planar.size();i+=3)planar[i+2]=-planar[i]-planar[i+1];
    require(!fte_jolt_add(w,&bad),"tilted zero-volume hull rejected");
    points[0]=std::numeric_limits<float>::quiet_NaN();bad=convex_desc(points);require(!fte_jolt_add(w,&bad),"nonfinite hull rejected");
    const char *invalid[]={"", "ftejolt 2", "ftejolt 1", "ftejolt 1 hull {}", "ftejolt 1 hull { 0 0", "ftejolt 1 hull { nan 0 0 }", "ftejolt 1 hull { 1,2 0 0 }", "ftejolt 1 box {}"};
    for(auto text:invalid) {std::string input=text;bad=compound_desc(input);require(!fte_jolt_add(w,&bad),"malformed compound rejected");}
    std::string malformed="ftejolt 1\n"+hull_text(left)+"garbage";bad=compound_desc(malformed);require(!fte_jolt_add(w,&bad),"trailing unknown tokens rejected");
    malformed="ftejolt 1\n";for(int i=0;i<65;i++)malformed+=hull_text(left);
    bad=compound_desc(malformed);require(!fte_jolt_add(w,&bad),"compound piece limit enforced");
    bad=compound_desc(recipe);bad.compound_length=262145;require(!fte_jolt_add(w,&bad),"recipe byte limit checked before reading");
    bad=compound_desc(recipe);bad.shape_scale=-1;require(!fte_jolt_add(w,&bad),"negative recipe scale rejected");
    malformed=recipe;malformed[4]=0;bad=compound_desc(malformed);require(!fte_jolt_add(w,&bad),"embedded NUL rejected");
    fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);require(stats.bodies==0,"rejected shape construction leaves no bodies");fte_jolt_destroy(w);
    std::puts("JOLT_CONVEX_OK: hulls, authored compounds, openings, CCD, COM/inertia, scale, stacks, lifts and bounded input");
}

#ifdef FTE_TEST_ODE
static void ode_reference() {
    require(dInitODE2(0)!=0,"initialize ODE reference");auto w=dWorldCreate();dWorldSetGravity(w,0,0,-800);
    auto b=dBodyCreate(w);dMass mass;dMassSetBoxTotal(&mass,2,32,32,32);dBodySetMass(b,&mass);dBodySetPosition(b,0,0,1024);
    for (int i=0;i<60;i++) dWorldQuickStep(w,1.0/120);
    double ode=dBodyGetPosition(b)[2],jolt=falling(30,1.0/60);
    require(std::fabs(ode-jolt)<.1,"ODE/Jolt freefall baseline in engine units");
    std::printf("ODE reference: z=%.6f; Jolt z=%.6f\n",ode,jolt);
    dWorldDestroy(w);dCloseODE();
}
#endif
static void animation_handoff() {
    auto *w=world();
    auto d=box(256,FTE_JOLT_KINEMATIC);d.gravity_scale=0;
    auto driven=add(w,d);
    auto target=state(w,driven);target.position[0]=12;
    target.rotation[2]=std::sin(.05f);target.rotation[3]=std::cos(.05f);
    require(fte_jolt_move(w,driven,&target)!=0,"animated target queued");
    advance(w,1,1.0/60,0);
    auto before=state(w,driven);
    require(fte_jolt_motion(w,driven,FTE_JOLT_DYNAMIC)!=0,"release animated bone");
    auto released=state(w,driven);
    require(std::fabs(released.position[0]-12)<.001 && std::fabs(released.rotation[2]-before.rotation[2])<1e-6,
            "release preserves actual pose and orientation");
    require(std::fabs(released.velocity[0]-720)<.05 && std::fabs(released.spin[2]-6)<.01,
            "release inherits driven linear and angular velocity");
    require(!fte_jolt_move(w,driven,&target),"released body rejects animation targets");
    // Restore a clean pose and verify inertia was calculated for the initially
    // kinematic box, rather than replacing it with an arbitrary unit tensor.
    d.state.position[2]=256;require(fte_jolt_set(w,driven,&d.state)!=0,"reset released pose");
    float impulse[]={64,0,0},torque[]={0,0,20};
    require(fte_jolt_impulse(w,driven,impulse)!=0,"released mass accepts impulse");
    require(std::fabs(state(w,driven).velocity[0]-32)<.001,"initially kinematic body retains authored mass");
    require(fte_jolt_torque(w,driven,torque)!=0,"released inertia accepts torque");advance(w,1,1.0/120,0);
    require(std::fabs(state(w,driven).spin[2]-20.0/(2.0*32*32/6)/120)<1e-6,
            "initially kinematic body retains shape inertia");
    require(fte_jolt_torque(w,driven,torque)!=0,"queue force before handoff");
    require(fte_jolt_motion(w,driven,FTE_JOLT_KINEMATIC)!=0,"take animation control");
    target=state(w,driven);target.position[0]+=5;
    require(fte_jolt_move(w,driven,&target)!=0,"new target after handoff");advance(w,1,1.0/60,0);
    advance(w,1,1.0/120,0); // A genuinely idle advancing tick clears release velocity.
    require(fte_jolt_motion(w,driven,FTE_JOLT_DYNAMIC)!=0,"release stationary animation");
    require(std::fabs(state(w,driven).velocity[0])<.001 && std::fabs(state(w,driven).spin[2])<.001,"stationary release has no old velocity or queued torque");
    require(fte_jolt_motion(w,driven,FTE_JOLT_KINEMATIC)!=0,"take control before partial tick");
    before=state(w,driven);target=before;target.position[0]+=512;
    require(fte_jolt_move(w,driven,&target)!=0,"queue future target");advance(w,1,.001,0);
    require(fte_jolt_motion(w,driven,FTE_JOLT_DYNAMIC)!=0,"release before pending target advances");
    advance(w,1,1.0/120,0);
    require(std::fabs(state(w,driven).position[0]-before.position[0])<.001 &&
            std::fabs(state(w,driven).spin[2])<.001,"release cancels pending pose and stale queued torque");
    auto fixed=add(w,box(100,FTE_JOLT_STATIC));
    require(!fte_jolt_motion(w,fixed,FTE_JOLT_DYNAMIC) && !fte_jolt_motion(w,driven,FTE_JOLT_STATIC) &&
            !fte_jolt_motion(w,driven,99),"unsupported motion changes rejected");
    auto *other=world();require(!fte_jolt_motion(other,driven,FTE_JOLT_KINEMATIC),"cross-world handoff rejected");fte_jolt_destroy(other);
    require(fte_jolt_remove(w,driven)!=0 && !fte_jolt_motion(w,driven,FTE_JOLT_KINEMATIC),"stale handoff rejected");
    // A retained joint must work after repeated mixed / fully driven / passive transitions.
    d=box(256);d.gravity_scale=0;auto a=add(w,d);d.state.position[2]=224;auto b=add(w,d);
    fte_jolt_joint_desc joint{};joint.type=FTE_JOLT_FIXED;joint.body1=a;joint.body2=b;
    auto link=fte_jolt_joint_add(w,&joint);require(link!=0,"mixed joint created");
    for(int cycle=0;cycle<8;cycle++) {
        require(fte_jolt_motion(w,a,FTE_JOLT_KINEMATIC)!=0,"drive parent");
        target=state(w,a);target.position[0]+=8;require(fte_jolt_move(w,a,&target)!=0,"move parent");
        advance(w,1,1.0/30,0);
        require(std::fabs(state(w,b).position[0]-state(w,a).position[0])<1,"dynamic child follows driven parent through joint");
        require(fte_jolt_motion(w,b,FTE_JOLT_KINEMATIC)!=0,"fully driven pair");advance(w,1,1.0/120,0);
        require(fte_jolt_motion(w,a,FTE_JOLT_DYNAMIC)!=0 && fte_jolt_motion(w,b,FTE_JOLT_DYNAMIC)!=0,"release both bodies");
        advance(w,2,1.0/120,0);
    }
    fte_jolt_stats stats{};fte_jolt_get_stats(w,&stats);require(stats.joints==1,"handoffs retain original constraint");
    require(fte_jolt_joint_remove(w,link)!=0,"remove retained constraint");
    fte_jolt_destroy(w);
}
static void offset_mesh_contacts() {
    for(float height : {0.f,992.f}) for(float width : {512.f,1024.f}) for(float x : {0.f,-240.f}) for(float size : {8.f,16.f,32.f}) {
        auto *w=world();auto d=box(height,FTE_JOLT_STATIC);d.shape=FTE_JOLT_MESH;
        const float v[]={-width,-width,-height,-width,width,-height,width,width,-height,width,-width,-height};
        const int ix[]={2,1,0,3,2,0};d.vertices=v;d.vertex_count=4;d.indices=ix;d.triangle_count=2;
        auto mesh=add(w,d);require(std::fabs(state(w,mesh).position[2]-height)<1e-5,"mesh centering preserves caller origin");
        d=box(128);d.size[0]=d.size[1]=d.size[2]=size;d.mass=1;d.state.position[0]=x;
        auto body=add(w,d);advance(w,120,.01);auto result=state(w,body);
        require(std::fabs(result.position[2]-size*.5f)<.8 && std::fabs(result.velocity[2])<1,
                "small convex lands on offset/centered mesh, on and off its diagonal");
        fte_jolt_destroy(w);
    }
    auto *w=world();auto d=box(192,FTE_JOLT_KINEMATIC);d.shape=FTE_JOLT_MESH;
    const float vertices[]={-128,-128,-128,128,-128,-128,128,128,-128,-128,128,-128};
    const int indices[]={0,1,2,0,2,3};d.vertices=vertices;d.vertex_count=4;d.indices=indices;d.triangle_count=2;
    auto lift=add(w,d);d=box(72);d.size[0]=d.size[1]=d.size[2]=16;auto rider=add(w,d);advance(w,120);
    for(int i=1;i<=60;i++) move_to(w,lift,target(w,lift,0,0,192+i*.5f));
    require(std::fabs(state(w,lift).position[2]-222)<.001 && std::fabs(state(w,rider).position[2]-102)<1,
            "offset mesh origin and swept kinematic contacts survive COM centering");
    fte_jolt_destroy(w);
}
int main() {
    offset_mesh_contacts();
    std::printf("Jolt %s production adapter tests\n",fte_jolt_version());
    primitives();mesh_and_ccd();lifecycle_forces_and_filters();validation_and_timestep();contacts_and_materials();constraints();dual_axis_constraints();suspension_controls();kinematic_motion();convex_and_compound();animation_handoff();
#ifdef FTE_TEST_ODE
    ode_reference();
#endif
    std::puts("JOLT_BACKEND_OK: shapes, mesh/CCD, units, forces, sleep, filters, lifecycle and timestep checks passed");
}
