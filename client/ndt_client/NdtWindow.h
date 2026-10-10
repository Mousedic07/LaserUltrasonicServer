#pragma once
#include <QWidget>
#include <QJsonObject>
#include "HttpMgr.h"
#include "TcpMgr.h"

class QStackedWidget;
class QTabWidget;
class QLineEdit;
class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
namespace QtCharts { class QLineSeries; class QValueAxis; }

class NdtWindow : public QWidget {
    Q_OBJECT
public:
    explicit NdtWindow(QWidget *parent = nullptr);
private:
    void buildLogin();
    void buildWorkspace();
    QWidget *makeControlPage();
    QWidget *makeWavePage();
    QWidget *makeTaskPage();
    void onMessage(int type, const QJsonObject &data);
    void plotWave(const QJsonObject &data);
    void plotSpectrum(const QJsonObject &data);
    HttpMgr http_;
    TcpMgr tcp_;
    QStackedWidget *stack_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QLineEdit *user_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLabel *loginError_ = nullptr;
    QLabel *node_ = nullptr;
    QLabel *device_ = nullptr;
    QLabel *scanState_ = nullptr;
    QLabel *position_ = nullptr;
    QLabel *peak_ = nullptr;
    QLabel *scanId_ = nullptr;
    QLabel *completed_ = nullptr;
    QLabel *fileName_ = nullptr;
    QLabel *upload_ = nullptr;
    QLabel *speed_ = nullptr;
    QDoubleSpinBox *sampleRate_ = nullptr;
    QSpinBox *sampleCount_ = nullptr;
    QDoubleSpinBox *excitation_ = nullptr;
    QDoubleSpinBox *step_ = nullptr;
    QSpinBox *points_ = nullptr;
    QSpinBox *channels_ = nullptr;
    QComboBox *channelSelect_ = nullptr;
    QtCharts::QLineSeries *waveSeries_ = nullptr;
    QtCharts::QLineSeries *fftSeries_ = nullptr;
    QtCharts::QValueAxis *waveX_ = nullptr;
    QtCharts::QValueAxis *waveY_ = nullptr;
    QtCharts::QValueAxis *fftX_ = nullptr;
    QtCharts::QValueAxis *fftY_ = nullptr;
    QMap<int, QJsonObject> waves_;
    QMap<int, QJsonObject> spectra_;
};
