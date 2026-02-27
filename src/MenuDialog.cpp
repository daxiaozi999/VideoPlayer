#include "MenuDialog.h"

MenuDialog::MenuDialog(QWidget* parent)
    : QDialog(parent)
    , mainLayout(nullptr)
    , modeGroupBox(nullptr)
    , localRadio(nullptr)
    , networkRadio(nullptr)
    , modeButtonGroup(nullptr)
    , localWidget(nullptr)
    , filePathEdit(nullptr)
    , browseButton(nullptr)
    , networkWidget(nullptr)
    , urlLineEdit(nullptr)
    , okButton(nullptr)
    , cancelButton(nullptr)
    , playMode(PlayMode::NONE)
    , filePath("")
    , networkUrl("")
    , validateTimer(nullptr) {

    setupUi();
    setupConnections();
}

void MenuDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && localRadio->isChecked()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString fileName = urls.first().toLocalFile();
            if (isValidVideoFile(fileName)) {
                event->acceptProposedAction();
                return;
            }
        }
    }

    event->ignore();
}

void MenuDialog::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString fileName = urls.first().toLocalFile();
        if (isValidVideoFile(fileName)) {
            filePathEdit->setText(fileName);
            event->acceptProposedAction();
        }
    }
}

void MenuDialog::onPlayModeChanged() {
    updateControlsVisible();

    if (localRadio->isChecked()) {
        if (!urlLineEdit->text().trimmed().isEmpty()) {
            updateInputFieldStyle(urlLineEdit, true);
            networkUrl.clear();
            urlLineEdit->clear();
        }
    }
    else if (networkRadio->isChecked()) {
        if (!filePathEdit->text().trimmed().isEmpty()) {
            updateInputFieldStyle(filePathEdit, true);
            filePath.clear();
            filePathEdit->clear();
        }
    }

    validateCurrentInput();
}

void MenuDialog::onBrowseClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Video File",
        QDir::homePath(),
        "Video Files (*.mp4 *.m4v *.mov *.avi *.mkv *.wmv *.flv *.3gp *.ts *.webm *.mpg *.mpeg);;All Files (*.*)"
    );

    if (!fileName.isEmpty()) {
        filePathEdit->setText(fileName);
    }
}

void MenuDialog::onFilePathChanged(const QString& text) {
    filePath = text.trimmed();
    if (localRadio->isChecked()) {
        validateTimer->start();
    }
}

void MenuDialog::onNetworkUrlChanged(const QString& text) {
    networkUrl = text.trimmed();
    if (networkRadio->isChecked()) {
        validateTimer->start();
    }
}

void MenuDialog::onOkClicked() {
    validateCurrentInput();

    if (playMode == PlayMode::NONE) {
        showValidationError();
        return;
    }

    accept();
}

void MenuDialog::onCancelClicked() {
    reject();
}

void MenuDialog::setupUi() {
    setWindowTitle("Video Player - Menu");
    setMinimumSize(500, 500);
    resize(500, 500);
    setStyleSheet(getDialogStyle());

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(25);

    setupModeSelections();
    setupLocalControls();
    setupNetworkControls();
    setupButtons();
    setupTimer();

    mainLayout->addWidget(modeGroupBox);
    mainLayout->addWidget(localWidget);
    mainLayout->addWidget(networkWidget);
    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 20, 0, 0);
    buttonLayout->setSpacing(20);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    setModal(true);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAcceptDrops(true);

    localRadio->setChecked(true);
    updateControlsVisible();
    okButton->setEnabled(false);
}

void MenuDialog::setupModeSelections() {
    modeGroupBox = new QGroupBox("Playback Mode", this);
    modeGroupBox->setFixedHeight(200);
    modeGroupBox->setStyleSheet(getGroupBoxStyle());

    auto* modeLayout = new QVBoxLayout(modeGroupBox);
    modeLayout->setSpacing(15);
    modeLayout->setContentsMargins(25, 25, 25, 20);

    modeButtonGroup = new QButtonGroup(this);

    localRadio = new QRadioButton("Local File", this);
    localRadio->setIcon(QIcon(":/VideoPlayer/icons/directory.png"));
    localRadio->setIconSize(QSize(40, 40));
    localRadio->setStyleSheet(getRadioButtonStyle());

    networkRadio = new QRadioButton("Network Stream", this);
    networkRadio->setIcon(QIcon(":/VideoPlayer/icons/network.png"));
    networkRadio->setIconSize(QSize(40, 40));
    networkRadio->setStyleSheet(getRadioButtonStyle());

    modeButtonGroup->addButton(localRadio, static_cast<int>(PlayMode::FILE));
    modeButtonGroup->addButton(networkRadio, static_cast<int>(PlayMode::NETWORK));

    modeLayout->addWidget(localRadio);
    modeLayout->addWidget(networkRadio);
}

void MenuDialog::setupLocalControls() {
    localWidget = new QWidget(this);
    auto* localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(0, 0, 0, 0);
    localLayout->setSpacing(12);

    auto* fileLabel = new QLabel("File Path:", this);
    fileLabel->setStyleSheet(R"(
        color: #333333; 
        font-size: 18px;
        font-weight: 600;
        margin-bottom: 5px;
    )");

    auto* fileRowLayout = new QHBoxLayout();
    fileRowLayout->setSpacing(15);

    filePathEdit = new QLineEdit(this);
    filePathEdit->setPlaceholderText("Select video file to play or paste file path...");
    filePathEdit->setStyleSheet(getLineEditStyle());

    browseButton = new QPushButton(this);
    browseButton->setIcon(QIcon(":/VideoPlayer/icons/directory.png"));
    browseButton->setIconSize(QSize(45, 45));
    browseButton->setStyleSheet(getButtonStyle());
    browseButton->setFixedSize(60, 50);
    browseButton->setToolTip("Browse for video file");

    fileRowLayout->addWidget(filePathEdit);
    fileRowLayout->addWidget(browseButton);
    localLayout->addWidget(fileLabel);
    localLayout->addLayout(fileRowLayout);
}

void MenuDialog::setupNetworkControls() {
    networkWidget = new QWidget(this);
    auto* networkLayout = new QVBoxLayout(networkWidget);
    networkLayout->setContentsMargins(0, 0, 0, 0);
    networkLayout->setSpacing(12);

    auto* urlLabel = new QLabel("Network URL:", this);
    urlLabel->setStyleSheet(R"(
        color: #333333; 
        font-size: 18px;
        font-weight: 600;
        margin-bottom: 5px;
    )");

    urlLineEdit = new QLineEdit(this);
    urlLineEdit->setPlaceholderText("Enter network stream URL (http://, rtmp://, rtsp://...)");
    urlLineEdit->setStyleSheet(getLineEditStyle());

    networkLayout->addWidget(urlLabel);
    networkLayout->addWidget(urlLineEdit);
}

void MenuDialog::setupButtons() {
    okButton = new QPushButton("OK", this);
    okButton->setStyleSheet(getButtonStyle() + R"(
        QPushButton {
            background: #27ae60;
            min-width: 80px;
            min-height: 45px;
            font-size: 18px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #2ecc71;
        }
        QPushButton:disabled {
            background: #bdc3c7;
            color: #7f8c8d;
        }
    )");
    okButton->setDefault(true);

    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setStyleSheet(getButtonStyle() + R"(
        QPushButton {
            background: #e74c3c;
            min-width: 80px;
            min-height: 45px;
            font-size: 18px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #c0392b;
        }
    )");
}

void MenuDialog::setupTimer() {
    validateTimer = new QTimer(this);
    validateTimer->setSingleShot(true);
    validateTimer->setInterval(300);
}

void MenuDialog::setupConnections() {
    connect(modeButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &MenuDialog::onPlayModeChanged);
    connect(browseButton, &QPushButton::clicked, this, &MenuDialog::onBrowseClicked);
    connect(okButton, &QPushButton::clicked, this, &MenuDialog::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &MenuDialog::onCancelClicked);
    connect(filePathEdit, &QLineEdit::textChanged, this, &MenuDialog::onFilePathChanged);
    connect(urlLineEdit, &QLineEdit::textChanged, this, &MenuDialog::onNetworkUrlChanged);
    connect(validateTimer, &QTimer::timeout, this, &MenuDialog::validateCurrentInput);
}

void MenuDialog::updateControlsVisible() {
    localWidget->setVisible(localRadio->isChecked());
    networkWidget->setVisible(networkRadio->isChecked());
}

void MenuDialog::updateInputFieldStyle(QLineEdit* field, bool isValid) {
    QString baseStyle = getLineEditStyle();

    if (!isValid && !field->text().trimmed().isEmpty()) {
        field->setStyleSheet(baseStyle + R"(
            QLineEdit {
                border: 2px solid #e74c3c;
            }
        )");
    }
    else if (isValid && !field->text().trimmed().isEmpty()) {
        field->setStyleSheet(baseStyle + R"(
            QLineEdit {
                border: 2px solid #27ae60;
            }
        )");
    }
    else {
        field->setStyleSheet(baseStyle);
    }
}

void MenuDialog::validateCurrentInput() {
    PlayMode newMode = PlayMode::NONE;

    if (localRadio->isChecked()) {
        bool isValid = isValidVideoFile(filePath);
        newMode = isValid ? PlayMode::FILE : PlayMode::NONE;
        updateInputFieldStyle(filePathEdit, isValid || filePath.isEmpty());
    }
    else if (networkRadio->isChecked()) {
        bool isValid = isValidNetworkUrl(networkUrl);
        newMode = isValid ? PlayMode::NETWORK : PlayMode::NONE;
        updateInputFieldStyle(urlLineEdit, isValid || networkUrl.isEmpty());
    }

    playMode = newMode;
    if (playMode != PlayMode::NONE) {
        okButton->setEnabled(true);
    }
    else {
        okButton->setEnabled(false);
    }
}

void MenuDialog::showValidationError() {
    QString message;
    QString title;

    if (localRadio->isChecked()) {
        title = "Invalid File";
        if (filePath.isEmpty()) {
            message = "Please select a video file.";
        }
        else {
            message = "The selected file does not exist or is not a valid video format.\n\n"
                "Supported formats:\n"
                "- MP4, AVI, MKV, MOV, WMV\n"
                "- FLV, WebM, 3GP, TS\n"
                "- MPG, MPEG";
        }
    }
    else if (networkRadio->isChecked()) {
        title = "Invalid URL";
        if (networkUrl.isEmpty()) {
            message = "Please enter a network stream URL.";
        }
        else {
            message = "The entered URL format is invalid.\n\n"
                "Supported protocols:\n"
                "- HTTP/HTTPS (http://...)\n"
                "- RTMP/RTMPS (rtmp://...)\n"
                "- RTSP/RTSPS (rtsp://...)\n"
                "- UDP, TCP, MMS";
        }
    }
    else {
        title = "Invalid Selection";
        message = "Please select a playback mode and provide valid input.";
    }

    QMessageBox::warning(this, title, message);
}

bool MenuDialog::isValidVideoFile(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() < 100) {
        return false;
    }

    static const QStringList extensions = {
        "mp4", "m4v", "mov", "avi", "mkv", "wmv",
        "flv", "3gp", "ts", "webm", "mpg", "mpeg",
        "f4v", "rmvb", "rm", "asf", "divx", "xvid"
    };

    QString suffix = fileInfo.suffix().toLower();
    if (extensions.contains(suffix)) {
        return true;
    }

    QMimeDatabase mimeDatabase;
    QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileInfo);
    return mimeType.name().startsWith("video/") ||
          (mimeType.name().startsWith("application/") &&
          (mimeType.name().contains("mp4") || mimeType.name().contains("mpeg")));
}

bool MenuDialog::isValidNetworkUrl(const QString& url) const {
    if (url.isEmpty()) {
        return false;
    }

    QUrl qurl(url);
    if (!qurl.isValid() || qurl.host().isEmpty()) {
        return false;
    }

    QString scheme = qurl.scheme().toLower();
    static const QStringList validSchemes = {
        "http", "https", "rtmp", "rtmps",
        "rtsp", "rtsps", "udp", "tcp", "mms", "mmsh"
    };

    return validSchemes.contains(scheme);
}

QString MenuDialog::getDialogStyle() const {
    return R"(
        QDialog {
            background-color: #ffffff;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QWidget {
            background-color: #ffffff;
        }
    )";
}

QString MenuDialog::getGroupBoxStyle() const {
    return R"(
        QGroupBox {
            font-size: 20px;
            font-weight: bold;
            color: #333333;
            border: 2px solid #3498db;
            border-radius: 10px;
            margin-top: 15px;
            padding-top: 20px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 10px;
            color: #3498db;
            font-weight: bold;
        }
    )";
}

QString MenuDialog::getRadioButtonStyle() const {
    return R"(
        QRadioButton {
            font-size: 18px;
            font-weight: 500;
            color: #333333;
            spacing: 15px;
        }
        QRadioButton::indicator {
            width: 20px;
            height: 20px;
        }
    )";
}

QString MenuDialog::getButtonStyle() const {
    return R"(
        QPushButton {
            background: #3498db;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            padding: 12px 24px;
        }
        QPushButton:hover {
            background: #2980b9;
        }
        QPushButton:pressed {
            background: #21618c;
        }
    )";
}

QString MenuDialog::getLineEditStyle() const {
    return R"(
        QLineEdit {
            background: #ffffff;
            color: #333333;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            font-size: 18px;
            font-weight: 500;
            padding: 15px 18px;
            min-height: 20px;
        }
        QLineEdit:focus {
            border: 2px solid #3498db;
        }
        QLineEdit:hover {
            border: 2px solid #95a5a6;
        }
        QLineEdit::placeholder {
            color: #7f8c8d;
            font-size: 16px;
            font-weight: normal;
        }
    )";
}