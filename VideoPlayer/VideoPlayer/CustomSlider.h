#pragma once

#include <QWidget>
#include <QSlider>
#include <QPropertyAnimation>
#include <QColor>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStyle>
#include <QTimer>
#include <QtMath>

class CustomSlider : public QSlider {
    Q_OBJECT
    Q_PROPERTY(QColor progressColor READ getProgressColor WRITE setProgressColor)
    Q_PROPERTY(QColor backgroundColor READ getBackgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(int handleSize READ getHandleSize WRITE setHandleSize)
    Q_PROPERTY(int grooveHeight READ getGrooveHeight WRITE setGrooveHeight)
    Q_PROPERTY(qreal animationValue READ getAnimationValue WRITE setAnimationValue)

public:
    explicit CustomSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    ~CustomSlider();

    QColor getProgressColor()   const { return progressColor; }
    QColor getBackgroundColor() const { return backgroundColor; }
    QColor getHoverColor()      const { return hoverColor; }
    QColor getPressedColor()    const { return pressedColor; }
    QColor getDisabledColor()   const { return disabledColor; }
    qreal getAnimationValue()   const { return animationValue; }
    int getHandleSize()         const { return handleSize; }
    int getGrooveHeight()       const { return grooveHeight; }
    int getAnimationDuration()  const { return animationDuration; }
    QEasingCurve::Type getAnimationEasing() const { return animationEasing; }
    bool isWheelEnabled()       const { return wheelEnabled; }

    void setProgressColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setHoverColor(const QColor& color);
    void setPressedColor(const QColor& color);
    void setDisabledColor(const QColor& color);
    void setAnimationValue(qreal value);
    void setAnimationDuration(int duration);
    void setAnimationEasing(QEasingCurve::Type easing);
    void setHandleSize(int size);
    void setGrooveHeight(int height);
    void setWheelEnabled(bool enabled);

signals:
    void sliderClicked(int value);
    void sliderMoved(int value);
    void sliderReleased(int value);

public slots:
    void setValue(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAnimationValueChanged();

private:
    enum MouseState { Normal, Hovered, Pressed, Dragging };

    void setupSlider();
    void setupAnimation();
    void updateDerivedColors();

    void invalidateGeometryCache();
    void updateGeometryCache() const;
    QRect getGrooveRect() const;
    QRect getHandleRect() const;
    QRect getProgressRect() const;

    bool isPointInHandle(const QPoint& point) const;
    int valueFromPosition(const QPoint& pos) const;
    void updateMouseState(MouseState newState);

    void startAnimation(qreal targetValue, int duration = -1);

    void drawGroove(QPainter& painter) const;
    void drawProgress(QPainter& painter) const;
    void drawHandle(QPainter& painter) const;

    QColor getCurrentProgressColor() const;
    QLinearGradient getHandleGradient(const QRect& handleRect) const;

    bool needsSignificantRepaint(int oldValue, int newValue) const;

private:
    QColor progressColor;
    QColor backgroundColor;
    QColor hoverColor;
    QColor pressedColor;
    QColor disabledColor;

    QPropertyAnimation* animation;
    qreal animationValue;
    int animationDuration;
    QEasingCurve::Type animationEasing;

    int handleSize;
    int grooveHeight;

    MouseState mouseState;
    int tempValue;
    bool wheelEnabled;

    mutable bool geometryCacheValid;
    mutable QRect cachedGrooveRect;
    mutable QRect cachedHandleRect;
    mutable QRect cachedProgressRect;

    bool needsFullRepaint;
    QRect lastHandleRect;
};