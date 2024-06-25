#ifndef VIEWER_H
#define VIEWER_H

#include <QOpenGLWidget>

class Viewer : public QOpenGLWidget
{
    Q_OBJECT
public:
    Viewer(QWidget *parent = nullptr);

    const QImage &image() const { return m_image; }

public slots:
    void setImage(const QImage &image);

signals:
    void requestExitFullScreen();
    void touch(int x, int y);
    void keyPressed(Qt::Key key);
    void keyReleased(Qt::Key key);

protected:
    virtual void paintGL() override;
    virtual void keyPressEvent(QKeyEvent *key) override;
    virtual void keyReleaseEvent(QKeyEvent *key) override;
    virtual void mousePressEvent(QMouseEvent *ev) override;
    virtual void mouseMoveEvent(QMouseEvent *ev) override;
    virtual void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    QTransform generateTransform();

    void emitTouchSignal(const QPointF &position);

    QImage m_image;

};

#endif // VIEWER_H