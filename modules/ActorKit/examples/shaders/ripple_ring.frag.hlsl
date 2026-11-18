// The ActorKit example's pond ripples, run on the pond's sprite: up to four ripples at once, each a band of
// rings around a front that travels outward from where a foot came down, with calm water inside and outside
// the band. All four are worked out in this one pass and added together, since a custom step reads the
// sprite's own art and a second step would draw over the first. Where the rings meet, a crest pushes the
// art outward along its radius and lightens it; a trough pulls it inward and darkens it. The push is rounded
// to whole pixels as a distance, so it is the same in every direction. Everything is in the sprite's art
// pixels. Float-only params, so the shader runs on the sprite path.

cbuffer Params : register(b1, space3) {
    float4 ringA;         // one ripple each: x and y of where the foot came down, the front's distance
    float4 ringB;         // from it, and its strength, 0 to 1; a strength of 0 leaves the art as it is
    float4 ringC;
    float4 ringD;
    float2 ringArt;       // the sprite's art size, in pixels
    float  ringWidth;     // how far the rings reach either side of a front, art pixels
    float  ringSpacing;   // the distance between crests, art pixels
    float  ringPush;      // how far a crest at full strength pushes the art, art pixels
    float  ringShade;     // how much a crest at full strength lightens the art, and a trough darkens it, 0 to 1
};

// One ripple's share at the art pixel `p`: its push along its radius in `push`, its crest (+) or trough (-)
// in the return value.
float ringAt(float4 ring, float2 p, inout float2 push) {
    if (ring.w <= 0.0f) {
        return 0.0f;
    }
    const float2 d = p - ring.xy;
    const float dist = length(d);
    const float fromFront = dist - ring.z;
    const float band = exp(-(fromFront * fromFront) / (ringWidth * ringWidth));
    const float wave = cos(6.2831853f * fromFront / ringSpacing) * band * ring.w;
    if (dist > 0.5f) {
        push += d / dist * (ringPush * wave);
    }
    return wave;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
    const float2 p = uv * ringArt;
    float2 push = float2(0.0f, 0.0f);
    float wave = ringAt(ringA, p, push) + ringAt(ringB, p, push) + ringAt(ringC, p, push) + ringAt(ringD, p, push);

    float2 read = uv;
    const float length2 = dot(push, push);
    if (length2 > 0.0f) {
        const float pushed = length(push);
        read = (p + push / pushed * round(pushed)) / ringArt;   // whole pixels, along the push's direction
    }
    float4 c = sampleSource(read);
    c.rgb = saturate(c.rgb * (1.0f + ringShade * clamp(wave, -1.0f, 1.0f)));
    return c;
}
