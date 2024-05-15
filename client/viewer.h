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

protected:
    virtual void paintGL() override;

private:
    QImage m_image;

};

#endif // VIEWER_H