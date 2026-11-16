#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "weapon_view.h"
#include "cw01c_layout.generated.h"
#include "model_loader.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>

namespace aegis {
namespace {
using Microsoft::WRL::ComPtr;

struct ComContext {
    HRESULT status=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComContext() { if (SUCCEEDED(status)) CoUninitialize(); }
};

GLuint CompileWeaponShader(GLenum kind,const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) { std::cerr << "Cannot read weapon shader: " << path << '\n'; return 0; }
    std::ostringstream buffer; buffer<<file.rdbuf();
    const std::string source=buffer.str(); const char* text=source.c_str();
    GLuint shader=glCreateShader(kind);
    glShaderSource(shader,1,&text,nullptr); glCompileShader(shader);
    GLint success{}; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    if (!success) {
        char log[2048]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        std::cerr << path << ": " << log << '\n'; glDeleteShader(shader); return 0;
    }
    return shader;
}

GLuint LoadPng(IWICImagingFactory* factory,const std::filesystem::path& path,bool srgb)
{
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,&decoder);
    if (SUCCEEDED(hr)) hr=decoder->GetFrame(0,&frame);
    if (SUCCEEDED(hr)) hr=factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    UINT width{},height{};
    if (SUCCEEDED(hr)) hr=converter->GetSize(&width,&height);
    if (FAILED(hr) || !width || !height || width>8192 || height>8192) {
        std::cerr << "Cannot decode weapon PNG: " << path << " HRESULT=" << std::hex << hr << std::dec << '\n';
        return 0;
    }
    const UINT stride=width*4;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(stride)*height);
    hr=converter->CopyPixels(nullptr,stride,static_cast<UINT>(pixels.size()),pixels.data());
    if (FAILED(hr)) { std::cerr << "Cannot read PNG pixels: " << path << '\n'; return 0; }
    // WIC is top-down; OBJ UVs use a bottom-left origin. Keep normal-map +Y.
    for(UINT y=0;y<height/2;++y)
        std::swap_ranges(pixels.begin()+static_cast<std::size_t>(y)*stride,
            pixels.begin()+static_cast<std::size_t>(y+1)*stride,
            pixels.begin()+static_cast<std::size_t>(height-1-y)*stride);
    GLuint texture{}; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,srgb?GL_SRGB8_ALPHA8:GL_RGBA8,
        static_cast<GLsizei>(width),static_cast<GLsizei>(height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    GLint uploadedWidth{}; glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&uploadedWidth);
    if (uploadedWidth!=static_cast<GLint>(width)) {
        std::cerr << "Weapon texture upload failed: " << path << '\n';
        glDeleteTextures(1,&texture); return 0;
    }
    return texture;
}
}

bool WeaponView::Init(const std::filesystem::path& root)
{
    Clear();
    const auto shaders=root/"game/shaders";
    const GLuint vertex=CompileWeaponShader(GL_VERTEX_SHADER,shaders/"weapon.vert");
    const GLuint fragment=CompileWeaponShader(GL_FRAGMENT_SHADER,shaders/"weapon.frag");
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment); return false;
    }
    program=glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,fragment);
    glLinkProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
    GLint linked{}; glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if (!linked) {
        char log[2048]{}; glGetProgramInfoLog(program,sizeof(log),nullptr,log);
        std::cerr << "Weapon program: " << log << '\n'; Clear(); return false;
    }
    transformLocation=glGetUniformLocation(program,"transform");
    modelLocation=glGetUniformLocation(program,"model");
    shotLocation=glGetUniformLocation(program,"shot");
    if (transformLocation<0 || modelLocation<0 || shotLocation<0) { Clear(); return false; }
    const auto runtime=root/"asset/cw01c/runtime";
    ComContext com;
    if (FAILED(com.status) && com.status!=RPC_E_CHANGED_MODE) {
        std::cerr << "Cannot initialize WIC COM apartment\n"; Clear(); return false;
    }
    ComPtr<IWICImagingFactory> factory;
    const HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { std::cerr << "Cannot initialize Windows PNG decoder\n"; Clear(); return false; }
    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program,"emissionMultiplier"),weapon_asset::emissionMultiplier);
    for(std::size_t i=0;i<textures.size();++i) {
        const auto& spec=weapon_asset::textures[i];
        textures[i]=LoadPng(factory.Get(),runtime/spec.file,spec.srgb);
        if (!textures[i]) { Clear(); return false; }
        const std::string sampler=std::string(spec.name)+"Map";
        const GLint location=glGetUniformLocation(program,sampler.c_str());
        if (location<0) { std::cerr << "Missing sampler: " << sampler << '\n'; Clear(); return false; }
        glUniform1i(location,static_cast<GLint>(i));
    }
    std::size_t totalTriangles=0;
    for(const auto& part:weapon_asset::parts) {
        ModelData model; std::string error;
        if (!LoadModel(runtime/part.mesh,model,error)) { std::cerr << error << '\n'; Clear(); return false; }
        std::size_t partTriangles=0;
        for(const auto& mesh:model.meshes) {
            if (!mesh.hasUvs || mesh.indices.empty()) {
                std::cerr << "Weapon mesh has no UV/triangles: " << part.mesh << '\n'; Clear(); return false;
            }
            auto& gpu=meshes.emplace_back(); gpu.offset=part.offset;
            glGenVertexArrays(1,&gpu.vao); glGenBuffers(1,&gpu.vbo); glGenBuffers(1,&gpu.ebo);
            glBindVertexArray(gpu.vao); glBindBuffer(GL_ARRAY_BUFFER,gpu.vbo);
            glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(ModelVertex),mesh.vertices.data(),GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*sizeof(unsigned int),mesh.indices.data(),GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ModelVertex),reinterpret_cast<void*>(offsetof(ModelVertex,position)));
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(ModelVertex),reinterpret_cast<void*>(offsetof(ModelVertex,normal)));
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(ModelVertex),reinterpret_cast<void*>(offsetof(ModelVertex,uv)));
            glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
            gpu.indices=static_cast<GLsizei>(mesh.indices.size());
            partTriangles+=mesh.indices.size()/3;
        }
        if (partTriangles!=static_cast<std::size_t>(part.triangles)) {
            std::cerr << "Weapon layout is stale: " << part.id << '\n'; Clear(); return false;
        }
        totalTriangles+=partTriangles;
    }
    glBindVertexArray(0); glBindTexture(GL_TEXTURE_2D,0); glUseProgram(0);
    std::cout << "CW-01 C viewmodel: parts=" << weapon_asset::parts.size()
        << " triangles=" << totalTriangles << " textures=" << textures.size() << '\n';
    return true;
}

void WeaponView::Reset() { recoil=0; shotCount=0; }
void WeaponView::OnShot() { recoil=1; ++shotCount; }

void WeaponView::Update(double realDt)
{
    if (std::isfinite(realDt) && realDt>=0) recoil*=std::exp(-14.0f*static_cast<float>(realDt));
}

void WeaponView::Draw(float aspect) const
{
    if (!program || aspect<=0) return;
    const glm::mat4 projection=glm::perspective(glm::radians(60.0f),aspect,0.02f,3.0f);
    glm::mat4 mount=glm::translate(glm::mat4(1),glm::vec3(0.16f,-0.16f,-0.50f)
        +glm::vec3(0,-0.008f*recoil,0.035f*recoil));
    mount=glm::rotate(mount,glm::pi<float>()+0.25f,glm::vec3(0,1,0));
    mount=glm::rotate(mount,glm::radians(-4.0f+recoil*5.0f),glm::vec3(1,0,0));
    // Viewmodel has its own depth pass; it must not inherit distant scene depth.
    glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glUseProgram(program);
    glUniform1f(shotLocation,recoil);
    for(std::size_t i=0;i<textures.size();++i) {
        glActiveTexture(GL_TEXTURE0+static_cast<GLenum>(i)); glBindTexture(GL_TEXTURE_2D,textures[i]);
    }
    for(const auto& mesh:meshes) {
        const glm::mat4 model=glm::translate(mount,mesh.offset);
        const glm::mat4 transform=projection*model;
        glUniformMatrix4fv(modelLocation,1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(transformLocation,1,GL_FALSE,glm::value_ptr(transform));
        glBindVertexArray(mesh.vao); glDrawElements(GL_TRIANGLES,mesh.indices,GL_UNSIGNED_INT,nullptr);
    }
    glBindVertexArray(0); glUseProgram(0);
    for(std::size_t i=0;i<textures.size();++i) {
        glActiveTexture(GL_TEXTURE0+static_cast<GLenum>(i)); glBindTexture(GL_TEXTURE_2D,0);
    }
    glActiveTexture(GL_TEXTURE0);
}

void WeaponView::Clear()
{
    for(const auto& mesh:meshes) {
        glDeleteVertexArrays(1,&mesh.vao); glDeleteBuffers(1,&mesh.vbo); glDeleteBuffers(1,&mesh.ebo);
    }
    meshes.clear();
    glDeleteTextures(static_cast<GLsizei>(textures.size()),textures.data()); textures.fill(0);
    if (program) { glUseProgram(0); glDeleteProgram(program); program=0; }
    transformLocation=modelLocation=shotLocation=-1;
    Reset();
}

} // namespace aegis
