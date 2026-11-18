#include "turret_view.h"
#include "destruction_effects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
namespace aegis {
namespace {
GLuint Compile(GLenum kind,const std::filesystem::path& path) {
    std::ifstream file(path); if(!file) { std::cerr<<"Cannot open "<<path<<'\n';return 0; }
    std::stringstream buffer;buffer<<file.rdbuf();const auto source=buffer.str();const char* text=source.c_str();
    GLuint shader=glCreateShader(kind);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[1024]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::cerr<<log<<'\n';glDeleteShader(shader);return 0; }
    return shader;
}
}
bool TurretView::Init(const std::filesystem::path& root) {
    Clear();
    const auto vs=Compile(GL_VERTEX_SHADER,root/"game/shaders/turret.vert"),fs=Compile(GL_FRAGMENT_SHADER,root/"game/shaders/turret.frag");
    if(!vs || !fs) { if(vs) glDeleteShader(vs);if(fs) glDeleteShader(fs);return false; }
    program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok{};glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) { char log[1024]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);std::cerr<<log<<'\n';Clear();return false; }
    transformLocation=glGetUniformLocation(program,"transform");colorLocation=glGetUniformLocation(program,"baseColor");
    if(transformLocation<0 || colorLocation<0) { Clear();return false; }
    const glm::vec3 corners[]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
    const unsigned indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,4,7,0,7,3,1,2,6,1,6,5,0,1,5,0,5,4,3,7,6,3,6,2};
    glm::vec3 vertices[36];for(int i=0;i<36;++i) vertices[i]=corners[indices[i]];
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),nullptr);glEnableVertexAttribArray(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);return true;
}
void TurretView::Box(const glm::mat4& vp,const glm::mat4& parent,const glm::vec3& low,const glm::vec3& high,const glm::vec3& color) const {
    const auto transform=vp*parent*glm::scale(glm::translate(glm::mat4(1),(low+high)*.5f),high-low);
    glUniformMatrix4fv(transformLocation,1,GL_FALSE,glm::value_ptr(transform));glUniform3fv(colorLocation,1,glm::value_ptr(color));glDrawArrays(GL_TRIANGLES,0,36);
}
void TurretView::Draw(const glm::mat4& vp,const combat::FixedTurret& turret) const {
    if(!program || !turret.Alive()) return;
    glUseProgram(program);glBindVertexArray(vao);
    const auto parent=glm::translate(glm::mat4(1),turret.Position());
    Box(vp,parent,{-.5f,0,-.5f},{.5f,2.2f,.5f},{.35f,.4f,.46f});
    Box(vp,parent,{-.5f,0,-.5f},{.5f,.2f,.5f},{.06f,.09f,.12f});
    const auto barrel=turret.BarrelTransform();
    if(turret.Armed()) Box(vp,barrel,{-.14f,-.14f,-.1f},{.14f,.14f,.8f},{.85f,.65f,.15f});
    const auto sensor=turret.Phase()==combat::TurretPhase::Warning ? glm::vec3(1,.03f,.02f):turret.Armed() ? glm::vec3(.05f,.6f,1):glm::vec3(.08f);
    Box(vp,barrel,{-.2f,.28f,.48f},{.2f,.4f,.52f},sensor);
    glBindVertexArray(0);glUseProgram(0);
}
void TurretView::DrawDebris(const glm::mat4& vp,const glm::mat4& world,float age) const {
    if(!program) return;
    glUseProgram(program);glBindVertexArray(vao);
    for(int i=0;i<combat::DestructionEffects::TurretPieceCount;++i) {
        const auto parent=combat::DestructionEffects::PieceTransform(world,i,age);
        Box(vp,parent,{-.5f,i*.73f,-.5f},{.5f,(i+1)*.73f,.5f},{.35f,.4f,.46f});
    }
    glBindVertexArray(0);glUseProgram(0);
}
void TurretView::Clear() { if(program) glDeleteProgram(program);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);program=vao=vbo=0; }
}
