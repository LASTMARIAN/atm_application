#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QLibrary>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLineEdit>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>

typedef void (*PrintDebugMessageFunc)();
typedef void (*SetCardReadCallbackFunc)(void (*callback)(const char*));
typedef bool (*InitReaderFunc)(const char*);
typedef void (*StartCardReadingFunc)();
typedef void (*StopCardReadingFunc)();

class MainWindow;
class PinInputWindow;
class WelcomeWindow;
class ActionWindow;
class ConfirmationWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    static MainWindow* getInstance();

    static void cardReadCallback(const char* cardNumber);

public slots:
    void show();

private slots:
    void onAuthenticationCompleted(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &token, const QString &cardType);

private:
    static MainWindow* instance;
    QString lastCardNumber;
    QLabel *statusLabel;
    QLibrary *rfidLibrary;
    QNetworkAccessManager *networkManager;

    PrintDebugMessageFunc PrintDebugMessage;
    SetCardReadCallbackFunc SetCardReadCallback;
    InitReaderFunc InitReader;
    StartCardReadingFunc StartCardReading;
    StopCardReadingFunc StopCardReading;
};

class PinInputWindow : public QMainWindow
{
    Q_OBJECT
public:
    PinInputWindow(const QString &cardNumber, QNetworkAccessManager *sharedNetworkManager, QWidget *parent = nullptr);
    ~PinInputWindow();

signals:
    void authenticationCompleted(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &token, const QString &cardType);

private slots:
    void onNumberButtonClicked(const QString &number);
    void onClearButtonClicked();
    void onSubmitButtonClicked();
    void onNetworkReply(QNetworkReply *reply);
    void updateTimer();

private:
    void resetPinInput();
    QString cardNumber;
    QString pinCode;
    QLabel *pinDisplayLabel;
    QLabel *timerLabel;
    QNetworkAccessManager *networkManager;
    int failedAttempts;
    QTimer *timer;
    int timeRemaining;
};

class WelcomeWindow : public QMainWindow
{
    Q_OBJECT
public:
    WelcomeWindow(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &token, const QString &cardType, QNetworkAccessManager *sharedNetworkManager, QWidget *parent = nullptr);
    ~WelcomeWindow();

signals:
    void actionCompleted();

private slots:
    void onWithdrawalClicked();
    void onTopUpClicked();
    void onBalanceClicked();
    void onHistoryClicked();

private:
    QString firstName;
    QString lastName;
    int accountId;
    QString cardNumber;
    QString pinCode;
    QString token;
    QString cardType;
    QNetworkAccessManager *networkManager;
};

class ActionWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum ActionType { Withdrawal, TopUp, Balance, History };
    ActionWindow(ActionType type, int accountId, const QString &cardNumber, const QString &pinCode, const QString &cardType, QNetworkAccessManager *sharedNetworkManager, WelcomeWindow *welcomeWindow, QWidget *parent = nullptr);
    ~ActionWindow();

signals:
    void actionFinished();
    void authenticationCompletedForAction(const QString &token);

private slots:
    void reAuthenticateAndProceed();
    void onReAuthenticationCompleted(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &newToken);
    void onSubmitButtonClicked();
    void performAction();
    void onCancelButtonClicked();
    void onCloseButtonClicked();
    void onNetworkReply(QNetworkReply *reply);

private:
    ActionType actionType;
    int accountId;
    QString cardNumber;
    QString pinCode;
    QString cardType;
    QNetworkAccessManager *networkManager;
    QLineEdit *amountInput;
    QLabel *resultLabel;
    double pendingAmount;
    WelcomeWindow *welcomeWindow;
};

class ConfirmationWindow : public QMainWindow
{
    Q_OBJECT
public:
    ConfirmationWindow(const QString &message, double newBalance, WelcomeWindow *welcomeWindow, QWidget *parent = nullptr);
    ~ConfirmationWindow();

signals:
    void topUpCompleted();

private slots:
    void onReturnButtonClicked();

private:
    QLabel *messageLabel;
    WelcomeWindow *welcomeWindow;
};

#endif // MAINWINDOW_H
