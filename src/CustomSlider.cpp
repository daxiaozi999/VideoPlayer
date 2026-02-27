#include "CustomSlider.h"

CustomSlider::CustomSlider(QWidget* parent)
    : QSlider(Qt::Horizontal, parent)
    , handleSize(17)
    , grooveHeight(7)
    , wheelEnabled(true)
    , progressColor(Qt::yellow)
    , mouseState(Normal)
    , cacheValid(false) {

    setMinimumHeight(handleSize + 6);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
}

void CustomSlider::setHandleSize(int size) {
    if (handleSize != size && size > 0) {
        handleSize = size;
        setMinimumHeight(handleSize + 6);
        cacheValid = false;
        update();
    }
}

void CustomSlider::setGrooveHeight(int height) {
    if (grooveHeight != height && height > 0) {
        grooveHeight = height;
        cacheValid = false;
        update();
    }
}

void CustomSlider::setWheelEnabled(bool enabled) {
    wheelEnabled = enabled;
}

void CustomSlider::setProgressColor(const QColor& color) {
    if (progressColor != color) {
        progressColor = color;
        update();
    }
}

void CustomSlider::setValue(int value, bool internal) {
    if (!internal && (mouseState == Pressed || mouseState == Dragging)) {
        return;
    }

    if (this->value() != value) {
        QSlider::setValue(value);
        cacheValid = false;
        update();
    }
}

void CustomSlider::updateCache() const {
    if (!cacheValid) {
        const QRect rect = this->rect();
        if (rect.isEmpty()) {
            return;
        }

        const int margin = handleSize / 2;
        const int grooveY = (rect.height() - grooveHeight) / 2;
        cachedGrooveRect = QRect(margin, grooveY, qMax(0, rect.width() - 2 * margin), grooveHeight);

        if (cachedGrooveRect.isEmpty()) {
            cachedHandleRect = QRect();
            cachedProgressRect = QRect();
            cacheValid = true;
            return;
        }

        const int displayValue = value();
        const int range = maximum() - minimum();

        if (range > 0) {
            const int moveableWidth = cachedGrooveRect.width() - handleSize;
            const int pos = QStyle::sliderPositionFromValue(minimum(), maximum(), displayValue, moveableWidth);
            cachedHandleRect = QRect(cachedGrooveRect.left() + pos, (height() - handleSize) / 2, handleSize, handleSize);
        }
        else {
            cachedHandleRect = QRect(cachedGrooveRect.left(), (height() - handleSize) / 2, handleSize, handleSize);
        }

        cachedProgressRect = cachedGrooveRect;
        if (range > 0) {
            const int progressWidth = (displayValue - minimum()) * cachedGrooveRect.width() / range;
            cachedProgressRect.setWidth(qMax(0, progressWidth));
        }
        else {
            cachedProgressRect = QRect();
        }

        cacheValid = true;
    }
}

QRect CustomSlider::getGrooveRect() const {
    updateCache();
    return cachedGrooveRect;
}

QRect CustomSlider::getHandleRect() const {
    updateCache();
    return cachedHandleRect;
}

QRect CustomSlider::getProgressRect() const {
    updateCache();
    return cachedProgressRect;
}

void CustomSlider::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!rect().isEmpty()) {
        drawGroove(painter);
        drawProgress(painter);
        drawHandle(painter);
    }
}

void CustomSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QSlider::mousePressEvent(event);
        return;
    }

    mouseState = Pressed;

    if (getHandleRect().contains(event->pos())) {
        emit sliderPressed(value());
    }
    else {
        const int newValue = qBound(minimum(), valueFromPosition(event->pos()), maximum());
        if (newValue != value()) {
            setValue(newValue, true);
            emit valueChanged(newValue);
        }
        emit sliderPressed(newValue);
    }

    event->accept();
}

void CustomSlider::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        if (mouseState == Pressed) {
            mouseState = Dragging;
        }

        if (mouseState == Dragging) {
            const int newValue = qBound(minimum(), valueFromPosition(event->pos()), maximum());
            if (newValue != value()) {
                setValue(newValue, true);
                emit sliderMoved(newValue);
                emit valueChanged(newValue);
            }
        }
    }

    QSlider::mouseMoveEvent(event);
}

void CustomSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QSlider::mouseReleaseEvent(event);
        return;
    }

    if (mouseState == Dragging || mouseState == Pressed) {
        emit sliderReleased(value());
    }

    if (rect().contains(event->pos())) {
        mouseState = Hovered;
    }
    else {
        mouseState = Normal;
    }

    event->accept();
}

void CustomSlider::enterEvent(QEvent* event) {
    if (mouseState != Dragging && mouseState != Pressed) {
        mouseState = Hovered;
    }

    QSlider::enterEvent(event);
}

void CustomSlider::leaveEvent(QEvent* event) {
    if (mouseState != Dragging && mouseState != Pressed) {
        mouseState = Normal;
    }

    QSlider::leaveEvent(event);
}

void CustomSlider::wheelEvent(QWheelEvent* event) {
    if (!wheelEnabled || mouseState == Dragging) {
        event->ignore();
        return;
    }

    QSlider::wheelEvent(event);
}

void CustomSlider::resizeEvent(QResizeEvent* event) {
    QSlider::resizeEvent(event);
    cacheValid = false;
}

int CustomSlider::valueFromPosition(const QPoint& pos) const {
    const QRect grooveRect = getGrooveRect();
    if (grooveRect.isEmpty()) {
        return value();
    }

    const int position = pos.x() - grooveRect.left();
    return QStyle::sliderValueFromPosition(minimum(), maximum(), position, grooveRect.width());
}

QColor CustomSlider::getHandleColor() const {
    if (!isEnabled()) {
        return QColor(200, 200, 200);
    }

    switch (mouseState) {
    case Normal:
        return QColor(255, 255, 255);
    case Hovered:
        return progressColor;
    case Pressed:
    case Dragging:
        return progressColor.darker(120);
    }

    return QColor(255, 255, 255);
}

void CustomSlider::drawGroove(QPainter& painter) const {
    const QRect grooveRect = getGrooveRect();
    if (grooveRect.isEmpty()) {
        return;
    }

    const int radius = grooveHeight / 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? QColor(255, 255, 255, 100) : QColor(255, 255, 255, 60));
    painter.drawRoundedRect(grooveRect, radius, radius);
}

void CustomSlider::drawHandle(QPainter& painter) const {
    const QRect handleRect = getHandleRect();
    if (handleRect.isEmpty()) {
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(getHandleColor());
    painter.drawEllipse(handleRect);

    if (isEnabled()) {
        QColor borderColor;
        if (mouseState == Normal) {
            borderColor = QColor(180, 180, 180);
        }
        else if (mouseState == Hovered) {
            borderColor = progressColor.darker(120);
        }
        else {
            borderColor = progressColor.darker(130);
        }

        painter.setPen(QPen(borderColor, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(handleRect);
    }
}

void CustomSlider::drawProgress(QPainter& painter) const {
    const QRect progressRect = getProgressRect();
    if (progressRect.width() <= 0 && progressRect.height() <= 0) {
        return;
    }

    const int radius = grooveHeight / 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? progressColor : QColor(200, 200, 200));
    painter.drawRoundedRect(progressRect, radius, radius);
}