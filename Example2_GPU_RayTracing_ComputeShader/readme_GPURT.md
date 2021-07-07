# GPU光线追踪(OpenGL-ComputeShader)
## 1.背景
此项目RayTracing的实现是基于OGL和ComputeShader的，计算的方法参考自Shirley的光线追踪教程，本文着重介绍在Compute Shader中如何编写Ray Tracing的计算过程。 
## 2.ComputeShader简介
 - 什么是Compute Shader？ 
Compute Shader 是GLSL着色器的一种，和Vertex和Fragment不同的是，CS是只有一级的管线，通过buffer（texture buffer、image buffer、SSBO）或者shared变量可以与CPU进行数据通信 
- Compute Shader特有的参数
计算着色器任务以组为单位，称为工作组Work Group，二级的单位称为本地工作组Local Workgroup，一级是Global Group； 例如location(local_size_x, local_size_y, local_size_z) 就是指的是本地工作组大小 

- Compute Shader的注意点
1）选择正确的工作组大小  2）使用内存屏障MemoryBarrier，防止乱序；在不同的请求之间插入控制流和内存屏障 3）使用共享变量  4）计算着色器在运行时，进行其他的任务工作

## 3.光线追踪代码
总的来说，简单的光线追踪器包括以下的部分：1. 光线的定义   2.物体位置及材质的定义 3.相交检测  4.递归计算光线。下面就从这些方面具体来看Compute Shader代码：

### 3.1光线的定义
光线可以表达为p(t) = A + t*B, 其中A为出发的位置，B为方向，t可以理解为时间。这样可以在C语言中定义出Ray结构体: 
```
// Ray Class
struct Ray { 
	vec3 A; // 起点 
	vec3 B; // 方向
};
```
### 3.2 物体位置及材质
这里以圆形为例，定义圆心和半径：材质的逻辑是：将hit里面的光线换成反射或者折射的光线，同时返回该点的颜色值 
```
// Sphere Class
struct Sphere {
	vec3 C;
	float R;
};
// 定义击中类 
struct HitInfo{
	bool hit; 
	vec3 hitpoint; 
	float t; 
	vec3 normal; 
	Ray r; 
	Material m; 

// 材质定义
// 漫反射材质 -- 调整发射后的光线A和B
vec3 Lambert(inout HitInfo hit) {
	hit.r.A = hit.hitpoint; 
	hit.r.B = hit.normal + RandomInUnitSphere(); 
	hit.hit = dot(hit.r.B, hit.normal) > 0.0f; 
	return(hit.hit)? hit.m.albedo : vecc3(0.0f);  // 返回这个点的颜色
	} 	
// 金属材质 -- 全反射
// 计算全反射的向量 
vec3 custom_reflect(vec3 I, vec3 N) {
	return (I - 2 * dot(I, normalize(N)) * normalize(N));
}
// 金属材质 
vec3 ScatterMetal(inout HitInfo hit) {
	hit.r.A = hit.hitpoint;
	hit.r.B = custom_reflect(hit.r.B,hit.normal) + 0.2f * RandomInUnitSphere() * hit.m.roughness;
	hit.hit = dot(hit.r.B,hit.normal) > 0.0f;
	return (hit.hit) ? hit.m.albedo : vec3(0.0f);
}
// 半导体材质 
// 计算折射 
float schlick(float cosine, float ri) {
	float r0 = (1.0f-ri)/(1.0f+ri);
	r0 = r0*r0;
	float anticos = (1.0f-cosine);
	return r0 + (1.0f - r0)*anticos*anticos*anticos*anticos*anticos;
}
//半导体材质 
vec3 refract_dielectric(vec3 v, vec3 norm, float nu, float refl_prob) {
	
	vec3 uv = normalize(v);
	float dt = dot(uv,norm);
	float disc = 1.0f - nu*nu*(1.0f-dt*dt);
	if (disc > 0.0f && rng() > refl_prob) {
		return nu*(uv - norm*dt) - norm*sqrt(disc);
	} else {
		return custom_reflect(uv, norm);
	}
}

// 将所有的类别写入函数中
vec3 Scatter(inout HitInfo hit) {
	if (hit.m.type == MAT_LAMBERT) {
		return ScatterLambert(hit);
	} else if (hit.m.type == MAT_METAL) {
		return ScatterMetal(hit);
	} else if (hit.m.type == MAT_DIELECRIC) {
		return ScatterDielectric(hit);
	} else {
		return ScatterLambert(hit);
	}
}

```
### 3.3 相交检测 

 - 对于光线与球体的相交判断问题：可以通过解一元二次方程的方法，判断是否存在根。 也可以优化方式来判断： 
```
// 判断是否击中 
HitInfo HitSphere(Sphere s, Ray r, float tmin) {
	vec3 oc = r.A - s.C;
	float a = dot(r.B, r.B);
	float b = 2.0f * dot(oc, r.B);
	float c = dot(oc, oc) - s.R*s.R;
	float disc = b*b - 4*a*c;
	HitInfo h;
	h.hit = (disc > 0.0f);
	if (!h.hit) {
		return h;
	}
	float t1 = (-b - sqrt(disc))/(2.0f*a);
	if (t1 < tmin) {
		t1 = (-b + sqrt(disc))/(2.0f*a);
		if (t1 < tmin) {
			h.t = t1;
			h.hit = false;
			return h;
		}
	}
	h.t = t1;
	h.hitpoint = r.A + h.t * r.B;
	h.normal = (h.hitpoint - s.C)/s.R;
	return h;
}
```
### 3.4 计算颜色
通过设定最大的深度来终止for循环 
```
vec4 Color(Ray r) {
	vec3 color = vec3(1.0f);
	for (int depth = 0; depth < MAX_DEPTH; depth++) {
		HitInfo h = WorldHit(r, 0.001f, 1000.0f);
		if (h.hit) {
			color *= Scatter(h);
			if (!h.hit) break;
			r = h.r;
		} 
		else {
			float t = 0.5f * (normalize(r.B).y + 1.0f);
			color*= ((1.0f-t)*vec3(1.0f) + t * vec3(0.5f, 0.7f, 1.0f));
			break;
		}
	}
	return vec4(color, 1.0f);
}


```
### 3.5 随机采样迭代 
对于一个pixel而言，为了减少噪点，可以通过随机采样的形式，在某一个像素点随机发出数条光线，然后再平均，减少屏幕的噪点 

```
uniform int iteration

int main()
{
//..... 
float x = (pixel_coords.x + rng()) / float(dims.x); 
float y = (pixel_coords.y + rng()) / float(dims.y); 
Ray r = GetRay(cam, x, y);
vec4 pixel = (Color(r)+ iteration * img)*(1.0f/(1.0f + iteration));
imageStore(img_output, pixel_coords, pixel);
state[pixel_coords.y * dims.x + pixel_coords.x] = rngstate();
```

### 3.6 CPU端设置
创建TexImageBuffer，传递数据到ComputeShader 
```
const int TEX_W = SCR_WIDTH, TEX_H = SCR_HEIGHT; 
	GLuint tex_output; 
	glGenTextures(1, &tex_output); 
	glActiveTexture(GL_TEXTURE0); 
	glBindTexture(GL_TEXTURE_2D, tex_output); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEX_W, TEX_H, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(0, tex_output, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); 

```

### 3.7 结果展示
Iteration=5 （采样数）FPS=24
