struct Vertex {
    float2 pos;
    float2 uv;
}

struct Fragment {
    float4 sv_pos : SV_Position;
    float2 uv;
}

[[vk::binding(0, 3)]]
ConstantBuffer<float> w : register(b0, space3);

[[vk::binding(0, 2)]]
Sampler2D tex : register(t0, space2): register(s0, space2);

[[shader("vertex")]]
Fragment vs_main(Vertex vtx) {
    return Fragment(float4(vtx.pos, 0, 1), vtx.uv);
}

[[shader("fragment")]]
float4 fs_main(Fragment frg) : SV_Target {
    return tex.Sample(frg.uv);
}