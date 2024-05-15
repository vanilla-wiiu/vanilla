#ifndef VIEWER_H
#define VIEWER_H

#include <QOpenGLWidget>

class Viewer : public QOpenGLWidget
{
    Q_OBJECT
public:
    Viewer(QWidget *parent = nullptr);

public slots:
    void setImage(const QImage &image);

signals:
    void requestExitFullScreen();

protected:
    virtual void paintGL() override;
    virtual void keyPressEvent(QKeyEvent *key) override;

private:
    QImage m_image;

};

#endif // VIEWER_H