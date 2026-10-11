#version 450
layout(set=2,binding=0) uniform sampler2D base;
layout(set=2,binding=1) uniform sampler2D normalmap;
layout(set=2,binding=2) uniform sampler2D specular;
layout(set=2,binding=3) uniform sampler2D glow;
layout(set=2,binding=4) uniform sampler2D lightmap;
layout(set=2,binding=5) uniform sampler2D uppermap;
layout(set=2,binding=6) uniform sampler2D lowermap;
layout(set=2,binding=7) uniform samplerCubeArrayShadow shadows;
layout(set=2,binding=8) uniform sampler2D deluxe;
layout(set=2,binding=9) uniform sampler2D reflection;
layout(set=2,binding=10) uniform sampler2D refraction;
layout(set=2,binding=11) uniform samplerCube skybox;
layout(set=3,binding=0,std140) uniform Fragment {
    vec4 eye,fog,effects,material,ambient,diffuse,lightdir,upper,lower,settings;
    vec4 clipplane;
    mat4 screenmatrix;
    vec4 lightpos[32],lightcolor[32],lightshadow[32],lightparams[32];
} u;
layout(location=0) in vec2 v_uv;
layout(location=1) in vec2 v_lmuv;
layout(location=2) in vec4 v_color;
layout(location=3) in vec3 v_position;
layout(location=4) in vec3 v_normal;
layout(location=5) in vec3 v_tangent;
layout(location=6) in vec3 v_bitangent;
layout(location=0) out vec4 outputcolor;
void main() {
    if(u.settings.y>0 && dot(v_position,u.clipplane.xyz)<u.clipplane.w) discard;
    vec3 n=normalize(v_normal), t=normalize(v_tangent), b=normalize(v_bitangent);
    vec3 viewdir=normalize(u.eye.xyz-v_position);
    vec2 tc=v_uv;
    if(u.material.x>0 && u.material.x<3 && u.effects.z!=0) {
        vec3 tsview=vec3(dot(viewdir,t),dot(viewdir,b),dot(viewdir,n));
        tc+=(texture(normalmap,tc).a-0.5)*u.effects.z*tsview.xy/max(abs(tsview.z),0.1);
    }
    if(u.material.x==3 || u.material.x==5) tc+=sin(tc.yx+u.eye.w)*0.125;
    if(u.material.x==6) {
        vec3 sky=texture(skybox,v_position-u.eye.xyz).rgb;
        float amount=1-exp(-pow(length(u.eye.xyz-v_position)*max(u.fog.w,0),2));
        outputcolor=vec4(mix(sky,u.fog.rgb,clamp(amount,0,1)),1);
        return;
    }
    if(u.material.x==4) {
        vec3 direction=v_position-u.eye.xyz; direction.z*=3;
        vec2 skyuv=direction.xy/max(0.5*length(direction),0.00001);
        vec3 sky=texture(base,skyuv+u.eye.w*0.03125).rgb;
        vec4 clouds=texture(glow,skyuv+u.eye.w*0.0625);
        outputcolor=vec4(mix(sky,clouds.rgb,clouds.a),1);
        return;
    }
    vec4 texel=texture(base,tc);
    if(u.material.z==6 && texel.a<u.upper.a) discard;
    vec4 albedo=texel*v_color;
    if((u.material.z==1 && albedo.a<=0) || (u.material.z==2 && albedo.a>=0.5) || (u.material.z==3 && albedo.a<0.5)) discard;
    if((u.material.z==4 && albedo.a<u.upper.a) || (u.material.z==5 && albedo.a>=u.upper.a)) discard;
    if(u.material.z==4 || u.material.z==5) albedo.a=1;
    vec3 result=albedo.rgb;
    if(u.material.x==5) {
        vec4 projected=u.screenmatrix*vec4(v_position,1);
        vec2 screen=projected.xy/projected.w*0.5+0.5;
        screen.y=1-screen.y;
        vec2 distortion=sin(v_uv.yx*3+u.eye.w)*0.008;
        vec3 transmitted=albedo.rgb;
        if(u.settings.w>0) transmitted=texture(refraction,clamp(screen+distortion,0.001,0.999)).rgb;
        // Refraction keeps the liquid tint and the original grazing-angle surface.
        transmitted*=vec3(0.7,0.8,0.7);
        float surface=max(u.effects.x,pow(1-abs(dot(n,viewdir)),4));
        result=mix(transmitted,albedo.rgb,surface);
        if(u.settings.z>0) {
            float fresnel=u.settings.w>0 ? 0.05+0.95*pow(1-abs(dot(n,viewdir)),4) : 1;
            result=mix(result,texture(reflection,clamp(screen+distortion,0.001,0.999)).rgb,fresnel);
        }
        albedo.a=1;
    }
    if(u.material.x>0 && u.material.x<3) {
        albedo.rgb+=(texture(uppermap,tc).rgb*u.upper.rgb+texture(lowermap,tc).rgb*u.lower.rgb)*v_color.rgb;
        if(u.effects.x>0) {
            vec3 mapped=texture(normalmap,tc).xyz*2-1;
            n=normalize(t*mapped.x+b*mapped.y+n*mapped.z);
        }
        vec3 lighting=vec3(1);
        if(u.material.x==1) lighting=texture(lightmap,v_lmuv).rgb*u.material.y;
        else lighting=u.ambient.rgb+u.diffuse.rgb*max(0,dot(n,normalize(u.lightdir.xyz+vec3(0.00001))));
        vec4 spec=texture(specular,tc);
        vec3 staticshine=vec3(0);
        if(u.material.x==1 && u.settings.x>0) {
            vec3 direction=normalize(texture(deluxe,v_lmuv).rgb*2-1);
            if(u.settings.x==1) direction=normalize(t*direction.x+b*direction.y+normalize(v_normal)*direction.z);
            float baseline=max(0.25,dot(normalize(v_normal),direction));
            float lambert=max(0,dot(n,direction));
            float shine=pow(max(0,dot(n,normalize(direction+viewdir))),max(1,u.effects.w*max(spec.a,0.01)))*u.effects.y;
            staticshine=spec.rgb*shine*lighting;
            lighting*=lambert/baseline;
        }
        result=albedo.rgb*lighting+staticshine;
        for(int i=0;i<int(u.material.w);i++) {
            vec3 delta=u.lightpos[i].xyz-v_position;
            float distance=length(delta);
            float attenuation=max(0,1-distance/u.lightpos[i].w);
            vec3 direction=delta/max(distance,0.00001);
            float lambert=max(0,dot(n,direction));
            float shine=pow(max(0,dot(n,normalize(direction+viewdir))),max(1,u.effects.w*max(spec.a,0.01)))*u.effects.y;
            float visibility=1;
            if(u.lightcolor[i].w>0) {
                vec4 p=u.lightshadow[i];
                float depth=max(max(abs(delta.x),abs(delta.y)),abs(delta.z));
                float compare=p.y/(p.y-p.x)-(p.y*p.x)/((p.y-p.x)*max(depth,p.x))-p.z/max(depth,1);
                visibility=texture(shadows,vec4(-delta,u.lightcolor[i].w-1),compare);
            }
            result+=(albedo.rgb*(u.lightparams[i].x+lambert*u.lightparams[i].y)+spec.rgb*shine*u.lightparams[i].z)*u.lightcolor[i].rgb*attenuation*attenuation*visibility;
        }
        result+=texture(glow,tc).rgb;
    }
    float fogamount=1-exp(-pow(length(u.eye.xyz-v_position)*max(u.fog.w,0),2));
    result=mix(result,u.fog.rgb,clamp(fogamount,0,1));
    outputcolor=vec4(result,albedo.a);
}
