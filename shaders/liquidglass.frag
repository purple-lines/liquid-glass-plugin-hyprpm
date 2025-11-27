#version 300 es
precision highp float;

/*
 * Apple-style Liquid Glass Fragment Shader
 * 
 * Implements the key visual elements of Apple's iOS 26 Liquid Glass design:
 * 1. Edge refraction with displacement mapping
 * 2. Chromatic aberration (RGB channel separation)
 * 3. Fresnel effect (edge glow based on viewing angle)
 * 4. Specular highlights (sharp light reflections)
 * 5. Subtle interior blur for glass thickness
 */

// Uniforms
uniform sampler2D tex;
uniform vec2 topLeft;
uniform vec2 fullSize;
uniform vec2 fullSizeUntransformed;
uniform float radius;
uniform float time;

// Configurable parameters
uniform float blurStrength;        // Interior blur amount (0.0 - 2.0)
uniform float refractionStrength;  // Edge refraction intensity (0.0 - 0.15)
uniform float chromaticAberration; // RGB separation amount (0.0 - 0.02)
uniform float fresnelStrength;     // Edge glow intensity (0.0 - 1.0)
uniform float specularStrength;    // Highlight brightness (0.0 - 1.0)
uniform float glassOpacity;        // Overall glass opacity (0.0 - 1.0)
uniform float edgeThickness;       // How thick the refractive edge is (0.0 - 0.3)

in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

// Constants
const float PI = 3.14159265359;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Compute signed distance to rounded rectangle
float roundedBoxSDF(vec2 p, vec2 size, float r) {
    vec2 q = abs(p) - size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

// Smooth edge mask with configurable falloff
float getEdgeMask(vec2 uv, float thickness) {
    vec2 center = vec2(0.5);
    vec2 pos = uv - center;
    vec2 size = vec2(0.5);
    
    // Compute distance from edge
    float cornerRadius = radius / max(fullSize.x, fullSize.y) * 2.0;
    float dist = roundedBoxSDF(pos, size - thickness, cornerRadius);
    
    // Create smooth gradient from edge to center
    float edgeFactor = smoothstep(-thickness, 0.0, dist);
    return edgeFactor;
}

// Generate refraction displacement based on edge proximity
vec2 getRefractionOffset(vec2 uv, float edgeMask) {
    vec2 center = vec2(0.5);
    vec2 fromCenter = uv - center;
    float dist = length(fromCenter);
    
    // Direction from center, normalized
    vec2 dir = normalize(fromCenter + 0.0001);
    
    // Refraction is stronger at edges (like looking through curved glass)
    // Use a sine-based curve for more natural glass-like distortion
    float refractionAmount = edgeMask * sin(edgeMask * PI * 0.5);
    
    // Add subtle wave distortion for liquid feel
    float wave = sin(dist * 8.0 + time * 0.5) * 0.1 + 1.0;
    
    return dir * refractionAmount * refractionStrength * wave;
}

// ============================================================================
// BLUR FUNCTION - Gaussian approximation
// ============================================================================

vec3 gaussianBlur(vec2 uv, vec2 texelSize, float strength) {
    // 9-tap Gaussian blur
    vec3 result = texture(tex, uv).rgb * 0.1633;
    
    vec2 off1 = texelSize * strength;
    vec2 off2 = texelSize * strength * 2.0;
    
    result += texture(tex, uv + vec2(off1.x, 0.0)).rgb * 0.1531;
    result += texture(tex, uv - vec2(off1.x, 0.0)).rgb * 0.1531;
    result += texture(tex, uv + vec2(0.0, off1.y)).rgb * 0.1531;
    result += texture(tex, uv - vec2(0.0, off1.y)).rgb * 0.1531;
    result += texture(tex, uv + vec2(off2.x, 0.0)).rgb * 0.0561;
    result += texture(tex, uv - vec2(off2.x, 0.0)).rgb * 0.0561;
    result += texture(tex, uv + vec2(0.0, off2.y)).rgb * 0.0561;
    result += texture(tex, uv - vec2(0.0, off2.y)).rgb * 0.0561;
    
    return result;
}

// Simpler 5-tap blur for performance
vec3 fastBlur(vec2 uv, vec2 texelSize, float strength) {
    vec3 result = texture(tex, uv).rgb * 0.2270270270;
    
    vec2 off1 = vec2(1.3846153846) * texelSize * strength;
    vec2 off2 = vec2(3.2307692308) * texelSize * strength;
    
    result += texture(tex, uv + off1).rgb * 0.3162162162;
    result += texture(tex, uv - off1).rgb * 0.3162162162;
    result += texture(tex, uv + off2).rgb * 0.0702702703;
    result += texture(tex, uv - off2).rgb * 0.0702702703;
    
    return result;
}

// ============================================================================
// CHROMATIC ABERRATION
// ============================================================================

vec3 chromaticSample(vec2 uv, vec2 texelSize, float edgeMask) {
    // Different refraction amounts for each color channel
    // Red bends least, blue bends most (like real glass)
    float caAmount = chromaticAberration * edgeMask;
    
    vec2 center = vec2(0.5);
    vec2 dir = normalize(uv - center + 0.0001);
    
    vec2 offsetR = dir * caAmount * 0.8;
    vec2 offsetG = vec2(0.0);  // Green is reference
    vec2 offsetB = dir * caAmount * 1.2;
    
    float r = texture(tex, uv + offsetR).r;
    float g = texture(tex, uv + offsetG).g;
    float b = texture(tex, uv + offsetB).b;
    
    return vec3(r, g, b);
}

// ============================================================================
// FRESNEL EFFECT - Edge glow based on viewing angle
// ============================================================================

float fresnelEffect(vec2 uv) {
    vec2 center = vec2(0.5);
    vec2 pos = uv - center;
    
    // Distance from center, normalized
    float dist = length(pos) * 2.0;
    
    // Fresnel approximation: stronger reflection at grazing angles
    // F = F0 + (1 - F0) * (1 - cos(theta))^5
    float fresnel = pow(dist, 3.0);
    
    // Apply edge mask to limit to actual edges
    float edgeMask = getEdgeMask(uv, edgeThickness);
    
    return fresnel * edgeMask * fresnelStrength;
}

// ============================================================================
// SPECULAR HIGHLIGHTS - Sharp light reflections
// ============================================================================

float specularHighlight(vec2 uv) {
    // Simulate light coming from top-left
    vec2 lightDir = normalize(vec2(-0.7, -0.7));
    vec2 center = vec2(0.5);
    vec2 pos = uv - center;
    
    // Dot product with light direction
    float highlight = dot(normalize(pos + 0.0001), lightDir);
    
    // Sharp falloff for specular look
    highlight = pow(max(highlight, 0.0), 16.0);
    
    // Only show on edges
    float edgeMask = getEdgeMask(uv, edgeThickness * 0.5);
    
    // Add secondary highlight from bottom-right for depth
    vec2 lightDir2 = normalize(vec2(0.7, 0.7));
    float highlight2 = dot(normalize(pos + 0.0001), lightDir2);
    highlight2 = pow(max(highlight2, 0.0), 24.0) * 0.5;
    
    return (highlight + highlight2) * edgeMask * specularStrength;
}

// ============================================================================
// MAIN SHADER
// ============================================================================

void main() {
    vec2 uv = v_texcoord;
    vec2 texelSize = 1.0 / fullSize;
    
    // Calculate edge mask for effects
    float edgeMask = getEdgeMask(uv, edgeThickness);
    
    // ========================================
    // 1. REFRACTION - Bend the background at edges
    // ========================================
    vec2 refractionOffset = getRefractionOffset(uv, edgeMask);
    vec2 refractedUV = uv + refractionOffset;
    
    // Clamp to valid UV range
    refractedUV = clamp(refractedUV, 0.001, 0.999);
    
    // ========================================
    // 2. BLUR - Glass thickness effect
    // ========================================
    // More blur at center, less at edges (like thick glass)
    float blurAmount = blurStrength * (1.0 - edgeMask * 0.5);
    vec3 blurredColor = fastBlur(refractedUV, texelSize, blurAmount);
    
    // ========================================
    // 3. CHROMATIC ABERRATION - Color fringing at edges
    // ========================================
    vec3 caColor = chromaticSample(refractedUV, texelSize, edgeMask);
    
    // Blend between blurred and chromatic based on edge proximity
    vec3 glassColor = mix(blurredColor, caColor, edgeMask * 0.7);
    
    // ========================================
    // 4. FRESNEL EFFECT - Edge glow
    // ========================================
    float fresnel = fresnelEffect(uv);
    vec3 fresnelColor = vec3(1.0, 1.0, 1.0) * fresnel;
    
    // ========================================
    // 5. SPECULAR HIGHLIGHTS - Sharp reflections
    // ========================================
    float specular = specularHighlight(uv);
    vec3 specularColor = vec3(1.0, 0.98, 0.95) * specular;
    
    // ========================================
    // COMBINE ALL EFFECTS
    // ========================================
    
    // Base glass color with slight tint (Apple uses a subtle blue-ish tint)
    vec3 glassTint = vec3(0.95, 0.97, 1.0);
    vec3 finalColor = glassColor * glassTint;
    
    // Add Fresnel glow (additive)
    finalColor += fresnelColor * 0.15;
    
    // Add specular highlights (additive)
    finalColor += specularColor;
    
    // Slight saturation reduction for glass look
    float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
    finalColor = mix(vec3(luminance), finalColor, 0.9);
    
    // Output with glass opacity
    fragColor = vec4(finalColor, glassOpacity);
}
