#ifndef DESKTOPBASEWINDOW_H
#define DESKTOPBASEWINDOW_H

#include <QWidget>

class QMoveEvent;
class QPaintEvent;

namespace Ui {
class DesktopBaseWindow;
}

class DesktopBaseWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopBaseWindow(QWidget *parent = nullptr);
    ~DesktopBaseWindow();
    Ui::DesktopBaseWindow *ui;

protected:
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    void scale();
};

#endif // DESKTOPBASEWINDOW_H
