#ifndef LINKAMPSLIDER_H
#define LINKAMPSLIDER_H

#include <QSlider>
#include <QImage>
#include <QColor>

class LinkampSlider : public QSlider
{
    Q_OBJECT
public:
    LinkampSlider(QWidget* parent);

    void setGradient(QColor from, QColor to, Qt::Orientation orientation);

    void setGradient(QList<QColor> steps, Qt::Orientation orientation);
    QString baseStyleSheet() const { return baseStylesheet; }
    void refreshSkin() { changeColor(value()); }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void changeColor(int);

private:
    QImage gradient;
    QString baseStylesheet;
};

#endif // LINKAMPSLIDER_H
