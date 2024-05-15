#include "viewer.h"

#include <QPainter>

Viewer::Viewer(QWidget *parent) : QOpenGLWidget(parent)
{
}

void Viewer::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void Viewer::paintGL()
{
    if (m_image.isNull()) {
        return;
    }

    QPainter p(this);

    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QTransform transform;

    transform.translate(this->width()/2, this->height()/2);

    qreal scale = qMin(this->height() / (qreal) m_image.height(), this->width() / (qreal) m_image.width());
    transform.scale(scale, scale);

    p.setTransform(transform);

    p.drawImage(-m_image.width()/2, -m_image.height()/2, m_image);
}