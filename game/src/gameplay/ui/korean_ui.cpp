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
const float TimeBarX = 340, TimeBarY = 704, TimeBarWidth = 344, TimeBarHeight = 36;
const std::uint32_t MenuShade = 0x58000000u;
const COLORREF MainText = RGB(205, 201, 188);
const COLORREF SoftText = RGB(174, 181, 179);
const COLORREF DimText = RGB(143, 151, 150);
const COLORREF KeyText = RGB(190, 181, 163);
const COLORREF HealthFill = RGB(166, 164, 150);
const COLORREF Rule = RGB(108, 112, 118);
const COLORREF Alert = RGB(219, 112, 101);
const UINT SingleLine = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
const UINT BaselineLine = DT_SINGLELINE | DT_END_ELLIPSIS;
const wchar_t* const KoreanFace = L"Malgun Gothic";
const wchar_t* const InstrumentFace = L"Bahnschrift";

const wchar_t* DeviceLabelFace(const std::wstring& text)
{
    for (wchar_t ch : text)
    {
        if (ch < L' ' || ch > L'~')
            return KoreanFace;
    }
    return InstrumentFace;
}

// 메뉴와 안내판의 1024 × 768 좌표를 Windows 비트맵에 그린다.
struct MenuCanvas
{
    HDC dc;
    float sx, sy;
    LONG originX = 0, originY = 0;
    bool shadowText{};
    std::vector<std::pair<std::wstring, void*>>* fonts{};

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
    void Frame(float x, float y, float w, float h, COLORREF color) const
    {
        RECT r = Rectangle(x, y, w, h);
        const auto brush = CreateSolidBrush(color);
        FrameRect(dc, &r, brush);
        DeleteObject(brush);
    }
    HFONT Font(int size, bool bold, const wchar_t* face) const
    {
        const int height = std::max(1, static_cast<int>(std::round(size * std::min(sx, sy))));
        const auto key = std::to_wstring(height) + (bold ? L"B/" : L"N/") + face;
        for (const auto& cached : *fonts)
            if (cached.first == key) return static_cast<HFONT>(cached.second);
        const auto font = CreateFontW(-height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
        if (font) fonts->push_back({key, font});
        return font;
    }
    float TextWidth(const std::wstring& value, int size, bool bold, const wchar_t* face = KoreanFace) const
    {
        if (value.empty())
            return 0;
        const auto font = Font(size, bold, face);
        const auto oldFont = SelectObject(dc, font);
        LONG width = 0;
        for (std::size_t start = 0; start < value.size();)
        {
            const auto next = value.find(L'\n', start);
            const auto end = next == std::wstring::npos ? value.size() : next;
            SIZE extent{};
            GetTextExtentPoint32W(dc, value.c_str() + start, static_cast<int>(end - start), &extent);
            width = std::max(width, extent.cx);
            start = end + 1;
        }
        SelectObject(dc, oldFont);
        return width / sx;
    }
    void Text(const std::wstring& value, float x, float y, float w, float h, int size, bool bold,
              COLORREF color, UINT format = DT_LEFT | DT_WORDBREAK, const wchar_t* face = KoreanFace) const
    {
        if (value.empty() || w <= 0 || h <= 0)
            return;
        const auto font = Font(size, bold, face);
        const auto oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        RECT r = Rectangle(x, y, w, h);
        if (format == BaselineLine)
        {
            TEXTMETRICW metrics{};
            GetTextMetricsW(dc, &metrics);
            r.top -= metrics.tmAscent;
            r.bottom = r.top + metrics.tmHeight;
        }
        if (shadowText)
        {
            RECT shadow = r;
            OffsetRect(&shadow, 1, 1);
            SetTextColor(dc, RGB(24, 26, 25));
            DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &shadow, format | DT_NOPREFIX);
        }
        SetTextColor(dc, color);
        DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r,
                  format | DT_NOPREFIX);
        SelectObject(dc, oldFont);
    }
    void TextAtBaseline(const std::wstring& value, float x, float y, float w, int size, bool bold,
                        COLORREF color, const wchar_t* face = KoreanFace) const
    {
        Text(value, x, y, w, 1, size, bold, color, BaselineLine, face);
    }
};

void DrawNotice(const MenuCanvas& canvas, const std::wstring& text, float y, float height,
                float maxWidth, int size, COLORREF color, bool danger = false)
{
    if (text.empty())
        return;
    const float width = std::min(maxWidth, canvas.TextWidth(text, size, false) + 30.f);
    const float x = (1024.f - width) * .5f;
    if (danger)
        canvas.Fill(x + 3, y + 8, 3, height - 16, Alert);
    canvas.Text(text, x + 12, y, width - 24, height, size, false, color, SingleLine | DT_CENTER);
}

void DrawTimeStatus(const MenuCanvas& canvas, const std::wstring& timeStatus)
{
    DrawNotice(canvas, timeStatus, TimeBarY, TimeBarHeight, TimeBarWidth, 18, SoftText);
}

void DrawPrompt(const MenuCanvas& canvas, const std::wstring& prompt)
{
    if (prompt.find(L"E · ") != 0)
    {
        DrawNotice(canvas, prompt, 426, 38, 684, 19, MainText);
        return;
    }
    const auto action = prompt.substr(4);
    const float keyWidth = canvas.TextWidth(L"E", 23, false, InstrumentFace) + 14;
    const float actionWidth = std::min(684.f - keyWidth, canvas.TextWidth(action, 19, false) + 4.f);
    const float x = (1024.f - keyWidth - actionWidth) * .5f;
    canvas.TextAtBaseline(L"E", x, 451, keyWidth, 23, false, KeyText, InstrumentFace);
    canvas.TextAtBaseline(action, x + keyWidth, 451, actionWidth, 19, false, MainText);
}

void DrawEquipment(const MenuCanvas& canvas, const UiPage& page)
{
    const int health = std::clamp(page.health, 0, 100);
    const COLORREF healthColor = health <= 25 ? Alert : MainText;
    const auto count = std::to_wstring(health);
    const float countWidth = canvas.TextWidth(count, 40, false, InstrumentFace);
    canvas.TextAtBaseline(L"+", 40, 700, 17, 17, false, health <= 25 ? Alert : SoftText, InstrumentFace);
    canvas.TextAtBaseline(count, 62, 707, countWidth + 4, 40, false, healthColor, InstrumentFace);
    canvas.TextAtBaseline(L"/ 100", 71 + countWidth, 707, 60, 16, false, SoftText, InstrumentFace);
    canvas.Fill(40, 728, 160, 3, RGB(74, 74, 71));
    canvas.Fill(40, 728, 160.f * health / 100, 3, health <= 25 ? Alert : HealthFill);

    const auto split = page.status.find(L'\n');
    const auto label = page.status.substr(0, split);
    const bool hasState = split != std::wstring::npos && split + 1 < page.status.size();
    canvas.Text(label, 704, hasState ? 672.f : 699.f, 280, 32, 24, false, MainText,
                SingleLine | DT_RIGHT, DeviceLabelFace(label));
    if (hasState)
    {
        canvas.Text(page.status.substr(split + 1), 704, 706, 280, 27, 19, false,
                    SoftText, SingleLine | DT_RIGHT);
    }
}

bool ButtonOverTimeBar(const UiPage& page)
{
    for (const auto& button : page.buttons)
    {
        if (button.x < TimeBarX + TimeBarWidth && button.x + button.width > TimeBarX &&
            button.y < TimeBarY + TimeBarHeight && button.y + button.height > TimeBarY)
            return true;
    }
    return false;
}

void DrawHud(const MenuCanvas& canvas, const UiPage& page)
{
    if (!page.title.empty() || !page.body.empty())
    {
        const bool twoLines = page.body.find(L'\n') != std::wstring::npos;
        const float goalHeight = page.body.empty() ? 0.f : twoLines ? 53.f : 29.f;
        const float titleHeight = page.title.empty() ? 0.f : 36.f;
        const float goalWidth = canvas.TextWidth(page.body, 18, false);
        const float width = std::min(590.f, std::max(canvas.TextWidth(page.title, 28, false), goalWidth) + 32.f);
        canvas.Text(page.title, 38, 26, width - 28, titleHeight, 28, false, MainText, SingleLine);
        canvas.Text(page.body, 38, 26 + titleHeight, width - 28, goalHeight, 18, false, SoftText,
                    twoLines ? DT_WORDBREAK : SingleLine);
    }
    if (!page.counters.empty())
    {
        const bool twoLines = page.counters.find(L'\n') != std::wstring::npos;
        const float height = twoLines ? 60.f : 36.f;
        const float width = std::min(284.f, canvas.TextWidth(page.counters, 20, false) + 28.f);
        const float x = 1000.f - width;
        canvas.Text(page.counters, x + 12, twoLines ? 28.f : 22.f, width - 24, height - (twoLines ? 10.f : 0.f),
                    20, false, MainText, DT_RIGHT | (twoLines ? DT_WORDBREAK : SingleLine));
    }
    DrawNotice(canvas, page.warning, 122, 38, 700, page.danger ? 22 : 19,
               page.danger ? MainText : SoftText, page.danger);
    DrawPrompt(canvas, page.prompt);
    if (!page.status.empty())
        DrawEquipment(canvas, page);
    DrawTimeStatus(canvas, page.timeStatus);
    DrawNotice(canvas, page.footer, 744, 22, 790, 15, SoftText);
}

void DrawButtons(const MenuCanvas& canvas, const UiPage& page, int hover)
{
    for (int i = 0; i < static_cast<int>(page.buttons.size()); ++i)
    {
        const auto& b = page.buttons[i];
        const bool hovered = b.enabled && hover == i;
        const bool focused = b.enabled && page.focus == i;
        const int textSize = page.panel ? 22 : 20;
        const COLORREF color = !b.enabled ? DimText : hovered || focused ? MainText : SoftText;
        const float textX = b.x + 19, maxWidth = b.width - 33;
        if (focused)
            canvas.Fill(b.x + 6, b.y + 8, 3, b.height - 16, Alert);
        canvas.Text(b.label, textX, b.y, maxWidth, b.height, textSize, false, color, SingleLine);
        if (hovered && maxWidth > 0)
        {
            const float underlineY = b.y + std::min(b.height - 3.f, b.height * .5f + textSize * .62f);
            const float width = std::min(maxWidth, canvas.TextWidth(b.label, textSize, false));
            canvas.Fill(textX, underlineY, width, 2, MainText);
        }
    }
}

void AddCacheField(std::wstring& key, const std::wstring& field)
{
    key += std::to_wstring(field.size()) + L"/" + field;
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
    consumed = page.panel;
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

bool KoreanUi::Rasterize(const UiPage& page, int width, int height, int hover, UiRegion part)
{
    const bool timeOnly = part == UiRegion::Time;
    MenuCanvas canvas{nullptr, static_cast<float>(width) / 1024, static_cast<float>(height) / 768};
    RECT region{0, 0, width, height};
    // 그림자 여백까지 포함한다. 장비 영역과 겹치는 시간 표시는 함께 다시 그린다.
    switch (part)
    {
    case UiRegion::Time: region = canvas.Rectangle(TimeBarX, TimeBarY, TimeBarWidth, TimeBarHeight); break;
    case UiRegion::Equipment: region = canvas.Rectangle(28, 660, 968, 82); break;
    case UiRegion::Warning: region = canvas.Rectangle(148, 116, 728, 52); break;
    case UiRegion::Prompt: region = canvas.Rectangle(150, 414, 728, 64); break;
    default: break;
    }
    const int rasterWidth = region.right - region.left, rasterHeight = region.bottom - region.top;
    if (rasterWidth <= 0 || rasterHeight <= 0)
    {
        return true;
    }
    // 화면 크기만 바뀔 때 작업 비트맵을 만든다. HUD 변경에는 같은 DC·픽셀 버퍼를 쓴다.
    if (!canvasDc || canvasWidth != width || canvasHeight != height)
    {
        ClearCanvas();
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        const auto dc = CreateCompatibleDC(nullptr);
        const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &canvasPixels, nullptr, 0);
        if (!dc || !bitmap || !canvasPixels)
        {
            if (bitmap) DeleteObject(bitmap);
            if (dc) DeleteDC(dc);
            canvasPixels = nullptr;
            return false;
        }
        canvasOldBitmap = SelectObject(dc, bitmap);
        canvasDc = dc;
        canvasBitmap = bitmap;
        canvasWidth = width;
        canvasHeight = height;
    }
    const auto dc = static_cast<HDC>(canvasDc);
    const auto pixels = canvasPixels;
    const int savedDc = SaveDC(dc);
    IntersectClipRect(dc, 0, 0, rasterWidth, rasterHeight);
    for (int y = 0; y < rasterHeight; ++y)
        std::fill_n(static_cast<std::uint32_t*>(pixels) + y * canvasWidth, rasterWidth, 0u);
    canvas.fonts = &fonts;
    canvas.dc = dc;
    canvas.originX = region.left;
    canvas.originY = region.top;
    canvas.shadowText = !page.sign;
    if (timeOnly)
    {
        DrawTimeStatus(canvas, page.timeStatus);
    }
    else if (part == UiRegion::Equipment)
    {
        if (!page.status.empty()) DrawEquipment(canvas, page);
        DrawTimeStatus(canvas, page.timeStatus);
    }
    else if (part == UiRegion::Warning)
    {
        DrawNotice(canvas, page.warning, 122, 38, 700, page.danger ? 22 : 19,
                   page.danger ? MainText : SoftText, page.danger);
    }
    else if (part == UiRegion::Prompt)
    {
        DrawPrompt(canvas, page.prompt);
    }
    else if (page.sign)
    {
        canvas.Fill(0, 0, 1024, 768, RGB(25, 27, 30));
        canvas.Frame(2, 3, 1020, 762, Rule);
        canvas.Fill(6, 12, 12, 744, RGB(153, 65, 60));
        canvas.Text(page.title, 58, 107, 914, 215, 127, true, RGB(241, 242, 243));
        canvas.Text(page.body, 58, 376, 914, 308, 72, true, RGB(207, 210, 214));
    }
    else if (page.panel)
    {
        canvas.Text(page.title, 218, 136, 590, 85, 50, false, MainText,
                    SingleLine, DeviceLabelFace(page.title));
        canvas.Text(page.body, 218, 235, 590, 225, 23, false, SoftText);
        canvas.Text(page.footer, 218, 624, 590, 36, 16, false, DimText, SingleLine);
    }
    else
    {
        DrawHud(canvas, page);
    }
    if (part == UiRegion::Full)
        DrawButtons(canvas, page, hover);
    GdiFlush();
    auto* rgba = static_cast<std::uint32_t*>(pixels);
    const bool shadeMenu = page.panel && !page.sign && part == UiRegion::Full;
    for (int y = 0; y < rasterHeight; ++y)
    {
        for (int x = 0; x < rasterWidth; ++x)
        {
            auto& pixel = rgba[y * canvasWidth + x];
            const auto rgb = pixel & 0x00ffffffu;
            pixel = rgb != 0 ? (rgb | 0xff000000u) : shadeMenu ? MenuShade : 0u;
        }
    }
    GLint oldRowLength{};
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &oldRowLength);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, canvasWidth);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (textureWidth == width && textureHeight == height)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, region.left, region.top, rasterWidth, rasterHeight,
                        GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        textureWidth = width;
        textureHeight = height;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, oldRowLength);
    RestoreDC(dc, savedDc);
    return true;
}

void KoreanUi::Draw(const UiPage& page, int width, int height, float mouseX, float mouseY)
{
    if (!program || width <= 0 || height <= 0)
    {
        return;
    }
    int hover = -1;
    std::wstring key = L"SCREEN/" + std::to_wstring(width) + L"/" + std::to_wstring(height) + L"/" +
                       std::to_wstring(page.focus) + L"/" + std::to_wstring(page.health) +
                       (page.danger ? L"D" : L"N") + (page.sign ? L"S" : page.panel ? L"P" : L"H");
    AddCacheField(key, page.title);
    AddCacheField(key, page.body);
    AddCacheField(key, page.footer);
    AddCacheField(key, page.status);
    AddCacheField(key, page.warning);
    AddCacheField(key, page.counters);
    AddCacheField(key, page.prompt);
    for (std::size_t i = 0; i < page.buttons.size(); ++i)
    {
        const auto& b = page.buttons[i];
        if (b.Contains(mouseX, mouseY))
        {
            hover = static_cast<int>(i);
        }
        AddCacheField(key, b.label);
        key += (b.enabled ? L"Y/" : L"N/") + std::to_wstring(b.x) + L"/" + std::to_wstring(b.y) +
               L"/" + std::to_wstring(b.width) + L"/" + std::to_wstring(b.height) + L"/";
    }
    key += L"H/" + std::to_wstring(hover);
    const bool timeChanged = !page.panel && !page.sign && page.timeStatus != cachedTimeStatus;
    const bool fullUpdate = key != cacheKey || (timeChanged && ButtonOverTimeBar(page));
    if (fullUpdate || timeChanged)
    {
        const auto simpleHud = [](const UiPage& value)
        {
            return !value.panel && !value.sign && value.buttons.empty() && value.title.empty() &&
                   value.body.empty() && value.counters.empty() && value.footer.empty();
        };
        const bool partialHud = cacheKey.find(L"SCREEN/") == 0 && textureWidth == width && textureHeight == height &&
                                simpleHud(page) && simpleHud(cachedPage);
        if (partialHud)
        {
            const bool equipment = page.health != cachedPage.health || page.status != cachedPage.status;
            if (equipment && !Rasterize(page, width, height, hover, UiRegion::Equipment)) return;
            if ((page.warning != cachedPage.warning || page.danger != cachedPage.danger) &&
                !Rasterize(page, width, height, hover, UiRegion::Warning)) return;
            if (page.prompt != cachedPage.prompt && !Rasterize(page, width, height, hover, UiRegion::Prompt)) return;
            if (timeChanged && !equipment && !Rasterize(page, width, height, hover, UiRegion::Time)) return;
        }
        else if (!Rasterize(page, width, height, hover, fullUpdate ? UiRegion::Full : UiRegion::Time))
        {
            return;
        }
        cacheKey = key;
        cachedTimeStatus = page.timeStatus;
        cachedPage = page;
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
    std::wstring key = L"WORLD/";
    AddCacheField(key, title);
    AddCacheField(key, body);
    if (key != cacheKey)
    {
        if (!Rasterize(page, 512, 256, -1, UiRegion::Full))
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

void KoreanUi::ClearCanvas()
{
    if (canvasDc)
    {
        const auto dc = static_cast<HDC>(canvasDc);
        SelectObject(dc, static_cast<HGDIOBJ>(canvasOldBitmap));
        DeleteObject(static_cast<HBITMAP>(canvasBitmap));
        DeleteDC(dc);
    }
    for (const auto& font : fonts) DeleteObject(static_cast<HFONT>(font.second));
    fonts.clear();
    canvasDc = canvasBitmap = canvasOldBitmap = canvasPixels = nullptr;
    canvasWidth = canvasHeight = 0;
}

void KoreanUi::Clear()
{
    ClearCanvas();
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
    cachedPage = {};
    textureWidth = textureHeight = 0;
}
} // namespace aegis
