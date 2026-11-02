#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace Ui {
class TitleBar;
}

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar();
    void setTitle(const QString &title);
    void setClassicEqualizerStyle();

signals:
    void menuClicked();
    void shadeClicked();
    void moveStarted(bool detach);
    void moveFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::TitleBar *ui;
    void scale();
    QPointF m_pCursor;
    bool m_dragging = false;
    bool m_classicEqualizerStyle = false;
    bool m_pressedEqualizerClose = false;
    bool m_pressedEqualizerShade = false;
};

#endif // TITLEBAR_H
