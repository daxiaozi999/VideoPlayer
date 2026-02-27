#pragma once

#include <QUrl>
#include <QTimer>
#include <QLabel>
#include <QDialog>
#include <QMimeData>
#include <QLineEdit>
#include <QFileInfo>
#include <QGroupBox>
#include <QDropEvent>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMimeDatabase>
#include <QDragEnterEvent>

class MenuDialog : public QDialog {
    Q_OBJECT

public:
    explicit MenuDialog(QWidget* parent = nullptr);
    ~MenuDialog() = default;

    enum class PlayMode {
        NONE = 0,
        FILE = 1,
        NETWORK = 2
    };

    PlayMode getPlayMode()  const { return playMode; }
    QString getFilePath()   const { return filePath; }
    QString getNetworkUrl() const { return networkUrl; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onPlayModeChanged();
    void onBrowseClicked();
    void onFilePathChanged(const QString& text);
    void onNetworkUrlChanged(const QString& text);
    void onOkClicked();
    void onCancelClicked();
    void validateCurrentInput();

private:
    void setupUi();
    void setupModeSelections();
    void setupLocalControls();
    void setupNetworkControls();
    void setupButtons();
    void setupTimer();
    void setupConnections();
    void updateControlsVisible();
    void updateInputFieldStyle(QLineEdit* field, bool isValid);
    void showValidationError();
    bool isValidVideoFile(const QString& filePath) const;
    bool isValidNetworkUrl(const QString& url) const;
    QString getDialogStyle() const;
    QString getGroupBoxStyle() const;
    QString getRadioButtonStyle() const;
    QString getButtonStyle() const;
    QString getLineEditStyle() const;

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
    PlayMode playMode;
    QString filePath;
    QString networkUrl;
    QTimer* validateTimer;
};