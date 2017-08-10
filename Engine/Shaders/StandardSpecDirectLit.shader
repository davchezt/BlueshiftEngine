shader "Lit/StandardSpecDirectLit" {
    litSurface
    inheritProperties "StandardSpec.shader"
    
    generatePerforatedVersion
    generatePremulAlphaVersion
    generateGpuSkinningVersion
    generateParallelShadowVersion
    generateSpotShadowVersion
    generatePointShadowVersion

    glsl_vp {
        #define STANDARD_SPECULAR_LIGHTING
        #define DIRECT_LIGHTING 1
        $include "StandardCore.vp"
    }
    glsl_fp {
        #define STANDARD_SPECULAR_LIGHTING
        #define DIRECT_LIGHTING 1
        $include "StandardCore.fp"
    }
}
