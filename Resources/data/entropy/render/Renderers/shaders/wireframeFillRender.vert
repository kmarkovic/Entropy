#version 330

// OF default uniforms and attributes
uniform mat4 modelViewProjectionMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 normalMatrix;

uniform float alpha;

in vec4 position;
in vec4 color;
in vec3 normal;

out vec4 f_color;
out float f_distanceToCamera;
out float f_distanceToCenter;

#define SHADE_NORMALS 1
#define FOG_ENABLED 1
#define WIREFRAME 0
#define SPHERICAL_CLIP 0

float bright(vec3 rgb){
	return max(max(rgb.r, rgb.g), rgb.b);
}

void main()
{
    #if WIREFRAME
		float bright = bright(color.rgb);
		f_color = vec4(vec3(bright), alpha);
	#else
		#if SHADE_NORMALS
			vec3 f_normal = (normalMatrix * vec4(normal, 0.0)).xyz;
			f_normal = normalize(f_normal);
			f_normal = f_normal * 0.5 + 0.5;
			f_color = vec4(f_normal, alpha);
		#else
	        f_color = color;
			f_color.a *= alpha;
		#endif
    #endif

    #if SPHERICAL_CLIP
		f_distanceToCenter = length(position.xyz);
    #endif

    #if FOG_ENABLED
		vec4 eyePosition =  modelViewMatrix * position;
		//float distanceToCamera = length(eyePosition);
		//f_color.a *= fog(distanceToCamera, fogMinDistance, fogMaxDistance, fogPower);
		f_distanceToCamera = length(eyePosition);
		gl_Position = projectionMatrix * eyePosition;
    #else
		gl_Position = modelViewProjectionMatrix * position;
    #endif

}

