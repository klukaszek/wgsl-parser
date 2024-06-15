// particle_render.wgsl
struct Particle {
    position: vec2<f32>;
    velocity: vec2<f32>;
    color: vec4<f32>;
    size: f32;
    density: f32;
    pressure: f32;
};

// Binding for particle data
@group(0) @binding(0) var<storage, read> particles : array<Particle>;

// Vertex output structure
struct VertexOutput {
    @builtin(position) position: vec4<f32>;
    @location(0) color: vec4<f32>;
    @location(1) size: f32;
    @location(2) point_coord: vec2<f32>;
};

// Vertex shader
@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var output: VertexOutput;
    let particle = particles[vertex_index];
    output.position = vec4<f32>(particle.position, 0.0, 1.0);
    output.color = particle.color;
    output.size = particle.size;
    output.point_coord = vec2<f32>(0.5, 0.5); // Center of the point sprite
    return output;
}

// Fragment shader
@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let dist = length(input.point_coord - vec2<f32>(0.5, 0.5));
    if dist > 0.5 {
        discard; // Discard pixels outside the circle
    }
    // Elastic effect: adjust alpha based on distance from center
    let alpha = 1.0 - smoothstep(0.45, 0.5, dist);
    return vec4<f32>(input.color.rgb, input.color.a * alpha);
}
