// Windows 글꼴로 한글 텍스처를 만들고 메뉴 또는 세계 좌표 안내판으로 그린다.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "korean_ui.h"
#include "loader/Shader.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace aegis
{
namespace
{
constexpr float TimeBarX = 24, TimeBarY = 560, TimeBarWidth = 976, TimeBarHeight = 34;

// 메뉴와 안내판의 1024 × 768 좌표를 Windows 비트맵에 그린다.
struct MenuCanvas
{
    HDC dc;
    float sx, sy;
    LONG originX = 0, originY = 0;

    RECT Rectangle(float x, float y, float w, float h) const
    {
        return RECT{static_cast<LONG>(x * sx) - originX, static_cast<LONG>(y * sy) - originY,
                    static_cast<LONG>((x + w) * sx) - originX, static_cast<LONG>((y + h) * sy) - originY};
    }
    void Fill(float x, float y, float w, float h, COLORREF color) const
    {
        RECT r = Rectangle(x, y, w, h);
        const auto brush = CreateSolidBrush(color);
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    }
    void Text(const std::wstring& value, float x, float y, float w, float h, int size, bool bold,
              COLORREF color) const
    {
        const auto font =
            CreateFontW(-static_cast<int>(std::round(size * std::min(sx, sy))), 0, 0, 0,
                        bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
        const auto oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        RECT r = Rectangle(x, y, w, h);
        DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
};

void DrawTimeStatus(const MenuCanvas& canvas, const std::wstring& timeStatus)
{
    if (!timeStatus.empty())
    {
        canvas.Fill(TimeBarX, TimeBarY, TimeBarWidth, TimeBarHeight, RGB(12, 21, 34));
        canvas.Text(timeStatus, 42, 565, 944, 26, 16, false, RGB(167, 211, 234));
    }
}
} // namespace

bool KoreanUi::Init(const std::filesystem::path& shaderDirectory)
{
    Clear();
    program = LoadShaderProgram(shaderDirectory / "ui.vert", shaderDirectory / "ui.frag");
    if (!program)
    {
        return false;
    }
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "ui"), 0);
    glUseProgram(0);
    glGenVertexArrays(1, &vao);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

UiAction KoreanUi::Hit(const UiPage& page, float x, float y, bool& consumed)
{
    consumed = page.panel || (x >= 24 && x <= 1000 && y >= 604 && y <= 726);
    for (const auto& b : page.buttons)
    {
        if (b.Contains(x, y))
        {
            consumed = true;
            return b.enabled ? b.action : UiAction::None;
        }
    }
    return UiAction::None;
}

bool KoreanUi::Rasterize(const UiPage& page, int width, int height, int hover, bool timeOnly)
{
    MenuCanvas canvas{nullptr, static_cast<float>(width) / 1024, static_cast<float>(height) / 768};
    const RECT region = timeOnly ? canvas.Rectangle(TimeBarX, TimeBarY, TimeBarWidth, TimeBarHeight)
                                 : RECT{0, 0, width, height};
    const int rasterWidth = region.right - region.left, rasterHeight = region.bottom - region.top;
    if (rasterWidth <= 0 || rasterHeight <= 0)
    {
        return true;
    }
    const auto pixelCount = static_cast<std::size_t>(rasterWidth) * rasterHeight;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rasterWidth;
    info.bmiHeader.biHeight = -rasterHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels{};
    const auto dc = CreateCompatibleDC(nullptr);
    const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dc || !bitmap || !pixels)
    {
        if (bitmap)
        {
            DeleteObject(bitmap);
        }
        if (dc)
        {
            DeleteDC(dc);
        }
        return false;
    }
    const auto oldBitmap = SelectObject(dc, bitmap);
    std::fill_n(static_cast<std::uint32_t*>(pixels), pixelCount, 0u);
    canvas.dc = dc;
    canvas.originX = region.left;
    canvas.originY = region.top;
    if (timeOnly)
    {
        DrawTimeStatus(canvas, page.timeStatus);
    }
    else if (page.sign)
    {
        canvas.Fill(0, 0, 1024, 768, RGB(12, 24, 34));
        canvas.Fill(0, 0, 20, 768, RGB(45, 212, 191));
        canvas.Text(page.title, 56, 70, 920, 250, 132, true, RGB(167, 243, 208));
        canvas.Text(page.body, 56, 360, 920, 350, 76, false, RGB(228, 242, 249));
    }
    else if (page.panel)
    {
        canvas.Fill(170, 100, 684, 570, RGB(12, 21, 34));
        canvas.Fill(170, 100, 8, 570, RGB(45, 212, 191));
        canvas.Text(page.title, 218, 145, 590, 70, 38, true, RGB(226, 245, 249));
        canvas.Text(page.body, 218, 235, 590, 225, 23, false, RGB(197, 212, 229));
        canvas.Text(page.footer, 218, 624, 590, 36, 16, false, RGB(139, 170, 192));
    }
    else
    {
        DrawTimeStatus(canvas, page.timeStatus);
        if (!page.status.empty())
        {
            const auto accent = page.danger ? RGB(255, 120, 95) : RGB(100, 235, 205);
            canvas.Fill(24, 20, 976, 112, RGB(12, 21, 34));
            canvas.Text(page.status, 42, 30, 944, 32, 22, true, RGB(228, 242, 249));
            canvas.Fill(42, 69, 240, 10, RGB(51, 65, 81));
            canvas.Fill(42, 69, 240.0f * std::clamp(page.health, 0, 100) / 100, 10,
                 page.health <= 25 ? RGB(255, 120, 95) : RGB(100, 235, 205));
            canvas.Text(page.warning, 42, 89, 944, 32, 20, true, accent);
        }
        canvas.Fill(24, 604, 976, 122, RGB(12, 21, 34));
        canvas.Fill(24, 604, 5, 122, RGB(45, 212, 191));
        canvas.Text(page.title, 46, 618, 640, 34, 23, true, RGB(167, 243, 208));
        canvas.Text(page.body, 46, 659, page.buttons.empty() ? 930.0f : 650.0f, 54, 17, false, RGB(219, 234, 244));
    }
    for (std::size_t i = 0; !timeOnly && i < page.buttons.size(); ++i)
    {
        const auto& b = page.buttons[i];
        const bool primary = i == 0;
        COLORREF bg = !b.enabled ? RGB(51, 65, 81) : primary ? RGB(45, 212, 191) : RGB(40, 62, 83);
        if (static_cast<int>(i) == hover && b.enabled)
        {
            bg = primary ? RGB(94, 234, 212) : RGB(57, 88, 113);
        }
        canvas.Fill(b.x, b.y, b.width, b.height, bg);
        canvas.Text(b.label, b.x + 18, b.y + 11, b.width - 32, b.height - 12, 21, true,
             !b.enabled ? RGB(148, 163, 184)
             : primary  ? RGB(8, 32, 40)
                        : RGB(240, 249, 255));
    }
    auto* rgba = static_cast<std::uint32_t*>(pixels);
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        if (rgba[i] & 0x00ffffffu)
        {
            rgba[i] |= 0xff000000u;
        }
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    if (timeOnly)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, region.left, region.top, rasterWidth, rasterHeight,
                        GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return true;
}

void KoreanUi::Draw(const UiPage& page, int width, int height, float mouseX, float mouseY)
{
    if (!program || width <= 0 || height <= 0)
    {
        return;
    }
    int hover = -1;
    std::wstring key = page.title + page.body + page.footer + page.status + page.warning +
                       std::to_wstring(page.health) + (page.danger ? L"D" : L"S") + std::to_wstring(width) +
                       L"/" + std::to_wstring(height) + (page.panel ? L"P" : L"H");
    for (std::size_t i = 0; i < page.buttons.size(); ++i)
    {
        const auto& b = page.buttons[i];
        if (b.Contains(mouseX, mouseY))
        {
            hover = static_cast<int>(i);
        }
        key += b.label + (b.enabled ? L"Y" : L"N");
    }
    key += std::to_wstring(hover);
    const bool fullUpdate = key != cacheKey;
    if (fullUpdate || (!page.panel && !page.sign && page.timeStatus != cachedTimeStatus))
    {
        if (!Rasterize(page, width, height, hover, !fullUpdate))
        {
            return;
        }
        cacheKey = key;
        cachedTimeStatus = page.timeStatus;
    }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawTexture(glm::mat4(1));
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void KoreanUi::DrawWorld(const std::wstring& title, const std::wstring& body, const glm::mat4& viewProjection,
                         const glm::vec3& position, const glm::vec2& size)
{
    if (!program)
    {
        return;
    }
    UiPage page;
    page.sign = true;
    page.title = title;
    page.body = body;
    const auto key = L"WORLD SIGN:" + title + L"/" + body;
    if (key != cacheKey)
    {
        if (!Rasterize(page, 512, 256, -1, false))
        {
            return;
        }
        cacheKey = key;
    }
    const auto transform = viewProjection * glm::scale(glm::translate(glm::mat4(1), position),
                                                       glm::vec3(size.x * .5f, size.y * .5f, 1));
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    DrawTexture(transform);
}

void KoreanUi::DrawTexture(const glm::mat4& transform) const
{
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "clipTransform"), 1, GL_FALSE,
                       glm::value_ptr(transform));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void KoreanUi::Clear()
{
    if (program)
    {
        glDeleteProgram(program);
    }
    if (texture)
    {
        glDeleteTextures(1, &texture);
    }
    if (vao)
    {
        glDeleteVertexArrays(1, &vao);
    }
    program = texture = vao = 0;
    cacheKey.clear();
    cachedTimeStatus.clear();
}
} // namespace aegis
