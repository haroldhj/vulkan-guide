//glsl version 4.5
#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 inNormal;

layout (location = 3) in vec4 inShadowCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform  SceneData{   
    vec4 fogColor; // w is for exponent
	vec4 fogDistances; //x for min, y for max, zw unused.
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	mat4 sunlightShadowMatrix;
} sceneData;




layout(set = 0, binding = 2) uniform sampler2D shadowSampler;

layout(set = 2, binding = 0) uniform sampler2D tex1;
#define ambientshadow 0.1
float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowSampler, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambientshadow;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowSampler, 0);
	float scale = 0.3;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 3;

    vec3 rand = vec3(0,1,1);
	
	sc.x += dx * rand.z;
    sc.y += dx * rand.y;
	vec2 dirA = rand.xy;
	vec2 dirB = vec2(-dirA.y,dirA.x);
	
	dirA *= dx;
	dirB *= dy;
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}	
	}
	return shadowFactor / count;
}

void main() 
{
	vec3 color = texture(tex1,texCoord).xyz;
	float shadow = mix(0.f,1.f , filterPCF(inShadowCoord / inShadowCoord.w));


	float lightAngle = clamp(dot(inNormal, sceneData.sunlightDirection.xyz),0.f,1.f);
	vec3 lightColor = sceneData.sunlightColor.xyz * lightAngle;
	vec3 ambient = color * sceneData.ambientColor.xyz;
	vec3 diffuse = lightColor * color * shadow;

	outFragColor = vec4(diffuse+ ambient,1.0f);
}