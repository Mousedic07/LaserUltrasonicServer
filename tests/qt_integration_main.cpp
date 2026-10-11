#include "NdtWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
template <class Predicate> bool waitFor(Predicate predicate, int milliseconds) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return predicate();
}
QPushButton *button(NdtWindow &window, const QString &name) {
    for (auto *value : window.findChildren<QPushButton *>())
        if (value->text() == name) return value;
    return nullptr;
}
QLabel *field(NdtWindow &window, const QString &name) {
    for (auto *form : window.findChildren<QFormLayout *>()) {
        for (int row = 0; row < form->rowCount(); ++row) {
            auto *labelItem = form->itemAt(row, QFormLayout::LabelRole);
            auto *fieldItem = form->itemAt(row, QFormLayout::FieldRole);
            auto *label = labelItem ? qobject_cast<QLabel *>(labelItem->widget()) : nullptr;
            auto *value = fieldItem ? qobject_cast<QLabel *>(fieldItem->widget()) : nullptr;
            if (label && value && label->text() == name) return value;
        }
    }
    return nullptr;
}
int fail(const QString &message) {
    std::cerr << message.toStdString() << '\n';
    return 1;
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    NdtWindow window;
    window.show();
    auto *stack = window.findChild<QStackedWidget *>();
    auto *login = button(window, "Login");
    auto *start = button(window, "Start scan");
    auto *scanState = field(window, "Scan");
    auto *upload = field(window, "Upload");
    auto *scanId = field(window, "Scan ID");
    QLineEdit *username = nullptr, *password = nullptr;
    for (auto *edit : window.findChildren<QLineEdit *>()) {
        if (edit->echoMode() == QLineEdit::Password) password = edit;
        else username = edit;
    }
    if (!stack || !login || !start || !scanState || !upload || !scanId ||
        !username || !password || qgetenv("NDT_TEST_PASSWORD").isEmpty())
        return fail("Qt integration fixture is incomplete");
    QByteArray user = qgetenv("NDT_TEST_USER");
    if (user.isEmpty()) user = "demo";
    username->setText(QString::fromUtf8(user.constData()));
    password->setText(QString::fromUtf8(qgetenv("NDT_TEST_PASSWORD").constData()));
    login->click();
    if (!waitFor([&] { return stack->currentIndex() == 1 && scanState->text() == "authenticated"; }, 15000))
        return fail("Qt login/TCP JWT failed: " + scanState->text());
    for (auto *spin : window.findChildren<QSpinBox *>())
        if (spin->maximum() == 10000) spin->setValue(2);
    start->click();
    const auto charts = window.findChildren<QtCharts::QChartView *>();
    if (charts.size() != 2) return fail("Qt waveform/FFT charts are missing");
    if (!waitFor([&] {
            const auto wave = qobject_cast<QtCharts::QLineSeries *>(charts[0]->chart()->series().value(0));
            const auto fft = qobject_cast<QtCharts::QLineSeries *>(charts[1]->chart()->series().value(0));
            return wave && fft && wave->count() > 0 && fft->count() > 0 &&
                scanState->text() == "complete" && upload->text().contains("complete") &&
                scanId->text() != "--";
        }, 60000))
        return fail("Qt scan/FFT/archive UI incomplete: scan=" + scanState->text() +
                    " upload=" + upload->text() + " id=" + scanId->text());
    std::cout << "Qt 5.9.9 login, JWT TCP, waveform chart, FFT chart and archive UI passed; scan_id="
              << scanId->text().toStdString() << '\n';
    return 0;
}
