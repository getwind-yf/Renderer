#version 430 core

layout(binding = 0) uniform sampler2D u_colorTexture;

uniform vec2 u_texelStep;
uniform int u_showEdges;
uniform int u_fxaaOn;

uniform float u_lumaThreshold;
uniform float u_mulReduce;
uniform float u_minReduce;
uniform float u_maxSpan;

in vec2 v_texCoord;

out vec4 fragColor;

void main(void)
{
	vec3 rgbM = texture(u_colorTexture, v_texCoord).rgb;

	if (u_fxaaOn == 0)
	{
		fragColor = vec4(rgbM, 1.0);
		return;
	}

	// Sampling neighbour texels, offsets are adapted to OpenGL texture coordinates 
	vec3 rgbNW = textureOffset(u_colorTexture, v_texCoord, ivec2(-1, 1)).rgb;
	vec3 rgbNE = textureOffset(u_colorTexture, v_texCoord, ivec2(1, 1)).rgb;
	vec3 rgbSW = textureOffset(u_colorTexture, v_texCoord, ivec2(-1, -1)).rgb;
	vec3 rgbSE = textureOffset(u_colorTexture, v_texCoord, ivec2(1, -1)).rgb;

	// based on wiki GrayScale 
	const vec3 toLuma = vec3(0.299, 0.587, 0.114);

	// Convert from RGB to luma 
	float lumaNW = dot(rgbNW, toLuma);
	float lumaNE = dot(rgbNE, toLuma);
	float lumaSW = dot(rgbSW, toLuma);
	float lumaSE = dot(rgbSE, toLuma);
	float lumaM = dot(rgbM, toLuma);

	//  Gather minimum and maximum luma 
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	// if contrast is lower than a maximum threshold 

	if (lumaMax - lumaMin <= lumaMax * u_lumaThreshold)
	{
		fragColor = vec4(rgbM, 1.0);
		return;
	}

	// Sampling is done along the gradient  
	vec2 samplingDirection;
	samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	samplingDirection.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	// Sampling step distance depends on the luma 
	 
	float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * u_mulReduce, u_minReduce);

	//Factor for norming the sampling direction plus adding the brightness influence 
	float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);

	//Calculate final sampling direction vector by reducing, clamping to a range 
	samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-u_maxSpan), vec2(u_maxSpan))* u_texelStep;

	// Inner samples on the tab 
	vec3 rgbSampleNeg = texture(u_colorTexture, v_texCoord + samplingDirection * (1.0 / 3.0 - 0.5)).rgb;
	vec3 rgbSamplePos = texture(u_colorTexture, v_texCoord + samplingDirection * (2.0 / 3.0 - 0.5)).rgb;

	vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;

	// Outer samples on the tab 

	vec3 rgbSampleNegOuter = texture(u_colorTexture, v_texCoord + samplingDirection * (0.0 / 3.0 - 0.5)).rgb;
	vec3 rgbSamplePosOuter = texture(u_colorTexture, v_texCoord + samplingDirection * (3.0 / 3.0 - 0.5)).rgb;

	vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;

	// Calculate Luma for checking against the minimum and maximum value 
	float lumaFourTab = dot(rgbFourTab, toLuma);

	if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
	{
		fragColor = vec4(rgbTwoTab, 1.0);
	}
	else
	{
		fragColor = vec4(rgbFourTab, 1.0);
	}



	if (u_showEdges != 0)
	{
		fragColor.r = 1.0;
	}
}