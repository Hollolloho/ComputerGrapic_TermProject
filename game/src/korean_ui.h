#pragma once
#include <GL/glew.h>
#include <string>
#include <vector>
namespace aegis {
enum class UiAction { None, Start, Help, Title, Advance, Resume, Retry, Restart, Quit };
struct UiButton {
    UiAction action;
    std::wstring label;
    float x{},y{},width{},height{};
    bool enabled{true};
    bool Contains(float px,float py) const { return px>=x && px<=x+width && py>=y && py<=y+height; }
};
struct UiPage {
    std::wstring title,body,footer;
    std::wstring status,warning;
    int health{100};
    bool danger{};
    bool panel{true};
    std::vector<UiButton> buttons;
};
class KoreanUi {
public:
    bool Init();
    void Draw(const UiPage& page,int width,int height,float mouseX,float mouseY);
    void Clear();
    static UiAction Hit(const UiPage& page,float x,float y,bool& consumed);
private:
    bool Rasterize(const UiPage& page,int width,int height,int hover);
    GLuint program{},texture{},vao{};
    std::wstring cacheKey;
};
}
