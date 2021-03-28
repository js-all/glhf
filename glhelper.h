#define DEBUG
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "vector.h"
#include "maps.h"

//? should scale be a camera property ?, like a bigger camera displaying thing smaller
// in that case GlhCamera should just include a GlhTransform property
struct GlhCamera {
    vec3 position;
    vec3 rotation;
    bool perspective;
    float fov;
    float zNear;
    float zFar;
};

struct GlhTransforms {
    vec3 scale;
    vec3 translation;
    vec3 rotation;
    vec3 transformsOrigin;
};
// mostly internal stuff, should not be touched directly
struct GlhMeshBufferData {
    GLuint vertexBuffer;
    GLuint normalBuffer;
    GLuint indexsBuffer;
    GLuint tcoordBuffer;
    GLuint VAO;
    int vertexCount;
};

struct GlhMesh {
    struct GlhMeshBufferData bufferData;
    Vector verticies;
    Vector normals;
    Vector indexes;
    Vector texCoords;
};

enum GlhObjectTypes {
    regular,
    text
};
// here type must be the first member, to allow to check type before knowing what struct it is
struct GlhObject {
    enum GlhObjectTypes type;
    struct GlhTransforms transforms;
    // reference here, to be able to use the same mesh on multiple objects
    struct GlhMesh *mesh;
    // reference heren, to be able to use the same program on multiple objects
    struct GlhProgram *program;
    mat4 cachedModelMatrix;
    GLuint texture;
};

struct GlhTextObject {
    enum GlhObjectTypes type;
    struct GlhFont *font;
    struct GlhProgram *program;
    struct GlhTransforms transforms;
    struct GlhMeshBufferData bufferData;
    char* _text;
    size_t _textAllocated;
    mat4 cachedModelMatrix;
    vec4 color;
    Vector verticies;
    Vector texCoords;
};

struct GlhFont {
    GLuint texture;
    int textureSideLength;
    Map glyphsData;
};

struct GlhFontGLyphData {
    int x0;
    int y0;
    int x1;
    int y1;
    float wgl;
    float hgl;
    float x_off;
    float y_off;
    float advance;
};

//! as of now, applications should only have a single context, and would probably break otherwise
struct GlhContext {
    GLFWwindow *window;
    struct GlhCamera camera;
    mat4 cachedViewMatrix;
    mat4 cachedProjectionMatrix;
    Vector children;
};

struct GlhProgram {
    Vector uniforms;
    Vector uniformsLocation;
    Vector attributes;
    GLuint shaderProgram; 
    // a function pointer for a function setting the uniforms to their correct values.
    // it is not directly implemented in the helper as no shaders are provided by default
    // and as such should be dealt with by the user
    void (*setGlobalUniforms)(void*, struct GlhContext*);
};

struct GlhComputeShader {
    GLuint program;
};

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)());
void GlhFreeProgram(struct GlhProgram *prg);
// initialize context, windowWidth and windowHeight can be 0, windowTitle can be NULL
void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle);
void GlhFreeContext(struct GlhContext *ctx);
// add GlhObject child to context. (child can be TextObject or Object)
void GlhContextAppendChild(struct GlhContext *ctx, void *child);
// compute camera's view matrix, you most likely want to update it every frame
void GlhComputeContextViewMatrix(struct GlhContext *ctx);
// compute camera's projection matrix, does not need to be recomputed regularly unless specifics changes a made to the camera (fov, not transforms)
void GlhComputeContextProjectionMatrix(struct GlhContext *ctx);
// draw every object to the screen
void GlhRenderContext(struct GlhContext *ctx);
void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount);
// generates buffers for mesh, called internally, should not be called explicitly in most cases.
void GlhGenerateMeshBuffers(struct GlhMesh *mesh);
void GlhFreeMesh(struct GlhMesh *mesh);
// render an object, called internaly and doesn't do any buffer swaping and such, should not be called explicitly in most cases
void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx);
void GlhInitObject(struct GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program);
// update object matrix, should be called after any change to the transforms of an object
void GlhUpdateObjectModelMatrix(struct GlhObject *obj);
void GlhFreeObject(struct GlhObject *obj);
// used to load and setup a texture from a file
int loadTexture(GLuint *texture, char* filename, bool alpha);
// initialize freetype, needs to be called before working with fonts, only once (or after freeing the last one)
void GlhInitFreeType();
// stops freetype should be called when every fonts has been initilized or at the end or execution
void GlhFreeFreeType();
// init a font, size is the resolution of the characters, glyphcount the number of them (can be -1 for all in the font,
// will have multiple times the same characters if glyphcount is higher than the total number of glyphs in the font)
// packingPrecision is how dense (and how long it takes to compute) will the packing be, from 0 to 1 (exclusive)
// 1 will be defaulted to 0.99, 0 to 0.75, the higher the value, the denser it gets.
void GlhInitFont(struct GlhFont *font, char* ttfFileName, int size, int glyphCount, float packingPrecision);
void GlhFreeFont(struct GlhFont *font);
void GlhTextObjectUpdateMesh(struct GlhTextObject *tob, char* OldString);
void GlhUpdateTextObjectModelMatrix(struct GlhTextObject *tob);
void GlhTextObjectSetText(struct GlhTextObject *tob, char* string);
char* GlhTextObjectGetText(struct GlhTextObject *tob);
// transforms can be NULL
void GlhInitTextObject(struct GlhTextObject *tob, char* string, struct GlhFont *font, struct GlhProgram *prg, vec4 color, struct GlhTransforms *tsf);
void GlhRenderTextObject(struct GlhTextObject *tob, struct GlhContext *ctx);
void GlhFreeTextObject(struct GlhTextObject *tob);
void GlhInitComputeShader(struct GlhComputeShader *cs, char* filename);
void GlhRunComputeShader(struct GlhComputeShader *cs, GLuint inputTexture, GLuint outputTexture, GLenum sizedInFormat, GLenum sizedOutFormat, int workGroupsWidth, int workGroupsHeight);
void createSingleColorTexture(GLuint *texture, float r, float g, float b);
void createEmptySizedTexture(GLuint *texture, int width, int height, GLenum sizedFormat, GLenum format, GLenum type);
