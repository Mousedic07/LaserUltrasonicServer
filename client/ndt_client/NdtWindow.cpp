#include "NdtWindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <algorithm>

using namespace QtCharts;

NdtWindow::NdtWindow(QWidget *parent) : QWidget(parent), http_(this), tcp_(this) {
    setWindowTitle(tr("Laser Ultrasound NDT"));
    resize(1100, 760);
    setStyleSheet("QWidget{font-size:13px;background:#f5f7fb;color:#17253d;}"
                  "QGroupBox{border:1px solid #d4deea;border-radius:8px;margin-top:12px;padding:14px;}"
                  "QPushButton{background:#1565c0;color:white;border:0;border-radius:5px;padding:8px 14px;}"
                  "QPushButton:hover{background:#0d47a1;}"
                  "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:white;border:1px solid #cbd5e1;"
                  "border-radius:4px;padding:4px;min-height:22px;}");
    stack_ = new QStackedWidget(this);
    auto *outer = new QVBoxLayout(this);
    outer->addWidget(stack_);
    buildLogin();
    buildWorkspace();
    connect(&http_, &HttpMgr::loginFinished, this, [this](const QJsonObject &obj, const QString &error) {
        if (!error.isEmpty() || !obj.value("ok").toBool()) {
            loginError_->setText(error.isEmpty() ? obj.value("error").toString(tr("Login failed")) : error);
            return;
        }
        QString host = obj.value("host").toString();
        int port = obj.value("port").toInt();
        QString token = obj.value("token").toString();
        if (host.isEmpty() || port < 1 || port > 65535 || token.isEmpty()) {
            loginError_->setText(tr("Server returned an incomplete route"));
            return;
        }
        node_->setText(QString("%1:%2").arg(host).arg(port));
        stack_->setCurrentIndex(1);
        tcp_.connectTo(host, quint16(port), token);
    });
    connect(&tcp_, &TcpMgr::connected, this, [this]() { scanState_->setText(tr("TCP connected; authenticating")); });
    connect(&tcp_, &TcpMgr::disconnected, this, [this](const QString &why) {
        scanState_->setText(why); device_->setText(tr("Disconnected"));
    });
    connect(&tcp_, &TcpMgr::message, this, &NdtWindow::onMessage);
}

void NdtWindow::buildLogin() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addStretch();
    auto *box = new QGroupBox(tr("Sign in to NDT system"));
    box->setMaximumWidth(430);
    auto *form = new QFormLayout(box);
    user_ = new QLineEdit;
    password_ = new QLineEdit;
    password_->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Username"), user_);
    form->addRow(tr("Password"), password_);
    auto *button = new QPushButton(tr("Login"));
    form->addRow(button);
    loginError_ = new QLabel;
    loginError_->setStyleSheet("color:#b91c1c;");
    form->addRow(loginError_);
    layout->addWidget(box, 0, Qt::AlignHCenter);
    layout->addStretch();
    connect(button, &QPushButton::clicked, this, [this]() {
        loginError_->setText(tr("Connecting..."));
        QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
        QString url = settings.value("gate/login_url", "http://127.0.0.1:8080/login").toString();
        http_.login(url, user_->text().trimmed(), password_->text());
    });
    stack_->addWidget(page);
}

void NdtWindow::buildWorkspace() {
    tabs_ = new QTabWidget;
    tabs_->addTab(makeControlPage(), tr("Scan control"));
    tabs_->addTab(makeWavePage(), tr("Waveform && FFT"));
    tabs_->addTab(makeTaskPage(), tr("Task && files"));
    stack_->addWidget(tabs_);
}

QWidget *NdtWindow::makeControlPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *state = new QGroupBox(tr("Connection"));
    auto *stateForm = new QFormLayout(state);
    node_ = new QLabel(tr("Not assigned"));
    device_ = new QLabel(tr("Unknown"));
    scanState_ = new QLabel(tr("Idle"));
    stateForm->addRow(tr("AcqServer"), node_);
    stateForm->addRow(tr("Device"), device_);
    stateForm->addRow(tr("Scan"), scanState_);
    layout->addWidget(state);
    auto *params = new QGroupBox(tr("Scan parameters"));
    auto *form = new QFormLayout(params);
    sampleRate_ = new QDoubleSpinBox;
    sampleRate_->setRange(6.0, 100.0); sampleRate_->setValue(20.0); sampleRate_->setSuffix(" MHz");
    sampleCount_ = new QSpinBox;
    sampleCount_->setRange(64, 16384); sampleCount_->setValue(1024);
    excitation_ = new QDoubleSpinBox;
    excitation_->setRange(0.1, 20.0); excitation_->setValue(2.75); excitation_->setSuffix(" MHz");
    step_ = new QDoubleSpinBox;
    step_->setRange(0.001, 100.0); step_->setDecimals(3); step_->setValue(0.5); step_->setSuffix(" mm");
    points_ = new QSpinBox;
    points_->setRange(1, 10000); points_->setValue(10);
    channels_ = new QSpinBox;
    channels_->setRange(1, 16); channels_->setValue(4);
    form->addRow(tr("Sample rate"), sampleRate_);
    form->addRow(tr("Samples per channel"), sampleCount_);
    form->addRow(tr("Excitation"), excitation_);
    form->addRow(tr("Step"), step_);
    form->addRow(tr("Scan points"), points_);
    form->addRow(tr("Channels"), channels_);
    layout->addWidget(params);
    auto *buttons = new QHBoxLayout;
    auto *deviceButton = new QPushButton(tr("Connect devices"));
    auto *start = new QPushButton(tr("Start scan"));
    auto *stop = new QPushButton(tr("Stop scan"));
    buttons->addWidget(deviceButton); buttons->addWidget(start); buttons->addWidget(stop);
    layout->addLayout(buttons);
    layout->addStretch();
    connect(deviceButton, &QPushButton::clicked, this, [this]() { tcp_.send(TcpMgr::Status, QJsonObject{{"command", "connect_devices"}}); });
    connect(start, &QPushButton::clicked, this, [this]() {
        const double rate = sampleRate_->value() * 1e6;
        const double excitation = excitation_->value() * 1e6;
        if (rate <= 2.0 * excitation) { scanState_->setText(tr("Sample rate violates Nyquist condition")); return; }
        tcp_.send(TcpMgr::Start, QJsonObject{{"sample_rate_hz", rate},
              {"sample_count", sampleCount_->value()}, {"excitation_frequency_hz", excitation},
              {"scan_step_mm", step_->value()}, {"scan_points", points_->value()},
              {"channel_count", channels_->value()}});
        scanState_->setText(tr("Start requested"));
    });
    connect(stop, &QPushButton::clicked, this, [this]() { tcp_.send(TcpMgr::Stop, QJsonObject()); });
    return page;
}

QWidget *NdtWindow::makeWavePage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *top = new QHBoxLayout;
    position_ = new QLabel(tr("Position: --"));
    peak_ = new QLabel(tr("Dominant frequency: --"));
    channelSelect_ = new QComboBox;
    for (int i = 0; i < 16; ++i) channelSelect_->addItem(QString("Channel %1").arg(i + 1), i);
    top->addWidget(position_); top->addWidget(channelSelect_); top->addWidget(peak_); top->addStretch();
    layout->addLayout(top);
    auto makeChart = [layout](QLineSeries *&series, QValueAxis *&x, QValueAxis *&y,
                              const QString &title, const QString &xTitle, const QString &yTitle) {
        auto *chart = new QChart;
        chart->setTitle(title);
        chart->legend()->hide();
        series = new QLineSeries;
        x = new QValueAxis; y = new QValueAxis;
        x->setTitleText(xTitle); y->setTitleText(yTitle);
        x->setRange(0, 1); y->setRange(-1, 1);
        chart->addSeries(series); chart->addAxis(x, Qt::AlignBottom); chart->addAxis(y, Qt::AlignLeft);
        series->attachAxis(x); series->attachAxis(y);
        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);
        layout->addWidget(view, 1);
    };
    makeChart(waveSeries_, waveX_, waveY_, tr("Time domain"), tr("Time (μs)"), tr("Amplitude"));
    makeChart(fftSeries_, fftX_, fftY_, tr("FFT spectrum"), tr("Frequency (MHz)"), tr("Magnitude"));
    connect(channelSelect_, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int i) { if (waves_.contains(i)) plotWave(waves_[i]);
                             if (spectra_.contains(i)) plotSpectrum(spectra_[i]); });
    return page;
}

QWidget *NdtWindow::makeTaskPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *box = new QGroupBox(tr("Current task"));
    auto *form = new QFormLayout(box);
    scanId_ = new QLabel("--"); completed_ = new QLabel("0 / 0");
    fileName_ = new QLabel("--"); upload_ = new QLabel("0 %"); speed_ = new QLabel("0 B/s");
    form->addRow(tr("Scan ID"), scanId_); form->addRow(tr("Completed points"), completed_);
    form->addRow(tr("File"), fileName_); form->addRow(tr("Upload"), upload_);
    form->addRow(tr("Speed"), speed_);
    layout->addWidget(box); layout->addStretch();
    return page;
}

void NdtWindow::onMessage(int type, const QJsonObject &data) {
    if (type == TcpMgr::Wave) {
        const int channel = data.value("channel_id").toInt();
        waves_[channel] = data;
        if (channel == channelSelect_->currentData().toInt()) plotWave(data);
    } else if (type == TcpMgr::Spectrum) {
        const int channel = data.value("channel_id").toInt();
        spectra_[channel] = data;
        if (channel == channelSelect_->currentData().toInt()) plotSpectrum(data);
    } else if (type == TcpMgr::Status || type == TcpMgr::Hello || type == TcpMgr::Error) {
        scanState_->setText(data.value("state").toString(data.value("error").toString()));
        if (data.contains("device_connected"))
            device_->setText(data.value("device_connected").toBool() ? tr("Connected") : tr("Disconnected"));
        if (data.contains("scan_id")) scanId_->setText(data.value("scan_id").toString());
        if (data.contains("completed_points"))
            completed_->setText(QString("%1 / %2").arg(data.value("completed_points").toInt())
                                .arg(data.value("total_points").toInt()));
    } else if (type == TcpMgr::File) {
        fileName_->setText(data.value("filename").toString());
        upload_->setText(QString::number(data.value("percent").toDouble(), 'f', 1) + " %  " + data.value("status").toString());
        speed_->setText(QString::number(data.value("speed_bytes_per_sec").toDouble(), 'f', 0) + " B/s");
    }
}

void NdtWindow::plotWave(const QJsonObject &data) {
    const QJsonArray samples = data.value("samples").toArray();
    const double rate = data.value("sample_rate_hz").toDouble();
    if (samples.isEmpty() || rate <= 0) return;
    QVector<QPointF> points;
    double maxAbs = 0;
    const int stride = std::max(1, samples.size() / 1200);
    for (int i = 0; i < samples.size(); i += stride) {
        double y = samples.at(i).toDouble();
        points.append(QPointF(i * 1e6 / rate, y));
        maxAbs = std::max(maxAbs, std::abs(y));
    }
    waveSeries_->replace(points);
    waveX_->setRange(0, (samples.size() - 1) * 1e6 / rate);
    waveY_->setRange(-std::max(0.1, maxAbs * 1.1), std::max(0.1, maxAbs * 1.1));
    position_->setText(QString("Position: x=%1 mm, y=%2 mm")
                       .arg(data.value("position_x_mm").toDouble()).arg(data.value("position_y_mm").toDouble()));
}

void NdtWindow::plotSpectrum(const QJsonObject &data) {
    const QJsonArray bins = data.value("frequency_bins_hz").toArray();
    const QJsonArray amps = data.value("amplitude_spectrum").toArray();
    const int count = std::min(bins.size(), amps.size());
    if (!count) return;
    QVector<QPointF> points;
    double maxY = 0;
    const int stride = std::max(1, count / 1200);
    for (int i = 0; i < count; i += stride) {
        double y = amps.at(i).toDouble();
        points.append(QPointF(bins.at(i).toDouble() / 1e6, y));
        maxY = std::max(maxY, y);
    }
    fftSeries_->replace(points);
    fftX_->setRange(0, bins.at(count - 1).toDouble() / 1e6);
    fftY_->setRange(0, std::max(0.1, maxY * 1.1));
    peak_->setText(QString("Dominant frequency: %1 MHz")
                   .arg(data.value("dominant_frequency_hz").toDouble() / 1e6, 0, 'f', 4));
}
