#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include "mesher.h"
#include "shader.h"
#include "camera.h"
#include "noise.h"
#include "utility.h"

void create_chunk();

GLFWwindow* init_window() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);

  GLFWwindow* window = glfwCreateWindow(1280, 720, "Binary Greedy Meshing", nullptr, nullptr);
  if (!window) {
    fprintf(stderr, "Unable to create GLFW window\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwMakeContextCurrent(window);

  if (!gladLoadGL()) {
    fprintf(stderr, "Unable to initialize glad\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  return window;
}

void GLAPIENTRY message_callback(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  const void* userParam
) {
  std::string SEVERITY = "";
  switch (severity) {
  case GL_DEBUG_SEVERITY_LOW:
    SEVERITY = "LOW";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    SEVERITY = "MEDIUM";
    break;
  case GL_DEBUG_SEVERITY_HIGH:
    SEVERITY = "HIGH";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    SEVERITY = "NOTIFICATION";
    break;
  }
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
    type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "",
    type, SEVERITY.c_str(), message);
}

bool init_opengl() {
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  glDebugMessageCallback(message_callback, 0);

  glEnable(GL_DEPTH_TEST);

  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);

  glClearColor(0.529f, 0.808f, 0.922f, 0.0f);

  glEnable(GL_MULTISAMPLE);

  return true;
};

void glfw_error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

Shader* shader = nullptr;
Camera* camera = nullptr;
Noise noise;

float last_x = 0.0f;
float last_y = 0.0f;

enum class MESH_TYPE: int {
  TERRAIN,
  RANDOM,
  CHECKERBOARD,
  SPHERE,
  EMPTY,
  Count
};

int mesh_type = (int)MESH_TYPE::TERRAIN;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
  camera->processMouseMovement(xpos - last_x, last_y - ypos);
  last_x = xpos;
  last_y = ypos;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, true);
  }

  else if (key == GLFW_KEY_X && action == GLFW_RELEASE) {
    GLint lastPolyMode[2];
    glGetIntegerv(GL_POLYGON_MODE, lastPolyMode);
    if (lastPolyMode[0] == GL_FILL) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
  }

  else if (key == GLFW_KEY_1 && action == GLFW_RELEASE) {
    printf("Forward: %.1f, %.1f, %.1f \n", camera->front.x, camera->front.y, camera->front.z);
  }

  else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE) {
    create_chunk();
  }

  else if (key == GLFW_KEY_TAB && action == GLFW_RELEASE) {
    mesh_type++;
    if (mesh_type >= (int)MESH_TYPE::Count) {
      mesh_type = 0;
    }
    create_chunk();
  }
}

GLuint VAO, VBO;
int vertexCount = 0;

void create_chunk() {
  std::vector<uint8_t> voxels(CS_P3);
  std::fill(voxels.begin(), voxels.end(), 0);

  switch (mesh_type) {
    case (int)MESH_TYPE::TERRAIN: {
      noise.generateTerrain(voxels, std::rand());
      break;
    }

    case (int)MESH_TYPE::RANDOM: {
      noise.generateWhiteNoiseTerrain(voxels, std::rand());
      break;
    }

    case (int)MESH_TYPE::CHECKERBOARD: {
      for (int x = 1; x < CS_P; x++) {
        for (int y = 1; y < CS_P; y++) {
          for (int z = 1; z < CS_P; z++) {
            if (x % 2 == 0 && y % 2 == 0 && z % 2 == 0) {
              voxels.at(get_yzx_index(x, y, z)) = 1;
              voxels.at(get_yzx_index(x - 1, y - 1, z)) = 2;
              voxels.at(get_yzx_index(x - 1, y, z - 1)) = 3;
              voxels.at(get_yzx_index(x, y - 1, z - 1)) = 4;
            }
          }
        }
      }
      break;
    }

    case (int)MESH_TYPE::SPHERE: {
      int r = CS_P / 2;
      for (int x = -r; x < r; x++) {
        for (int y = -r; y < r; y++) {
          for (int z = -r; z < r; z++) {
            if (std::sqrt(x * x + y * y + z * z) < 30.0f) {
              voxels.at(get_yzx_index(x+r, y+r, z+r)) = 1;
            }
          }
        }
      }
      break;
    }

    case (int)MESH_TYPE::EMPTY: {
      // empty!
      break;
    }
  }

  auto vertices = mesh(voxels);
  if (vertices == nullptr) {
    vertexCount = 0;
  } else {
    vertexCount = vertices->size();

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(uint32_t), vertices->data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    vertices->clear();
    vertices->shrink_to_fit();
    delete vertices;
  }

  printf("vertex count: %i\n", vertexCount);
}

int main(int argc, char* argv[]) {
  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit()) {
    fprintf(stderr, "Unable to initialize GLFW\n");
    return 1;
  }

  auto window = init_window();
  if (!window) {
    return 1;
  }
  glfwSetWindowPos(window, 0, 31);
  glfwSwapInterval(1);

  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetKeyCallback(window, key_callback);

  if (!init_opengl()) {
    fprintf(stderr, "Unable to initialize glad/opengl\n");
    return 1;
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBindVertexArray(VAO);
  glEnableVertexAttribArray(0);
  glVertexAttribIPointer(0, sizeof(uint32_t), GL_UNSIGNED_INT, sizeof(uint32_t), (void*)0);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  create_chunk();

  shader = new Shader("main", "main");
  camera = new Camera(glm::vec3(31, 65, -5));
  camera->handleResolution(1280, 720);

  srand(time(NULL));

  float forwardMove = 0.0f;
  float rightMove = 0.0f;
  float noclipSpeed = 10.0f;

  float deltaTime = 0.0f;

  auto lastFrame = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) forwardMove = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) forwardMove = -1.0f;
    else forwardMove = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) rightMove = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) rightMove = -1.0f;
    else rightMove = 0.0f;
    auto wishdir = (camera->front * forwardMove) + (camera->right * rightMove);
    camera->position += noclipSpeed * wishdir * deltaTime;

    if (vertexCount > 0) {
      shader->use();
      shader->setMat4("u_projection", camera->projection);
      shader->setMat4("u_view", camera->getViewMatrix());
      shader->setVec3("eye_position", camera->position);
      glBindVertexArray(VAO);
      glDrawArrays(GL_TRIANGLES, 0, vertexCount);
      glBindVertexArray(0);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}