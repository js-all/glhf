#ifndef _GLHELPER_H
#define _GLHELPER_H
#define DEBUG
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "vector.h"
#include "maps.h"

// do it in advance because circular dependency
typedef struct GlhContext GlhContext;

//? should scale be a camera property ?, like a bigger camera displaying thing smaller
// in that case GlhCamera should just include a GlhTransform property
typedef struct {
    vec3 position;
    vec3 rotation;
    bool perspective;
    float fov;
    float zNear;
    float zFar;
} GlhCamera;

typedef struct {
    vec3 scale;
    vec3 translation;
    vec3 rotation;
    vec3 transformsOrigin;
} GlhTransforms;
// mostly internal stuff, should not be touched directly
typedef struct {
    GLuint vertexBuffer;
    GLuint normalBuffer;
    GLuint indexsBuffer;
    GLuint tcoordBuffer;
    GLuint colorsBuffer;
    GLuint VAO;
    int vertexCount;
} GlhMeshBufferData;

typedef struct {
    GlhMeshBufferData bufferData;
    Vector verticies;
    Vector normals;
    Vector indexes;
    Vector texCoords;
} GlhMesh;

typedef struct {
    Vector uniforms;
    Vector uniformsLocation;
    Vector attributes;
    GLuint shaderProgram; 
    // a function pointer for a function setting the uniforms to their correct values.
    // it is not directly implemented in the helper as no shaders are provided by default
    // and as such should be dealt with by the user
    void (*setGlobalUniforms)(void*, GlhContext *);
} GlhProgram;

typedef enum {
    regular,
    text
} GlhObjectTypes;

typedef struct {
    GlhObjectTypes type;
} GlhAbstractElement;

// here type must be the first member, to allow to check type before knowing what struct it is
typedef struct {
    GlhObjectTypes type;
    GlhTransforms transforms;
    // reference here, to be able to use the same mesh on multiple objects
    GlhMesh *mesh;
    // reference heren, to be able to use the same program on multiple objects
    GlhProgram *program;
    mat4 cachedModelMatrix;
    GLuint texture;
} GlhObject;

typedef struct {
    GLuint texture;
    int textureSideLength;
    Map glyphsData;
} GlhFont;

typedef struct {
    GlhObjectTypes type;
    GlhFont *font;
    GlhProgram *glyphProgram;
    GlhProgram *textProgram;
    GlhTransforms transforms;
    GlhMeshBufferData bufferData;
    GlhMeshBufferData backgroundQuadBufferData;
    char* _text;
    size_t _textAllocated;
    mat4 cachedModelMatrix;
    vec4 color;
    vec4 backgroundColor;
    Vector verticies;
    Vector texCoords;
} GlhTextObject;

typedef union {
    GlhAbstractElement any;
    GlhObject regular;
    GlhTextObject text;
} GlhElement;

typedef struct {
    int x0;
    int y0;
    int x1;
    int y1;
    float wgl;
    float hgl;
    float x_off;
    float y_off;
    float advance;
} GlhFontGLyphData;

typedef struct {
    vec3 start;
    vec3 end;
} GlhBoundingBox;

typedef enum {
    FBSizedTexture
} GlhFBOType;

typedef struct {
    GLuint FBO;
    GLuint attachments[2];
    bool active;
    GlhFBOType type;
    unsigned long id;
} GlhFBO;

typedef struct {
    Vector FBOs;
    GlhContext *ctx;
} GlhFBOProvider;

//! as of now, applications should only have a single context, and would probably break otherwise
struct GlhContext{
    GLFWwindow *window;
    GlhCamera camera;
    mat4 cachedViewMatrix;
    mat4 cachedProjectionMatrix;
    Vector children;
    GlhFBOProvider FBOProvider;
};

typedef struct {
    GlhProgram glyphs;
    GlhProgram text;
} GlhGlobalShaders;

// ^([a-z]+) ([a-zA-Z]+) \{((?:\n[^}]+)+)\};
// typedef $1 {$3} $2;
typedef struct {
    GLuint program;
} GlhComputeShader;

void GlhInitProgram(GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)());
void GlhFreeProgram(GlhProgram *prg);
// initialize context, windowWidth and windowHeight can be 0, windowTitle can be NULL
void GlhInitContext(GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle);
void GlhFreeContext(GlhContext *ctx);
// add GlhObject child to context. (child can be TextObject or Object)
void GlhContextAppendChild(GlhContext *ctx, GlhElement *child);
// compute camera's view matrix, you most likely want to update it every frame
void GlhComputeContextViewMatrix(GlhContext *ctx);
// compute camera's projection matrix, does not need to be recomputed regularly unless specifics changes a made to the camera (fov, not transforms)
void GlhComputeContextProjectionMatrix(GlhContext *ctx);
// draw every object to the screen
void GlhRenderContext(GlhContext *ctx);
void GlhInitMesh(GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount);
// generates buffers for mesh, called internally, should not be called explicitly in most cases.
void GlhGenerateMeshBuffers(GlhMesh *mesh);
void GlhFreeMesh(GlhMesh *mesh);
// render an object, called internaly and doesn't do any buffer swaping and such, should not be called explicitly in most cases
void GlhRenderObject(GlhObject *obj, GlhContext *ctx);
void GlhInitObject(GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, GlhMesh *mesh, GlhProgram *program);
// update object matrix, should be called after any change to the transforms of an object
void GlhUpdateObjectModelMatrix(GlhObject *obj);
void GlhFreeObject(GlhObject *obj);
// used to load and setup a texture from a file
int loadTexture(GLuint *texture, char* filename, bool alpha, GLenum interpolation);
// initialize freetype, needs to be called before working with fonts, only once (or after freeing the last one)
void GlhInitFreeType();
// stops freetype should be called when every fonts has been initilized or at the end or execution
void GlhFreeFreeType();
// init a font, size is the resolution of the characters, glyphcount the number of them (can be -1 for all in the font,
// will have multiple times the same characters if glyphcount is higher than the total number of glyphs in the font)
// packingPrecision is how dense (and how long it takes to compute) will the packing be, from 0 to 1 (exclusive)
// 1 will be defaulted to 0.99, 0 to 0.75, the higher the value, the denser it gets.
void GlhInitFont(GlhFont *font, char* ttfFileName, int size, int glyphCount, float packingPrecision);
void GlhFreeFont(GlhFont *font);
float GlhFontGetTextWidth(GlhFont *font, char* text);
GlhBoundingBox GlhTextObjectGetBoundingBox(GlhTextObject *tob, float margin);
void GlhApplyTransformsToBoundingBox(GlhBoundingBox *box, GlhTransforms transforms);
void GlhTextObjectUpdateMesh(GlhTextObject *tob, char* OldString);
void GlhUpdateTextObjectModelMatrix(GlhTextObject *tob);
void GlhTextObjectSetText(GlhTextObject *tob, char* string);
char* GlhTextObjectGetText(GlhTextObject *tob);
// transforms can be NULL
void GlhInitTextObject(GlhTextObject *tob, char* string, GlhFont *font, vec4 color, vec4 backgroundColor, GlhTransforms *tsf);
void GlhRenderTextObject(GlhTextObject *tob, GlhContext *ctx);
void GlhFreeTextObject(GlhTextObject *tob);
GlhTransforms GlhGetIdentityTransform();
GlhFBO GlhRequestFBO(GlhFBOProvider *provider, GlhFBOType type);
void GlhReleaseFBO(GlhFBOProvider provider, GlhFBO fbo);
void saveImage(char* filepath, GLFWwindow* w);
void GlhInitComputeShader(GlhComputeShader *cs, char* filename);
void GlhRunComputeShader(GlhComputeShader *cs, GLuint inputTexture, GLuint outputTexture, GLenum sizedInFormat, GLenum sizedOutFormat, int workGroupsWidth, int workGroupsHeight);
void createSingleColorTexture(GLuint *texture, float r, float g, float b);
void createEmptySizedTexture(GLuint *texture, int width, int height, GLenum sizedFormat, GLenum format, GLenum type);
#endif