#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "korean_ui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace aegis {
namespace {
GLuint Shader(GLenum kind,const char* source) {
    const auto shader=glCreateShader(kind); glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    GLint ok{}; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) { glDeleteShader(shader); return 0; } return shader;
}
}
bool KoreanUi::Init() {
    Clear();
    const auto vs=Shader(GL_VERTEX_SHADER,R"(#version 330 core
out vec2 uv;
uniform mat4 clipTransform;
void main() {
 vec2 p[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
 vec2 v=p[gl_VertexID]; uv=vec2((v.x+1)*0.5,1-(v.y+1)*0.5); gl_Position=clipTransform*vec4(v,0,1);
})");
    const auto fs=Shader(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec2 uv; out vec4 color; uniform sampler2D ui;
void main(){color=texture(ui,uv);}
)");
    if (!vs || !fs) { if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return false; }
    program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok{}; glGetProgramiv(program,GL_LINK_STATUS,&ok); if(!ok) { Clear(); return false; }
    glUseProgram(program); glUniform1i(glGetUniformLocation(program,"ui"),0); glUseProgram(0);
    glGenVertexArrays(1,&vao); glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0); return true;
}
UiAction KoreanUi::Hit(const UiPage& page,float x,float y,bool& consumed) {
    consumed=page.panel || (x>=24 && x<=1000 && y>=604 && y<=726);
    for(const auto& b:page.buttons) if(b.Contains(x,y)) { consumed=true; return b.enabled ? b.action:UiAction::None; }
    return UiAction::None;
}
bool KoreanUi::Rasterize(const UiPage& page,int width,int height,int hover) {
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels{}; const auto dc=CreateCompatibleDC(nullptr);
    const auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!dc || !bitmap || !pixels) { if(bitmap) DeleteObject(bitmap); if(dc) DeleteDC(dc); return false; }
    const auto oldBitmap=SelectObject(dc,bitmap);
    std::fill_n(static_cast<std::uint32_t*>(pixels),static_cast<std::size_t>(width)*height,0u);
    const float sx=static_cast<float>(width)/1024,sy=static_cast<float>(height)/768,scale=std::min(sx,sy);
    const auto rect=[&](float x,float y,float w,float h) { return RECT{static_cast<LONG>(x*sx),static_cast<LONG>(y*sy),static_cast<LONG>((x+w)*sx),static_cast<LONG>((y+h)*sy)}; };
    const auto fill=[&](float x,float y,float w,float h,COLORREF color) {
        auto r=rect(x,y,w,h); const auto brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush);
    };
    const auto text=[&](const std::wstring& value,float x,float y,float w,float h,int size,bool bold,COLORREF color) {
        const auto font=CreateFontW(-static_cast<int>(std::round(static_cast<float>(size)*scale)),0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        const auto oldFont=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
        auto r=rect(x,y,w,h); DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,oldFont); DeleteObject(font);
    };
    if(page.sign) {
        fill(0,0,1024,768,RGB(12,24,34));fill(0,0,20,768,RGB(45,212,191));
        text(page.title,56,70,920,250,132,true,RGB(167,243,208));
        text(page.body,56,360,920,350,76,false,RGB(228,242,249));
    } else if(page.panel) {
        fill(170,100,684,570,RGB(12,21,34)); fill(170,100,8,570,RGB(45,212,191));
        text(page.title,218,145,590,70,38,true,RGB(226,245,249));
        text(page.body,218,235,590,225,23,false,RGB(197,212,229));
        text(page.footer,218,624,590,36,16,false,RGB(139,170,192));
    } else {
        if(!page.timeStatus.empty()) {
            fill(24,560,976,34,RGB(12,21,34));
            text(page.timeStatus,42,565,944,26,16,false,RGB(167,211,234));
        }
        if(!page.status.empty()) {
            const auto accent=page.danger ? RGB(255,120,95):RGB(100,235,205);
            fill(24,20,976,112,RGB(12,21,34));
            text(page.status,42,30,944,32,22,true,RGB(228,242,249));
            fill(42,69,240,10,RGB(51,65,81));
            fill(42,69,240.0f*std::clamp(page.health,0,100)/100,10,page.health<=25 ? RGB(255,120,95):RGB(100,235,205));
            text(page.warning,42,89,944,32,20,true,accent);
        }
        fill(24,604,976,122,RGB(12,21,34)); fill(24,604,5,122,RGB(45,212,191));
        text(page.title,46,618,640,34,23,true,RGB(167,243,208));
        text(page.body,46,659,page.buttons.empty()?930.0f:650.0f,54,17,false,RGB(219,234,244));
    }
    for(std::size_t i=0;i<page.buttons.size();++i) {
        const auto& b=page.buttons[i]; const bool primary=i==0;
        COLORREF bg=!b.enabled?RGB(51,65,81):primary?RGB(45,212,191):RGB(40,62,83);
        if(static_cast<int>(i)==hover && b.enabled) bg=primary?RGB(94,234,212):RGB(57,88,113);
        fill(b.x,b.y,b.width,b.height,bg);
        text(b.label,b.x+18,b.y+11,b.width-32,b.height-12,21,true,
            !b.enabled?RGB(148,163,184):primary?RGB(8,32,40):RGB(240,249,255));
    }
    auto* rgba=static_cast<std::uint32_t*>(pixels);
    for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i) if(rgba[i]&0x00ffffffu) rgba[i]|=0xff000000u;
    glBindTexture(GL_TEXTURE_2D,texture); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels); glBindTexture(GL_TEXTURE_2D,0);
    SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); return true;
}
void KoreanUi::Draw(const UiPage& page,int width,int height,float mouseX,float mouseY) {
    if (!program || width<=0 || height<=0) return;
    int hover=-1; std::wstring key=page.title+page.body+page.footer+page.status+page.warning+page.timeStatus+std::to_wstring(page.health)+(page.danger?L"D":L"S")+std::to_wstring(width)+L"/"+std::to_wstring(height)+(page.panel?L"P":L"H");
    for(std::size_t i=0;i<page.buttons.size();++i) {
        const auto& b=page.buttons[i]; if(b.Contains(mouseX,mouseY)) hover=static_cast<int>(i);
        key+=b.label+(b.enabled?L"Y":L"N");
    }
    key+=std::to_wstring(hover);
    if(key!=cacheKey) { if(!Rasterize(page,width,height,hover)) return; cacheKey=key; }
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program); const glm::mat4 identity(1);glUniformMatrix4fv(glGetUniformLocation(program,"clipTransform"),1,GL_FALSE,glm::value_ptr(identity)); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture); glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0); glBindTexture(GL_TEXTURE_2D,0); glUseProgram(0); glDisable(GL_BLEND); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
}
void KoreanUi::DrawWorld(const std::wstring& title,const std::wstring& body,const glm::mat4& vp,const glm::vec3& position,const glm::vec2& size) {
    if(!program) return;
    UiPage page;page.sign=true;page.title=title;page.body=body;
    const auto key=L"WORLD SIGN:"+title+L"/"+body;
    if(key!=cacheKey) { if(!Rasterize(page,512,256,-1)) return;cacheKey=key; }
    const auto transform=vp*glm::scale(glm::translate(glm::mat4(1),position),glm::vec3(size.x*.5f,size.y*.5f,1));
    glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"clipTransform"),1,GL_FALSE,glm::value_ptr(transform));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,0);glUseProgram(0);
}
void KoreanUi::Clear() {
    if(program) glDeleteProgram(program); if(texture) glDeleteTextures(1,&texture); if(vao) glDeleteVertexArrays(1,&vao);
    program=texture=vao=0; cacheKey.clear();
}
}
