#pragma once
#include <QAbstractButton>
#include <QColor>

// Interruttore on/off disegnato a mano; i colori arrivano dal tema via QSS (qproperty-*).
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(QColor onColor READ onColor WRITE setOnColor)
    Q_PROPERTY(QColor offColor READ offColor WRITE setOffColor)
    Q_PROPERTY(QColor knobColor READ knobColor WRITE setKnobColor)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(44, 24); }

    QColor onColor() const { return m_on; }
    QColor offColor() const { return m_off; }
    QColor knobColor() const { return m_knob; }
    void setOnColor(const QColor& c) { m_on = c; update(); }
    void setOffColor(const QColor& c) { m_off = c; update(); }
    void setKnobColor(const QColor& c) { m_knob = c; update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor m_on{"#e50914"}, m_off{"#444444"}, m_knob{"#ffffff"};
};
