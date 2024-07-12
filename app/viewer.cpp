#include "viewer.h"

#include <QKeyEvent>
#include <QPainter>

Viewer::Viewer(QWidget *parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void Viewer::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void Viewer::paintGL()
{
    QPainter p(this);
    
    p.fillRect(this->rect(), Qt::black);
    
    if (m_image.isNull()) {
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.setTransform(generateTransform());

    p.drawImage(-m_image.width()/2, -m_image.height()/2, m_image);
}

void Viewer::keyPressEvent(QKeyEvent *key)
{
    QOpenGLWidget::keyPressEvent(key);

    if (key->key() == Qt::Key_Escape) {
        emit requestExitFullScreen();
    } else {
        emit keyPressed(static_cast<Qt::Key>(key->key()));
    }
}

void Viewer::keyReleaseEvent(QKeyEvent *key)
{
    QOpenGLWidget::keyPressEvent(key);

    if (key->key() != Qt::Key_Escape) {
        emit keyReleased(static_cast<Qt::Key>(key->key()));
    }
}

void Viewer::mousePressEvent(QMouseEvent *ev)
{
    QOpenGLWidget::mousePressEvent(ev);

    emitTouchSignal(ev->position());
}

void Viewer::mouseMoveEvent(QMouseEvent *ev)
{
    QOpenGLWidget::mouseMoveEvent(ev);

    if (ev->buttons() != Qt::NoButton) {
        emitTouchSignal(ev->position());
    }
}

void Viewer::mouseReleaseEvent(QMouseEvent *ev)
{
    QOpenGLWidget::mouseReleaseEvent(ev);

    emit touch(-1, -1);
}

QTransform Viewer::generateTransform()
{
    QTransform transform;

    transform.translate(this->width()/2, this->height()/2);

    qreal scale = qMin(this->height() / (qreal) m_image.height(), this->width() / (qreal) m_image.width());
    transform.scale(scale, scale);

    return transform;
}

void Viewer::emitTouchSignal(const QPointF &position)
{
    QTransform t = generateTransform();
    t.translate(-m_image.width()/2, -m_image.height()/2);
    QPointF p = t.inverted().map(position);
    emit touch(std::clamp((int) p.x(), 0, m_image.width()-1), std::clamp((int) p.y(), 0, m_image.height()-1));
}