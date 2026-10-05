#pragma once
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

// Scala/ritaglia una copertina su w×h con gli angoli superiori arrotondati (r = 0 → angoli dritti).
inline QPixmap makeRoundedCover(const QPixmap& src, int w, int h, qreal r) {
    QPixmap out(w, h);
    out.fill(Qt::transparent);
    if (src.isNull()) return out;
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, w, h + r), r, r); // il bordo inferiore resta dritto
    p.setClipPath(path);
    QPixmap scaled = src.scaled(QSize(w, h), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap((w - scaled.width()) / 2, (h - scaled.height()) / 2, scaled);
    p.end();
    return out;
}
