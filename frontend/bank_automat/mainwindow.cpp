#include "mainwindow.h"
#include <QJsonObject>
#include <QApplication>
#include <QJsonDocument>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDebug>
#include <QNetworkCookieJar>
#include <QJsonArray>
#include <QDateTime>
#include <QPushButton>
#include <QHBoxLayout>

// Määritä staattinen instanssi
MainWindow* MainWindow::instance = nullptr;

// MainWindow toteutus (Odottaa kortin skannausta)
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), lastCardNumber("")
{
    // Aseta tämä instanssi staattiseksi osoittimeksi
    instance = this;

    // Luo keskuswidget ja asettelu
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Näytä odotusviesti
    statusLabel = new QLabel("Kortin skannausta odotetaan", this);
    layout->addWidget(statusLabel);

    // Alusta verkkomanageri evästevarastolla
    networkManager = new QNetworkAccessManager(this);
    QNetworkCookieJar *cookieJar = new QNetworkCookieJar(networkManager);
    networkManager->setCookieJar(cookieJar);

    // Lataa rfidlib DLL (QLibrary odottaa "rfidlib", joka vastaa librfidlib.dll-tiedostoa)
    rfidLibrary = new QLibrary("librfidlib", this);
    if (!rfidLibrary->load()) {
        statusLabel->setText("librfidlib.dll lataaminen epäonnistui: " + rfidLibrary->errorString());
        return;
    }

    // Ratkaise DLL:n funktiot
    PrintDebugMessage = (PrintDebugMessageFunc)rfidLibrary->resolve("PrintDebugMessage");
    SetCardReadCallback = (SetCardReadCallbackFunc)rfidLibrary->resolve("SetCardReadCallback");
    InitReader = (InitReaderFunc)rfidLibrary->resolve("InitReader");
    StartCardReading = (StartCardReadingFunc)rfidLibrary->resolve("StartCardReading");
    StopCardReading = (StopCardReadingFunc)rfidLibrary->resolve("StopCardReading");

    if (!PrintDebugMessage || !SetCardReadCallback || !InitReader || !StartCardReading || !StopCardReading) {
        statusLabel->setText("DLL-funktioiden ratkaiseminen epäonnistui: " + rfidLibrary->errorString());
        return;
    }

    // Kutsu PrintDebugMessage-funktiota
    PrintDebugMessage();
    qDebug() << "DLL-funktio PrintDebugMessage kutsuttu onnistuneesti EXE:stä";

    // Aseta takaisinkutsufunktio kortin lukemiselle
    SetCardReadCallback(cardReadCallback);

    // Alusta RFID-lukija
    if (!InitReader("COM3")) {
        statusLabel->setText("RFID-lukijan alustaminen epäonnistui");
        return;
    }

    // Aloita kortin lukeminen
    StartCardReading();

    // Aseta ikkunan ominaisuudet
    setWindowTitle("RFID-kortinlukija");
    resize(300, 150);
}

MainWindow::~MainWindow()
{
    // Pysäytä kortin lukeminen ja vapauta resurssit
    if (StopCardReading) {
        StopCardReading();
    }
    if (rfidLibrary && rfidLibrary->isLoaded()) {
        rfidLibrary->unload();
    }

    // Tyhjennä staattinen instanssi
    instance = nullptr;
}

// Implementation of the show slot
void MainWindow::show()
{
    QMainWindow::show();
}

// Getter for the instance
MainWindow* MainWindow::getInstance()
{
    return instance;
}

void MainWindow::cardReadCallback(const char* cardNumber)
{
    if (!MainWindow::getInstance()) {
        qDebug() << "Virhe: MainWindow-instanssia ei ole asetettu!";
        return;
    }

    // Käsittele korttinumero DLL:stä
    QString cardNum = QString(cardNumber).trimmed();
    qDebug() << "Kortti luettu DLL:stä (takaisinkutsu):" << cardNum;

    // Tarkista, onko kyseessä uusi korttinumero duplikaattien välttämiseksi
    if (cardNum != MainWindow::getInstance()->lastCardNumber) {
        MainWindow::getInstance()->lastCardNumber = cardNum;
        MainWindow::getInstance()->statusLabel->setText("Kortti skannattu: " + cardNum);

        // Avaa PIN-syöttöikkuna ja välitä jaettu verkkomanageri
        PinInputWindow *pinWindow = new PinInputWindow(cardNum, MainWindow::getInstance()->networkManager, MainWindow::getInstance());
        QObject::connect(pinWindow, &PinInputWindow::authenticationCompleted, MainWindow::getInstance(),
                         &MainWindow::onAuthenticationCompleted);
        pinWindow->show();
        MainWindow::getInstance()->hide();
    }
}

void MainWindow::onAuthenticationCompleted(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &token, const QString &cardType)
{
    // Only proceed if authentication was successful (firstName is not empty and accountId is not -1)
    if (firstName.isEmpty() || accountId == -1) {
        qDebug() << "Authentication failed, returning to MainWindow";
        lastCardNumber = "";
        statusLabel->setText("Kortin skannausta odotetaan");
        show();
        return;
    }

    // Reset lastCardNumber and status
    lastCardNumber = "";
    statusLabel->setText("Kortin skannausta odotetaan");

    // Open WelcomeWindow
    WelcomeWindow *welcomeWindow = new WelcomeWindow(firstName, lastName, accountId, cardNumber, pinCode, token, cardType, networkManager, this);
    QObject::connect(welcomeWindow, &WelcomeWindow::actionCompleted, this, &MainWindow::show);
    welcomeWindow->show();
    hide();
}

// PinInputWindow toteutus (PIN-koodin syöttö painikkeilla)
PinInputWindow::PinInputWindow(const QString &cardNumber, QNetworkAccessManager *sharedNetworkManager, QWidget *parent)
    : QMainWindow(parent), cardNumber(cardNumber), pinCode(""), networkManager(sharedNetworkManager), failedAttempts(0), timeRemaining(10)
{
    // Luo keskuswidget ja asettelu
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Näyttö PIN-koodille
    pinDisplayLabel = new QLabel("****", this);
    pinDisplayLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(pinDisplayLabel);

    // Näyttö ajastimelle
    timerLabel = new QLabel(QString("Aikaa jäljellä: %1 s").arg(timeRemaining), this);
    timerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(timerLabel);

    // Alusta ajastin
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PinInputWindow::updateTimer);
    timer->start(1000); // Käynnistä ajastin, päivitys kerran sekunnissa

    // Luo ruudukkoasettelu numeropainikkeille
    QGridLayout *buttonLayout = new QGridLayout;
    for (int i = 1; i <= 9; ++i) {
        QPushButton *button = new QPushButton(QString::number(i), this);
        connect(button, &QPushButton::clicked, this, [this, i]() {
            onNumberButtonClicked(QString::number(i));
        });
        buttonLayout->addWidget(button, (i - 1) / 3, (i - 1) % 3);
    }

    // Lisää 0-painike
    QPushButton *zeroButton = new QPushButton("0", this);
    connect(zeroButton, &QPushButton::clicked, this, [this]() {
        onNumberButtonClicked("0");
    });
    buttonLayout->addWidget(zeroButton, 3, 1);

    // Lisää Tyhjennä-painike
    QPushButton *clearButton = new QPushButton("Tyhjennä", this);
    connect(clearButton, &QPushButton::clicked, this, &PinInputWindow::onClearButtonClicked);
    buttonLayout->addWidget(clearButton, 3, 0);

    // Lisää Lähetä-painike
    QPushButton *submitButton = new QPushButton("Lähetä", this);
    connect(submitButton, &QPushButton::clicked, this, &PinInputWindow::onSubmitButtonClicked);
    buttonLayout->addWidget(submitButton, 3, 2);

    layout->addLayout(buttonLayout);

    // Aseta ikkunan ominaisuudet
    setWindowTitle("Syötä PIN-koodi");
    resize(300, 300);
}

PinInputWindow::~PinInputWindow()
{
    if (timer) {
        timer->stop();
    }
}

void PinInputWindow::updateTimer()
{
    timeRemaining--;
    timerLabel->setText(QString("Aikaa jäljellä: %1 s").arg(timeRemaining));

    if (timeRemaining <= 0) {
        timer->stop();
        // Emit authenticationCompleted with empty/invalid values to indicate failure
        emit authenticationCompleted("", "", -1, cardNumber, "", "", "");
        close();
    }
}

void PinInputWindow::onNumberButtonClicked(const QString &number)
{
    if (pinCode.length() < 4) {
        pinCode += number;
        pinDisplayLabel->setText(pinCode + QString("****").mid(pinCode.length()));
    }
}

void PinInputWindow::onClearButtonClicked()
{
    pinCode.clear();
    pinDisplayLabel->setText("****");
}

void PinInputWindow::onSubmitButtonClicked()
{
    if (pinCode.length() != 4) {
        QMessageBox::warning(this, "Virhe", "PIN-koodin on oltava 4 numeroa!");
        return;
    }

    // Pysäytä ajastin, koska käyttäjä lähetti PIN-koodin
    timer->stop();

    // Luo JSON-objekti korttinumerolla ja PIN-koodilla
    QJsonObject json;
    json["card_number"] = cardNumber;
    json["pin_code"] = pinCode;

    // Muunna tavutabelliksi
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    // Luo pyyntö
    QNetworkRequest request(QUrl("http://localhost:3000/cards/auth"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Debuggaa pyyntö
    qDebug() << "Lähetetään pyyntö osoitteeseen:" << request.url().toString();
    qDebug() << "Pyynnön sisältö:" << data;

    // Lähetä POST-pyyntö
    QNetworkReply *reply = networkManager->post(request, data);

    // Yhdistä vastaussignaali
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkReply(reply);
    });
}

void PinInputWindow::resetPinInput()
{
    pinCode.clear();
    pinDisplayLabel->setText("****");
}

void PinInputWindow::onNetworkReply(QNetworkReply *reply)
{
    QByteArray response = reply->readAll();
    qDebug() << "Raaka vastaus /cards/auth-osoitteesta:" << response;

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject json = doc.object();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;
        if (!doc.isNull() && json.contains("error")) {
            errorMsg = json["error"].toString();
            if (errorMsg.contains("Kortti on estetty")) {
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    if (widget != MainWindow::getInstance() && widget != this) {
                        qDebug() << "Closing window:" << widget->windowTitle();
                        widget->close();
                    }
                }
                if (MainWindow::getInstance()) {
                    MainWindow::getInstance()->show();
                }
                QMessageBox::warning(this, "Virhe", errorMsg);
                close();
                reply->deleteLater();
                return;
            }
        } else {
            errorMsg = "Tunnistautuminen epäonnistui: " + reply->errorString();
        }
        qDebug() << "Verkkovirhe:" << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    if (doc.isNull()) {
        QString errorMsg = "Vastauksen jäsentäminen JSON-muotoon epäonnistui";
        qDebug() << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    if (!json.contains("success") || !json["success"].toBool()) {
        QString errorMsg = json.contains("error") ? json["error"].toString() : "Tuntematon virhe";
        qDebug() << "Tunnistautuminen epäonnistui virheellä:" << errorMsg;
        if (errorMsg.contains("Kortti on estetty")) {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (widget != MainWindow::getInstance() && widget != this) {
                    qDebug() << "Closing window:" << widget->windowTitle();
                    widget->close();
                }
            }
            if (MainWindow::getInstance()) {
                MainWindow::getInstance()->show();
            }
            QMessageBox::warning(this, "Virhe", errorMsg);
            close();
            reply->deleteLater();
            return;
        }
        QMessageBox::warning(this, "Virhe", "Virhe: Väärä PIN-koodi");
        resetPinInput();
        reply->deleteLater();
        return;
    }

    if (!json.contains("customer") || !json["customer"].isObject()) {
        QString errorMsg = "Vastauksesta puuttuu 'customer'-objekti";
        qDebug() << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    QJsonObject customer = json["customer"].toObject();
    if (!customer.contains("first_name") || !customer.contains("last_name")) {
        QString errorMsg = "Vastauksesta puuttuu 'first_name' tai 'last_name' asiakasobjektissa";
        qDebug() << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    if (!json.contains("account_id")) {
        QString errorMsg = "Vastauksesta puuttuu 'account_id'";
        qDebug() << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    if (!json.contains("card_type")) {
        QString errorMsg = "Vastauksesta puuttuu 'card_type'";
        qDebug() << errorMsg;
        QMessageBox::warning(this, "Virhe", errorMsg);
        resetPinInput();
        reply->deleteLater();
        return;
    }

    QString firstName = customer["first_name"].toString();
    QString lastName = customer["last_name"].toString();
    int accountId = json["account_id"].toInt();
    QString cardType = json["card_type"].toString();

    qDebug() << "Tunnistautuminen onnistui. Etunimi:" << firstName << ", Sukunimi:" << lastName << ", Tilin ID:" << accountId << ", Korttityyppi:" << cardType;
    emit authenticationCompleted(firstName, lastName, accountId, cardNumber, pinCode, "", cardType);

    close();
    reply->deleteLater();
}

// WelcomeWindow toteutus
WelcomeWindow::WelcomeWindow(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &token, const QString &cardType, QNetworkAccessManager *sharedNetworkManager, QWidget *parent)
    : QMainWindow(parent), firstName(firstName), lastName(lastName), accountId(accountId), cardNumber(cardNumber), pinCode(pinCode), token(token), cardType(cardType), networkManager(sharedNetworkManager)
{
    // Luo keskuswidget ja asettelu
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Tervetuloa-teksti
    QLabel *welcomeLabel = new QLabel("Tervetuloa, " + firstName + " " + lastName, this);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(welcomeLabel);

    // Korttityyppi-teksti
    QLabel *cardTypeLabel = new QLabel(QString("Korttityyppi: %1").arg(cardType), this);
    cardTypeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(cardTypeLabel);

    // Painikkeet
    QPushButton *withdrawalButton = new QPushButton("Nosto", this);
    QPushButton *topUpButton = new QPushButton("Talletus", this);
    QPushButton *balanceButton = new QPushButton("Saldo", this);
    QPushButton *historyButton = new QPushButton("Historia", this);

    connect(withdrawalButton, &QPushButton::clicked, this, &WelcomeWindow::onWithdrawalClicked);
    connect(topUpButton, &QPushButton::clicked, this, &WelcomeWindow::onTopUpClicked);
    connect(balanceButton, &QPushButton::clicked, this, &WelcomeWindow::onBalanceClicked);
    connect(historyButton, &QPushButton::clicked, this, &WelcomeWindow::onHistoryClicked);

    layout->addWidget(withdrawalButton);
    layout->addWidget(topUpButton);
    layout->addWidget(balanceButton);
    layout->addWidget(historyButton);

    // Aseta ikkunan ominaisuudet
    setWindowTitle("Tervetuloa");
    resize(300, 200);
}

WelcomeWindow::~WelcomeWindow()
{
}

void WelcomeWindow::onWithdrawalClicked()
{
    qDebug() << "Nosto-painiketta klikattu";
    ActionWindow *actionWindow = new ActionWindow(ActionWindow::Withdrawal, accountId, cardNumber, pinCode, cardType, networkManager, this, this);
    connect(actionWindow, &ActionWindow::actionFinished, this, [this]() {
        show();
        emit actionCompleted();
    });
    actionWindow->show();
    hide();
}

void WelcomeWindow::onTopUpClicked()
{
    qDebug() << "Talletus-painiketta klikattu";
    ActionWindow *actionWindow = new ActionWindow(ActionWindow::TopUp, accountId, cardNumber, pinCode, cardType, networkManager, this, this);
    connect(actionWindow, &ActionWindow::actionFinished, this, [this]() {
        show();
        emit actionCompleted();
    });
    actionWindow->show();
    hide();
}

void WelcomeWindow::onBalanceClicked()
{
    qDebug() << "Saldo-painiketta klikattu";
    ActionWindow *actionWindow = new ActionWindow(ActionWindow::Balance, accountId, cardNumber, pinCode, cardType, networkManager, this, this);
    connect(actionWindow, &ActionWindow::actionFinished, this, [this]() {
        show();
        emit actionCompleted();
    });
    actionWindow->show();
    hide();
}

void WelcomeWindow::onHistoryClicked()
{
    qDebug() << "Historia-painiketta klikattu";
    ActionWindow *actionWindow = new ActionWindow(ActionWindow::History, accountId, cardNumber, pinCode, cardType, networkManager, this, this);
    connect(actionWindow, &ActionWindow::actionFinished, this, [this]() {
        show();
        emit actionCompleted();
    });
    actionWindow->show();
    hide();
}

// ActionWindow toteutus
ActionWindow::ActionWindow(ActionType type, int accountId, const QString &cardNumber, const QString &pinCode, const QString &cardType, QNetworkAccessManager *sharedNetworkManager, WelcomeWindow *welcomeWindow, QWidget *parent)
    : QMainWindow(parent), actionType(type), accountId(accountId), cardNumber(cardNumber), pinCode(pinCode), cardType(cardType), networkManager(sharedNetworkManager), amountInput(nullptr), resultLabel(nullptr), pendingAmount(0.0), welcomeWindow(welcomeWindow)
{
    // Luo keskuswidget ja asettelu
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Aseta ikkuna toimintotyypin perusteella
    if (actionType == Withdrawal) {
        // Ohje
        QLabel *instructionLabel = new QLabel("Valitse nostettava summa:", this);
        instructionLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(instructionLabel);

        // Painikkeet ennalta määritellyille summille
        QHBoxLayout *amountButtonsLayout1 = new QHBoxLayout();
        QHBoxLayout *amountButtonsLayout2 = new QHBoxLayout();

        QPushButton *amount20Button = new QPushButton("20", this);
        QPushButton *amount40Button = new QPushButton("40", this);
        QPushButton *amount50Button = new QPushButton("50", this);
        QPushButton *amount100Button = new QPushButton("100", this);
        QPushButton *otherAmountButton = new QPushButton("Muu summa", this);

        amountButtonsLayout1->addWidget(amount20Button);
        amountButtonsLayout1->addWidget(amount40Button);
        amountButtonsLayout2->addWidget(amount50Button);
        amountButtonsLayout2->addWidget(amount100Button);

        layout->addLayout(amountButtonsLayout1);
        layout->addLayout(amountButtonsLayout2);
        layout->addWidget(otherAmountButton);

        // Syöte muulle summalle (piilotettu aluksi)
        amountInput = new QLineEdit(this);
        amountInput->setPlaceholderText("Syötä summa");
        amountInput->setVisible(false); // Piilotetaan aluksi
        layout->addWidget(amountInput);

        // Lähetä ja Peruuta -painikkeet
        QHBoxLayout *actionButtonsLayout = new QHBoxLayout();
        QPushButton *submitButton = new QPushButton("Lähetä", this);
        QPushButton *cancelButton = new QPushButton("Peruuta", this);
        actionButtonsLayout->addWidget(submitButton);
        actionButtonsLayout->addWidget(cancelButton);
        layout->addLayout(actionButtonsLayout);

        // Yhdistä painikkeet
        connect(amount20Button, &QPushButton::clicked, this, [this]() {
            pendingAmount = 20.0;
            amountInput->setVisible(false);
            onSubmitButtonClicked();
        });
        connect(amount40Button, &QPushButton::clicked, this, [this]() {
            pendingAmount = 40.0;
            amountInput->setVisible(false);
            onSubmitButtonClicked();
        });
        connect(amount50Button, &QPushButton::clicked, this, [this]() {
            pendingAmount = 50.0;
            amountInput->setVisible(false);
            onSubmitButtonClicked();
        });
        connect(amount100Button, &QPushButton::clicked, this, [this]() {
            pendingAmount = 100.0;
            amountInput->setVisible(false);
            onSubmitButtonClicked();
        });
        connect(otherAmountButton, &QPushButton::clicked, this, [this]() {
            amountInput->setVisible(true);
            amountInput->setFocus();
        });
        connect(submitButton, &QPushButton::clicked, this, &ActionWindow::onSubmitButtonClicked);
        connect(cancelButton, &QPushButton::clicked, this, &ActionWindow::onCancelButtonClicked);

        setWindowTitle("Nosto");
    } else if (actionType == TopUp) {
        // Teksti ja syöte summalle
        QLabel *amountLabel = new QLabel("Syötä talletettava summa:", this);
        amountInput = new QLineEdit(this);
        amountInput->setPlaceholderText("Syötä summa");

        // Lähetä ja Peruuta -painikkeet
        QPushButton *submitButton = new QPushButton("Lähetä", this);
        QPushButton *cancelButton = new QPushButton("Peruuta", this);

        connect(submitButton, &QPushButton::clicked, this, &ActionWindow::onSubmitButtonClicked);
        connect(cancelButton, &QPushButton::clicked, this, &ActionWindow::onCancelButtonClicked);

        layout->addWidget(amountLabel);
        layout->addWidget(amountInput);
        layout->addWidget(submitButton);
        layout->addWidget(cancelButton);

        setWindowTitle("Talletus");
    } else {
        // Saldolle ja historialle haetaan tiedot uudelleentunnistautumisen jälkeen
        resultLabel = new QLabel("Ole hyvä ja tunnistaudu jatkaaksesi...", this);
        resultLabel->setWordWrap(true);
        layout->addWidget(resultLabel);

        QPushButton *closeButton = new QPushButton("Sulje", this);
        connect(closeButton, &QPushButton::clicked, this, &ActionWindow::onCloseButtonClicked);
        layout->addWidget(closeButton);

        setWindowTitle(actionType == Balance ? "Saldo" : "Tapahtumahistoria");

        // Aloita uudelleentunnistautumisprosessi
        qDebug() << "Aloitetaan uudelleentunnistautuminen toiminnolle:" << (actionType == Balance ? "Saldo" : "Historia");
        reAuthenticateAndProceed();
    }

    resize(400, 200);
}

ActionWindow::~ActionWindow()
{
}

void ActionWindow::reAuthenticateAndProceed()
{
    qDebug() << "Avataan PinInputWindow uudelleentunnistautumista varten";
    PinInputWindow *pinWindow = new PinInputWindow(cardNumber, networkManager, this);
    connect(pinWindow, &PinInputWindow::authenticationCompleted, this, &ActionWindow::onReAuthenticationCompleted);
    pinWindow->show();
}

void ActionWindow::onReAuthenticationCompleted(const QString &firstName, const QString &lastName, int accountId, const QString &cardNumber, const QString &pinCode, const QString &newToken)
{
    qDebug() << "Uudelleentunnistautuminen valmis. Etunimi:" << firstName << ", Tilin ID:" << accountId;
    if (firstName.isEmpty() || accountId == -1) {
        // Tunnistautuminen epäonnistui, sulje ikkuna
        QMessageBox::warning(this, "Virhe", "Tunnistautuminen epäonnistui. Toimintoa ei voi jatkaa.");
        emit actionFinished();
        close();
        return;
    }

    // Päivitä accountId, cardNumber ja pinCode (token on evästeessä)
    this->accountId = accountId;
    this->cardNumber = cardNumber;
    this->pinCode = pinCode;

    emit authenticationCompletedForAction(newToken);

    // Talletukselle sulje ActionWindow onnistuneen tunnistautumisen jälkeen
    if (actionType == TopUp) {
        qDebug() << "Suljetaan ActionWindow onnistuneen tunnistautumisen jälkeen talletukselle";
        close();
    }

    performAction();
}

void ActionWindow::onSubmitButtonClicked()
{
    // Jos amountInput on näkyvissä, käytä sen arvoa (Muu summa)
    if (amountInput && amountInput->isVisible()) {
        bool ok;
        double amount = amountInput->text().toDouble(&ok);
        if (!ok || amount <= 0) {
            QMessageBox::warning(this, "Virhe", "Syötä kelvollinen summa.");
            return;
        }
        pendingAmount = amount;
    } else if (pendingAmount == 0.0) {
        // Jos painiketta ei ole vielä valittu ja amountInput ei ole näkyvissä
        QMessageBox::warning(this, "Virhe", "Valitse summa tai syötä muu summa.");
        return;
    }

    // Poista syöte käytöstä uudelleentunnistautumisen ajaksi
    if (amountInput) {
        amountInput->setEnabled(false);
    }
    qDebug() << "Lähetä-painiketta klikattu toiminnolle:" << (actionType == Withdrawal ? "Nosto" : "Talletus") << ", Summa:" << pendingAmount;
    reAuthenticateAndProceed();
}

void ActionWindow::performAction()
{
    qDebug() << "Suoritetaan toiminto:" << (actionType == Withdrawal ? "Nosto" : actionType == TopUp ? "Talletus" : actionType == Balance ? "Saldo" : "Historia");
    QNetworkRequest request;
    QJsonObject json;
    QJsonDocument doc;
    QByteArray data;

    if (actionType == Withdrawal) {
        request.setUrl(QUrl("http://localhost:3000/transactions/withdraw"));
        json["card_number"] = cardNumber;
        json["pin_code"] = pinCode;
        json["amount"] = pendingAmount;
    } else if (actionType == TopUp) {
        request.setUrl(QUrl("http://localhost:3000/transactions/top_up"));
        json["account_id"] = accountId;  // Sisällytä account_id
        json["amount"] = pendingAmount;
    } else if (actionType == Balance) {
        request.setUrl(QUrl("http://localhost:3000/transactions/balance"));
        json["card_number"] = cardNumber;
        json["pin_code"] = pinCode;
    } else { // Historia
        request.setUrl(QUrl("http://localhost:3000/transactions/get_transactions"));
        json["account_id"] = accountId;  // Sisällytä account_id
    }

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Debuggaa pyyntö
    qDebug() << "Lähetetään toimintopyyntö osoitteeseen:" << request.url().toString();

    doc.setObject(json);
    data = doc.toJson();
    qDebug() << "Pyynnön sisältö:" << data;

    QNetworkReply *reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkReply(reply);
    });
}

void ActionWindow::onCancelButtonClicked()
{
    qDebug() << "Peruuta-painiketta klikattu";
    emit actionFinished();
    close();
}

void ActionWindow::onCloseButtonClicked()
{
    qDebug() << "Sulje-painiketta klikattu";
    emit actionFinished();
    close();
}

void ActionWindow::onNetworkReply(QNetworkReply *reply)
{
    QString responseText;
    QByteArray response = reply->readAll();
    qDebug() << "Vastaus toiminnosta:" << response;

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject json = doc.object();

    if (reply->error() != QNetworkReply::NoError) {
        // Verkko- tai HTTP-virhe (esim. 400 Bad Request)
        qDebug() << "Verkkovirhe toiminnossa:" << reply->errorString();
        if (!doc.isNull() && json.contains("error")) {
            responseText = "Epäonnistui: " + json["error"].toString();
            // Check if the card is blocked
            if (responseText.contains("Kortti on estetty")) {
                QMessageBox::warning(this, "Virhe", responseText);
                close();
                emit actionFinished();
                // Show MainWindow (initial interface)
                if (MainWindow::getInstance()) {
                    MainWindow::getInstance()->show();
                }
                reply->deleteLater();
                return;
            }
        } else {
            responseText = "Virhe: " + reply->errorString();
        }
    } else {
        // Onnistunut vastaus (HTTP 200)
        if (actionType == Withdrawal) {
            if (json.contains("message") && json["message"].toString() == "Withdrawal successful") {
                QJsonObject transaction = json["transaction"].toObject();
                double newBalance = transaction["new_balance"].toDouble();
                responseText = QString("Onnistui! Uusi saldo: %1").arg(newBalance);
            } else {
                QString errorMsg = json.contains("error") ? json["error"].toString() : "Tuntematon virhe";
                responseText = "Epäonnistui: " + errorMsg;
                // Check if the card is blocked
                if (responseText.contains("Kortti on estetty")) {
                    QMessageBox::warning(this, "Virhe", responseText);
                    close();
                    emit actionFinished();
                    // Show MainWindow (initial interface)
                    if (MainWindow::getInstance()) {
                        MainWindow::getInstance()->show();
                    }
                    reply->deleteLater();
                    return;
                }
            }
        } else if (actionType == TopUp) {
            if (json.contains("success") && json["success"].toBool()) {
                double newBalance = json["newBalance"].toDouble();
                responseText = QString("Toiminto onnistui!\nUusi saldo: %1").arg(newBalance);
                ConfirmationWindow *confirmationWindow = new ConfirmationWindow(responseText, newBalance, welcomeWindow, nullptr);
                connect(confirmationWindow, &ConfirmationWindow::topUpCompleted, welcomeWindow, [this]() {
                    welcomeWindow->show();
                    emit actionFinished();
                });
                confirmationWindow->show();
                reply->deleteLater();
                return; // Poistu aikaisin
            } else {
                QString errorMsg = json.contains("error") ? json["error"].toString() : "Tuntematon virhe";
                responseText = "Epäonnistui: " + errorMsg;
                // Talletuksen epäonnistuessa näytä virhe ja palaa WelcomeWindow-ikkunaan
                QMessageBox::warning(welcomeWindow, "Talletus", responseText);
                welcomeWindow->show();
                emit actionFinished();
                reply->deleteLater();
                return;
            }
        } else if (actionType == Balance) {
            if (json.contains("message") && json["message"].toString() == "Balance retrieved successfully") {
                double balance = json["balance"].toDouble();
                responseText = QString("Saldosi: %1").arg(balance);
            } else {
                QString errorMsg = json.contains("error") ? json["error"].toString() : "Tuntematon virhe";
                responseText = "Epäonnistui: " + errorMsg;
            }
        } else { // Historia
            if (doc.isArray()) {
                QJsonArray transactions = doc.array();
                QStringList transactionList;
                for (const QJsonValue &value : transactions) {
                    QJsonObject tx = value.toObject();
                    double amount = tx["summa"].toDouble();
                    QString type = amount < 0 ? "Nosto" : "Talletus";
                    QString dateTime = QDateTime::fromString(tx["transaction_time"].toString(), Qt::ISODate)
                                           .toString("yyyy-MM-dd HH:mm:ss");
                    QString entry = QString("ID: %1, Tyyppi: %2, Summa: %3, Aika: %4")
                                        .arg(tx["transaction_id"].toInt())
                                        .arg(type)
                                        .arg(amount, 0, 'f', 2)
                                        .arg(dateTime);
                    transactionList << entry;
                }
                responseText = transactionList.join("\n");
                if (responseText.isEmpty()) responseText = "Ei tapahtumia.";
            } else {
                QString errorMsg = json.contains("error") ? json["error"].toString() : "Tuntematon virhe";
                responseText = "Epäonnistui: " + errorMsg;
            }
        }
    }

    if (actionType == Withdrawal) {
        // Näytä tulos viesti-ikkunassa ja sulje
        QMessageBox::information(this, "Nosto", responseText);
        emit actionFinished();
        close();
    } else if (actionType == TopUp) {
        // Tätä lohkoa ei pitäisi saavuttaa aikaisen poistu-komennon vuoksi
    } else {
        // Päivitä teksti Saldolle tai Historielle
        resultLabel->setText(responseText);
    }

    reply->deleteLater();
}

// ConfirmationWindow toteutus
ConfirmationWindow::ConfirmationWindow(const QString &message, double newBalance, WelcomeWindow *welcomeWindow, QWidget *parent)
    : QMainWindow(parent), welcomeWindow(welcomeWindow)
{
    // Luo keskuswidget ja asettelu
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Näytä onnistumisviesti ja uusi saldo
    messageLabel = new QLabel(message, this);
    messageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(messageLabel);

    // Lisää Palaa-painike
    QPushButton *returnButton = new QPushButton("Palaa", this);
    connect(returnButton, &QPushButton::clicked, this, &ConfirmationWindow::onReturnButtonClicked);
    layout->addWidget(returnButton);

    // Aseta ikkunan ominaisuudet
    setWindowTitle("Vahvistus");
    resize(300, 150);
}

ConfirmationWindow::~ConfirmationWindow()
{
}

void ConfirmationWindow::onReturnButtonClicked()
{
    emit topUpCompleted();
    close();
}
