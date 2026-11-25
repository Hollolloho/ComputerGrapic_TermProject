#pragma once
#include <GL/glew.h>
#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include <vector>

namespace aegis
{
enum class UiAction
{
    None,
    Start,
    Help,
    Credits,
    Title,
    Advance,
    Analyze,
    Resume,
    Retry,
    Restart,
    Quit
};

struct UiButton
{
    UiAction action;
    std::wstring label;
    float x{};
    float y{};
    float width{};
    float height{};
    bool enabled{true};

    bool Contains(float px, float py) const
    {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};

struct UiPage
{
    std::wstring title, body, footer;
    std::wstring status, warning, timeStatus;
    int health{100};
    bool danger{};
    bool panel{true};
    bool sign{};
    std::vector<UiButton> buttons;
};

class KoreanUi
{
public:
    bool Init(const std::filesystem::path& shaderDirectory = "shaders");
    void Draw(const UiPage& page, int width, int height, float mouseX, float mouseY);
    void DrawWorld(const std::wstring& title, const std::wstring& body, const glm::mat4& viewProjection,
                   const glm::vec3& position, const glm::vec2& size);
    void Clear();
    static UiAction Hit(const UiPage& page, float x, float y, bool& consumed);

private:
    void DrawTexture(const glm::mat4& transform) const;
    bool Rasterize(const UiPage& page, int width, int height, int hover, bool timeOnly);
    GLuint program{};
    GLuint texture{};
    GLuint vao{};
    std::wstring cacheKey;
    std::wstring cachedTimeStatus;
};
} // namespace aegis
