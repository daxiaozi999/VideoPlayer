#include "SettingsDialog.h"

SettingsDialog::SettingsDialog(QWidget* parent)
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
    , currentMode(PlayMode::NONE)
    , filePath("")
    , networkUrl("")
    , validationTimer(nullptr)
    , isValidating(false)
    , lastValidPath("")
    , lastValidUrl("") {

    setupUi();
    connectSignals();

    setModal(true);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAcceptDrops(true);

    validationTimer = new QTimer(this);
    validationTimer->setSingleShot(true);
    validationTimer->setInterval(300);
    connect(validationTimer, &QTimer::timeout, this, &SettingsDialog::onValidationTimer);

    if (localRadio) {
        localRadio->setChecked(true);
    }
    updateControlsVisibility();
    restoreWindowState();

    setOkButtonEnabled(false);
}

void SettingsDialog::setupUi() {
    setWindowTitle("Video Player Settings");
    setMinimumSize(500, 500);
    resize(500, 500);
    setStyleSheet(getDialogStyle());

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(25);

    setupModeSelection();
    setupLocalControls();
    setupNetworkControls();
    setupButtons();

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
}

void SettingsDialog::setupModeSelection() {
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
    localRadio->setCursor(Qt::PointingHandCursor);
    localRadio->setStyleSheet(getRadioButtonStyle());

    networkRadio = new QRadioButton("Network Stream", this);
    networkRadio->setIcon(QIcon(":/VideoPlayer/icons/network.png"));
    networkRadio->setIconSize(QSize(40, 40));
    networkRadio->setCursor(Qt::PointingHandCursor);
    networkRadio->setStyleSheet(getRadioButtonStyle());

    modeButtonGroup->addButton(localRadio, static_cast<int>(PlayMode::LOCAL_MODE));
    modeButtonGroup->addButton(networkRadio, static_cast<int>(PlayMode::NETWORK_MODE));

    modeLayout->addWidget(localRadio);
    modeLayout->addWidget(networkRadio);
}

void SettingsDialog::setupLocalControls() {
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

    browseButton = new QPushButton("", this);
    browseButton->setIcon(QIcon(":/VideoPlayer/icons/directory.png"));
    browseButton->setIconSize(QSize(40, 40));
    browseButton->setStyleSheet(getButtonStyle());
    browseButton->setFixedSize(60, 50);
    browseButton->setCursor(Qt::PointingHandCursor);
    browseButton->setToolTip("Browse for video file");

    fileRowLayout->addWidget(filePathEdit);
    fileRowLayout->addWidget(browseButton);

    localLayout->addWidget(fileLabel);
    localLayout->addLayout(fileRowLayout);
}

void SettingsDialog::setupNetworkControls() {
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
    urlLineEdit->setPlaceholderText("Enter network stream URL (rtmp://...)");
    urlLineEdit->setStyleSheet(getLineEditStyle());

    networkLayout->addWidget(urlLabel);
    networkLayout->addWidget(urlLineEdit);
}

void SettingsDialog::setupButtons() {
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
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setDefault(true);

    cancelButton = new QPushButton("NO", this);
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
    cancelButton->setCursor(Qt::PointingHandCursor);
}

void SettingsDialog::connectSignals() {
    if (modeButtonGroup) {
        connect(modeButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &SettingsDialog::onPlayModeChanged);
    }
    if (browseButton) {
        connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);
    }
    if (okButton) {
        connect(okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    }
    if (cancelButton) {
        connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    }
    if (filePathEdit) {
        connect(filePathEdit, &QLineEdit::textChanged, this, &SettingsDialog::onFilePathChanged);
    }
    if (urlLineEdit) {
        connect(urlLineEdit, &QLineEdit::textChanged, this, &SettingsDialog::onUrlChanged);
    }
}

void SettingsDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && localRadio && localRadio->isChecked()) {
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

void SettingsDialog::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && filePathEdit) {
        QString fileName = urls.first().toLocalFile();
        if (isValidVideoFile(fileName)) {
            filePathEdit->setText(fileName);
            saveLastUsedDirectory(fileName);
            event->acceptProposedAction();
        }
    }
}

void SettingsDialog::onPlayModeChanged() {
    updateControlsVisibility();

    if (localRadio && localRadio->isChecked()) {
        lastValidUrl.clear();
        if (urlLineEdit && !urlLineEdit->text().trimmed().isEmpty()) {
            updateInputFieldStyle(urlLineEdit, true);
        }
    }
    else if (networkRadio && networkRadio->isChecked()) {
        lastValidPath.clear();
        if (filePathEdit && !filePathEdit->text().trimmed().isEmpty()) {
            updateInputFieldStyle(filePathEdit, true);
        }
    }

    validateCurrentInput();
}

void SettingsDialog::onBrowseClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Video File",
        QDir::currentPath(),
        "Video Files (*.mp4 *.m4v *.mov *.avi *.mkv *.wmv *.flv *.3gp *.ts *.webm *.mpg *.mpeg);;All Files (*.*)"
    );

    if (!fileName.isEmpty() && filePathEdit) {
        filePathEdit->setText(fileName);
        saveLastUsedDirectory(fileName);
    }
}

void SettingsDialog::onFilePathChanged(const QString& text) {
    filePath = text.trimmed();

    if (localRadio && localRadio->isChecked() && validationTimer) {
        validationTimer->start();
    }
}

void SettingsDialog::onUrlChanged(const QString& text) {
    networkUrl = text.trimmed();

    if (networkRadio && networkRadio->isChecked() && validationTimer) {
        validationTimer->start();
    }
}

void SettingsDialog::onValidationTimer() {
    if (!isValidating) {
        validateCurrentInput();
    }
}

void SettingsDialog::onOkClicked() {
    validateCurrentInput();

    if (currentMode == PlayMode::NONE) {
        showValidationError();
        return;
    }

    saveWindowState();
    accept();
}

void SettingsDialog::onCancelClicked() {
    reject();
}

void SettingsDialog::validateCurrentInput() {
    if (isValidating) return;

    isValidating = true;
    PlayMode newMode = PlayMode::NONE;

    if (localRadio && localRadio->isChecked()) {
        bool isValid = isValidVideoFile(filePath);
        newMode = isValid ? PlayMode::LOCAL_MODE : PlayMode::NONE;
        if (filePathEdit) {
            updateInputFieldStyle(filePathEdit, isValid || filePath.isEmpty());
        }
        lastValidPath = filePath;
    }
    else if (networkRadio && networkRadio->isChecked()) {
        bool isValid = isValidNetworkUrl(networkUrl);
        newMode = isValid ? PlayMode::NETWORK_MODE : PlayMode::NONE;
        if (urlLineEdit) {
            updateInputFieldStyle(urlLineEdit, isValid || networkUrl.isEmpty());
        }
        lastValidUrl = networkUrl;
    }

    currentMode = newMode;
    setOkButtonEnabled(currentMode != PlayMode::NONE);

    isValidating = false;
}

void SettingsDialog::updateInputFieldStyle(QLineEdit* field, bool isValid) {
    if (!field) return;

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

void SettingsDialog::setOkButtonEnabled(bool enabled) {
    if (!okButton) return;

    okButton->setEnabled(enabled);

    if (enabled) {
        okButton->setToolTip("Click to confirm settings");
    }
    else {
        okButton->setToolTip("Please provide valid input first");
    }
}

void SettingsDialog::updateControlsVisibility() {
    bool showLocal = localRadio && localRadio->isChecked();
    bool showNetwork = networkRadio && networkRadio->isChecked();

    if (localWidget) {
        localWidget->setVisible(showLocal);
    }
    if (networkWidget) {
        networkWidget->setVisible(showNetwork);
    }
}

void SettingsDialog::showValidationError() {
    QString message;
    QString title;

    if (localRadio && localRadio->isChecked()) {
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
    else if (networkRadio && networkRadio->isChecked()) {
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

bool SettingsDialog::isValidVideoFile(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    if (fileInfo.size() < 100) {
        return false;
    }

    static const QStringList videoExtensions = {
        "mp4", "m4v", "mov", "avi", "mkv", "wmv",
        "flv", "3gp", "ts", "webm", "mpg", "mpeg",
        "f4v", "rmvb", "rm", "asf", "divx", "xvid"
    };

    QString suffix = fileInfo.suffix().toLower();
    if (videoExtensions.contains(suffix)) {
        return true;
    }

    QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileInfo);
    return mimeType.name().startsWith("video/") ||
        (mimeType.name().startsWith("application/") &&
            (mimeType.name().contains("mp4") || mimeType.name().contains("mpeg")));
}

bool SettingsDialog::isValidNetworkUrl(const QString& url) const {
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

QString SettingsDialog::getLastUsedDirectory() const {
    QSettings settings;
    QString lastDir = settings.value("LastVideoDirectory",
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString();
    return QFileInfo(lastDir).exists() ? lastDir : QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

void SettingsDialog::saveLastUsedDirectory(const QString& filePath) {
    QSettings settings;
    settings.setValue("LastVideoDirectory", QFileInfo(filePath).absolutePath());
}

void SettingsDialog::restoreWindowState() {
    QSettings settings;

    int lastMode = settings.value("LastPlayMode", static_cast<int>(PlayMode::LOCAL_MODE)).toInt();
    if (lastMode == static_cast<int>(PlayMode::NETWORK_MODE) && networkRadio) {
        networkRadio->setChecked(true);
    }
    else if (localRadio) {
        localRadio->setChecked(true);
    }

    QString lastUrl = settings.value("LastNetworkUrl", "").toString();
    if (!lastUrl.isEmpty() && urlLineEdit) {
        urlLineEdit->setText(lastUrl);
    }
}

void SettingsDialog::saveWindowState() {
    QSettings settings;

    if (localRadio && localRadio->isChecked()) {
        settings.setValue("LastPlayMode", static_cast<int>(PlayMode::LOCAL_MODE));
    }
    else if (networkRadio && networkRadio->isChecked()) {
        settings.setValue("LastPlayMode", static_cast<int>(PlayMode::NETWORK_MODE));
    }

    if (!networkUrl.isEmpty() && isValidNetworkUrl(networkUrl)) {
        settings.setValue("LastNetworkUrl", networkUrl);
    }
}

QString SettingsDialog::getDialogStyle() const {
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

QString SettingsDialog::getGroupBoxStyle() const {
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

QString SettingsDialog::getRadioButtonStyle() const {
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

QString SettingsDialog::getButtonStyle() const {
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

QString SettingsDialog::getLineEditStyle() const {
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