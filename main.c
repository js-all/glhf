#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string.h>
#include <unistd.h>
#ifndef glClear
#include <GL/gl.h>
#endif
#define DEBUG
#include <cglm/cglm.h>
#include "glhelper.h"

struct GlhContext ctx;
int width = 640;
int height = 480;

void setUniforms(struct GlhObject *obj, struct GlhContext *ctx) {
    mat4 mvp, mv;
    glm_mat4_mul(ctx->cachedViewMatrix, obj->cachedModelMatrix, mv);
    glm_mat4_mul(ctx->cachedProjectionMatrix, mv, mvp);
    glUniformMatrix4fv(vector_get(obj->program->uniformsLocation.data, 0, GLint), 1, GL_FALSE,(float*) mvp);
}

void handleDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) { 
    char* sev;
    char* typ;
    if(severity & GL_DEBUG_SEVERITY_HIGH) sev = "\033[33m[HIGH]\033[0m";
    else if(severity & GL_DEBUG_SEVERITY_MEDIUM) sev = "[MEDIUM]";
    else if(severity & GL_DEBUG_SEVERITY_LOW) sev = "[LOW]";
    else if(severity & GL_DEBUG_SEVERITY_NOTIFICATION) sev = "[NOTE]";
    if(type & GL_DEBUG_TYPE_ERROR) typ = "\033[33m[ERROR]\033[0m";
    else if(type & GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) typ = "\033[31m[DEPRECATED]\033[0m";
    else if(type & GL_DEBUG_TYPE_MARKER) typ = "[MARKER]";
    else if(type & GL_DEBUG_TYPE_PERFORMANCE) typ = "[PERFORMANCE]";
    else if(type & GL_DEBUG_TYPE_POP_GROUP) typ = "[POPGRP]";
    else if(type & GL_DEBUG_TYPE_PORTABILITY) typ = "[PORTABILITY]";
    else if(type & GL_DEBUG_TYPE_PUSH_GROUP) typ = "[PSHGRP]";
    else if(type & GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) typ = "[UNDEFINED_BEHAVIOUR]";
    printf("\033[32m[GLDEBUG]\033[0m%s%s: \033[34m%s\n\033[0m", sev, typ, message);
}

void framebuffer_size_callback(GLFWwindow* window, int wwidth, int wheight) {
    width = wwidth;
    height = wheight;
    glViewport(0, 0, width, height);
    GlhComputeContextProjectionMatrix(&ctx);
}

void mouse_pos_callback(GLFWwindow *window, double xpos, double ypos) {
    double yaw = (xpos / width * 2 - 1) * (M_PI_2);
    double pitch = (ypos / height * 2 - 1) * (M_PI_2);
    double roll = 0;

    double sensitivity = 1;

    yaw *= sensitivity;
    pitch *= sensitivity;
    roll *= sensitivity;

    ctx.camera.rotation[0] = pitch;
    ctx.camera.rotation[1] = yaw;
    ctx.camera.rotation[2] = roll;
    GlhComputeContextViewMatrix(&ctx);
}

int main() {
    
    if(!glfwInit()) {
        printf("glfw did not initialize\n");
        return -1;
    }
    GlhInitContext(&ctx, 640, 480, "nothing here");

    printf("\033[31mGL version: %s\n\033[0m", glGetString(GL_VERSION));
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(handleDebugMessage, NULL);

    glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);
    glfwSetCursorPosCallback(ctx.window, mouse_pos_callback);
    GlhInitFreeType();
    struct GlhMesh mesh;
    // fill mesh
    {
        vec3 verticies[] = {
            {-1, -1, 0}, {-1, 1, 0}, {1, 1, 0}, {1, -1, 0}
        };
        vec3 normals[] = {
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1}
        };
        vec3 indices[] = {
            {0, 1, 2}, {0, 2, 3}
        };
        vec2 texcoords[] = {
            {0, 0}, {0, 1}, {1, 1}, {1, 0}
        };
        GlhInitMesh(&mesh, verticies, sizeof(verticies) / sizeof(verticies[0]), normals, indices, sizeof(indices) / sizeof(indices[0]), texcoords, sizeof(texcoords) / sizeof(texcoords[0]));
        printf("\n");
    }

    char* uniforms[] = {
        "MVP"
    };

    struct GlhProgram prg;
    GlhInitProgram(&prg, "shaders/shader.frag", "shaders/shader.vert", uniforms, 1, setUniforms);

    struct GlhFont font;
    GlhInitFont(&font, "fonts/Roboto-Regular.ttf", 128, -1, 0.95);

    struct GlhTransforms tsf = GlhGetIdentityTransform();
    tsf.translation[2] = -1;
    glm_vec3_scale(tsf.scale, 0.1, tsf.scale);

    struct GlhTextObject to;
    vec4 color = {1.0, 1.0, 1.0, 1.0};
    vec4 backgoroundColor = {0.0, 0.0, 0.0, 0.8};
    GlhInitTextObject(&to, "Letters be rendered\0", &font, color, backgoroundColor, &tsf);
    GlhUpdateTextObjectModelMatrix(&to);

    // ctx.camera.perspective = false;

    GLuint tex;
    createSingleColorTexture(&tex, 1.0, 0.0, 0.0);

    struct GlhObject plane;
    GlhInitObject(&plane, tex, GLM_VEC3_ONE, GLM_VEC3_ZERO, GLM_VEC3_ZERO, &mesh, &prg);
    glm_vec3_scale(plane.transforms.scale, 20, plane.transforms.scale);
    plane.transforms.scale[0] *= 16.0 / 9;
    plane.transforms.translation[2] = -30;

    GlhUpdateObjectModelMatrix(&plane);
    GlhContextAppendChild(&ctx, &plane);
    GlhContextAppendChild(&ctx, &to);

    GlhComputeContextProjectionMatrix(&ctx);
    GlhComputeContextViewMatrix(&ctx);
    glClearColor(1, 1, 1, 1);
    while(!(glfwWindowShouldClose(ctx.window))) {
        float ratio = (float) width / height;
        struct GlhBoundingBox box = GlhTextObjectGetBoundingBox(&to, 0.2);
        GlhApplyTransformsToBoundingBox(&box, to.transforms);
        float toWidth = box.end[0] - box.start[0];

        to.transforms.translation[0] = ratio - toWidth + 0.2 * to.transforms.scale[0];
        GlhUpdateTextObjectModelMatrix(&to);

        double t = glfwGetTime();


        GlhRenderContext(&ctx);
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    GlhFreeMesh(&mesh);
    GlhFreeObject(&plane);
    GlhFreeProgram(&prg);
    GlhFreeContext(&ctx);
    glfwTerminate();
    return 0;
}