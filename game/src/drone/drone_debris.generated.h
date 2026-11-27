// 드론 원본의 닫힌 기계 부품을 8개 파편으로 분리한 런타임 메타데이터.
#pragma once
#include <array>
#include <glm/vec3.hpp>
namespace aegis::drone_debris_asset {
inline constexpr std::size_t pieceCount = 8;
struct Piece { const char* file; glm::vec3 origin, halfSize; };
inline const std::array<Piece, pieceCount> pieces{{
    {"00_chassis.obj", {0.00000000f, 0.00000000f, 0.02700000f}, {0.58700001f, 0.19499999f, 0.48199999f}},
    {"01_top_shell.obj", {0.00000000f, 0.31999999f, -0.03100000f}, {0.62000000f, 0.19499999f, 0.44400001f}},
    {"02_bottom_shell.obj", {0.00000000f, -0.23500001f, -0.04000001f}, {0.54000002f, 0.06500001f, 0.39499998f}},
    {"03_left_housing.obj", {-0.89999998f, 0.04175000f, -0.02000000f}, {0.48500001f, 0.11075000f, 0.36500001f}},
    {"04_left_rotor.obj", {-1.01999998f, 0.04900000f, -0.02000000f}, {0.25500000f, 0.04400000f, 0.25500000f}},
    {"05_right_housing.obj", {0.89999998f, 0.04175000f, -0.02000000f}, {0.48500001f, 0.11075000f, 0.36500001f}},
    {"06_right_rotor.obj", {1.01999998f, 0.04900000f, -0.02000000f}, {0.25500000f, 0.04400000f, 0.25500000f}},
    {"07_sensor.obj", {0.00000000f, -0.01500000f, 0.54449999f}, {0.22700000f, 0.22700000f, 0.11949998f}},
}};
}
