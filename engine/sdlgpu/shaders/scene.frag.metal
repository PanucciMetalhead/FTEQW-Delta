#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; float2 lmuv; float4 color; float3 worldpos; float3 normal; float3 tangent; float3 bitangent; };
struct Fragment { float4 eye,fog,effects,material,ambient,diffuse,lightdir,upper,lower,settings; float4 clipplane; float4x4 screenmatrix; float4 lightpos[32],lightcolor[32],lightshadow[32],lightparams[32]; float4x4 lightmatrix; float4 lightdepth,lightgrid; };
struct Light { float4 position,color,shadow,params; };
fragment float4 main0(Input v [[stage_in]],constant Fragment &u [[buffer(0)]],
    texture2d<float> base [[texture(0)]],sampler bs [[sampler(0)]],
    texture2d<float> normalmap [[texture(1)]],sampler ns [[sampler(1)]],
    texture2d<float> specular [[texture(2)]],sampler ss [[sampler(2)]],
    texture2d<float> glow [[texture(3)]],sampler gs [[sampler(3)]],
    texture2d<float> lightmap [[texture(4)]],sampler ls [[sampler(4)]],
    texture2d<float> uppermap [[texture(5)]],sampler us [[sampler(5)]],
    texture2d<float> lowermap [[texture(6)]],sampler los [[sampler(6)]],
    depthcube_array<float> shadows [[texture(7)]],sampler shs [[sampler(7)]],
    texture2d<float> deluxe [[texture(8)]],sampler ds [[sampler(8)]],
    texture2d<float> reflection [[texture(9)]],sampler rs [[sampler(9)]],
    texture2d<float> refraction [[texture(10)]],sampler rfs [[sampler(10)]],
    texturecube<float> skybox [[texture(11)]],sampler sks [[sampler(11)]],
    const device Light *lightdata [[buffer(1)]], const device uint2 *cells [[buffer(2)]],
    const device uint *lightindices [[buffer(3)]]) {
    if(u.settings.y>0 && dot(v.worldpos,u.clipplane.xyz)<u.clipplane.w) discard_fragment();
    float3 n=normalize(v.normal),t=normalize(v.tangent),b=normalize(v.bitangent);
    float3 viewdir=normalize(u.eye.xyz-v.worldpos); float2 tc=v.uv;
    if(u.material.x>0 && u.material.x<3 && u.effects.z!=0) {
        float3 tsview=float3(dot(viewdir,t),dot(viewdir,b),dot(viewdir,n));
        tc+=(normalmap.sample(ns,tc).a-0.5f)*u.effects.z*tsview.xy/max(abs(tsview.z),0.1f);
    }
    if(u.material.x==3 || u.material.x==5) tc+=sin(tc.yx+u.eye.w)*0.125f;
    if(u.material.x==6) {
        float3 sky=skybox.sample(sks,v.worldpos-u.eye.xyz).rgb;
        float amount=1-exp(-pow(length(u.eye.xyz-v.worldpos)*max(u.fog.w,0.0f),2.0f));
        return float4(mix(sky,u.fog.rgb,clamp(amount,0.0f,1.0f)),1);
    }
    if(u.material.x==4) {
        float3 direction=v.worldpos-u.eye.xyz; direction.z*=3;
        float2 skyuv=direction.xy/max(0.5f*length(direction),0.00001f);
        float3 sky=base.sample(bs,skyuv+u.eye.w*0.03125f).rgb;
        float4 clouds=glow.sample(gs,skyuv+u.eye.w*0.0625f);
        return float4(mix(sky,clouds.rgb,clouds.a),1);
    }
    float4 texel=base.sample(bs,tc);
    if(u.material.z==6 && texel.a<u.upper.a) discard_fragment();
    float4 albedo=texel*v.color;
    if((u.material.z==1 && albedo.a<=0) || (u.material.z==2 && albedo.a>=0.5f) || (u.material.z==3 && albedo.a<0.5f)) discard_fragment();
    if((u.material.z==4 && albedo.a<u.upper.w) || (u.material.z==5 && albedo.a>=u.upper.w)) discard_fragment();
    if(u.material.z==4 || u.material.z==5) albedo.a=1;
    float3 result=albedo.rgb;
    if(u.material.x==5) {
        float4 projected=u.screenmatrix*float4(v.worldpos,1);
        float2 screen=projected.xy/projected.w*0.5f+0.5f;
        screen.y=1-screen.y;
        float2 distortion=sin(v.uv.yx*3+u.eye.w)*0.008f;
        float3 transmitted=albedo.rgb;
        if(u.settings.w>0) transmitted=refraction.sample(rfs,clamp(screen+distortion,0.001f,0.999f)).rgb;
        // Refraction keeps the liquid tint and the original grazing-angle surface.
        transmitted*=float3(0.7f,0.8f,0.7f);
        float surface=max(u.effects.x,pow(1-abs(dot(n,viewdir)),4.0f));
        result=mix(transmitted,albedo.rgb,surface);
        if(u.settings.z>0) {
            float fresnel=u.settings.w>0 ? 0.05f+0.95f*pow(1-abs(dot(n,viewdir)),4.0f) : 1;
            result=mix(result,reflection.sample(rs,clamp(screen+distortion,0.001f,0.999f)).rgb,fresnel);
        }
        albedo.a=1;
    }
    if(u.material.x>0 && u.material.x<3) {
        albedo.rgb+=(uppermap.sample(us,tc).rgb*u.upper.rgb+lowermap.sample(los,tc).rgb*u.lower.rgb)*v.color.rgb;
        if(u.effects.x>0) { float3 mapped=normalmap.sample(ns,tc).xyz*2-1; n=normalize(t*mapped.x+b*mapped.y+n*mapped.z); }
        float3 lighting=float3(1);
        if(u.material.x==1) lighting=lightmap.sample(ls,v.lmuv).rgb*u.material.y;
        else lighting=u.ambient.rgb+u.diffuse.rgb*max(0.0f,dot(n,normalize(u.lightdir.xyz+float3(0.00001f))));
        float4 spec=specular.sample(ss,tc);
        float3 staticshine=float3(0);
        if(u.material.x==1 && u.settings.x>0) {
            float3 direction=normalize(deluxe.sample(ds,v.lmuv).rgb*2-1);
            if(u.settings.x==1) direction=normalize(t*direction.x+b*direction.y+normalize(v.normal)*direction.z);
            float baseline=max(0.25f,dot(normalize(v.normal),direction));
            float lambert=max(0.0f,dot(n,direction));
            float shine=pow(max(0.0f,dot(n,normalize(direction+viewdir))),max(1.0f,u.effects.w*max(spec.a,0.01f)))*u.effects.y;
            staticshine=spec.rgb*shine*lighting;
            lighting*=lambert/baseline;
        }
        result=albedo.rgb*lighting+staticshine;
        uint lightcount=uint(u.material.w), offset=0;
        bool indexed=false;
        if(u.lightgrid.x==1) {
            float4 clip=u.lightmatrix*float4(v.worldpos,1);
            float depth=dot(u.lightdepth,float4(v.worldpos,1));
            float2 grid=(clip.xy/clip.w*0.5+0.5)*float2(16,9);
            if(clip.w>0 && depth>=u.lightgrid.y && depth<u.lightgrid.z &&
               all(grid>=float2(0)) && all(grid<float2(16,9))) {
                uint z=min(23u,uint(log(depth/u.lightgrid.y)*u.lightgrid.w));
                uint2 cell=cells[(z*9u+uint(grid.y))*16u+uint(grid.x)];
                if(cell.y!=0xffffffffu) { offset=cell.x; lightcount=cell.y; indexed=true; }
            }
        }
        for(uint j=0u;j<lightcount;j++) {
            uint i=indexed ? lightindices[offset+j] : j;
            float4 lp,lc,ls,lt;
            if(u.lightgrid.x>0) {
                Light l=lightdata[i]; lp=l.position; lc=l.color; ls=l.shadow; lt=l.params;
            } else { lp=u.lightpos[i]; lc=u.lightcolor[i]; ls=u.lightshadow[i]; lt=u.lightparams[i]; }
            float3 delta=lp.xyz-v.worldpos; float distance=length(delta);
            float attenuation=max(0.0f,1-distance/lp.w); float3 direction=delta/max(distance,0.00001f);
            float lambert=max(0.0f,dot(n,direction));
            float shine=pow(max(0.0f,dot(n,normalize(direction+viewdir))),max(1.0f,u.effects.w*max(spec.a,0.01f)))*u.effects.y;
            float visibility=1;
            if(lc.w>0) {
                float4 p=ls;
                float depth=max(max(abs(delta.x),abs(delta.y)),abs(delta.z));
                float compare=p.y/(p.y-p.x)-(p.y*p.x)/((p.y-p.x)*max(depth,p.x))-p.z/max(depth,1.0f);
                visibility=shadows.sample_compare(shs,-delta,uint(lc.w-1),compare);
            }
            result+=(albedo.rgb*(lt.x+lambert*lt.y)+spec.rgb*shine*lt.z)*lc.rgb*attenuation*attenuation*visibility;
        }
        result+=glow.sample(gs,tc).rgb;
    }
    float fogamount=1-exp(-pow(length(u.eye.xyz-v.worldpos)*max(u.fog.w,0.0f),2.0f));
    result=mix(result,u.fog.rgb,clamp(fogamount,0.0f,1.0f));
    return float4(result,albedo.a);
}
