#pragma once

#include <atomic>
#include <QStyle>
#include <QColor>
#include <QSlider>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>

class CustomSlider : public QSlider {
    Q_OBJECT
    Q_PROPERTY(int handleSize READ getHandleSize WRITE setHandleSize)
    Q_PROPERTY(int grooveHeight READ getGrooveHeight WRITE setGrooveHeight)
    Q_PROPERTY(QColor progressColor READ getProgressColor WRITE setProgressColor)

public:
    explicit CustomSlider(QWidget* parent = nullptr);
    ~CustomSlider() = default;

    int getHandleSize()       const { return handleSize; }
    int getGrooveHeight()     const { return grooveHeight; }
    bool isWheelEnabled()     const { return wheelEnabled; }
    QColor getProgressColor() const { return progressColor; }

    void setHandleSize(int size);
    void setGrooveHeight(int height);
    void setWheelEnabled(bool enabled);
    void setProgressColor(const QColor& color);

public slots:
    void setValue(int value, bool internal = false);

signals:
    void sliderPressed(int value);
    void sliderMoved(int value);
    void sliderReleased(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum MouseState {
        Normal,
        Hovered,
        Pressed,
        Dragging
    };

    void updateCache() const;
    QRect getGrooveRect() const;
    QRect getHandleRect() const;
    QRect getProgressRect() const;
    int valueFromPosition(const QPoint& pos) const;
    QColor getHandleColor() const;
    void drawGroove(QPainter& painter) const;
    void drawHandle(QPainter& painter) const;
    void drawProgress(QPainter& painter) const;

private:
    int handleSize;
    int grooveHeight;
    bool wheelEnabled;
    QColor progressColor;
    MouseState mouseState;

    mutable bool cacheValid;
    mutable QRect cachedGrooveRect;
    mutable QRect cachedHandleRect;
    mutable QRect cachedProgressRect;
};