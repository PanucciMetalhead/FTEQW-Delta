gpu_sprite_background
{
    cull none
    surfaceparm nodlight
    surfaceparm noshadows
    {
        map $whiteimage
        rgbgen vertex
        alphagen vertex
        depthwrite
    }
}
