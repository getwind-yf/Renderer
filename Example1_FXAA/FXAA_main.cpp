
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// #include "FXAA_render.h"
#include <headers/shader.h>
#include <headers/camera.h>
#include <headers/model.h>

// IMGUI

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);


// Define fxaa parameter 
#define MAX_MUL_REDUCE_RECIPROCAL 256.0f
#define MAX_MIN_REDUCE_RECIPROCAL 512.0f
#define MAX_MAX_SPAN 16.0f
 
// fxaa fragment的参数 
//static unsigned int g_texelStepLocation;  
//static unsigned int g_showEdgesLocation;  
//static unsigned int g_fxaaOnLocation;  // 对应fxaaOn 
//
//static unsigned int g_lumaThresholdLocation;
//static unsigned int g_mulReduceLocation;
//static unsigned int g_minReduceLocation;
//static unsigned int g_maxSpanLocation;

// 定义屏幕大小
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;


// FXAA Settings 
static int showEdges = 0;
//static int fxaaOn = 1;
static bool fxaa_on = true; 

static float lumaThreshold = 0.5f;
static float mulReduceReciprocal = 8.0f;
static float minReduceReciprocal = 128.0f;
static float maxSpan = 8.0f;


int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "FXAA_DEMO", NULL, NULL);
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

	// tell GLFW to capture our mouse
	//修改鼠标设置 
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	glEnable(GL_DEPTH_TEST);

	// shader settings 
	Shader ourShader("src/vertex_model_loading.vs", "src/fragment_load_model.fs"); 
	Shader fxaa_shader("src/fxaa.vert.glsl", "src/fxaa.frag.glsl"); 
	
	// model 
	Model ourModel("resources/objects/rock/rock.obj");

	//fxaa_shader setting 
	fxaa_shader.use(); 
	fxaa_shader.setVec2("u_texelStep", glm::vec2((1.0f / (float)SCR_WIDTH), (1.0f / (float)SCR_HEIGHT))); 
	unsigned int g_vao; 
	glGenVertexArrays(1, &g_vao); 
	glBindVertexArray(g_vao); 
	glBindVertexArray(0);


	// offscreen framebuffer setting 
	unsigned int g_framebuffer;
	glGenFramebuffers(1, &g_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer);
	
	unsigned int g_texturecolor; 
	glGenTextures(1, &g_texturecolor); 
	glBindTexture(GL_TEXTURE_2D, g_texturecolor); 
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_texturecolor, 0);

	unsigned int g_depthRenderbuffer; 
	glGenRenderbuffers(1, &g_depthRenderbuffer); 
	glBindRenderbuffer(GL_RENDERBUFFER, g_depthRenderbuffer); 
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH,  SCR_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depthRenderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" <<std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//IMGUI Setup 
	const char* glsl_version = "#version 430";
	IMGUI_CHECKVERSION(); 
	ImGui::CreateContext(); 
	ImGuiIO &io = ImGui::GetIO(); (void)io; 
	
	ImGui::StyleColorsDark(); 
	ImGui_ImplGlfw_InitForOpenGL(window, true); 
	ImGui_ImplOpenGL3_Init(glsl_version); 

	
	bool show_demo_window = true;
	//render loop 
	int counter = 0; 
	float sumDeltaTime = 0; 

	while (!glfwWindowShouldClose(window))
	{
	// per-frame time logic 
		float currentFrame = glfwGetTime(); 
		deltaTime = currentFrame - lastFrame; 
		lastFrame = currentFrame; 
		sumDeltaTime += deltaTime; 
		counter++; 

	// process Input 
		processInput(window);

	// Start ImGUI 
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);
	// 设置窗口
		 {
			ImGui::Begin("FXAA_Flan_Engine");   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Checkbox("FXAA ON/OFF", &fxaa_on);
			ImGui::Text("FXAA ON/OFF: %.0f", float(fxaa_on));
			//ImGui::Text("FPS:%.1f", counter / sumDeltaTime);
			//设置小数位
			ImGui::Text("FPS:%.0f", 1/deltaTime);
			//if (ImGui::Button("fxaa_off"))
			//	fxaaOn = 0;
			ImGui::End();
		}
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	
		// rendering the scene to the off-screen framebuffer 
		glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer); 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ourShader.use(); 
		// view/projection transformations
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);

		// render the loaded model
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		ourShader.setMat4("model", model);
		ourModel.Draw(ourShader);

		glBindFramebuffer(GL_FRAMEBUFFER, 0); 


	// rendering off-screen color buffer using FXAA 

		glBindTexture(GL_TEXTURE_2D, g_texturecolor); 
		glEnable(GL_DEPTH_TEST); 
		//glDisable(GL_DEPTH_TEST); 
		fxaa_shader.use(); 
		fxaa_shader.setInt("u_showEdges", showEdges); 
		fxaa_shader.setInt("u_fxaaOn", int(fxaa_on));

		fxaa_shader.setFloat("u_lumaThreshold", lumaThreshold); 
		fxaa_shader.setFloat("u_mulReduce", 1.0f / mulReduceReciprocal); 
		fxaa_shader.setFloat("u_minReduce", 1.0f / minReduceReciprocal); 
		fxaa_shader.setFloat("u_maxSpan", maxSpan); 
	
		glBindVertexArray(g_vao); 
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
		glBindVertexArray(0); 

		ImGui::Render();
		int display_w, display_h;
		glfwMakeContextCurrent(window); 
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		//glClear(GL_COLOR_BUFFER_BIT);
		//ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();

	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
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
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
		lumaThreshold -= 0.05f; 
	if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
		lumaThreshold += 0.05f; 
	 if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
	{
			fxaa_on = !fxaa_on;
		std::cout << "fxaaOn = " << fxaa_on << endl;
	} 
	if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
		showEdges = !showEdges; 

}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}
