#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>
#include <QGroupBox>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>
#include <QUrl>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

enum class PlayMode {
    NONE,
    LOCAL_MODE,
    NETWORK_MODE
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() = default;

    PlayMode getPlayMode()  const { return currentMode; }
    QString getFilePath()   const { return filePath; }
    QString getNetworkUrl() const { return networkUrl; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onPlayModeChanged();
    void onBrowseClicked();
    void onFilePathChanged(const QString& text);
    void onUrlChanged(const QString& text);
    void onOkClicked();
    void onCancelClicked();
    void onValidationTimer();

private:
    void setupUi();
    void setupModeSelection();
    void setupLocalControls();
    void setupNetworkControls();
    void setupButtons();
    void connectSignals();
    void updateControlsVisibility();
    void validateCurrentInput();
    void showValidationError();

    bool isValidVideoFile(const QString& filePath) const;
    bool isValidNetworkUrl(const QString& url) const;
    void updateInputFieldStyle(QLineEdit* field, bool isValid);
    void setOkButtonEnabled(bool enabled);

    void saveLastUsedDirectory(const QString& filePath);
    QString getLastUsedDirectory() const;
    void restoreWindowState();
    void saveWindowState();

    QString getDialogStyle() const;
    QString getButtonStyle() const;
    QString getLineEditStyle() const;
    QString getGroupBoxStyle() const;
    QString getRadioButtonStyle() const;

private:
    QVBoxLayout* mainLayout;
    QGroupBox* modeGroupBox;
    QRadioButton* localRadio;
    QRadioButton* networkRadio;
    QButtonGroup* modeButtonGroup;

    QWidget* localWidget;
    QLineEdit* filePathEdit;
    QPushButton* browseButton;

    QWidget* networkWidget;
    QLineEdit* urlLineEdit;

    QPushButton* okButton;
    QPushButton* cancelButton;

    PlayMode currentMode;
    QString filePath;
    QString networkUrl;
    QMimeDatabase mimeDatabase;

    QTimer* validationTimer;
    bool isValidating;
    QString lastValidPath;
    QString lastValidUrl;
};