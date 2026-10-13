#pragma once
#include <imgui.h>

// Draws a filled circle at the cursor, vertically centered on a line of
// height `lineHeight`, then moves the cursor to the right of it (on the same
// line) leaving `gapAfter` pixels before whatever is drawn next.
//
// Used for status lights instead of a Unicode dot glyph: ImGui's default
// font is ASCII-only and would render such a glyph as a missing-glyph box.
inline void drawFilledDot(ImVec4 color, float lineHeight, float gapAfter) {
    float radius = lineHeight * 0.35f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center(cursor.x + radius, cursor.y + lineHeight * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(color));
    ImGui::Dummy(ImVec2(radius * 2.0f + gapAfter, lineHeight));
    ImGui::SameLine();
}
