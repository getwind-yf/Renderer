
#include <iostream>
#include "glad/glad.h"

#include "GLFW/glfw3.h"

#include "headers/Raytracing/shader_raytracing.h"
#include "headers/Raytracing/camera_raytracing.h"

#include <random>

//extern "C" {
//	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
//}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
// screen setting
const int SCR_WIDTH = 1280; 
const int SCR_HEIGHT = 900; 

// camera setting 
//设置camera 
Camera camera({ -2,2,1 }, { 0,0,-1 }, { 0,1,0 }, 30.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f);

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
	// glfwwindow 
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RayTracing_Demo_byDong", NULL, NULL);
	
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	//glad loader 

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	/*std::cout << glGetString(GL_VENDOR) << ' ' << glGetString(GL_RENDERER) << std::endl;*/

	// generate texture buffer for the output from raytracer 
	const int TEX_W = SCR_WIDTH, TEX_H = SCR_HEIGHT; 
	GLuint tex_output; 
	glGenTextures(1, &tex_output); 
	glActiveTexture(GL_TEXTURE0); 
	glBindTexture(GL_TEXTURE_2D, tex_output); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// texture image unit and image unit are different, get image from texture 
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEX_W, TEX_H, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(0, tex_output, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); 

	// 查看显卡上工作组的数量情况
	int work_grp_cnt[3]; 
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]); 
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]); 
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);
	std::cout << "Max global work group sizes <" << work_grp_cnt[0] << ',' << work_grp_cnt[1] << ',' << work_grp_cnt[2] << '>' << std::endl;

	int work_grp_size[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);
	std::cout << "Max local work group sizes <" << work_grp_size[0] << ',' << work_grp_size[1] << ',' << work_grp_size[2] << '>' << std::endl;

	int work_grp_inv; 
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
	std::cout << "Max invocations: " << work_grp_inv << std::endl;

	// Load compute shaders  
	Shader<ShaderType::COMPUTE> compshdr("./shaders/raytracing2/raycompute.comp"); 
	Shader<ShaderType::RENDER> drawshdr("./shaders/raytracing2/Draw.vs", "./shaders/raytracing2/Draw.frag"); 

	float quad[] = {
		-1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f
	}; 

	GLuint VAO, VBO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	int iteration = 0; 
	//Camera(glm::vec3 lookFrom, glm::vec3 lookAt, glm::vec3 up, float vfov, float aspect, float aperture = 0, float focal_length = -1, float yaw = YAW, float pitch = PITCH)

	//Camera camera({ -2,2,1 }, { 0,0,-1 }, { 0,1,0 }, 30.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f);
	//camera.Bind(compshdr);



	// SSBO for rng ;与之类似的有SSBO(Shader Storage Buffer Object) --> 这个和UBO- uniform buffer object类似 
	// 为了Shader 之间共享数据，而且SSBO有较大的空间

	std::vector<GLuint> init_rng; 
	init_rng.resize(SCR_WIDTH* SCR_HEIGHT); 
	// 填充随机数 
	for (GLuint& i : init_rng) {
		i = rand(); 
	}
	
	compshdr.use(); 
	GLuint SSBO; 
	glGenBuffers(1, &SSBO); 
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBO); 
	glBufferData(GL_SHADER_STORAGE_BUFFER, init_rng.size() * sizeof(GLuint), init_rng.data(), GL_DYNAMIC_DRAW); 

	bool wait_after_quit = false; 

	double start = glfwGetTime(); 

	//// Framebuffer setting 
	//unsigned int framebuffer;
	//glGenFramebuffers(1, &framebuffer);
	//glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	//// create a color attachment texture
	//unsigned int textureColorbuffer;
	//glGenTextures(1, &textureColorbuffer);
	//glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glm::vec3 lookfrom = { -2, 2, 1 }; 
	glm::vec3 lookat = { 0,0, -1 }; 

	while (!glfwWindowShouldClose(window)) {

		// per-frame time logic
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		Camera camera(lookfrom, lookat, { 0,1,0 }, 30.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f);
		camera.Bind(compshdr);

		//glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		//glEnable(GL_DEPTH_TEST); // enable depth testing (is disabled for rendering screen-space quad)

		//// make sure we clear the framebuffer's content
		//glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for (int iteration = 0; iteration <= 5; iteration++) // 设置采样数 
		{ 
			{
				compshdr.use();
				compshdr.setInt("iteration", iteration);
				//compshdr.setInt("iteration", iteration++);  // compute shader的迭代次数 ； pixel = 
				compshdr.setFloat("time", (float)glfwGetTime());  // 生成随机数
				compshdr.setInt("seed", rand()); // 生成随机数 
				glDispatchCompute((GLuint)TEX_W, (GLuint)TEX_H, 1);
			}

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			{
				glClear(GL_COLOR_BUFFER_BIT);
				drawshdr.use();
				glBindVertexArray(VAO);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, tex_output);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}
		}

		glfwPollEvents(); 
		glfwSwapBuffers(window);
		// 平均的FPS
		std::cout << "Number of iterations: " << iteration << " FPS: "  << 1.0 / deltaTime << "\t\r";
		lookfrom.x += 0.01; 
	
	}
	std::cout << std::endl;

	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &SSBO);
	glDeleteTextures(1, &tex_output);
	glDeleteVertexArrays(1, &VAO);

	glfwDestroyWindow(window);
	glfwTerminate();

	if (wait_after_quit) {
		std::cin.ignore();
	}

	return 0;

}

void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}
