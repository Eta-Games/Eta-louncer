#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QColor>
#include <QtMath>

// Icone vettoriali disegnate a runtime (niente font, niente file): restano nitide a qualsiasi scala.
namespace Icons {

inline QIcon fromPainter(void (*draw)(QPainter&, const QColor&), const QColor& color) {
    const int S = 64;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    draw(p, color);
    p.end();
    return QIcon(pm);
}

// Freccia circolare (controlla aggiornamenti)
inline QIcon refresh(const QColor& c = QColor("#e8e8e8")) {
    return fromPainter([](QPainter& p, const QColor& col) {
        QPen pen(col, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        const QPointF ctr(32, 32);
        const double r = 20;
        p.drawArc(QRectF(ctr.x() - r, ctr.y() - r, 2 * r, 2 * r), 60 * 16, 270 * 16);

        // punta della freccia alla fine dell'arco (330°, senso antiorario)
        const double th = qDegreesToRadians(330.0);
        const QPointF end(ctr.x() + r * qCos(th), ctr.y() - r * qSin(th));
        const QPointF t(-qSin(th), -qCos(th));          // tangente
        const QPointF n(-t.y(), t.x());                 // normale
        QPolygonF head;
        head << end + t * 12 << end + n * 10 << end - n * 10;
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPolygon(head);
    }, c);
}

// Freccia verso il basso con vassoio (aggiornamento forzato)
inline QIcon download(const QColor& c = QColor("#e8e8e8")) {
    return fromPainter([](QPainter& p, const QColor& col) {
        QPen pen(col, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(32, 10), QPointF(32, 40));
        p.drawPolyline(QPolygonF() << QPointF(19, 28) << QPointF(32, 41) << QPointF(45, 28));
        p.drawLine(QPointF(14, 54), QPointF(50, 54));
    }, c);
}

// Stella piena (recensioni)
inline QIcon star(const QColor& c = QColor("#f5b301")) {
    return fromPainter([](QPainter& p, const QColor& col) {
        QPolygonF poly;
        const QPointF ctr(32, 34);
        for (int i = 0; i < 10; ++i) {
            const double rad = (i % 2 == 0) ? 26.0 : 11.0;
            const double a = qDegreesToRadians(-90.0 + i * 36.0);
            poly << QPointF(ctr.x() + rad * qCos(a), ctr.y() + rad * qSin(a));
        }
        p.setPen(QPen(col, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(col);
        p.drawPolygon(poly);
    }, c);
}

} // namespace Icons
