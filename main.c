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

GlhContext ctx;
int width = 640;
int height = 480;

void setUniforms(GlhObject *obj, GlhContext *ctx) {
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

    double sensitivity = -1;

    yaw *= sensitivity;
    pitch *= sensitivity;
    roll *= sensitivity;

    ctx.camera.rotation[0] = pitch;
    ctx.camera.rotation[1] = yaw;
    ctx.camera.rotation[2] = roll;
    GlhComputeContextViewMatrix(&ctx);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        ctx.camera.perspective = !ctx.camera.perspective;
            GlhComputeContextProjectionMatrix(&ctx);
    }
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
    glfwSetKeyCallback(ctx.window, key_callback);
    GlhInitFreeType();
    GlhMesh quadMesh;
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
        GlhInitMesh(&quadMesh, verticies, sizeof(verticies) / sizeof(verticies[0]), normals, indices, sizeof(indices) / sizeof(indices[0]), texcoords, sizeof(texcoords) / sizeof(texcoords[0]));
        printf("\n");
    }

    char* uniforms[] = {
        "MVP"
    };

    GlhProgram prg;
    GlhInitProgram(&prg, "shaders/shader.frag", "shaders/shader.vert", uniforms, 1, setUniforms);

    GlhFont font;
    GlhInitFont(&font, "fonts/Roboto-Regular.ttf", 128, -1, 0.95);

    GlhTransforms tsf = GlhGetIdentityTransform();
    tsf.translation[2] = -2;
    glm_vec3_scale(tsf.scale, 0.3, tsf.scale);

    GlhTextObject to0;
    GlhTextObject to1;
    GlhTextObject to2;
    GlhTextObject to3;
    vec4 color = {1.0, 1.0, 1.0, 1.0};
    vec4 backgoroundColor = {0.0, 0.0, 0.0, 0.0};
    GlhInitTextObject(&to0, "Game Name\0", &font, color, backgoroundColor, &tsf);
    glm_vec3_scale(tsf.scale, 0.5, tsf.scale);
    backgoroundColor[3] = 0.5;
    GlhInitTextObject(&to1, "play\0", &font, color, backgoroundColor, &tsf);
    GlhInitTextObject(&to2, "multiplayer\0", &font, color, backgoroundColor, &tsf);
    GlhInitTextObject(&to3, "quit\0", &font, color, backgoroundColor, &tsf);

    ctx.camera.perspective = false;

    GLuint tex;
    loadTexture(&tex, "images/nature.jpg", false, GL_NEAREST);

    GlhObject plane;
    GlhInitObject(&plane, tex, GLM_VEC3_ONE, GLM_VEC3_ZERO, GLM_VEC3_ZERO, &quadMesh, &prg);
    plane.transforms.translation[2] = -2.5;

    GlhContextAppendChild(&ctx, (GlhElement*)&plane);
    GlhContextAppendChild(&ctx, (GlhElement*)&to0);
    GlhContextAppendChild(&ctx, (GlhElement*)&to1);
    GlhContextAppendChild(&ctx, (GlhElement*)&to2);
    GlhContextAppendChild(&ctx, (GlhElement*)&to3);

    GlhComputeContextProjectionMatrix(&ctx);
    GlhComputeContextViewMatrix(&ctx);
    glClearColor(1, 1, 1, 1);

    GlhBoundingBox box0 = GlhTextObjectGetBoundingBox(&to0, 0.2);
    GlhBoundingBox box1 = GlhTextObjectGetBoundingBox(&to1, 0.2);
    GlhBoundingBox box2 = GlhTextObjectGetBoundingBox(&to2, 0.2);
    GlhBoundingBox box3 = GlhTextObjectGetBoundingBox(&to3, 0.2);
    GlhApplyTransformsToBoundingBox(&box0, to0.transforms);
    GlhApplyTransformsToBoundingBox(&box1, to1.transforms);
    GlhApplyTransformsToBoundingBox(&box2, to2.transforms);
    GlhApplyTransformsToBoundingBox(&box3, to3.transforms);
    float to0width = box0.end[0] - box0.start[0];
    float to0height = box0.end[1] - box0.start[1];
    float to1width = box1.end[0] - box1.start[0];
    float to1height = box1.end[1] - box1.start[1];
    float to2width = box2.end[0] - box2.start[0];
    float to2height = box2.end[1] - box2.start[1];
    float to3width = box3.end[0] - box3.start[0];
    float to3height = box3.end[1] - box3.start[1];
    while(!(glfwWindowShouldClose(ctx.window))) {
        float ratio = (float) width / height;
        float texRatio = 1920.0 / 1072;

        if(ratio > texRatio) {
            plane.transforms.scale[0] = ratio;
            plane.transforms.scale[1] = ratio / texRatio;
            GlhUpdateObjectModelMatrix(&plane);
        }

        to0.transforms.translation[0] = 0.1 * to0.transforms.scale[0] - to0width / 2;
        to0.transforms.translation[1] = 0.2 * to0.transforms.scale[1] - to0height / 2 + 0.7;
        to1.transforms.translation[0] = 0.1 * to1.transforms.scale[0] - to1width / 2;
        to1.transforms.translation[1] = 0.2 * to1.transforms.scale[1] - to1height / 2 + 0.3;
        to2.transforms.translation[0] = 0.1 * to2.transforms.scale[0] - to2width / 2;
        to2.transforms.translation[1] = 0.2 * to2.transforms.scale[1] - to2height / 2 + 0.0;
        to3.transforms.translation[0] = 0.1 * to3.transforms.scale[0] - to3width / 2;
        to3.transforms.translation[1] = 0.2 * to3.transforms.scale[1] - to3height / 2 + -0.3;
        GlhUpdateTextObjectModelMatrix(&to0);
        GlhUpdateTextObjectModelMatrix(&to1);
        GlhUpdateTextObjectModelMatrix(&to2);
        GlhUpdateTextObjectModelMatrix(&to3);

        GlhRenderContext(&ctx);
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    GlhFreeMesh(&quadMesh);
    GlhFreeObject(&plane);
    GlhFreeProgram(&prg);
    GlhFreeContext(&ctx);
    glfwTerminate();
    return 0;
}