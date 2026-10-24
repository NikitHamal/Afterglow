#include "ui/layout.h"
#include <algorithm>

namespace ag::ui {

std::vector<Rectangle> flow(const std::vector<float>& widthsIn, float x, float y, float w, float rowH, float rowPitch, float gap,
                            bool balance, float* bottom) {
    std::vector<float> cw(widthsIn);
    for (float& c : cw) c = std::min(c, w);
    auto rowEnds = [&](float wrap) {   // greedy line breaks at this width: one past each row's last item
        std::vector<size_t> ends;
        for (size_t i = 0; i < cw.size();) {
            size_t j = i + 1;
            for (float used = cw[i]; j < cw.size() && used + gap + cw[j] <= wrap; ++j) used += gap + cw[j];
            ends.push_back(j);
            i = j;
        }
        return ends;
    };
    std::vector<size_t> ends = rowEnds(w);
    if (balance && ends.size() > 1) {   // the narrowest wrap that keeps the row count evens the rows out
        float lo = cw.empty() ? 0.f : *std::max_element(cw.begin(), cw.end()), hi = w;
        for (int it = 0; it < 20; ++it) {
            float mid = 0.5f * (lo + hi);
            if (rowEnds(mid).size() <= ends.size()) hi = mid; else lo = mid;
        }
        ends = rowEnds(hi);
    }
    std::vector<Rectangle> out(cw.size());
    size_t i = 0;
    for (size_t e : ends) {
        float used = gap * (float)(e - i - 1);
        for (size_t k = i; k < e; ++k) used += cw[k];
        float extra = (balance && ends.size() > 1) ? (w - used) / (float)(e - i) : 0.f;
        for (float cx = x; i < e; ++i) { out[i] = Rectangle{cx, y, cw[i] + extra, rowH}; cx += cw[i] + extra + gap; }
        y += rowPitch;
    }
    if (bottom) *bottom = y;
    return out;
}

std::vector<Rectangle> chipFlow(Ui& U, const std::vector<std::string>& labels, float x, float y, float w, float* bottom) {
    const float pad = U.u(18.f);
    std::vector<float> cw;
    for (auto& l : labels) cw.push_back(U.measure(Face::SansMedium, U.u(16.f), l, 0.04f).x + 2.f * pad);
    return flow(cw, x, y, w, U.u(32.f), U.u(40.f), U.u(10.f), true, bottom);
}

}  // namespace ag::ui
