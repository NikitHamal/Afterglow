// Afterglow UI — layout helpers over the immediate-mode kit (ui.h). Screens use these so that what they draw
// always fits: RAII scale scopes and containers, equal cells, and a measured flow layout for rows of chips/cards.
#pragma once
#include "ui/ui.h"
#include <string>
#include <vector>

namespace ag::ui {

// u() inside is multiplied by k until the scope ends (see Ui::pushScale)
struct ScaleScope {
    Ui& U;
    ScaleScope(Ui& u, float k) : U(u) { U.pushScale(k); }
    ~ScaleScope() { U.popScale(); }
    ScaleScope(const ScaleScope&) = delete;
    ScaleScope& operator=(const ScaleScope&) = delete;
};

// a container for the QA guard: what is drawn inside must stay inside (see Ui::pushClip)
struct ClipScope {
    Ui& U;
    ClipScope(Ui& u, Rectangle r, const std::string& id) : U(u) { U.pushClip(r, id); }
    ~ClipScope() { U.popClip(); }
    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;
};

// cell i of n equal cells across `row`, `gap` apart
inline Rectangle cell(Rectangle row, int n, int i, float gap) {
    float w = (row.width - gap * (float)(n - 1)) / (float)n;
    return Rectangle{row.x + (w + gap) * (float)i, row.y, w, row.height};
}

// Flow layout: items of the given widths in as few rows as `w` allows. balance = the rows are evened out (no lone
// item on the last row) and justified to the full width; otherwise rows are left-aligned at their natural widths.
// Items wider than `w` are clamped to it (their labels ellipsise). Returns one rect per item; *bottom = the y under
// the last row (rowPitch included).
std::vector<Rectangle> flow(const std::vector<float>& widths, float x, float y, float w, float rowH, float rowPitch, float gap,
                            bool balance, float* bottom);

// chips sized to their labels (pad each side), flowed and balanced across `w`
std::vector<Rectangle> chipFlow(Ui& U, const std::vector<std::string>& labels, float x, float y, float w, float* bottom);

}  // namespace ag::ui
