#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifndef glClear
#include <GL/gl.h>
#endif
#define DEBUG
#include <cglm/cglm.h>
#include "glhelper.h"

struct GlhContext ctx;

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
    printf("\033[32m[GLDEBUG]\033[0m%s%s: %s\n", sev, typ, message);
}

int main() {
    
    if(!glfwInit()) {
        printf("glfw did not initialize\n");
        return -1;
    }

    GlhInitContext(&ctx, 640, 480, "nothing here");
    GLuint tex;
    GLuint tex2;
    loadTexture(&tex, "images/tuxs.png", true);
    loadTexture(&tex2, "images/texture.jpeg", false);
    ctx.camera.fov = glm_rad(45);
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
    printf("GL version: %s\n", glGetString(GL_VERSION));
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(handleDebugMessage, NULL);

    struct GlhProgram prg;
    GlhInitProgram(&prg, "shaders/shader.frag", "shaders/shader.vert", uniforms, 1, setUniforms);

    struct GlhObject obj;
    struct GlhObject ground;
    GlhInitObject(&obj,tex, GLM_VEC3_ONE, GLM_VEC3_ZERO, GLM_VEC3_ZERO, &mesh, &prg);
    GlhInitObject(&ground, tex2, GLM_VEC3_ONE, GLM_VEC3_ZERO, GLM_VEC3_ZERO, &mesh, &prg);

    glm_vec3_scale(obj.transforms.scale, 0.3, obj.transforms.scale);
    obj.transforms.translation[2] = -1;
    obj.transforms.translation[1] = -0.05;
    glm_vec3_scale(ground.transforms.scale, 1.4, ground.transforms.scale);
    ground.transforms.rotation[0] = M_PI_2;
    ground.transforms.rotation[2] = M_PI;
    ground.transforms.translation[1] = -0.3;
    ground.transforms.translation[2] = -2.1;
    GlhUpdateObjectModelMatrix(&ground);
    GlhContextAppendChild(&ctx, &ground);
    GlhContextAppendChild(&ctx, &obj);
    //ctx.camera.perspective = false;
    GlhComputeContextProjectionMatrix(&ctx);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    double time;
    while(!(glfwWindowShouldClose(ctx.window))) {
        time = glfwGetTime();
        obj.transforms.rotation[1] = sin(time*1.5) * 0.1;
        obj.transforms.rotation[2] = cos(time*1.5) * 0.05;
        obj.transforms.scale[0] = ( sin(time*2+1) + 1) / 2 / 12 + 0.3;
        obj.transforms.translation[0] = sin(time) / 4;
        obj.transforms.translation[2] = cos(time) / 4 -1.5;
        obj.transforms.translation[1] = fabs(cos(time* 4)) / 16;
        GlhComputeContextViewMatrix(&ctx);
        GlhUpdateObjectModelMatrix(&obj);
        GlhRenderContext(&ctx);
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    GlhFreeMesh(&mesh);
    GlhFreeObject(&obj);
    GlhFreeObject(&ground);
    GlhFreeProgram(&prg);
    GlhFreeContext(&ctx);
    glfwTerminate();

    return 0;
}