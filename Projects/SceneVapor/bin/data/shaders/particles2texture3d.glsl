#version 450
#extension GL_NV_shader_atomic_float : enable
//#extension GL_NV_gpushader5 : enable
//#extension GL_NV_shader_atomic_fp16_vector : enable
#extension GL_EXT_shader_image_load_store : enable

struct Box{
	vec3 min;
	vec3 max;
};

coherent uniform layout(r16f, binding=0, location=0) image3D volume;
layout(location = 1) uniform samplerBuffer particles;

uniform float size;
uniform float idx_offset;
uniform float next;
uniform float minDensity;
uniform float maxDensity;
uniform float scale;
uniform vec3 offset;

float map(float value, float inMin, float inMax, float outMin, float outMax)
{
	return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

vec3 offset_scale(vec3 value, vec3 offset, float scale)
{
	return (value + offset) * scale;
}


float boxVolume(float size) {
	return size * size * size;
}

int idx_clamp(int value, int size){
	return clamp(value, 0, size);
}

float boxesIntersectionVolume(Box b1, Box b2){
	return max(min(b1.max.x, b2.max.x) - max(b1.min.x, b2.min.x),0.f)
	* max(min(b1.max.y, b2.max.y) - max(b1.min.y, b2.min.y),0.f)
	* max(min(b1.max.z, b2.max.z) - max(b1.min.z, b2.min.z),0.f);
}

void add(ivec3 coord, float density){
	vec4 current_d = imageLoad(volume, ivec3(coord));
	//float d = map(density, minDensity, maxDensity, 0, 1);
	imageStore(volume, ivec3(coord), vec4(vec3(current_d.r + density), 1));
}

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main()
{
	int idx = int(idx_offset) + int(gl_GlobalInvocationID.x);
	if(idx>=int(next)) return;

	vec2 particleposxy = texelFetch(particles, idx*3).rg;
	vec2 particleposzw = texelFetch(particles, idx*3+1).rg;
	vec4 particlepos4 = vec4(particleposxy, particleposzw);
	vec2 size_density = texelFetch(particles, idx*3+2).rg;
	float particle_size = size_density.x * 2;
	float particle_density = size_density.y;

	vec3 particlepos = offset_scale(particlepos4.xyz, offset, scale);
	float psize = particle_size * scale;
	int isize = int(size);

	float particleVolume = boxVolume(psize);
	if(int(psize)>1){
		Box particleBox = Box( particlepos - vec3(psize*0.5,psize*0.5,psize*0.5), particlepos + vec3(psize*0.5,psize*0.5,psize*0.5));
		int min_x = idx_clamp(int(particlepos.x-psize), isize-1);
		int max_x = idx_clamp(int(particlepos.x+psize), isize);
		int min_y = idx_clamp(int(particlepos.y-psize), isize-1);
		int max_y = idx_clamp(int(particlepos.y+psize), isize);
		int min_z = idx_clamp(int(particlepos.z-psize), isize-1);
		int max_z = idx_clamp(int(particlepos.z+psize), isize);
		for(int z = min_z; z<max_z; z++){
			for(int y = min_y; y<max_y; y++){
				for(int x = min_x; x<max_x; x++){
					vec3 voxelpos = vec3(x, y, z);
					Box voxel = Box(voxelpos, voxelpos + vec3(1.0f, 1.0f, 1.0f));
					float factor = boxesIntersectionVolume(voxel, particleBox) / particleVolume;
					add(ivec3(x, y, z), particle_density * factor);
				}
			}
		}
	}else{
		add(ivec3(particlepos), particle_density/particleVolume);
	}
}
