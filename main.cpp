// main.cpp: a 4-car, 3-lap race with menus, a HUD, trees, lighting, collisions, drifting, shadows, and sound.

#define GL_SILENCE_DEPRECATION     // hide Apple's "OpenGL is old" warnings
#define GLFW_INCLUDE_GLCOREARB     // load modern OpenGL functions (before the GLFW include)

#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <cstdlib>      // NEW: std::system, which runs a Terminal command from inside our program

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------------------------------------------------
// SHADERS (unchanged)
// ---------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uTint;
out vec3 vColor;
out vec3 vNormal;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor * uTint;
    vNormal = mat3(uModel) * aNormal;
}
)";

const char* fragmentShaderSource = R"(
#version 410 core
in vec3 vColor;
in vec3 vNormal;
uniform float uAlpha;
uniform float uUseLighting;
uniform vec3  uLightDir;
out vec4 FragColor;
void main() {
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(uLightDir)), 0.0);
    vec3 ambient = mix(vec3(0.28, 0.25, 0.22), vec3(0.42, 0.48, 0.58), n.y * 0.5 + 0.5);
    vec3 sun = vec3(1.0, 0.95, 0.82) * 0.65;
    vec3 lit = vColor * (ambient + sun * diffuse);
    FragColor = vec4(mix(vColor, lit, uUseLighting), uAlpha);
}
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cout << "Shader compile error:\n" << log << std::endl;
    }
    return shader;
}

// ---------------------------------------------------------------------
// NEW: PLAYING A SOUND
// Your Mac has built-in sounds in /System/Library/Sounds (Basso.aiff, Glass.aiff, and so on),
// and a Terminal tool called "afplay" that plays a sound file. This function builds
// the Terminal command text, for example:
//     afplay /System/Library/Sounds/Basso.aiff > /dev/null 2>&1 &
// The "> /dev/null 2>&1" part throws away any text the command would print.
// The "&" at the end runs the command in the BACKGROUND, so the game doesn't
// freeze while the sound plays.
// ---------------------------------------------------------------------
void playSound(const std::string& soundName) {
    std::string command = "afplay /System/Library/Sounds/" + soundName + ".aiff > /dev/null 2>&1 &";
    std::system(command.c_str());   // run the command
}

// ---------------------------------------------------------------------
// MESH HELPER (unchanged): each vertex is x y z, r g b, nx ny nz
// ---------------------------------------------------------------------
struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

Mesh createMesh(const std::vector<float>& vertices,
                const std::vector<unsigned int>& indices) {
    Mesh mesh;
    mesh.indexCount = (GLsizei)indices.size();

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return mesh;
}

void destroyMesh(Mesh& mesh) {
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteBuffers(1, &mesh.ebo);
}

// ---------------------------------------------------------------------
// BOX HELPER (unchanged)
// ---------------------------------------------------------------------
Mesh createBoxMesh(float r, float g, float b) {
    static const float corners[6][4][3] = {
        {{-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},  // top
        {{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}},  // bottom
        {{-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},  // front (+Z)
        {{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}},  // back (-Z)
        {{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}},  // left (-X)
        {{ 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}}   // right (+X)
    };

    static const float normals[6][3] = {
        { 0.0f,  1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},
        { 0.0f,  0.0f,  1.0f},
        { 0.0f,  0.0f, -1.0f},
        {-1.0f,  0.0f,  0.0f},
        { 1.0f,  0.0f,  0.0f}
    };

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int face = 0; face < 6; face++) {
        for (int corner = 0; corner < 4; corner++) {
            vertices.push_back(corners[face][corner][0]);
            vertices.push_back(corners[face][corner][1]);
            vertices.push_back(corners[face][corner][2]);
            vertices.push_back(r);
            vertices.push_back(g);
            vertices.push_back(b);
            vertices.push_back(normals[face][0]);
            vertices.push_back(normals[face][1]);
            vertices.push_back(normals[face][2]);
        }
        unsigned int base = face * 4;
        indices.push_back(base);     indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base);
    }

    return createMesh(vertices, indices);
}

// ---------------------------------------------------------------------
// BLOB SHADOW MESH (unchanged)
// ---------------------------------------------------------------------
Mesh createBlobMesh(int segments) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f});

    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * 3.14159265f * (float)i / (float)segments;
        vertices.insert(vertices.end(),
            {std::cos(angle), 0.0f, std::sin(angle),   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f});
    }

    for (int i = 0; i < segments; i++) {
        indices.push_back(0);
        indices.push_back(1 + i);
        indices.push_back(1 + ((i + 1) % segments));
    }

    return createMesh(vertices, indices);
}

// ---------------------------------------------------------------------
// CAR AND TREE STATE (unchanged)
// ---------------------------------------------------------------------
struct Car {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    float heading = 0.0f;   // 0 = facing -Z
    float speed   = 0.0f;   // speed FORWARD, along the way the car points
    float slide   = 0.0f;   // speed SIDEWAYS, to the car's right (negative = to the left)
};

struct Tree {
    glm::vec3 position = glm::vec3(0.0f);
    float scale    = 1.0f;
    float rotation = 0.0f;
};

struct AIRacer {
    Car   car;
    float t = 0.0f;
    float startT = 0.0f;
    float speed = 18.0f;
    float lane = 0.0f;
    int   laps = 0;
    const Mesh* bodyMesh = nullptr;
};

// ---------------------------------------------------------------------
// GAME STATES (unchanged)
// ---------------------------------------------------------------------
enum class GameState {
    Menu,
    Playing,
    Paused,
    Finished
};

// ---------------------------------------------------------------------
// TRACK SHAPE (unchanged)
// ---------------------------------------------------------------------
const float PI              = 3.14159265f;
const float TRACK_A         = 60.0f;
const float TRACK_B         = 35.0f;
const float ROAD_HALF_WIDTH = 5.0f;
const int   TRACK_SEGMENTS  = 160;

const float WALL_DISTANCE  = 8.0f;
const float WALL_THICKNESS = 0.6f;
const float WALL_HEIGHT    = 1.0f;
const float CAR_RADIUS     = 1.5f;
const int   TRACK_SAMPLES  = 1440;

const float CAR_HALF_LENGTH = 2.0f;
const float CAR_HALF_WIDTH  = 0.9f;

const int   TOTAL_LAPS     = 3;
const int   TOTAL_RACERS   = 4;

glm::vec3 trackCenter(float t) {
    return glm::vec3(TRACK_A * std::cos(t), 0.0f, -TRACK_B * std::sin(t));
}

glm::vec3 trackTangent(float t) {
    return glm::normalize(glm::vec3(-TRACK_A * std::sin(t), 0.0f, -TRACK_B * std::cos(t)));
}

glm::vec3 trackRight(float t) {
    return glm::normalize(glm::cross(trackTangent(t), glm::vec3(0.0f, 1.0f, 0.0f)));
}

float headingFromDirection(const glm::vec3& direction) {
    return std::atan2(-direction.x, -direction.z);
}

glm::vec3 forwardOf(float heading) {
    return glm::vec3(-std::sin(heading), 0.0f, -std::cos(heading));
}
glm::vec3 rightOf(float heading) {
    return glm::vec3(std::cos(heading), 0.0f, -std::sin(heading));
}

struct CollisionResult {
    bool      hit    = false;
    glm::vec3 normal = glm::vec3(0.0f);
    float     depth  = 0.0f;
};

CollisionResult testCarCollision(const Car& a, const Car& b) {
    CollisionResult result;

    glm::vec3 forwardA = forwardOf(a.heading);
    glm::vec3 rightA   = rightOf(a.heading);
    glm::vec3 forwardB = forwardOf(b.heading);
    glm::vec3 rightB   = rightOf(b.heading);

    glm::vec3 axes[4] = { forwardA, rightA, forwardB, rightB };

    glm::vec3 delta = a.position - b.position;
    delta.y = 0.0f;

    float smallestOverlap = 1.0e30f;
    glm::vec3 bestAxis(0.0f);

    for (int i = 0; i < 4; i++) {
        const glm::vec3& axis = axes[i];

        float reachA = CAR_HALF_LENGTH * std::abs(glm::dot(forwardA, axis))
                     + CAR_HALF_WIDTH  * std::abs(glm::dot(rightA,   axis));
        float reachB = CAR_HALF_LENGTH * std::abs(glm::dot(forwardB, axis))
                     + CAR_HALF_WIDTH  * std::abs(glm::dot(rightB,   axis));

        float distance = glm::dot(delta, axis);
        float overlap = reachA + reachB - std::abs(distance);

        if (overlap <= 0.0f) {
            return result;
        }

        if (overlap < smallestOverlap) {
            smallestOverlap = overlap;
            bestAxis = (distance >= 0.0f) ? axis : -axis;
        }
    }

    result.hit    = true;
    result.normal = bestAxis;
    result.depth  = smallestOverlap;
    return result;
}

void addQuad(std::vector<float>& vertices, std::vector<unsigned int>& indices,
             glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
             glm::vec3 color, glm::vec3 normal) {
    unsigned int base = (unsigned int)(vertices.size() / 9);
    glm::vec3 corners[4] = {a, b, c, d};
    for (int i = 0; i < 4; i++) {
        vertices.push_back(corners[i].x);
        vertices.push_back(corners[i].y);
        vertices.push_back(corners[i].z);
        vertices.push_back(color.r);
        vertices.push_back(color.g);
        vertices.push_back(color.b);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
    }
    indices.push_back(base);     indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base);
}

Mesh createRoadMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const glm::vec3 roadLift(0.0f, 0.02f, 0.0f);
    const glm::vec3 normalUp(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < TRACK_SEGMENTS; i++) {
        float t0 = 2.0f * PI * (float)i       / (float)TRACK_SEGMENTS;
        float t1 = 2.0f * PI * (float)(i + 1) / (float)TRACK_SEGMENTS;

        glm::vec3 c0 = trackCenter(t0) + roadLift;
        glm::vec3 c1 = trackCenter(t1) + roadLift;
        glm::vec3 r0 = trackRight(t0);
        glm::vec3 r1 = trackRight(t1);

        glm::vec3 color = (i % 2 == 0) ? glm::vec3(0.25f, 0.25f, 0.28f)
                                       : glm::vec3(0.29f, 0.29f, 0.32f);

        addQuad(vertices, indices,
                c0 - r0 * ROAD_HALF_WIDTH,
                c0 + r0 * ROAD_HALF_WIDTH,
                c1 + r1 * ROAD_HALF_WIDTH,
                c1 - r1 * ROAD_HALF_WIDTH,
                color, normalUp);
    }
    return createMesh(vertices, indices);
}

Mesh createLineMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const glm::vec3 lineLift(0.0f, 0.04f, 0.0f);
    const glm::vec3 white(0.92f, 0.92f, 0.92f);
    const glm::vec3 normalUp(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < TRACK_SEGMENTS; i++) {
        float t0 = 2.0f * PI * (float)i       / (float)TRACK_SEGMENTS;
        float t1 = 2.0f * PI * (float)(i + 1) / (float)TRACK_SEGMENTS;

        glm::vec3 c0 = trackCenter(t0) + lineLift;
        glm::vec3 c1 = trackCenter(t1) + lineLift;
        glm::vec3 r0 = trackRight(t0);
        glm::vec3 r1 = trackRight(t1);

        for (int side = -1; side <= 1; side += 2) {
            float offset = (float)side * (ROAD_HALF_WIDTH - 0.35f);
            addQuad(vertices, indices,
                    c0 + r0 * (offset - 0.15f),
                    c0 + r0 * (offset + 0.15f),
                    c1 + r1 * (offset + 0.15f),
                    c1 + r1 * (offset - 0.15f),
                    white, normalUp);
        }

        if (i % 4 < 2) {
            addQuad(vertices, indices,
                    c0 - r0 * 0.125f,
                    c0 + r0 * 0.125f,
                    c1 + r1 * 0.125f,
                    c1 - r1 * 0.125f,
                    white, normalUp);
        }
    }
    return createMesh(vertices, indices);
}

Mesh createBarrierMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const glm::vec3 lift(0.0f, WALL_HEIGHT, 0.0f);
    const glm::vec3 normalUp(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < TRACK_SEGMENTS; i++) {
        float t0 = 2.0f * PI * (float)i       / (float)TRACK_SEGMENTS;
        float t1 = 2.0f * PI * (float)(i + 1) / (float)TRACK_SEGMENTS;

        glm::vec3 c0 = trackCenter(t0);
        glm::vec3 c1 = trackCenter(t1);
        glm::vec3 r0 = trackRight(t0);
        glm::vec3 r1 = trackRight(t1);

        glm::vec3 color = ((i / 2) % 2 == 0) ? glm::vec3(0.85f, 0.10f, 0.10f)
                                             : glm::vec3(0.90f, 0.90f, 0.90f);

        for (int side = -1; side <= 1; side += 2) {
            float inner = (float)side * WALL_DISTANCE;
            float outer = (float)side * (WALL_DISTANCE + WALL_THICKNESS);

            glm::vec3 in0  = c0 + r0 * inner;
            glm::vec3 in1  = c1 + r1 * inner;
            glm::vec3 out0 = c0 + r0 * outer;
            glm::vec3 out1 = c1 + r1 * outer;

            glm::vec3 faceNormal = -(float)side * r0;

            addQuad(vertices, indices, in0, in1, in1 + lift, in0 + lift, color, faceNormal);
            addQuad(vertices, indices, in0 + lift, in1 + lift, out1 + lift, out0 + lift, color, normalUp);
        }
    }
    return createMesh(vertices, indices);
}

int nearestCenterIndex(const std::vector<glm::vec3>& samples, const glm::vec3& pos) {
    float bestDistSq = 1.0e30f;
    int bestIndex = 0;
    for (int i = 0; i < (int)samples.size(); i++) {
        float dx = pos.x - samples[i].x;
        float dz = pos.z - samples[i].z;
        float distSq = dx * dx + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// ---------------------------------------------------------------------
// THE DOT-MATRIX FONT (unchanged)
// ---------------------------------------------------------------------
const unsigned char* getGlyph(char c) {
    static const unsigned char gA[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const unsigned char gC[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const unsigned char gD[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const unsigned char gE[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const unsigned char gF[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const unsigned char gG[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
    static const unsigned char gI[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const unsigned char gL[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const unsigned char gM[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const unsigned char gN[7] = {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
    static const unsigned char gO[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const unsigned char gP[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const unsigned char gQ[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    static const unsigned char gR[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const unsigned char gS[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const unsigned char gT[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const unsigned char gU[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const unsigned char gV[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const unsigned char gW[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
    static const unsigned char gY[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};

    switch (c) {
        case 'A': return gA;  case 'C': return gC;  case 'D': return gD;
        case 'E': return gE;  case 'F': return gF;  case 'G': return gG;
        case 'I': return gI;  case 'L': return gL;  case 'M': return gM;
        case 'N': return gN;  case 'O': return gO;  case 'P': return gP;
        case 'Q': return gQ;  case 'R': return gR;  case 'S': return gS;
        case 'T': return gT;  case 'U': return gU;  case 'V': return gV;
        case 'W': return gW;  case 'Y': return gY;
        default:  return nullptr;
    }
}

int main() {
    // --- Window setup (unchanged) ---
    if (!glfwInit()) {
        std::cout << "GLFW failed to start." << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Racing Game", nullptr, nullptr);
    if (!window) {
        std::cout << "Could not create the window." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // --- Shader program (unchanged) ---
    GLuint vertexShader   = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linked;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
        std::cout << "Shader link error:\n" << log << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- World meshes (unchanged) ---
    Mesh grass = createMesh(
        {
            -200.0f, 0.0f, -200.0f,   0.20f, 0.60f, 0.20f,  0.0f, 1.0f, 0.0f,
             200.0f, 0.0f, -200.0f,   0.20f, 0.60f, 0.20f,  0.0f, 1.0f, 0.0f,
             200.0f, 0.0f,  200.0f,   0.20f, 0.60f, 0.20f,  0.0f, 1.0f, 0.0f,
            -200.0f, 0.0f,  200.0f,   0.20f, 0.60f, 0.20f,  0.0f, 1.0f, 0.0f
        },
        { 0, 1, 2,  2, 3, 0 });

    Mesh road     = createRoadMesh();
    Mesh lines    = createLineMesh();
    Mesh barriers = createBarrierMesh();

    std::vector<glm::vec3> trackSamples;
    for (int i = 0; i < TRACK_SAMPLES; i++) {
        float t = 2.0f * PI * (float)i / (float)TRACK_SAMPLES;
        trackSamples.push_back(trackCenter(t));
    }

    // --- Car parts (unchanged) ---
    Mesh bodyRed    = createBoxMesh(0.80f, 0.10f, 0.10f);
    Mesh bodyBlue   = createBoxMesh(0.10f, 0.25f, 0.85f);
    Mesh bodyPurple = createBoxMesh(0.55f, 0.20f, 0.75f);
    Mesh bodyYellow = createBoxMesh(0.95f, 0.80f, 0.10f);
    Mesh cabinBox   = createBoxMesh(0.15f, 0.20f, 0.30f);
    Mesh wheelBox   = createBoxMesh(0.08f, 0.08f, 0.08f);
    Mesh headBox    = createBoxMesh(1.00f, 0.90f, 0.40f);
    Mesh tailBox    = createBoxMesh(0.90f, 0.00f, 0.00f);

    // --- Tree parts (unchanged) ---
    Mesh trunkBox = createBoxMesh(0.40f, 0.25f, 0.12f);
    Mesh leafLow  = createBoxMesh(0.12f, 0.45f, 0.15f);
    Mesh leafHigh = createBoxMesh(0.16f, 0.58f, 0.20f);

    Mesh blob = createBlobMesh(24);

    // --- HUD square (unchanged) ---
    Mesh hudQuad = createMesh(
        {
            0.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f, 1.0f
        },
        { 0, 1, 2,  2, 3, 0 });

    GLint mvpLocation      = glGetUniformLocation(shaderProgram, "uMVP");
    GLint modelLocation    = glGetUniformLocation(shaderProgram, "uModel");
    GLint tintLocation     = glGetUniformLocation(shaderProgram, "uTint");
    GLint alphaLocation    = glGetUniformLocation(shaderProgram, "uAlpha");
    GLint lightingLocation = glGetUniformLocation(shaderProgram, "uUseLighting");
    GLint lightDirLocation = glGetUniformLocation(shaderProgram, "uLightDir");

    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.6f, 1.0f, 0.4f));
    glm::vec3 shadowDirection = -glm::normalize(glm::vec3(sunDirection.x, 0.0f, sunDirection.z));

    // --- Plant the trees (unchanged) ---
    std::vector<Tree> trees;

    uint32_t seed = 12345u;
    auto random01 = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (float)(seed >> 8) / 16777216.0f;
    };

    for (int i = 0; i < 160; i++) {
        float t = random01() * 2.0f * PI;
        float side = (random01() < 0.75f) ? 1.0f : -1.0f;
        float distance = (side > 0.0f) ? (13.0f + random01() * 45.0f)
                                       : (12.0f + random01() * 6.0f);

        glm::vec3 treePosition = trackCenter(t) + trackRight(t) * side * distance;

        int nearestIdx = nearestCenterIndex(trackSamples, treePosition);
        glm::vec3 fromRoad = treePosition - trackSamples[nearestIdx];
        fromRoad.y = 0.0f;
        if (glm::length(fromRoad) < 11.0f) {
            continue;
        }

        Tree tree;
        tree.position = treePosition;
        tree.scale    = 0.8f + random01() * 0.7f;
        tree.rotation = random01() * 2.0f * PI;
        trees.push_back(tree);
    }

    // --- The player's car ---
    Car car;
    car.position = trackCenter(0.0f);
    car.heading  = 0.0f;

    const float maxSpeed     = 25.0f;
    const float maxReverse   = 8.0f;
    const float acceleration = 10.0f;
    const float brakePower   = 20.0f;
    const float friction     = 5.0f;
    const float turnRate     = 1.8f;

    const float gripNormal     = 14.0f;
    const float gripDrift      = 3.0f;
    const float driftTurnBoost = 1.35f;

    // --- The AI cars (unchanged) ---
    std::vector<AIRacer> aiRacers(3);

    aiRacers[0].startT   = 0.34f;
    aiRacers[0].speed    = 20.0f;
    aiRacers[0].lane     = -2.5f;
    aiRacers[0].bodyMesh = &bodyBlue;

    aiRacers[1].startT   = 0.27f;
    aiRacers[1].speed    = 17.0f;
    aiRacers[1].lane     = 2.5f;
    aiRacers[1].bodyMesh = &bodyPurple;

    aiRacers[2].startT   = 0.20f;
    aiRacers[2].speed    = 14.0f;
    aiRacers[2].lane     = 0.0f;
    aiRacers[2].bodyMesh = &bodyYellow;

    auto placeAI = [&](AIRacer& ai) {
        ai.car.position = trackCenter(ai.t) + trackRight(ai.t) * ai.lane;
        ai.car.heading  = headingFromDirection(trackTangent(ai.t));
        ai.car.speed    = ai.speed;
    };

    for (AIRacer& ai : aiRacers) {
        ai.t = ai.startT;
        placeAI(ai);
    }

    // --- Race state (unchanged) ---
    int   lapsCompleted = 0;
    int   previousIndex = nearestCenterIndex(trackSamples, car.position);
    bool  raceStarted   = false;
    bool  raceFinished  = false;
    float raceTime      = 0.0f;
    int   finalPosition = 0;

    GameState state = GameState::Menu;

    // NEW: sound settings.
    bool  soundOn       = true;    // N turns sound on and off
    float crashCooldown = 0.0f;    // seconds until another crash sound is allowed

    // NEW: plays a sound only if sound is turned on.
    auto sfx = [&](const std::string& soundName) {
        if (soundOn) {
            playSound(soundName);
        }
    };

    auto resetRace = [&]() {
        car = Car();
        car.position = trackCenter(0.0f);
        car.heading  = 0.0f;

        for (AIRacer& ai : aiRacers) {
            ai.t    = ai.startT;
            ai.laps = 0;
            placeAI(ai);
        }

        lapsCompleted = 0;
        previousIndex = nearestCenterIndex(trackSamples, car.position);
        raceStarted   = false;
        raceFinished  = false;
        raceTime      = 0.0f;
        finalPosition = 0;
        crashCooldown = 0.0f;
    };

    auto computePosition = [&]() {
        int idx = nearestCenterIndex(trackSamples, car.position);
        float playerProgress = (float)lapsCompleted + (float)idx / (float)TRACK_SAMPLES;

        int carsAhead = 0;
        for (const AIRacer& ai : aiRacers) {
            float aiProgress = (float)ai.laps + ai.t / (2.0f * PI);
            if (aiProgress > playerProgress) {
                carsAhead++;
            }
        }
        return 1 + carsAhead;
    };

    bool keyWasDown[GLFW_KEY_LAST + 1] = {false};
    auto justPressed = [&](int key) {
        bool down    = (glfwGetKey(window, key) == GLFW_PRESS);
        bool pressed = down && !keyWasDown[key];
        keyWasDown[key] = down;
        return pressed;
    };

    double lastTime = glfwGetTime();

    // --- Game loop ---
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f;

        bool enterPressed = justPressed(GLFW_KEY_ENTER);
        bool escPressed   = justPressed(GLFW_KEY_ESCAPE);
        bool pPressed     = justPressed(GLFW_KEY_P);
        bool rPressed     = justPressed(GLFW_KEY_R);
        bool mPressed     = justPressed(GLFW_KEY_M);
        bool nPressed     = justPressed(GLFW_KEY_N);     // NEW

        // NEW: N flips sound on/off, in every state. (The ! means "not": it flips true to false and back.)
        if (nPressed) {
            soundOn = !soundOn;
        }

        // ---------- State machine ----------
        if (state == GameState::Menu) {
            if (enterPressed) {
                resetRace();
                state = GameState::Playing;
                sfx("Pop");                       // NEW: a small "go" sound
            }
            if (escPressed) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        } else if (state == GameState::Playing) {
            if (pPressed || escPressed) {
                state = GameState::Paused;
            } else if (rPressed) {
                resetRace();
            }
        } else if (state == GameState::Paused) {
            if (pPressed || escPressed) {
                state = GameState::Playing;
            } else if (rPressed) {
                resetRace();
                state = GameState::Playing;
            } else if (mPressed) {
                resetRace();
                state = GameState::Menu;
            }
        } else if (state == GameState::Finished) {
            if (rPressed) {
                resetRace();
                state = GameState::Playing;
            } else if (mPressed) {
                resetRace();
                state = GameState::Menu;
            }
        }

        // ---------- The simulation only runs while PLAYING ----------
        if (state == GameState::Playing) {

            // NEW: count the crash cooldown down towards zero.
            crashCooldown -= dt;

            auto keyDown = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

            bool accelerating = keyDown(GLFW_KEY_W) || keyDown(GLFW_KEY_UP);
            bool braking      = keyDown(GLFW_KEY_S) || keyDown(GLFW_KEY_DOWN);
            bool steerLeft    = keyDown(GLFW_KEY_A) || keyDown(GLFW_KEY_LEFT);
            bool steerRight   = keyDown(GLFW_KEY_D) || keyDown(GLFW_KEY_RIGHT);
            bool handbrake    = keyDown(GLFW_KEY_SPACE);

            if (accelerating) {
                car.speed += acceleration * dt;
            } else if (braking) {
                car.speed -= (car.speed > 0.0f ? brakePower : acceleration) * dt;
            } else {
                if (car.speed > 0.0f) {
                    car.speed -= friction * dt;
                    if (car.speed < 0.0f) car.speed = 0.0f;
                } else if (car.speed < 0.0f) {
                    car.speed += friction * dt;
                    if (car.speed > 0.0f) car.speed = 0.0f;
                }
            }

            if (handbrake) {
                car.speed -= car.speed * 0.3f * dt;
            }
            car.speed = glm::clamp(car.speed, -maxReverse, maxSpeed);

            float steerInput = 0.0f;
            if (steerLeft)  steerInput += 1.0f;
            if (steerRight) steerInput -= 1.0f;

            float totalSpeed = std::sqrt(car.speed * car.speed + car.slide * car.slide);
            float direction  = (car.speed >= 0.0f) ? 1.0f : -1.0f;
            float turnScale  = glm::clamp(totalSpeed / 6.0f, 0.0f, 1.0f) * direction;
            float currentTurnRate = handbrake ? turnRate * driftTurnBoost : turnRate;

            float deltaHeading = steerInput * currentTurnRate * turnScale * dt;
            car.heading += deltaHeading;

            float cosD = std::cos(deltaHeading);
            float sinD = std::sin(deltaHeading);
            float newSpeed = car.speed * cosD - car.slide * sinD;
            float newSlide = car.speed * sinD + car.slide * cosD;
            car.speed = newSpeed;
            car.slide = newSlide;

            float grip = handbrake ? gripDrift : gripNormal;
            car.slide *= std::exp(-grip * dt);

            car.speed = glm::clamp(car.speed, -maxReverse, maxSpeed);

            glm::vec3 fwd = forwardOf(car.heading);
            glm::vec3 rgt = rightOf(car.heading);

            glm::vec3 velocity = fwd * car.speed + rgt * car.slide;
            car.position += velocity * dt;

            // Collision with the AI cars.
            for (const AIRacer& ai : aiRacers) {
                CollisionResult hit = testCarCollision(car, ai.car);

                if (hit.hit) {
                    car.position += hit.normal * hit.depth;

                    glm::vec3 playerVelocity = fwd * car.speed + rgt * car.slide;
                    glm::vec3 aiVelocity     = forwardOf(ai.car.heading) * ai.speed;

                    float closing = glm::dot(playerVelocity - aiVelocity, hit.normal);

                    if (closing < 0.0f) {
                        // NEW: a thud, but only for a hard hit (closing faster than 3 units/s)
                        // and only if the cooldown has run out. This stops a stream of sounds
                        // while the cars stay in contact.
                        if (closing < -3.0f && crashCooldown <= 0.0f) {
                            sfx("Basso");
                            crashCooldown = 0.5f;     // wait half a second before the next crash sound
                        }

                        playerVelocity -= hit.normal * closing;

                        car.speed = glm::dot(playerVelocity, fwd);
                        car.slide = glm::dot(playerVelocity, rgt);
                        car.speed = glm::clamp(car.speed, -maxReverse, maxSpeed);
                    }
                }
            }

            // Wall collision.
            int currentIndex = nearestCenterIndex(trackSamples, car.position);
            glm::vec3 nearest = trackSamples[currentIndex];
            glm::vec3 offset = car.position - nearest;
            offset.y = 0.0f;
            float distFromCenter = glm::length(offset);
            const float maxOffset = WALL_DISTANCE - CAR_RADIUS;

            if (distFromCenter > maxOffset) {
                glm::vec3 normal = offset / distFromCenter;
                car.position = nearest + normal * maxOffset;

                glm::vec3 wallVelocity = fwd * car.speed + rgt * car.slide;
                float into = glm::dot(wallVelocity, normal);

                if (into > 0.0f) {
                    // NEW: a thud for a hard wall hit (moving into the wall faster than 5 units/s).
                    // A gentle scrape along the wall stays silent.
                    if (into > 5.0f && crashCooldown <= 0.0f) {
                        sfx("Basso");
                        crashCooldown = 0.5f;
                    }

                    wallVelocity -= normal * into * 1.1f;

                    car.speed = glm::dot(wallVelocity, fwd);
                    car.slide = glm::dot(wallVelocity, rgt);
                    car.speed = glm::clamp(car.speed, -maxReverse, maxSpeed);
                }
            }

            // Lap counting.
            if (previousIndex > TRACK_SAMPLES * 3 / 4 && currentIndex < TRACK_SAMPLES / 4) {
                lapsCompleted++;
                // NEW: a bell for each new lap. (The last lap has its own finish sound below.)
                if (lapsCompleted < TOTAL_LAPS) {
                    sfx("Glass");
                }
            } else if (previousIndex < TRACK_SAMPLES / 4 && currentIndex > TRACK_SAMPLES * 3 / 4) {
                lapsCompleted--;
            }
            previousIndex = currentIndex;

            for (AIRacer& ai : aiRacers) {
                float arcRate = std::sqrt(TRACK_A * TRACK_A * std::sin(ai.t) * std::sin(ai.t)
                                        + TRACK_B * TRACK_B * std::cos(ai.t) * std::cos(ai.t));
                ai.t += ai.speed * dt / arcRate;
                if (ai.t > 2.0f * PI) {
                    ai.t -= 2.0f * PI;
                    ai.laps++;
                }
                placeAI(ai);
            }

            if (!raceStarted && accelerating) {
                raceStarted = true;
            }
            if (raceStarted && !raceFinished) {
                raceTime += dt;
            }

            if (!raceFinished && lapsCompleted >= TOTAL_LAPS) {
                raceFinished  = true;
                finalPosition = computePosition();
                state = GameState::Finished;

                // NEW: a fanfare for a win, a different sound otherwise.
                if (finalPosition == 1) {
                    sfx("Hero");
                } else {
                    sfx("Funk");
                }
            }
        }

        // ---------- Values the camera and the HUD need every frame ----------
        glm::vec3 forward = forwardOf(car.heading);
        int position = computePosition();
        int lapShown = glm::clamp(lapsCompleted + 1, 1, TOTAL_LAPS);

        // ---------- Drawing the 3D world ----------
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glEnable(GL_DEPTH_TEST);

        glClearColor(0.5f, 0.7f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 cameraPos    = car.position - forward * 9.0f + glm::vec3(0.0f, 4.0f, 0.0f);
        glm::vec3 cameraTarget = car.position + forward * 3.0f + glm::vec3(0.0f, 0.8f, 0.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

        float aspect = (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.5f, 300.0f);
        glm::mat4 viewProjection = projection * view;

        glUseProgram(shaderProgram);
        glUniform3f(tintLocation, 1.0f, 1.0f, 1.0f);
        glUniform1f(alphaLocation, 1.0f);
        glUniform1f(lightingLocation, 1.0f);
        glUniform3f(lightDirLocation, sunDirection.x, sunDirection.y, sunDirection.z);

        auto drawMesh = [&](const Mesh& mesh, const glm::mat4& model) {
            glm::mat4 mvp = viewProjection * model;
            glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        };

        drawMesh(grass,    glm::mat4(1.0f));
        drawMesh(road,     glm::mat4(1.0f));
        drawMesh(lines,    glm::mat4(1.0f));
        drawMesh(barriers, glm::mat4(1.0f));

        for (const Tree& tree : trees) {
            glm::mat4 treeModel = glm::translate(glm::mat4(1.0f), tree.position)
                                * glm::rotate(glm::mat4(1.0f), tree.rotation, glm::vec3(0.0f, 1.0f, 0.0f))
                                * glm::scale(glm::mat4(1.0f), glm::vec3(tree.scale));

            auto drawTreePart = [&](const Mesh& mesh, glm::vec3 partPosition, glm::vec3 size) {
                glm::mat4 part = treeModel
                               * glm::translate(glm::mat4(1.0f), partPosition)
                               * glm::scale(glm::mat4(1.0f), size);
                drawMesh(mesh, part);
            };

            drawTreePart(trunkBox, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.6f, 2.0f, 0.6f));
            drawTreePart(leafLow,  glm::vec3(0.0f, 3.1f, 0.0f), glm::vec3(3.2f, 2.2f, 3.2f));
            drawTreePart(leafHigh, glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(2.0f, 1.8f, 2.0f));
        }

        auto drawCar = [&](const Car& c, const Mesh& bodyMesh) {
            glm::mat4 carModel = glm::translate(glm::mat4(1.0f), c.position)
                               * glm::rotate(glm::mat4(1.0f), c.heading, glm::vec3(0.0f, 1.0f, 0.0f));

            auto drawPart = [&](const Mesh& mesh, glm::vec3 partPosition, glm::vec3 size) {
                glm::mat4 part = carModel
                               * glm::translate(glm::mat4(1.0f), partPosition)
                               * glm::scale(glm::mat4(1.0f), size);
                drawMesh(mesh, part);
            };

            drawPart(bodyMesh, glm::vec3(0.0f, 0.55f, 0.0f),  glm::vec3(1.8f, 0.5f, 4.0f));
            drawPart(cabinBox, glm::vec3(0.0f, 1.05f, 0.4f),  glm::vec3(1.4f, 0.5f, 2.0f));

            drawPart(wheelBox, glm::vec3(-0.95f, 0.3f, -1.3f), glm::vec3(0.3f, 0.6f, 0.6f));
            drawPart(wheelBox, glm::vec3( 0.95f, 0.3f, -1.3f), glm::vec3(0.3f, 0.6f, 0.6f));
            drawPart(wheelBox, glm::vec3(-0.95f, 0.3f,  1.3f), glm::vec3(0.3f, 0.6f, 0.6f));
            drawPart(wheelBox, glm::vec3( 0.95f, 0.3f,  1.3f), glm::vec3(0.3f, 0.6f, 0.6f));

            drawPart(headBox,  glm::vec3(-0.6f, 0.55f, -2.0f), glm::vec3(0.4f, 0.2f, 0.1f));
            drawPart(headBox,  glm::vec3( 0.6f, 0.55f, -2.0f), glm::vec3(0.4f, 0.2f, 0.1f));

            drawPart(tailBox,  glm::vec3(-0.6f, 0.55f,  2.0f), glm::vec3(0.4f, 0.2f, 0.1f));
            drawPart(tailBox,  glm::vec3( 0.6f, 0.55f,  2.0f), glm::vec3(0.4f, 0.2f, 0.1f));
        };

        drawCar(car, bodyRed);

        for (const AIRacer& ai : aiRacers) {
            drawCar(ai.car, *ai.bodyMesh);
        }

        // =================================================================
        // THE BLOB SHADOWS (unchanged)
        // =================================================================
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniform1f(lightingLocation, 0.0f);

        auto drawShadow = [&](glm::vec3 center, float heading, float radiusX, float radiusZ, float alpha) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(center.x, 0.06f, center.z))
                            * glm::rotate(glm::mat4(1.0f), heading, glm::vec3(0.0f, 1.0f, 0.0f))
                            * glm::scale(glm::mat4(1.0f), glm::vec3(radiusX, 1.0f, radiusZ));
            glUniform1f(alphaLocation, alpha);
            drawMesh(blob, model);
        };

        auto drawCarShadow = [&](const Car& c) {
            drawShadow(c.position + shadowDirection * 0.6f, c.heading, 1.5f,  2.9f, 0.18f);
            drawShadow(c.position + shadowDirection * 0.4f, c.heading, 1.15f, 2.4f, 0.22f);
        };

        drawCarShadow(car);
        for (const AIRacer& ai : aiRacers) {
            drawCarShadow(ai.car);
        }

        for (const Tree& tree : trees) {
            drawShadow(tree.position + shadowDirection * (1.2f * tree.scale), 0.0f,
                       2.6f * tree.scale, 2.6f * tree.scale, 0.28f);
        }

        glDepthMask(GL_TRUE);
        glUniform1f(alphaLocation, 1.0f);

        // =================================================================
        // THE HUD AND MENUS
        // =================================================================
        glDisable(GL_DEPTH_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniform1f(lightingLocation, 0.0f);

        float hudHeight = 720.0f;
        float hudWidth  = hudHeight * aspect;
        glm::mat4 hudProjection = glm::ortho(0.0f, hudWidth, 0.0f, hudHeight, -1.0f, 1.0f);

        auto drawRect = [&](float x, float y, float w, float h, glm::vec3 color, float alpha = 1.0f) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f))
                            * glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
            glm::mat4 mvp = hudProjection * model;
            glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(tintLocation, color.r, color.g, color.b);
            glUniform1f(alphaLocation, alpha);
            glBindVertexArray(hudQuad.vao);
            glDrawElements(GL_TRIANGLES, hudQuad.indexCount, GL_UNSIGNED_INT, 0);
        };

        auto drawSegments = [&](int pattern, float x, float y, float w, float h, float th, glm::vec3 color) {
            if (pattern & 1)  drawRect(x + th,     y + h - th,   w - 2 * th, th,       color);
            if (pattern & 2)  drawRect(x + w - th, y + h / 2.0f, th,         h / 2.0f, color);
            if (pattern & 4)  drawRect(x + w - th, y,            th,         h / 2.0f, color);
            if (pattern & 8)  drawRect(x + th,     y,            w - 2 * th, th,       color);
            if (pattern & 16) drawRect(x,          y,            th,         h / 2.0f, color);
            if (pattern & 32) drawRect(x,          y + h / 2.0f, th,         h / 2.0f, color);
            if (pattern & 64) drawRect(x + th,     y + h / 2.0f - th / 2.0f, w - 2 * th, th, color);
        };

        auto drawDigit = [&](int digit, float x, float y, float w, float h, float th, glm::vec3 color) {
            static const int patterns[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
            if (digit < 0 || digit > 9) return;
            drawSegments(patterns[digit], x, y, w, h, th, color);
        };

        auto drawNumberRight = [&](int value, float rightX, float y, float w, float h, float th,
                                   float gap, glm::vec3 color) {
            std::string digits = std::to_string(value);
            int count = (int)digits.size();
            for (int i = 0; i < count; i++) {
                float x = rightX - (float)(count - i) * w - (float)(count - 1 - i) * gap;
                drawDigit(digits[i] - '0', x, y, w, h, th, color);
            }
        };

        const int LETTER_L = 8 + 16 + 32;
        const int LETTER_A = 1 + 2 + 4 + 16 + 32 + 64;
        const int LETTER_P = 1 + 2 + 16 + 32 + 64;
        const int LETTER_O = 63;
        const int LETTER_S = 109;

        auto drawLabel = [&](int p1, int p2, int p3, float x, float y, glm::vec3 color) {
            const float w = 18.0f, h = 30.0f, th = 5.0f, gap = 6.0f;
            drawSegments(p1, x,                 y, w, h, th, color);
            drawSegments(p2, x + (w + gap),     y, w, h, th, color);
            drawSegments(p3, x + 2 * (w + gap), y, w, h, th, color);
        };

        auto drawFraction = [&](int value, int total, float x, float y,
                                glm::vec3 valueColor, glm::vec3 totalColor) {
            drawDigit(value, x,        y,        28.0f, 44.0f, 6.0f, valueColor);
            drawRect(x + 38.0f, y + 19.0f, 14.0f, 6.0f, totalColor);
            drawDigit(total, x + 62.0f, y + 6.0f, 20.0f, 32.0f, 5.0f, totalColor);
        };

        auto drawText = [&](const std::string& text, float x, float y, float ps, glm::vec3 color) {
            for (char c : text) {
                const unsigned char* glyph = getGlyph(c);
                if (glyph) {
                    for (int row = 0; row < 7; row++) {
                        for (int col = 0; col < 5; col++) {
                            if ((glyph[row] >> (4 - col)) & 1) {
                                drawRect(x + (float)col * ps, y + (float)(6 - row) * ps, ps, ps, color);
                            }
                        }
                    }
                }
                x += 6.0f * ps;
            }
        };

        auto drawTextCentered = [&](const std::string& text, float centerX, float y, float ps, glm::vec3 color) {
            float textWidth = (float)text.size() * 6.0f * ps - ps;
            drawText(text, centerX - textWidth / 2.0f, y, ps, color);
        };

        const glm::vec3 panelColor (0.08f, 0.08f, 0.10f);
        const glm::vec3 whiteColor (0.95f, 0.95f, 0.95f);
        const glm::vec3 grayColor  (0.60f, 0.60f, 0.70f);
        const glm::vec3 goldColor  (1.00f, 0.85f, 0.20f);
        const glm::vec3 greenColor (0.30f, 0.95f, 0.40f);
        const glm::vec3 orangeColor(1.00f, 0.60f, 0.20f);
        const glm::vec3 blackColor (0.00f, 0.00f, 0.00f);

        if (state != GameState::Menu) {

            float panelW = 260.0f;
            float panelH = 130.0f;
            float panelX = hudWidth - panelW - 30.0f;
            float panelY = 30.0f;

            drawRect(panelX, panelY, panelW, panelH, panelColor);

            float hudTotalSpeed = std::sqrt(car.speed * car.speed + car.slide * car.slide);
            int speedKmh = (int)(hudTotalSpeed * 3.6f);
            drawNumberRight(speedKmh, panelX + panelW - 30.0f, panelY + 52.0f,
                            38.0f, 64.0f, 8.0f, 12.0f, whiteColor);

            float ratio = glm::clamp(hudTotalSpeed / maxSpeed, 0.0f, 1.0f);
            drawRect(panelX + 20.0f, panelY + 20.0f, 220.0f, 16.0f, glm::vec3(0.25f, 0.25f, 0.28f));
            glm::vec3 barColor = glm::mix(glm::vec3(0.20f, 0.90f, 0.30f),
                                          glm::vec3(0.95f, 0.20f, 0.20f), ratio);
            drawRect(panelX + 20.0f, panelY + 20.0f, 220.0f * ratio, 16.0f, barColor);

            float timePanelW = 270.0f;
            float timePanelH = 90.0f;
            float timePanelX = 30.0f;
            float timePanelY = hudHeight - 30.0f - timePanelH;

            drawRect(timePanelX, timePanelY, timePanelW, timePanelH, panelColor);

            int totalTenths = (int)(raceTime * 10.0f);
            int tenths = totalTenths % 10;
            int secs   = (totalTenths / 10) % 60;
            int mins   = glm::min(totalTenths / 600, 99);

            glm::vec3 timeColor = raceFinished ? goldColor : whiteColor;
            float dw = 30.0f, dh = 50.0f, dth = 7.0f, dgap = 8.0f;
            float tx = timePanelX + 20.0f;
            float ty = timePanelY + 20.0f;

            drawDigit(mins / 10, tx, ty, dw, dh, dth, timeColor);  tx += dw + dgap;
            drawDigit(mins % 10, tx, ty, dw, dh, dth, timeColor);  tx += dw + dgap;
            drawRect(tx + 4.0f, ty + dh * 0.28f, 7.0f, 7.0f, timeColor);
            drawRect(tx + 4.0f, ty + dh * 0.62f, 7.0f, 7.0f, timeColor);
            tx += 14.0f + dgap;
            drawDigit(secs / 10, tx, ty, dw, dh, dth, timeColor);  tx += dw + dgap;
            drawDigit(secs % 10, tx, ty, dw, dh, dth, timeColor);  tx += dw + dgap;
            drawRect(tx, ty, 7.0f, 7.0f, timeColor);
            tx += 7.0f + dgap;
            drawDigit(tenths, tx, ty, dw, dh, dth, timeColor);

            float infoPanelW = 250.0f;
            float infoPanelH = 150.0f;
            float infoPanelX = hudWidth - 30.0f - infoPanelW;
            float infoPanelY = hudHeight - 30.0f - infoPanelH;

            drawRect(infoPanelX, infoPanelY, infoPanelW, infoPanelH, panelColor);

            float row1Y = infoPanelY + 85.0f;
            drawLabel(LETTER_L, LETTER_A, LETTER_P, infoPanelX + 18.0f, row1Y + 7.0f, grayColor);
            drawFraction(lapShown, TOTAL_LAPS, infoPanelX + 110.0f, row1Y, whiteColor, grayColor);

            float row2Y = infoPanelY + 20.0f;
            int shownPosition = raceFinished ? finalPosition : position;
            glm::vec3 positionColor = (shownPosition == 1) ? greenColor : orangeColor;
            drawLabel(LETTER_P, LETTER_O, LETTER_S, infoPanelX + 18.0f, row2Y + 7.0f, grayColor);
            drawFraction(shownPosition, TOTAL_RACERS, infoPanelX + 110.0f, row2Y, positionColor, grayColor);
        }

        float centerX = hudWidth / 2.0f;

        // NEW: the text of the mute line changes with the sound setting.
        // (The "? :" picks the first text when soundOn is true, and the second when it is false.)
        std::string muteText = soundOn ? "N TO MUTE" : "N TO UNMUTE";

        if (state == GameState::Menu) {
            drawRect(0.0f, 0.0f, hudWidth, hudHeight, blackColor, 0.55f);

            drawTextCentered("RACING GAME", centerX, hudHeight * 0.60f, 12.0f, goldColor);
            drawRect(centerX - 390.0f, hudHeight * 0.60f - 24.0f, 780.0f, 6.0f, goldColor);

            drawTextCentered("PRESS ENTER TO START", centerX, hudHeight * 0.43f, 6.0f, whiteColor);
            drawTextCentered("W A S D TO DRIVE",     centerX, hudHeight * 0.35f, 5.0f, grayColor);
            drawTextCentered("SPACE TO DRIFT",       centerX, hudHeight * 0.28f, 5.0f, grayColor);
            drawTextCentered(muteText,               centerX, hudHeight * 0.21f, 5.0f, grayColor);   // NEW
            drawTextCentered("ESC TO QUIT",          centerX, hudHeight * 0.14f, 5.0f, grayColor);

        } else if (state == GameState::Paused) {
            drawRect(0.0f, 0.0f, hudWidth, hudHeight, blackColor, 0.55f);

            drawTextCentered("PAUSED", centerX, hudHeight * 0.58f, 12.0f, whiteColor);

            drawTextCentered("P OR ESC TO RESUME", centerX, hudHeight * 0.43f, 5.0f, whiteColor);
            drawTextCentered("R TO RESTART",       centerX, hudHeight * 0.35f, 5.0f, whiteColor);
            drawTextCentered("M FOR MENU",         centerX, hudHeight * 0.27f, 5.0f, whiteColor);
            drawTextCentered(muteText,             centerX, hudHeight * 0.19f, 5.0f, grayColor);    // NEW

        } else if (state == GameState::Finished) {
            drawRect(0.0f, 0.0f, hudWidth, hudHeight, blackColor, 0.55f);

            if (finalPosition == 1) {
                drawTextCentered("YOU WIN", centerX, hudHeight * 0.58f, 12.0f, greenColor);
            } else {
                drawTextCentered("YOU LOSE", centerX, hudHeight * 0.58f, 12.0f, orangeColor);
            }

            drawTextCentered("R TO RESTART", centerX, hudHeight * 0.40f, 5.0f, whiteColor);
            drawTextCentered("M FOR MENU",   centerX, hudHeight * 0.31f, 5.0f, whiteColor);
        }

        glUniform3f(tintLocation, 1.0f, 1.0f, 1.0f);
        glUniform1f(alphaLocation, 1.0f);
        glDisable(GL_BLEND);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    destroyMesh(grass);
    destroyMesh(road);
    destroyMesh(lines);
    destroyMesh(barriers);
    destroyMesh(bodyRed);
    destroyMesh(bodyBlue);
    destroyMesh(bodyPurple);
    destroyMesh(bodyYellow);
    destroyMesh(cabinBox);
    destroyMesh(wheelBox);
    destroyMesh(headBox);
    destroyMesh(tailBox);
    destroyMesh(trunkBox);
    destroyMesh(leafLow);
    destroyMesh(leafHigh);
    destroyMesh(blob);
    destroyMesh(hudQuad);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}