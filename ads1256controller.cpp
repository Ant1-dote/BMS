#include "ads1256controller.h"

#include <QDateTime>
#include <QDir>
#include <QPointF>
#include <QStandardPaths>
#include <QTimer>

#include <QtSerialPort/qserialportinfo.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kRenderMaxPoints = 1800;
constexpr qint64 kPlotUpdateIntervalNs = 25'000'000;
constexpr double kAutoYClipRatio = 0.01;

QString shortDescription(const QString &text)
{
    if (text.size() <= 22) {
        return text;
    }
    return text.left(19) + QStringLiteral("...");
}

template <typename T>
void appendCapped(QVector<T> &vec, const T &value, int maxSize)
{
    vec.push_back(value);
    const int overflow = vec.size() - maxSize;
    if (overflow > 0) {
        vec.remove(0, overflow);
    }
}

QList<QPointF> decimatedPoints(const QVector<double> &x, const QVector<double> &y)
{
    QList<QPointF> out;
    const int count = qMin(x.size(), y.size());
    if (count <= 0) {
        return out;
    }

    const int step = qMax(1, count / kRenderMaxPoints);
    out.reserve((count + step - 1) / step + 1);

    for (int i = 0; i < count; i += step) {
        out.push_back(QPointF(x.at(i), y.at(i)));
    }

    if (out.isEmpty() || out.back().x() != x.at(count - 1)) {
        out.push_back(QPointF(x.at(count - 1), y.at(count - 1)));
    }

    return out;
}

std::pair<double, double> yBoundsFromValues(const QVector<double> &values)
{
    if (values.isEmpty()) {
        return { -0.01, 0.01 };
    }

    double yMin = 0.0;
    double yMax = 0.0;

    if (values.size() >= 20) {
        QVector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        const int maxIdx = sorted.size() - 1;
        int lowIdx = static_cast<int>(maxIdx * kAutoYClipRatio);
        int highIdx = static_cast<int>(maxIdx * (1.0 - kAutoYClipRatio));
        lowIdx = qBound(0, lowIdx, maxIdx);
        highIdx = qBound(0, highIdx, maxIdx);
        if (highIdx < lowIdx) {
            highIdx = lowIdx;
        }
        yMin = sorted.at(lowIdx);
        yMax = sorted.at(highIdx);
    } else {
        const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
        yMin = *minIt;
        yMax = *maxIt;
    }

    const double pad = (yMin == yMax)
        ? qMax(0.01, std::abs(yMin) * 0.04)
        : qMax(0.01, (yMax - yMin) * 0.12);

    return { yMin - pad, yMax + pad };
}

} // namespace

ADS1256Controller::ADS1256Controller(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_ekfFilters(8, ScalarEkf(1e-6, 1e-4, 1.0))
    , m_vScan(8)
    , m_scanVisible(8, true)
    , m_scanSeries(8)
    , m_adPattern(QStringLiteral(R"(AD\s*:\s*(-?\d+))"), QRegularExpression::CaseInsensitiveOption)
    , m_ad8Pattern(QStringLiteral(R"(AD8\s*:\s*(-?\d+(?:\s*,\s*-?\d+){7}))"), QRegularExpression::CaseInsensitiveOption)
    , m_hexPattern(QStringLiteral(R"(HEX\s*:\s*0x([0-9A-Fa-f]+))"), QRegularExpression::CaseInsensitiveOption)
    , m_pgaPattern(QStringLiteral(R"(PGA\s*(?:set|changed)?\s*to\s*x?\s*(\d+))"), QRegularExpression::CaseInsensitiveOption)
{
    connect(m_serial, &QSerialPort::readyRead, this, &ADS1256Controller::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &ADS1256Controller::onSerialError);

    initializeDatabase();
    applyEkfParams();
    refreshPorts();
}

QStringList ADS1256Controller::availablePorts() const
{
    return m_portLabels;
}

bool ADS1256Controller::connected() const
{
    return m_connected;
}

bool ADS1256Controller::acquisitionEnabled() const
{
    return m_acquisitionEnabled;
}

QString ADS1256Controller::statusText() const
{
    return m_statusText;
}

QString ADS1256Controller::latestAdText() const
{
    return m_latestAdText;
}

QString ADS1256Controller::latestVoltageText() const
{
    return m_latestVoltageText;
}

QString ADS1256Controller::latestHexText() const
{
    return m_latestHexText;
}

qlonglong ADS1256Controller::sampleCount() const
{
    return m_sampleCount;
}

double ADS1256Controller::vref() const
{
    return m_vref;
}

int ADS1256Controller::pga() const
{
    return m_pga;
}

QString ADS1256Controller::psel() const
{
    return m_psel;
}

QString ADS1256Controller::nsel() const
{
    return m_nsel;
}

int ADS1256Controller::drate() const
{
    return m_drate;
}

QString ADS1256Controller::acqMode() const
{
    return m_acqMode;
}

int ADS1256Controller::viewChannel() const
{
    return m_viewChannel;
}

QString ADS1256Controller::xAxisMode() const
{
    return m_xAxisMode;
}

double ADS1256Controller::windowSeconds() const
{
    return m_windowSeconds;
}

bool ADS1256Controller::ekfEnabled() const
{
    return m_ekfEnabled;
}

double ADS1256Controller::ekfQ() const
{
    return m_ekfQ;
}

double ADS1256Controller::ekfR() const
{
    return m_ekfR;
}

double ADS1256Controller::ekfP0() const
{
    return m_ekfP0;
}

double ADS1256Controller::axisXMin() const
{
    return m_axisXMin;
}

double ADS1256Controller::axisXMax() const
{
    return m_axisXMax;
}

double ADS1256Controller::axisYMin() const
{
    return m_axisYMin;
}

double ADS1256Controller::axisYMax() const
{
    return m_axisYMax;
}

QAbstractListModel *ADS1256Controller::logModel()
{
    return &m_logModel;
}

void ADS1256Controller::setVref(double value)
{
    value = qBound(0.1, value, 5.0);
    if (qFuzzyCompare(m_vref, value)) {
        return;
    }
    m_vref = value;
    emit configChanged();
}

void ADS1256Controller::setPga(int value)
{
    static const QVector<int> allowed { 1, 2, 4, 8, 16, 32, 64 };
    if (!allowed.contains(value) || m_pga == value) {
        return;
    }
    m_pga = value;
    emit configChanged();
}

void ADS1256Controller::setPsel(const QString &value)
{
    const QString v = value.trimmed().toUpper();
    if (v.isEmpty() || m_psel == v) {
        return;
    }
    m_psel = v;
    emit configChanged();
}

void ADS1256Controller::setNsel(const QString &value)
{
    const QString v = value.trimmed().toUpper();
    if (v.isEmpty() || m_nsel == v) {
        return;
    }
    m_nsel = v;
    emit configChanged();
}

void ADS1256Controller::setDrate(int value)
{
    if (m_drate == value) {
        return;
    }
    m_drate = value;
    emit configChanged();
}

void ADS1256Controller::setAcqMode(const QString &value)
{
    const QString mode = value.trimmed().toUpper();
    if ((mode != QStringLiteral("SINGLE") && mode != QStringLiteral("SCAN8")) || m_acqMode == mode) {
        return;
    }
    m_acqMode = mode;
    emit configChanged();
    updatePlot(true);
}

void ADS1256Controller::setViewChannel(int value)
{
    const int channel = qBound(0, value, 7);
    if (m_viewChannel == channel) {
        return;
    }
    m_viewChannel = channel;
    emit configChanged();
}

void ADS1256Controller::setXAxisMode(const QString &value)
{
    const QString mode = value.trimmed().toUpper();
    if ((mode != QStringLiteral("FULL") && mode != QStringLiteral("WINDOW")) || m_xAxisMode == mode) {
        return;
    }
    m_xAxisMode = mode;
    emit configChanged();
    updatePlot(true);
}

void ADS1256Controller::setWindowSeconds(double value)
{
    value = qBound(2.0, value, 21600.0);
    if (qFuzzyCompare(m_windowSeconds, value)) {
        return;
    }
    m_windowSeconds = value;
    emit configChanged();
    updatePlot(true);
}

void ADS1256Controller::setEkfEnabled(bool value)
{
    if (m_ekfEnabled == value) {
        return;
    }
    m_ekfEnabled = value;
    resetEkfFilters(false);
    appendLog(QStringLiteral("info"), QStringLiteral("EKF %1").arg(value ? QStringLiteral("启用") : QStringLiteral("关闭")));
    emit configChanged();
}

void ADS1256Controller::setEkfQ(double value)
{
    value = qBound(1e-12, value, 1.0);
    if (qFuzzyCompare(m_ekfQ, value)) {
        return;
    }
    m_ekfQ = value;
    applyEkfParams();
    emit configChanged();
}

void ADS1256Controller::setEkfR(double value)
{
    value = qBound(1e-12, value, 10.0);
    if (qFuzzyCompare(m_ekfR, value)) {
        return;
    }
    m_ekfR = value;
    applyEkfParams();
    emit configChanged();
}

void ADS1256Controller::setEkfP0(double value)
{
    value = qBound(1e-9, value, 1000.0);
    if (qFuzzyCompare(m_ekfP0, value)) {
        return;
    }
    m_ekfP0 = value;
    applyEkfParams();
    emit configChanged();
}

void ADS1256Controller::refreshPorts()
{
    QStringList labels;
    QStringList devices;

    const auto ports = QSerialPortInfo::availablePorts();
    labels.reserve(ports.size());
    devices.reserve(ports.size());

    for (const QSerialPortInfo &port : ports) {
        const QString desc = port.description().isEmpty() ? QStringLiteral("未知设备") : port.description();
        labels.push_back(QStringLiteral("%1 - %2").arg(port.portName(), shortDescription(desc)));
        devices.push_back(port.portName());
    }

    m_portLabels = labels;
    m_portDevices = devices;
    emit availablePortsChanged();
}

QString ADS1256Controller::portDeviceAt(int index) const
{
    if (index < 0 || index >= m_portDevices.size()) {
        return {};
    }
    return m_portDevices.at(index);
}

void ADS1256Controller::toggleConnection(const QString &device, int baud)
{
    if (m_connected) {
        disconnectSerial();
        return;
    }

    const QString port = device.trimmed();
    if (port.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("未选择串口端口"));
        return;
    }

    m_serial->setPortName(port);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        appendLog(QStringLiteral("error"), QStringLiteral("打开串口失败: %1").arg(m_serial->errorString()));
        return;
    }

    m_connected = true;
    m_statusText = QStringLiteral("已连接 %1（未采样）").arg(port);
    emit connectedChanged();
    emit statusTextChanged();

    resetStats();
    appendLog(QStringLiteral("ok"), QStringLiteral("已连接 %1 @ %2").arg(port).arg(baud));
    sendLine(QStringLiteral("STOP"));
}

void ADS1256Controller::disconnectSerial()
{
    if (!m_connected) {
        return;
    }

    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_connected = false;
    m_acquisitionEnabled = false;
    m_statusText = QStringLiteral("未连接");
    emit connectedChanged();
    emit acquisitionEnabledChanged();
    emit statusTextChanged();

    appendLog(QStringLiteral("info"), QStringLiteral("串口已断开"));
}

void ADS1256Controller::toggleCapture()
{
    if (!m_connected || !m_serial->isOpen()) {
        appendLog(QStringLiteral("error"), QStringLiteral("设备未连接，无法采样"));
        return;
    }

    if (!m_acquisitionEnabled) {
        resetStats();
        m_acquisitionEnabled = true;
        m_statusText = (m_acqMode == QStringLiteral("SCAN8"))
            ? QStringLiteral("采样中（8通道轮询）")
            : QStringLiteral("采样中（单通道）");
        emit acquisitionEnabledChanged();
        emit statusTextChanged();

        appendLog(QStringLiteral("info"), QStringLiteral("开始连续采样"));

        const QString cfg = buildCfgCommand();
        QTimer::singleShot(0, this, [this, cfg]() { sendLine(cfg); });
        QTimer::singleShot(180, this, [this]() { sendLine(QStringLiteral("SELFCAL")); });
        QTimer::singleShot(360, this, [this]() { sendLine(QStringLiteral("START")); });
        return;
    }

    m_acquisitionEnabled = false;
    m_statusText = QStringLiteral("已连接（已暂停采样）");
    emit acquisitionEnabledChanged();
    emit statusTextChanged();

    sendLine(QStringLiteral("STOP"));
    appendLog(QStringLiteral("info"), QStringLiteral("已停止连续采样"));
}

void ADS1256Controller::sendCommand(const QString &command)
{
    sendLine(command);
}

void ADS1256Controller::sendApplyConfig()
{
    sendLine(buildCfgCommand());
}

void ADS1256Controller::sendCustomCommand(const QString &command)
{
    const QString text = command.trimmed();
    if (text.isEmpty()) {
        return;
    }
    sendLine(text);
}

void ADS1256Controller::clearLogView()
{
    m_logModel.clearEntries();
}

void ADS1256Controller::clearLogStorage()
{
    m_logDatabase.clearLogs();
    appendLog(QStringLiteral("info"), QStringLiteral("SQL 日志已清空"), false);
}

void ADS1256Controller::resetEkf()
{
    resetEkfFilters(true);
}

void ADS1256Controller::attachSeries(
    QObject *singleShadow,
    QObject *single,
    QObject *ch0,
    QObject *ch1,
    QObject *ch2,
    QObject *ch3,
    QObject *ch4,
    QObject *ch5,
    QObject *ch6,
    QObject *ch7)
{
    m_singleShadowSeries = qobject_cast<QXYSeries *>(singleShadow);
    m_singleSeries = qobject_cast<QXYSeries *>(single);

    const QVector<QObject *> scanObjects { ch0, ch1, ch2, ch3, ch4, ch5, ch6, ch7 };
    for (int i = 0; i < m_scanSeries.size(); ++i) {
        m_scanSeries[i] = qobject_cast<QXYSeries *>(scanObjects.at(i));
        if (m_scanSeries[i]) {
            m_scanSeries[i]->setVisible(m_scanVisible.at(i));
        }
    }

    updatePlot(true);
}

void ADS1256Controller::setScanChannelVisible(int channel, bool visible)
{
    if (channel < 0 || channel >= m_scanVisible.size()) {
        return;
    }

    if (m_scanVisible[channel] == visible) {
        return;
    }

    m_scanVisible[channel] = visible;
    if (m_scanSeries[channel]) {
        m_scanSeries[channel]->setVisible(visible);
    }
    appendLog(
        QStringLiteral("info"),
        QStringLiteral("AIN%1 %2").arg(channel).arg(visible ? QStringLiteral("显示") : QStringLiteral("隐藏")));

    updatePlot(true);
}

void ADS1256Controller::initializeDatabase()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }

    const QString dbPath = QDir(root).filePath(QStringLiteral("bms_logs.db"));
    QString error;
    if (!m_logDatabase.initialize(dbPath, &error)) {
        appendLog(QStringLiteral("error"), QStringLiteral("初始化 SQL 日志失败: %1").arg(error), false);
        return;
    }

    m_logModel.appendEntries(m_logDatabase.recentLogs(200));
    appendLog(QStringLiteral("info"), QStringLiteral("SQL 日志数据库: %1").arg(dbPath), false);
}

void ADS1256Controller::appendLog(const QString &level, const QString &message, bool persist)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_logModel.appendEntry(LogEntry { timestamp, level, message });
    if (persist) {
        m_logDatabase.insertLog(level, message);
    }
}

void ADS1256Controller::resetStats()
{
    m_startNs = QDateTime::currentMSecsSinceEpoch() * 1'000'000;
    m_lastPlotUpdateNs = 0;
    m_sampleCount = 0;
    m_latestAdText = QStringLiteral("--");
    m_latestVoltageText = QStringLiteral("-- V");
    m_latestHexText = QStringLiteral("--");

    m_tSingle.clear();
    m_vSingle.clear();
    m_vSingleRaw.clear();
    m_tScan.clear();
    for (QVector<double> &vec : m_vScan) {
        vec.clear();
    }

    resetEkfFilters(false);
    emit metricChanged();
    updatePlot(true);
}

void ADS1256Controller::handleLine(const QString &line)
{
    const auto ad8Match = m_ad8Pattern.match(line);
    if (ad8Match.hasMatch()) {
        if (!m_acquisitionEnabled) {
            return;
        }

        const QStringList parts = ad8Match.captured(1).split(',');
        if (parts.size() != 8) {
            return;
        }

        QVector<int> values;
        values.reserve(8);
        bool ok = true;
        for (const QString &part : parts) {
            bool localOk = false;
            const int v = part.trimmed().toInt(&localOk);
            if (!localOk) {
                ok = false;
                break;
            }
            values.push_back(v);
        }

        if (ok) {
            recordScan8(values);
        }
        return;
    }

    const auto adMatch = m_adPattern.match(line);
    if (adMatch.hasMatch()) {
        if (!m_acquisitionEnabled) {
            return;
        }

        bool ok = false;
        const int adc = adMatch.captured(1).toInt(&ok);
        if (!ok) {
            return;
        }

        const auto hexMatch = m_hexPattern.match(line);
        const QString hexValue = hexMatch.hasMatch()
            ? QStringLiteral("0x%1").arg(hexMatch.captured(1).toUpper())
            : QStringLiteral("0x%1").arg(adc & 0x00FFFFFF, 6, 16, QLatin1Char('0')).toUpper();

        recordSingle(adc, hexValue);
        return;
    }

    QString level = QStringLiteral("line");
    const QString low = line.toLower();
    if (low.contains(QStringLiteral("failed")) || low.contains(QStringLiteral("error"))
        || low.contains(QStringLiteral("warn")) || line.contains(QStringLiteral("失败"))
        || line.contains(QStringLiteral("错误"))) {
        level = QStringLiteral("error");
    } else if (low.contains(QStringLiteral("init success")) || low.contains(QStringLiteral("connected"))
               || low.contains(QStringLiteral("changed")) || line.contains(QStringLiteral("成功"))
               || line.contains(QStringLiteral("已连接"))) {
        level = QStringLiteral("ok");
    }

    appendLog(level, line);

    const auto pgaMatch = m_pgaPattern.match(line);
    if (pgaMatch.hasMatch()) {
        bool ok = false;
        const int value = pgaMatch.captured(1).toInt(&ok);
        if (ok) {
            setPga(value);
        }
    }
}

void ADS1256Controller::recordSingle(int adc, const QString &hexValue)
{
    ++m_sampleCount;

    const qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1'000'000;
    const qint64 elapsedNs = nowNs - m_startNs;
    const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;

    const double rawVoltage = adcToVoltage(adc);
    const int channel = singleChannelIndex();
    const double filtered = applyEkf(channel, rawVoltage);

    appendCapped(m_tSingle, t, m_plotBufferMax);
    appendCapped(m_vSingleRaw, rawVoltage, m_plotBufferMax);
    appendCapped(m_vSingle, filtered, m_plotBufferMax);

    m_latestAdText = QString::number(adc);
    m_latestVoltageText = QStringLiteral("%1 V").arg(filtered, 0, 'f', 6);
    m_latestHexText = hexValue;
    emit metricChanged();

    m_logDatabase.insertSample(elapsedNs, QStringLiteral("SINGLE"), channel, filtered, adc, hexValue);
    updatePlot(false);
}

void ADS1256Controller::recordScan8(const QVector<int> &values)
{
    ++m_sampleCount;

    const qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1'000'000;
    const qint64 elapsedNs = nowNs - m_startNs;
    const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;

    appendCapped(m_tScan, t, m_plotBufferMax);

    QVector<double> voltages;
    voltages.reserve(8);
    for (int ch = 0; ch < 8; ++ch) {
        const double raw = adcToVoltage(values.at(ch));
        const double filtered = applyEkf(ch, raw);
        voltages.push_back(filtered);
        appendCapped(m_vScan[ch], filtered, m_plotBufferMax);

        const QString hexValue = QStringLiteral("0x%1")
                                     .arg(values.at(ch) & 0x00FFFFFF, 6, 16, QLatin1Char('0'))
                                     .toUpper();
        m_logDatabase.insertSample(elapsedNs, QStringLiteral("SCAN8"), ch, filtered, values.at(ch), hexValue);
    }

    const int channel = qBound(0, m_viewChannel, 7);
    m_latestAdText = QStringLiteral("CH%1:%2").arg(channel).arg(values.at(channel));
    m_latestVoltageText = QStringLiteral("%1 V").arg(voltages.at(channel), 0, 'f', 6);
    m_latestHexText = QStringLiteral("0x%1").arg(values.at(channel) & 0x00FFFFFF, 6, 16, QLatin1Char('0')).toUpper();
    emit metricChanged();

    updatePlot(false);
}

void ADS1256Controller::sendLine(const QString &line)
{
    const QString cmd = line.trimmed();
    if (cmd.isEmpty()) {
        return;
    }

    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("error"), QStringLiteral("设备未连接，跳过发送: %1").arg(cmd));
        return;
    }

    m_serial->write((cmd + QStringLiteral("\r\n")).toUtf8());
    appendLog(QStringLiteral("tx"), QStringLiteral("[TX] %1").arg(cmd));
}

QString ADS1256Controller::buildCfgCommand() const
{
    const QString mode = (m_acqMode == QStringLiteral("SCAN8")) ? QStringLiteral("RDATA") : QStringLiteral("RDATAC");
    return QStringLiteral("CFG PSEL=%1 NSEL=%2 PGA=%3 DRATE=0x%4 MODE=%5 ACQ=%6")
        .arg(m_psel)
        .arg(m_nsel)
        .arg(m_pga)
        .arg(m_drate, 2, 16, QLatin1Char('0'))
        .arg(mode)
        .arg(m_acqMode)
        .toUpper();
}

double ADS1256Controller::adcToVoltage(int adc) const
{
    const int gain = qMax(1, m_pga);
    const double fullScale = m_vref / static_cast<double>(gain);
    return (static_cast<double>(adc) * 2.0 * fullScale) / 8'388'608.0;
}

int ADS1256Controller::singleChannelIndex() const
{
    const QString text = m_psel.trimmed().toUpper();
    if (text.size() == 4 && text.startsWith(QStringLiteral("AIN")) && text.at(3).isDigit()) {
        const int idx = text.mid(3, 1).toInt();
        if (idx >= 0 && idx <= 7) {
            return idx;
        }
    }
    return 0;
}

double ADS1256Controller::applyEkf(int channel, double value)
{
    const int ch = qBound(0, channel, 7);
    if (!m_ekfEnabled) {
        return value;
    }
    return m_ekfFilters[ch].update(value);
}

void ADS1256Controller::applyEkfParams()
{
    for (ScalarEkf &ekf : m_ekfFilters) {
        ekf.configure(m_ekfQ, m_ekfR, m_ekfP0);
    }
}

void ADS1256Controller::resetEkfFilters(bool logEvent)
{
    applyEkfParams();
    for (ScalarEkf &ekf : m_ekfFilters) {
        ekf.reset();
    }
    if (logEvent) {
        appendLog(QStringLiteral("info"), QStringLiteral("EKF 状态已重置"));
    }
}

void ADS1256Controller::updatePlot(bool force)
{
    const qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1'000'000;
    if (!force && m_lastPlotUpdateNs > 0 && (nowNs - m_lastPlotUpdateNs) < kPlotUpdateIntervalNs) {
        return;
    }
    m_lastPlotUpdateNs = nowNs;

    if (m_acqMode == QStringLiteral("SCAN8")) {
        if (m_tScan.isEmpty()) {
            clearSeries();
            return;
        }

        const auto [xMin, xMax] = xRange(m_tScan);
        QVector<double> yVisible;

        for (int ch = 0; ch < 8; ++ch) {
            if (!m_scanSeries[ch]) {
                continue;
            }

            m_scanSeries[ch]->setVisible(m_scanVisible[ch]);
            if (!m_scanVisible[ch]) {
                m_scanSeries[ch]->clear();
                continue;
            }

            const QList<QPointF> points = decimatedPoints(m_tScan, m_vScan[ch]);
            m_scanSeries[ch]->replace(points);
            for (const QPointF &pt : points) {
                yVisible.push_back(pt.y());
            }
        }

        if (m_singleShadowSeries) {
            m_singleShadowSeries->clear();
        }
        if (m_singleSeries) {
            m_singleSeries->clear();
        }

        const auto [yMin, yMax] = yBoundsFromValues(yVisible);
        updateAxisRange(xMin, xMax, yMin, yMax);
        return;
    }

    if (m_tSingle.isEmpty()) {
        clearSeries();
        return;
    }

    for (int ch = 0; ch < 8; ++ch) {
        if (m_scanSeries[ch]) {
            m_scanSeries[ch]->clear();
        }
    }

    const auto [xMin, xMax] = xRange(m_tSingle);
    const QList<QPointF> rawPoints = decimatedPoints(m_tSingle, m_vSingleRaw);
    const QList<QPointF> filtPoints = decimatedPoints(m_tSingle, m_vSingle);

    QVector<double> yVisible;
    yVisible.reserve(filtPoints.size());
    for (const QPointF &pt : filtPoints) {
        yVisible.push_back(pt.y());
    }

    if (m_singleShadowSeries) {
        m_singleShadowSeries->replace(rawPoints);
    }
    if (m_singleSeries) {
        m_singleSeries->replace(filtPoints);
    }

    const auto [yMin, yMax] = yBoundsFromValues(yVisible);
    updateAxisRange(xMin, xMax, yMin, yMax);
}

void ADS1256Controller::clearSeries()
{
    if (m_singleShadowSeries) {
        m_singleShadowSeries->clear();
    }
    if (m_singleSeries) {
        m_singleSeries->clear();
    }
    for (const auto &series : m_scanSeries) {
        if (series) {
            series->clear();
        }
    }
    updateAxisRange(0.0, 1.0, -0.01, 0.01);
}

void ADS1256Controller::updateAxisRange(double xMin, double xMax, double yMin, double yMax)
{
    if (xMax <= xMin) {
        xMax = xMin + 0.05;
    }
    if (yMax <= yMin) {
        yMax = yMin + 0.01;
    }

    const bool changed = !qFuzzyCompare(m_axisXMin, xMin)
        || !qFuzzyCompare(m_axisXMax, xMax)
        || !qFuzzyCompare(m_axisYMin, yMin)
        || !qFuzzyCompare(m_axisYMax, yMax);

    if (!changed) {
        return;
    }

    m_axisXMin = xMin;
    m_axisXMax = xMax;
    m_axisYMin = yMin;
    m_axisYMax = yMax;
    emit axisRangeChanged();
}

std::pair<double, double> ADS1256Controller::xRange(const QVector<double> &x) const
{
    if (x.isEmpty()) {
        return { 0.0, 1.0 };
    }

    if (m_xAxisMode == QStringLiteral("FULL")) {
        const double left = x.first();
        double right = x.last();
        if (right <= left) {
            right = left + 0.05;
        }
        return { left, right };
    }

    const double right = x.last();
    const double span = qMax(0.05, m_windowSeconds);
    const double left = qMax(0.0, right - span);
    return { left, left + span };
}

void ADS1256Controller::onReadyRead()
{
    m_rxBuffer.append(m_serial->readAll());

    int newlineIndex = m_rxBuffer.indexOf('\n');
    while (newlineIndex >= 0) {
        QByteArray lineData = m_rxBuffer.left(newlineIndex);
        m_rxBuffer.remove(0, newlineIndex + 1);

        if (!lineData.isEmpty() && lineData.endsWith('\r')) {
            lineData.chop(1);
        }

        const QString line = QString::fromUtf8(lineData).trimmed();
        if (!line.isEmpty()) {
            handleLine(line);
        }

        newlineIndex = m_rxBuffer.indexOf('\n');
    }
}

void ADS1256Controller::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    if (!m_connected) {
        return;
    }

    appendLog(QStringLiteral("error"), QStringLiteral("串口错误: %1").arg(m_serial->errorString()));
    disconnectSerial();
}
