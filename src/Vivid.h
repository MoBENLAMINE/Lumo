#pragma once

#include <QColor>
#include <QImage>

#include <algorithm>

namespace Vivid {

// The most vivid color of an image, softened to read well as an accent on the
// dark island: Lumo's accent from the wallpaper, and the music card's tint
// from the album art.
inline QColor color(const QImage &source) {
    if (source.isNull()) return {};
    const QImage img = source.width() > 64 ? source.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : source;
    // Hue buckets weighted by saturation and brightness: the most vivid
    // color wins, not the most common (that is usually a gray).
    double weight[24] = {}, r[24] = {}, g[24] = {}, b[24] = {};
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            const double s = c.hsvSaturationF(), v = c.valueF();
            if (s < 0.2 || v < 0.2) continue;
            const int bucket = std::clamp(int(c.hsvHueF() * 24), 0, 23);
            const double w = s * s * v;
            weight[bucket] += w;
            r[bucket] += c.redF() * w; g[bucket] += c.greenF() * w; b[bucket] += c.blueF() * w;
        }
    const int best = int(std::max_element(weight, weight + 24) - weight);
    if (weight[best] <= 0) return {};
    QColor c = QColor::fromRgbF(float(r[best] / weight[best]), float(g[best] / weight[best]), float(b[best] / weight[best]));
    // readable on the dark island: not too dark, not washed out
    float h, s, l;
    c.getHslF(&h, &s, &l);
    return QColor::fromHslF(h, std::max(s, 0.5f), std::clamp(l, 0.52f, 0.68f));
}

}
