#include "CustomSlider.h"

CustomSlider::CustomSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
    , progressColor(QColor(64, 158, 255))
    , backgroundColor(QColor(240, 242, 247))
    , disabledColor(QColor(200, 200, 200))
    , animation(nullptr)
    , animationValue(0.0)
    , animationDuration(300)
    , animationEasing(QEasingCurve::OutCubic)
    , handleSize(17)
    , grooveHeight(7)
    , mouseState(Normal)
    , tempValue(0)
    , wheelEnabled(true)
    , geometryCacheValid(false)
    , needsFullRepaint(true) {

    setupSlider();
}

CustomSlider::~CustomSlider() {
    if (animation) {
        animation->stop();
    }
}

void CustomSlider::setupSlider() {
    setupAnimation();
    updateDerivedColors();
    setMinimumHeight(handleSize + 6);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    tempValue = value();
}

void CustomSlider::setupAnimation() {
    animation = new QPropertyAnimation(this, "animationValue");
    animation->setDuration(animationDuration);
    animation->setEasingCurve(animationEasing);
    connect(animation, &QPropertyAnimation::valueChanged, this, &CustomSlider::onAnimationValueChanged);
}

void CustomSlider::updateDerivedColors() {
    hoverColor = progressColor.lighter(120);
    pressedColor = progressColor.darker(110);
}

void CustomSlider::setProgressColor(const QColor& color) {
    if (progressColor != color) {
        progressColor = color;
        updateDerivedColors();
        update();
    }
}

void CustomSlider::setBackgroundColor(const QColor& color) {
    if (backgroundColor != color) {
        backgroundColor = color;
        update();
    }
}

void CustomSlider::setHoverColor(const QColor& color) {
    if (hoverColor != color) {
        hoverColor = color;
        update();
    }
}

void CustomSlider::setPressedColor(const QColor& color) {
    if (pressedColor != color) {
        pressedColor = color;
        update();
    }
}

void CustomSlider::setDisabledColor(const QColor& color) {
    if (disabledColor != color) {
        disabledColor = color;
        update();
    }
}

void CustomSlider::setAnimationValue(qreal value) {
    if (qAbs(animationValue - value) > 0.001) {
        animationValue = value;
        update();
    }
}

void CustomSlider::setAnimationDuration(int duration) {
    if (duration > 0 && animationDuration != duration) {
        animationDuration = duration;
        if (animation) {
            animation->setDuration(duration);
        }
    }
}

void CustomSlider::setAnimationEasing(QEasingCurve::Type easing) {
    if (animationEasing != easing) {
        animationEasing = easing;
        if (animation) {
            animation->setEasingCurve(easing);
        }
    }
}

void CustomSlider::setHandleSize(int size) {
    if (handleSize != size && size > 0) {
        handleSize = size;
        setMinimumHeight(handleSize + 6);
        invalidateGeometryCache();
        update();
    }
}

void CustomSlider::setGrooveHeight(int height) {
    if (grooveHeight != height && height > 0) {
        grooveHeight = height;
        invalidateGeometryCache();
        update();
    }
}

void CustomSlider::setWheelEnabled(bool enabled) {
    wheelEnabled = enabled;
}

void CustomSlider::onAnimationValueChanged() {
    if (!needsFullRepaint && geometryCacheValid) {
        QRect updateRect = lastHandleRect.united(getHandleRect());
        updateRect.adjust(-2, -2, 2, 2);
        QRect progressRect = getProgressRect();
        updateRect = updateRect.united(progressRect);
        update(updateRect);
    }
    else {
        update();
    }
}

void CustomSlider::startAnimation(qreal targetValue, int duration) {
    if (!animation) return;

    if (qAbs(animationValue - targetValue) < 0.001) return;

    animation->stop();
    animation->setStartValue(animationValue);
    animation->setEndValue(targetValue);
    animation->setDuration(duration > 0 ? duration : animationDuration);
    animation->setEasingCurve(animationEasing);
    animation->start();
}

void CustomSlider::invalidateGeometryCache() {
    geometryCacheValid = false;
    needsFullRepaint = true;
}

void CustomSlider::updateGeometryCache() const {
    if (geometryCacheValid) return;

    const QRect rect = this->rect();
    if (rect.isEmpty()) return;

    const int margin = handleSize / 2;

    if (orientation() == Qt::Horizontal) {
        const int grooveY = (rect.height() - grooveHeight) / 2;
        cachedGrooveRect = QRect(margin, grooveY,
            qMax(0, rect.width() - 2 * margin), grooveHeight);
    }
    else {
        const int grooveX = (rect.width() - grooveHeight) / 2;
        cachedGrooveRect = QRect(grooveX, margin, grooveHeight,
            qMax(0, rect.height() - 2 * margin));
    }

    if (cachedGrooveRect.width() <= 0 || cachedGrooveRect.height() <= 0) {
        cachedHandleRect = QRect();
        cachedProgressRect = QRect();
        geometryCacheValid = true;
        return;
    }

    const int displayValue = (mouseState == Dragging) ? tempValue : value();
    const int range = maximum() - minimum();

    if (range > 0) {
        if (orientation() == Qt::Horizontal) {
            const int pos = QStyle::sliderPositionFromValue(minimum(), maximum(),
                displayValue, cachedGrooveRect.width());
            cachedHandleRect = QRect(cachedGrooveRect.left() + pos - handleSize / 2,
                (height() - handleSize) / 2, handleSize, handleSize);
        }
        else {
            const int pos = QStyle::sliderPositionFromValue(minimum(), maximum(),
                displayValue, cachedGrooveRect.height());
            cachedHandleRect = QRect((width() - handleSize) / 2,
                cachedGrooveRect.bottom() - pos - handleSize / 2,
                handleSize, handleSize);
        }
    }
    else {
        if (orientation() == Qt::Horizontal) {
            cachedHandleRect = QRect(cachedGrooveRect.left() - handleSize / 2,
                (height() - handleSize) / 2, handleSize, handleSize);
        }
        else {
            cachedHandleRect = QRect((width() - handleSize) / 2,
                cachedGrooveRect.bottom() - handleSize / 2,
                handleSize, handleSize);
        }
    }

    cachedProgressRect = cachedGrooveRect;
    if (range > 0) {
        if (orientation() == Qt::Horizontal) {
            const int progressWidth = (displayValue - minimum()) * cachedGrooveRect.width() / range;
            cachedProgressRect.setWidth(qMax(0, progressWidth));
        }
        else {
            const int progressHeight = (displayValue - minimum()) * cachedGrooveRect.height() / range;
            cachedProgressRect.setTop(cachedGrooveRect.bottom() - qMax(0, progressHeight));
        }
    }
    else {
        cachedProgressRect = QRect();
    }

    geometryCacheValid = true;
}

QRect CustomSlider::getGrooveRect() const {
    updateGeometryCache();
    return cachedGrooveRect;
}

QRect CustomSlider::getHandleRect() const {
    updateGeometryCache();
    return cachedHandleRect;
}

QRect CustomSlider::getProgressRect() const {
    updateGeometryCache();
    return cachedProgressRect;
}

int CustomSlider::valueFromPosition(const QPoint& pos) const {
    const QRect grooveRect = getGrooveRect();
    if (grooveRect.isEmpty()) return value();

    if (orientation() == Qt::Horizontal) {
        const int position = pos.x() - grooveRect.left();
        return QStyle::sliderValueFromPosition(minimum(), maximum(),
            position, grooveRect.width());
    }
    else {
        const int position = grooveRect.bottom() - pos.y();
        return QStyle::sliderValueFromPosition(minimum(), maximum(),
            position, grooveRect.height());
    }
}

bool CustomSlider::isPointInHandle(const QPoint& point) const {
    return getHandleRect().contains(point);
}

bool CustomSlider::needsSignificantRepaint(int oldValue, int newValue) const {
    const int range = maximum() - minimum();
    if (range <= 0) return false;

    const int threshold = qMax(1, static_cast<int>(range * 0.05));
    return qAbs(newValue - oldValue) >= threshold;
}

void CustomSlider::updateMouseState(MouseState newState) {
    if (mouseState == newState) return;

    mouseState = newState;

    switch (mouseState) {
    case Normal:
        setCursor(Qt::ArrowCursor);
        startAnimation(0.0);
        break;
    case Hovered:
        setCursor(Qt::PointingHandCursor);
        startAnimation(0.8);
        break;
    case Pressed:
    case Dragging:
        setCursor(Qt::PointingHandCursor);
        startAnimation(1.0);
        break;
    }
}

void CustomSlider::setValue(int value) {
    const int clampedValue = qBound(minimum(), value, maximum());
    if (this->value() != clampedValue) {
        const int oldValue = this->value();
        lastHandleRect = getHandleRect();

        QSlider::setValue(clampedValue);
        tempValue = clampedValue;

        invalidateGeometryCache();
        needsFullRepaint = needsSignificantRepaint(oldValue, clampedValue);
        update();
    }
}

void CustomSlider::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (rect().isEmpty()) return;

    drawGroove(painter);
    drawProgress(painter);
    drawHandle(painter);

    needsFullRepaint = false;
}

void CustomSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QSlider::mousePressEvent(event);
        return;
    }

    if (isPointInHandle(event->pos())) {
        updateMouseState(Pressed);
        tempValue = value();
    }
    else {
        updateMouseState(Pressed);
        const int newValue = qBound(minimum(), valueFromPosition(event->pos()), maximum());
        if (newValue != value()) {
            lastHandleRect = getHandleRect();
            QSlider::setValue(newValue);
            tempValue = newValue;
            invalidateGeometryCache();
            needsFullRepaint = needsSignificantRepaint(value(), newValue);
            update();
            emit sliderClicked(newValue);
        }
    }

    event->accept();
}

void CustomSlider::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) && mouseState == Pressed) {
        updateMouseState(Dragging);
    }

    if (mouseState == Dragging) {
        const int newValue = qBound(minimum(), valueFromPosition(event->pos()), maximum());
        if (newValue != tempValue) {
            tempValue = newValue;
            invalidateGeometryCache();
            needsFullRepaint = needsSignificantRepaint(tempValue, newValue);
            update();
            emit sliderMoved(newValue);
        }
    }

    QSlider::mouseMoveEvent(event);
}

void CustomSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QSlider::mouseReleaseEvent(event);
        return;
    }

    const bool wasDragging = (mouseState == Dragging);

    if (wasDragging) {
        if (tempValue != value()) {
            lastHandleRect = getHandleRect();
            QSlider::setValue(tempValue);
            invalidateGeometryCache();
            needsFullRepaint = needsSignificantRepaint(value(), tempValue);
            update();
        }
        emit sliderReleased(tempValue);
    }
    else {
        emit sliderReleased(value());
    }

    updateMouseState(rect().contains(event->pos()) ? Hovered : Normal);
    event->accept();
}

void CustomSlider::enterEvent(QEvent* event) {
    if (mouseState != Dragging && mouseState != Pressed) {
        updateMouseState(Hovered);
    }
    QSlider::enterEvent(event);
}

void CustomSlider::leaveEvent(QEvent* event) {
    if (mouseState != Dragging && mouseState != Pressed) {
        updateMouseState(Normal);
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
    invalidateGeometryCache();
}

void CustomSlider::drawGroove(QPainter& painter) const {
    const QRect grooveRect = getGrooveRect();
    if (grooveRect.isEmpty()) return;

    const int grooveRadius = grooveHeight / 2;

    painter.setPen(Qt::NoPen);

    if (mouseState == Hovered && animationValue > 0.3) {
        QColor glowColor = progressColor;
        glowColor.setAlpha(static_cast<int>(15 * animationValue));
        const QRect glowRect = grooveRect.adjusted(-1, -1, 1, 1);
        painter.setBrush(glowColor);
        painter.drawRoundedRect(glowRect, grooveRadius + 1, grooveRadius + 1);
    }

    painter.setBrush(isEnabled() ? backgroundColor : disabledColor);
    painter.drawRoundedRect(grooveRect, grooveRadius, grooveRadius);
}

void CustomSlider::drawProgress(QPainter& painter) const {
    const QRect progressRect = getProgressRect();
    if (progressRect.width() <= 0 && progressRect.height() <= 0) return;

    const int grooveRadius = grooveHeight / 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? getCurrentProgressColor() : disabledColor);
    painter.drawRoundedRect(progressRect, grooveRadius, grooveRadius);
}

void CustomSlider::drawHandle(QPainter& painter) const {
    const QRect handleRect = getHandleRect();
    if (handleRect.isEmpty()) return;

    qreal scale = 1.0;
    if (mouseState == Pressed || mouseState == Dragging) {
        scale = 0.95 + 0.05 * animationValue;
    }
    else if (mouseState == Hovered) {
        scale = 1.0 + 0.03 * animationValue;
    }

    if (qAbs(scale - 1.0) > 0.001) {
        painter.save();
        const QPointF center = handleRect.center();
        painter.translate(center);
        painter.scale(scale, scale);
        painter.translate(-center);
    }

    painter.setPen(Qt::NoPen);

    if (isEnabled() && mouseState != Normal && animationValue > 0.2) {
        const QRect shadowRect = handleRect.adjusted(1, 1, 1, 1);
        const QColor shadowColor = QColor(0, 0, 0, static_cast<int>(20 * animationValue));
        painter.setBrush(shadowColor);
        painter.drawEllipse(shadowRect);
    }

    painter.setBrush(QBrush(getHandleGradient(handleRect)));
    painter.drawEllipse(handleRect);

    if (isEnabled()) {
        if (mouseState == Normal) {
            const QPen borderPen(QColor(180, 180, 180), 1);
            painter.setPen(borderPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(handleRect);
        }
        else if (mouseState == Hovered) {
            QColor borderColor = progressColor;
            borderColor.setAlpha(static_cast<int>(80 + 40 * animationValue));
            const QPen borderPen(borderColor, 1 + animationValue * 0.5);
            painter.setPen(borderPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(handleRect);
        }
    }

    if (qAbs(scale - 1.0) > 0.001) {
        painter.restore();
    }
}

QColor CustomSlider::getCurrentProgressColor() const {
    if (!isEnabled()) return disabledColor;

    switch (mouseState) {
    case Pressed:
    case Dragging: {
        const qreal factor = qMin(1.0, animationValue);
        return QColor(
            progressColor.red() + static_cast<int>((pressedColor.red() - progressColor.red()) * factor),
            progressColor.green() + static_cast<int>((pressedColor.green() - progressColor.green()) * factor),
            progressColor.blue() + static_cast<int>((pressedColor.blue() - progressColor.blue()) * factor)
        );
    }
    case Hovered: {
        const qreal factor = animationValue * 0.6;
        return QColor(
            progressColor.red() + static_cast<int>((hoverColor.red() - progressColor.red()) * factor),
            progressColor.green() + static_cast<int>((hoverColor.green() - progressColor.green()) * factor),
            progressColor.blue() + static_cast<int>((hoverColor.blue() - progressColor.blue()) * factor)
        );
    }
    default:
        return progressColor;
    }
}

QLinearGradient CustomSlider::getHandleGradient(const QRect& handleRect) const {
    QLinearGradient gradient;
    if (orientation() == Qt::Horizontal) {
        gradient = QLinearGradient(0, handleRect.top(), 0, handleRect.bottom());
    }
    else {
        gradient = QLinearGradient(handleRect.left(), 0, handleRect.right(), 0);
    }

    if (!isEnabled()) {
        gradient.setColorAt(0, QColor(220, 220, 220));
        gradient.setColorAt(0.5, QColor(200, 200, 200));
        gradient.setColorAt(1, QColor(180, 180, 180));
    }
    else {
        switch (mouseState) {
        case Pressed:
        case Dragging:
            gradient.setColorAt(0, QColor(210, 210, 210));
            gradient.setColorAt(0.5, QColor(195, 195, 195));
            gradient.setColorAt(1, QColor(180, 180, 180));
            break;
        case Hovered: {
            const int brightness = static_cast<int>(240 + 10 * animationValue);
            gradient.setColorAt(0, QColor(255, 255, 255));
            gradient.setColorAt(0.5, QColor(brightness, brightness, brightness));
            gradient.setColorAt(1, QColor(brightness - 8, brightness - 8, brightness - 8));
            break;
        }
        default:
            gradient.setColorAt(0, QColor(250, 250, 250));
            gradient.setColorAt(0.5, QColor(242, 242, 242));
            gradient.setColorAt(1, QColor(235, 235, 235));
            break;
        }
    }

    return gradient;
}