#include "ToggleSwitch.h"
#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? m_on : m_off);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);

    const qreal d = r.height() - 6;
    const qreal x = isChecked() ? r.right() - d - 3 : r.left() + 3;
    p.setBrush(m_knob);
    p.drawEllipse(QRectF(x, r.top() + 3, d, d));
}
