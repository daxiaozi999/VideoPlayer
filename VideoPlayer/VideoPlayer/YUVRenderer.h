#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMutex>

class YUVRenderer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit YUVRenderer(QWidget* parent = nullptr);
    ~YUVRenderer();

    void updateYUVFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData,
                        int width, int height,
                        int yLineSize, int uLineSize, int vLineSize);
    void showBackground();

protected:
    void paintGL() override;
    void initializeGL() override;
    void resizeGL(int width, int height) override;

private:
    bool initShaders();
    void initVertexData();
    void initTextures();
    void updateTextures();
    void updateAspectRatio();
    void cleanup();
    void clearFrame();
    void clearTextureData();
    bool isValidSize(int width, int height) const;

private:
    QOpenGLShaderProgram* shaderProgram;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer ebo;

    GLuint textureY;
    GLuint textureU;
    GLuint textureV;

    int videoWidth;
    int videoHeight;
    int widgetWidth;
    int widgetHeight;

    QMutex dataMutex;
    QByteArray yData;
    QByteArray uData;
    QByteArray vData;
    bool frameReady;
    bool initialized;

    int textureYLocation;
    int textureULocation;
    int textureVLocation;
    int aspectRatioLocation;
    int widgetAspectLocation;
    int hasVideoLocation;
};