// Data-free tests of the production C boundary and real Jolt simulation.
#include "plugins/jolt/jolt_api.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
int main() {
    std::printf("Jolt %s production adapter tests\n",fte_jolt_version());
    primitives();mesh_and_ccd();lifecycle_forces_and_filters();validation_and_timestep();contacts_and_materials();constraints();dual_axis_constraints();suspension_controls();
#ifdef FTE_TEST_ODE
    ode_reference();
#endif
    std::puts("JOLT_BACKEND_OK: shapes, mesh/CCD, units, forces, sleep, filters, lifecycle and timestep checks passed");
}
