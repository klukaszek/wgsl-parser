struct Data {
    value: f32,
};

@group(0) @binding(0) var<storage, read_write> input : array<Data>;
@group(0) @binding(1) var<storage, read_write> output : array<Data>;

fn f(x: f32) -> f32 {
    return x + 1.0;
}

@compute @workgroup_size(32)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let index = global_id.x;
    output[index].value = f(input[index].value);
}
