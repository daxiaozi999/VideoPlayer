#include "YUVRenderer.h"

static const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    void main()
    {
        gl_Position = vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

static const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;

    uniform sampler2D textureY;
    uniform sampler2D textureU; 
    uniform sampler2D textureV;
    uniform float videoAspectRatio;
    uniform float widgetAspectRatio;
    uniform bool hasVideo;

    void main()
    {
        if (!hasVideo) {
            FragColor = vec4(0.117647, 0.117647, 0.117647, 1.0);
            return;
        }

        vec2 adjustedTexCoord = TexCoord;
        
        if (videoAspectRatio > widgetAspectRatio) {
            float scale = widgetAspectRatio / videoAspectRatio;
            adjustedTexCoord.y = (adjustedTexCoord.y - 0.5) / scale + 0.5;
            
            if (adjustedTexCoord.y < 0.0 || adjustedTexCoord.y > 1.0) {
                FragColor = vec4(0.117647, 0.117647, 0.117647, 1.0);
                return;
            }
        } else {
            float scale = videoAspectRatio / widgetAspectRatio;
            adjustedTexCoord.x = (adjustedTexCoord.x - 0.5) / scale + 0.5;
            
            if (adjustedTexCoord.x < 0.0 || adjustedTexCoord.x > 1.0) {
                FragColor = vec4(0.117647, 0.117647, 0.117647, 1.0);
                return;
            }
        }

        float y = texture(textureY, adjustedTexCoord).r;
        float u = texture(textureU, adjustedTexCoord).r - 0.5;
        float v = texture(textureV, adjustedTexCoord).r - 0.5;
    
        float r = y + 1.403 * v;
        float g = y - 0.344 * u - 0.714 * v;  
        float b = y + 1.770 * u;
    
        FragColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
    }
)";

YUVRenderer::YUVRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
    , shaderProgram(nullptr)
    , vbo(QOpenGLBuffer::VertexBuffer)
    , ebo(QOpenGLBuffer::IndexBuffer)
    , textureY(0)
    , textureU(0)
    , textureV(0)
    , videoWidth(0)
    , videoHeight(0)
    , widgetWidth(0)
    , widgetHeight(0)
    , frameReady(false)
    , textureYLocation(-1)
    , textureULocation(-1)
    , textureVLocation(-1)
    , aspectRatioLocation(-1)
    , widgetAspectLocation(-1)
    , hasVideoLocation(-1)
    , initialized(false)
{
}

YUVRenderer::~YUVRenderer() {
    cleanup();
}

void YUVRenderer::cleanup() {
    if (!initialized) return;

    makeCurrent();

    if (textureY) {
        glDeleteTextures(1, &textureY);
        textureY = 0;
    }
    if (textureU) {
        glDeleteTextures(1, &textureU);
        textureU = 0;
    }
    if (textureV) {
        glDeleteTextures(1, &textureV);
        textureV = 0;
    }

    delete shaderProgram;
    shaderProgram = nullptr;

    initialized = false;
    doneCurrent();
}

void YUVRenderer::showBackground() {
    clearFrame();

    if (initialized) {
        makeCurrent();
        clearTextureData();
        doneCurrent();
    }

    update();
}

void YUVRenderer::updateYUVFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData,
                                 int width, int height,
                                 int yLineSize, int uLineSize, int vLineSize) {

    if (!yData || !uData || !vData) return;
    if (!isValidSize(width, height)) return;
    if (yLineSize <= 0 || uLineSize <= 0 || vLineSize <= 0) return;

    clearFrame();

    QMutexLocker locker(&dataMutex);

    videoWidth = width;
    videoHeight = height;

    int chromaHeight = (height + 1) / 2;

    int ySize = yLineSize * height;
    this->yData.resize(ySize);
    memcpy(this->yData.data(), yData, ySize);

    int uSize = uLineSize * chromaHeight;
    this->uData.resize(uSize);
    memcpy(this->uData.data(), uData, uSize);

    int vSize = vLineSize * chromaHeight;
    this->vData.resize(vSize);
    memcpy(this->vData.data(), vData, vSize);

    frameReady = true;
    update();
}

void YUVRenderer::clearFrame() {
    QMutexLocker locker(&dataMutex);

    frameReady = false;
    videoWidth = 0;
    videoHeight = 0;
    yData.clear();
    uData.clear();
    vData.clear();
}

void YUVRenderer::clearTextureData() {
    if (textureY) {
        glBindTexture(GL_TEXTURE_2D, textureY);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    }
    if (textureU) {
        glBindTexture(GL_TEXTURE_2D, textureU);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    }
    if (textureV) {
        glBindTexture(GL_TEXTURE_2D, textureV);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void YUVRenderer::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.117647f, 0.117647f, 0.117647f, 1.0f);

    if (!initShaders()) {
        return;
    }

    initVertexData();
    initTextures();
    initialized = true;
}

bool YUVRenderer::initShaders() {
    shaderProgram = new QOpenGLShaderProgram(this);

    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        return false;
    }

    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        return false;
    }

    if (!shaderProgram->link()) {
        return false;
    }

    textureYLocation = shaderProgram->uniformLocation("textureY");
    textureULocation = shaderProgram->uniformLocation("textureU");
    textureVLocation = shaderProgram->uniformLocation("textureV");
    aspectRatioLocation = shaderProgram->uniformLocation("videoAspectRatio");
    widgetAspectLocation = shaderProgram->uniformLocation("widgetAspectRatio");
    hasVideoLocation = shaderProgram->uniformLocation("hasVideo");

    return (textureYLocation >= 0 && textureULocation >= 0 && textureVLocation >= 0 &&
        aspectRatioLocation >= 0 && widgetAspectLocation >= 0 && hasVideoLocation >= 0);
}

void YUVRenderer::initVertexData() {
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    vao.create();
    vao.bind();

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    ebo.create();
    ebo.bind();
    ebo.allocate(indices, sizeof(indices));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    vao.release();
    vbo.release();
    ebo.release();
}

void YUVRenderer::initTextures() {
    glGenTextures(1, &textureY);
    glGenTextures(1, &textureU);
    glGenTextures(1, &textureV);

    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

bool YUVRenderer::isValidSize(int width, int height) const {
    return (width >= 16 && width <= 7680 &&
            height >= 16 && height <= 7680);
}

void YUVRenderer::updateTextures() {
    if (!frameReady || !initialized) return;

    QMutexLocker locker(&dataMutex);

    if (videoWidth <= 0 || videoHeight <= 0) return;

    int chromaWidth = (videoWidth + 1) / 2;
    int chromaHeight = (videoHeight + 1) / 2;

    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0,
        GL_RED, GL_UNSIGNED_BYTE, yData.constData());

    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, chromaWidth, chromaHeight, 0,
        GL_RED, GL_UNSIGNED_BYTE, uData.constData());

    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, chromaWidth, chromaHeight, 0,
        GL_RED, GL_UNSIGNED_BYTE, vData.constData());

    glBindTexture(GL_TEXTURE_2D, 0);
    frameReady = false;
}

void YUVRenderer::updateAspectRatio() {
    if (!shaderProgram || !initialized) return;

    shaderProgram->bind();

    bool hasVideo = (videoWidth > 0 && videoHeight > 0);
    shaderProgram->setUniformValue(hasVideoLocation, hasVideo);

    if (hasVideo && widgetWidth > 0 && widgetHeight > 0) {
        float videoAspect = static_cast<float>(videoWidth) / videoHeight;
        float widgetAspect = static_cast<float>(widgetWidth) / widgetHeight;

        shaderProgram->setUniformValue(aspectRatioLocation, videoAspect);
        shaderProgram->setUniformValue(widgetAspectLocation, widgetAspect);
    }

    shaderProgram->release();
}

void YUVRenderer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!initialized || !shaderProgram) {
        return;
    }

    updateTextures();
    updateAspectRatio();

    if (!shaderProgram->bind()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    shaderProgram->setUniformValue(textureYLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureU);
    shaderProgram->setUniformValue(textureULocation, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textureV);
    shaderProgram->setUniformValue(textureVLocation, 2);

    vao.bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    vao.release();

    shaderProgram->release();
}

void YUVRenderer::resizeGL(int width, int height) {
    widgetWidth = width;
    widgetHeight = height;
    glViewport(0, 0, width, height);
    updateAspectRatio();
}