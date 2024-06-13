// particle_compute.wgsl

struct Particle {
    position: vec2<f32>;
    velocity: vec2<f32>;
    color: vec4<f32>;
    size: f32;
    density: f32;
    pressure: f32;
};

struct SimulationParams {
    timestep: f32;
    grid_size: f32;
    mass: f32;
    gas_constant: f32;
    rest_density: f32;
};

// Input bindings for particles and simulation parameters
@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : SimulationParams;
@group(0) @binding(2) var<uniform> dimensions : vec2<f32>;

// Compute the kernel function for the given distance and grid size
fn kernel(r: f32, h: f32) -> f32 {
    if r > h {
        return 0.0;
    }
    let k = 315.0 / (64.0 * PI * pow(h, 9.0));
    return k * pow(h * h - r * r, 3.0);
}

// Compute the gradient of the kernel function for the given distance and grid size
fn grad_kernel(r: f32, h: f32) -> f32 {
    if r > h {
        return 0.0;
    }
    let k = -945.0 / (32.0 * PI * pow(h, 9.0));
    return k * pow(h * h - r * r, 2.0);
}

// Compute the physics simulation for each particle
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let index = global_id.x;
    if index >= arrayLength(&particles) {
        return;
    }

    var p = &particles[index];
    p.density = 0.0;
    p.pressure = 0.0;

    // Compute density and pressure
    for (var i = 0u; i < arrayLength(&particles); i++) {
        let q = &particles[i];
        let distance = distance(p.position, q.position);
        if distance < params.grid_size {
            p.density += params.mass * kernel(distance, params.grid_size);
        }
    }
    p.pressure = params.gas_constant * (p.density - params.rest_density);

    // Compute forces applied to the particle being simulated
    var force = vec2<f32>(0.0, 0.0);
    for (var i = 0u; i < arrayLength(&particles); i++) {
        if i != index {
            let q = &particles[i];
            let distance = distance(p.position, q.position);
            if distance < params.grid_size {
                let pressure_force = -0.5 * (p.pressure + q.pressure) / q.density * grad_kernel(distance, params.grid_size);
                force += pressure_force * normalize(p.position - q.position);
            }
        }
    }

    // Update velocity and position
    p.velocity += force * params.timestep;
    p.position += p.velocity * params.timestep;

    // Boundary conditions
    if p.position.x < 0.0 || p.position.x > dimensions.x {
        p.velocity.x = -p.velocity.x;
    }
    if p.position.y < 0.0 || p.position.y > dimensions.y {
        p.velocity.y = -p.velocity.y;
    }
}
