#ifndef CLASSICPLAYLISTWINDOW_H
#define CLASSICPLAYLISTWINDOW_H

#include <QDialog>
#include "qmediaplaylist.h"
#include <QSet>

class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QKeyEvent;
class QWheelEvent;

class ClassicPlaylistWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ClassicPlaylistWindow(QMediaPlaylist *playlist, QWidget *parent = nullptr);

public slots:
    void setPosition(qint64 position);

signals:
    void openFilesRequested();
    void addFolderRequested();
    void addYouTubeRequested();
    void songActivated(int row);
    void moveStarted(bool detach);
    void moveFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QMediaPlaylist *m_playlist = nullptr;
    QSet<int> m_selectedRows;
    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    int m_firstVisibleRow = 0;
    int m_scrollbarDragOffset = 0;
    int m_unshadedHeight = 116;
    qint64 m_position = 0;
    bool m_windowShaded = false;
    bool m_dragging = false;
    bool m_draggingScrollbar = false;
    bool m_draggingRow = false;
    int m_draggedRow = -1;
    bool m_resizing = false;

    int rowAt(const QPoint &position) const;
    void toggleWindowShade();
    void handleButton(const QPoint &position);
    void showAddMenu();
    void showSelectMenu();
    void showMiscMenu();
    void showListMenu();
    void removeSelectedRows();
    void reorderPlaylist(bool reverse);
    void updatePlaylist();
    void handleCurrentIndexChanged(int index);
};

#endif // CLASSICPLAYLISTWINDOW_H
