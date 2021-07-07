# 计算机图形学项目一：抗锯齿FXAA 

## FXAA特点
FXAA全程是Fast Approximate Anti-Aliasing，指的是快速近似抗锯齿，特点是处理速度快，方便整合在后处理的渲染管线。 
 - FXAA计算范围与MSAA或者SAA不同，MSAA发生在光栅化的阶段，而FXAA是借助延迟渲染的技术，只对全屏的像素进行处理。 
 - FXAA计算速度较快，一方面由于只计算屏幕像素，范围比MSAA少而更快；另一方面应用简单的边缘检测及梯度计算实现平滑效果。

## 计算步骤
这里主要说明场景已渲染至Framebuffer的texture的渲染对象之后，FXAA的Fragment Shader如何进行边缘锯齿的平滑：

根据个人理解，总体步骤可以分为：1）计算当前像素点和相邻像素的灰度值  2) 边缘判断：通过灰度最大值与灰度最小值的差距来判定 3）计算采样方向：x和y方向的梯度作为采样方向，同时考虑brightness的影响  4）采样及平均：分两种情况一种是Inner Sample，正负1/6方向；另一种情况是Outer Sample，在正负1/2方向上 


## Shader编写
具体GLSL代码见下面： 

 - 采集NW、NE、SW、SE四个方位的rgb值，并转化为灰度值luma
```javascript
// 当前像素的rgb值 
vec3 CurrentPixel = texture(colorTexture,texCoord).rgb; 
//获取四个对角方位的rgb值
vec3 NW = textureOffset(colorTexture, texCoord, ivec2(-1,1)).rgb; 
vec3 NE = textureOffset(colorTexture, texCoord, ivec2(1,1)).rgb; 
vec3 SW = textureOffset(colorTexture, texCoord, ivec2(-1,-1)).rgb; 
vec3 SE = textureOffset(colorTexture, texCoord, ivec2(1,-1)).rgb; 
//转化为灰度值 
const vec3 ToLuma = vec3(0.299, 0.587, 0.114); 
float lumaNW = dot(NW, ToLuma);
float lumaNE = dot(NE, ToLuma);
float lumaSW = dot(SW, ToLuma);
float lumaSE = dot(SE, ToLuma);
float lumaM = dot(CurrentPixel, ToLuma);
```
 - 边缘判定：通过最大和最小灰度值之差是否低于设定的Threshold值，来判定 
```javascript
float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
// 如果最大和最小值只差小于阈值，则不是边缘
if (lumaMax - lumaMin <= lumaMax * u_lumaThreshold)
	{
		fragColor = vec4(CurrentPixel , 1.0);
		return;
	}
```
 - 采样方向计算：先分别计算x和y的梯度值，然后计算采样缩减因子 
```javascript
// 计算x、y方向上的梯度 
vec2 samplingDirection;
samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
samplingDirection.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
// 计算采样缩减因子 -- luma平均值乘以缩减因子 
float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * u_mulReduce, u_minReduce);
// 最终缩减因子，上面计算的值越大，最终缩减值越小 
float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
// 获取最终的采样方向 = xy方向梯度 * 最小采样方向因子 * 步长 
samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-u_maxSpan), vec2(u_maxSpan))* u_texelStep;

```
 - 平滑计算：分两种情况：twoTab的平均和fourTab的平均 
```javascript
// Two Tabs -- 方向上乘以1/6 
vec3 rgbSampleNeg = texture(u_colorTexture, v_texCoord + samplingDirection * (1.0 / 3.0 - 0.5)).rgb;
vec3 rgbSamplePos = texture(u_colorTexture, v_texCoord + samplingDirection * (2.0 / 3.0 - 0.5)).rgb;

// Four Tabs -- 方向上乘以1/2 
vec3 rgbSampleNegOuter = texture(u_colorTexture, v_texCoord + samplingDirection * (0.0 / 3.0 - 0.5)).rgb;
vec3 rgbSamplePosOuter = texture(u_colorTexture, v_texCoord + samplingDirection * (3.0 / 3.0 - 0.5)).rgb;
vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;
vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;

//判定采用的情况
float lumaFourTab = dot(rgbFourTab, toLuma);

if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
	{
		fragColor = vec4(rgbTwoTab, 1.0);
	}
else
	{
		fragColor = vec4(rgbFourTab, 1.0);
	}
```

