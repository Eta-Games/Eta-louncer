#pragma once
#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void themeChanged(QString themeId);

private:
    QComboBox* m_themeCombo;
    QLabel* m_installDirLabel;
    QLabel* m_gzdoomLabel;
    QLabel* m_doom2WadLabel;

    void refreshLabels();
};
