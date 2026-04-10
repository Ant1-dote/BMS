#include "ads1256controller.h"

#include "crashlogger.h"
#include "samplesqlwriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMetaObject>
#include <QMetaType>
#include <QPointF>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <QtSerialPort/qserialportinfo.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

constexpr int kRenderTargetPointsSingle = 1800;
constexpr int kRenderTargetPointsScanMin = 240;
constexpr int kRenderTargetPointsScanBudget = 3200;
constexpr int kMaxLoadLogRows = 5000;
constexpr int kMaxLoadSampleRows = 0;
constexpr int kSqlBufferFlushCount = 256;
constexpr int kMaxPendingSamples = 200000;
constexpr int kCsvFlushInterval = 32;
constexpr int kMaxRxBufferBytes = 512 * 1024;
constexpr int kMaxSerialLineBytes = 16 * 1024;
constexpr qint64 kPlotUpdateIntervalNs = 25'000'000;
constexpr double kAutoYClipRatio = 0.01;
constexpr double kHourAxisThresholdSeconds = 3600.0;
constexpr double kLatestDataXAxisRatio = 0.7;
constexpr qint64 kZoomCsvMaxRows = 2'000'000;

QString shortDescription(const QString &text)
{
    if (text.size() <= 22) {
        return text;
    }
    return text.left(19) + QStringLiteral("...");
}

template <typename Series>
std::pair<int, int> rangeIndices(const Series &x, double minX, double maxX)
{
    const int count = static_cast<int>(x.size());
    if (count <= 0) {
        return { 0, 0 };
    }

    if (maxX < minX) {
        std::swap(minX, maxX);
    }

    const auto beginIt = std::lower_bound(x.begin(), x.end(), minX);
    const auto endIt = std::upper_bound(x.begin(), x.end(), maxX);

    int begin = static_cast<int>(beginIt - x.begin());
    int end = static_cast<int>(endIt - x.begin());

    begin = qBound(0, begin, count);
    end = qBound(0, end, count);
    if (end <= begin) {
        end = qMin(count, begin + 1);
    }

    return { begin, end };
}

template <typename SeriesX, typename SeriesY>
QList<QPointF> fullPointsInRange(const SeriesX &x, const SeriesY &y, double minX, double maxX)
{
    QList<QPointF> out;
    const int count = qMin(static_cast<int>(x.size()), static_cast<int>(y.size()));
    if (count <= 0) {
        return out;
    }

    const auto [begin, end] = rangeIndices(x, minX, maxX);
    const int localCount = qMax(0, qMin(end, count) - qMin(begin, count));
    if (localCount <= 0) {
        return out;
    }

    out.reserve(localCount);
    for (int i = begin; i < end && i < count; ++i) {
        out.push_back(QPointF(x[i], y[i]));
    }
    return out;
}

template <typename SeriesX, typename SeriesY>
QList<QPointF> decimatedMinMaxPoints(
    const SeriesX &x,
    const SeriesY &y,
    double minX,
    double maxX,
    int targetPoints)
{
    QList<QPointF> out;
    const int count = qMin(static_cast<int>(x.size()), static_cast<int>(y.size()));
    if (count <= 0) {
        return out;
    }

    targetPoints = qMax(50, targetPoints);
    const auto [begin, end] = rangeIndices(x, minX, maxX);
    const int localCount = qMax(0, qMin(end, count) - qMin(begin, count));
    if (localCount <= 0) {
        return out;
    }

    if (localCount <= targetPoints) {
        return fullPointsInRange(x, y, minX, maxX);
    }

    const int bucketCount = qMax(1, targetPoints / 2);
    const int bucketSize = qMax(1, static_cast<int>(std::ceil(static_cast<double>(localCount) / bucketCount)));
    QVector<int> indices;
    indices.reserve(targetPoints + 4);
    indices.push_back(begin);

    for (int bucketStart = begin; bucketStart < end; bucketStart += bucketSize) {
        const int bucketEnd = qMin(end, bucketStart + bucketSize);
        if (bucketStart >= bucketEnd) {
            continue;
        }

        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        int minIdx = bucketStart;
        int maxIdx = bucketStart;

        for (int i = bucketStart; i < bucketEnd; ++i) {
            const double value = y[i];
            if (value < minY) {
                minY = value;
                minIdx = i;
            }
            if (value > maxY) {
                maxY = value;
                maxIdx = i;
            }
        }

        indices.push_back(minIdx);
        indices.push_back(maxIdx);
    }

    indices.push_back(end - 1);
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    if (indices.size() > targetPoints) {
        QVector<int> reduced;
        const int step = qMax(1, static_cast<int>(std::ceil(static_cast<double>(indices.size()) / targetPoints)));
        reduced.reserve(targetPoints);
        for (int i = 0; i < indices.size(); i += step) {
            reduced.push_back(indices.at(i));
        }
        if (reduced.isEmpty() || reduced.back() != indices.back()) {
            reduced.push_back(indices.back());
        }
        indices = reduced;
    }

    out.reserve(indices.size());
    for (int idx : std::as_const(indices)) {
        if (idx >= 0 && idx < count) {
            out.push_back(QPointF(x[idx], y[idx]));
        }
    }
    return out;
}

QVector<double> extractY(const QList<QPointF> &points)
{
    QVector<double> values;
    values.reserve(points.size());
    for (const QPointF &pt : points) {
        values.push_back(pt.y());
    }
    return values;
}

template <typename SeriesX, typename SeriesY>
QList<QPointF> pickPlotPoints(
    const SeriesX &x,
    const SeriesY &y,
    double minX,
    double maxX,
    bool fullResolution,
    int targetPoints = kRenderTargetPointsSingle)
{
    if (fullResolution) {
        return fullPointsInRange(x, y, minX, maxX);
    }

    const int resolvedTargetPoints = (targetPoints > 0)
        ? targetPoints
        : kRenderTargetPointsSingle;
    return decimatedMinMaxPoints(x, y, minX, maxX, resolvedTargetPoints);
}

bool shouldUseHourAxis(double rightSeconds)
{
    return rightSeconds >= kHourAxisThresholdSeconds;
}

double axisDisplayScale(bool useHours)
{
    return useHours ? 3600.0 : 1.0;
}

class ElapsedTimelineNormalizer {
public:
    qint64 normalize(qint64 rawElapsedNs)
    {
        if (m_hasPrevious && rawElapsedNs < m_previousRawNs) {
            const qint64 step = qMax<qint64>(1, m_lastPositiveDeltaNs);
            m_sessionOffsetNs = m_previousNormalizedNs + step - rawElapsedNs;
        }

        const qint64 normalizedNs = rawElapsedNs + m_sessionOffsetNs;
        if (m_hasPrevious) {
            const qint64 delta = normalizedNs - m_previousNormalizedNs;
            if (delta > 0) {
                m_lastPositiveDeltaNs = delta;
            }
        }

        m_previousRawNs = rawElapsedNs;
        m_previousNormalizedNs = normalizedNs;
        m_hasPrevious = true;
        return normalizedNs;
    }

private:
    bool m_hasPrevious = false;
    qint64 m_previousRawNs = 0;
    qint64 m_previousNormalizedNs = 0;
    qint64 m_sessionOffsetNs = 0;
    qint64 m_lastPositiveDeltaNs = 1'000'000;
};

qint64 elapsedSinceFirstSample(qint64 nowNs, qint64 &startNs)
{
    if (startNs <= 0) {
        startNs = nowNs;
        return 0;
    }
    return qMax<qint64>(0, nowNs - startNs);
}

struct CsvParsedRow {
    qint64 elapsedNs = 0;
    SampleMode mode = SampleMode::Single;
    int channel = 0;
    double voltage = 0.0;
    int adc = 0;
    bool hasAdc = false;
};

bool parseLocalDateTimeMs(const QString &text, qint64 *outMs)
{
    if (!outMs) {
        return false;
    }

    const QString value = text.trimmed();
    if (value.isEmpty()) {
        return false;
    }

    QDateTime dt = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, Qt::ISODateWithMs);
    }
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, Qt::ISODate);
    }
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, QStringLiteral("yyyy/MM/dd HH:mm:ss"));
    }
    if (!dt.isValid()) {
        return false;
    }

    *outMs = dt.toMSecsSinceEpoch();
    return true;
}

bool parseTimestampElapsedNs(const QString &timestampText, qint64 &firstTimestampMs, qint64 *outElapsedNs)
{
    if (!outElapsedNs) {
        return false;
    }

    qint64 timestampMs = 0;
    if (!parseLocalDateTimeMs(timestampText, &timestampMs)) {
        return false;
    }

    if (firstTimestampMs < 0) {
        firstTimestampMs = timestampMs;
    }

    const qint64 elapsedMs = qMax<qint64>(0, timestampMs - firstTimestampMs);
    *outElapsedNs = elapsedMs * 1'000'000;
    return true;
}

bool parseCsvRow(const QString &line, qint64 &firstTimestampMs, CsvParsedRow *outRow)
{
    if (!outRow) {
        return false;
    }

    const QStringList parts = line.split(',');
    if (parts.size() >= 6) {
        bool okNs = false;
        const qint64 elapsedNs = parts.at(0).toLongLong(&okNs);
        if (!okNs || elapsedNs < 0) {
            return false;
        }

        bool okChannel = false;
        const int channel = qBound(0, parts.at(2).toInt(&okChannel), 7);
        bool okVoltage = false;
        const double voltage = parts.at(3).toDouble(&okVoltage);
        bool okAdc = false;
        const int adc = parts.at(4).toInt(&okAdc);
        if (!okVoltage || !okChannel) {
            return false;
        }

        const QString modeText = parts.at(1).trimmed().toUpper();

        outRow->elapsedNs = elapsedNs;
        outRow->mode = (modeText == QStringLiteral("SCAN8")) ? SampleMode::Scan8 : SampleMode::Single;
        outRow->channel = channel;
        outRow->voltage = voltage;
        outRow->adc = okAdc ? adc : 0;
        outRow->hasAdc = okAdc;
        return true;
    }

    if (parts.size() >= 2) {
        qint64 elapsedNs = 0;
        if (!parseTimestampElapsedNs(parts.at(0), firstTimestampMs, &elapsedNs)) {
            return false;
        }

        bool okVoltage = false;
        const double voltage = parts.at(1).toDouble(&okVoltage);
        if (!okVoltage) {
            return false;
        }

        outRow->elapsedNs = elapsedNs;
        outRow->mode = SampleMode::Single;
        outRow->channel = 0;
        outRow->voltage = voltage;
        outRow->adc = 0;
        outRow->hasAdc = false;
        return true;
    }

    return false;
}

QList<QPointF> scalePointsX(const QList<QPointF> &points, double xScale)
{
    if (qFuzzyCompare(xScale, 1.0)) {
        return points;
    }

    QList<QPointF> out;
    out.reserve(points.size());
    for (const QPointF &pt : points) {
        out.push_back(QPointF(pt.x() / xScale, pt.y()));
    }
    return out;
}

template <typename Series>
double inferredLatestX(const Series &x)
{
    if (x.empty()) {
        return 0.0;
    }
    return qMax(0.0, x.back());
}

template <typename Series>
std::pair<double, double> standardFullRange(const Series &x)
{
    const double latest = inferredLatestX(x);
    const double right = qMax(1.0, latest / kLatestDataXAxisRatio);
    return { 0.0, right };
}

template <typename Series>
std::pair<double, double> clampedZoomRange(const Series &x, double xMin, double xMax)
{
    if (x.empty()) {
        return { 0.0, 1.0 };
    }

    if (xMax < xMin) {
        std::swap(xMin, xMax);
    }

    const double dataMin = x.front();
    const double dataMax = x.back();
    xMin = qBound(dataMin, xMin, dataMax);
    xMax = qBound(dataMin, xMax, dataMax);
    if (xMax <= xMin) {
        xMax = qMin(dataMax, xMin + 0.05);
    }
    if (xMax <= xMin) {
        xMax = xMin + 0.05;
    }
    return { xMin, xMax };
}

std::pair<double, double> defaultYRange()
{
    return { -0.01, 0.01 };
}

bool hasVisibleDataPoints(const QList<QPointF> &points)
{
    return !points.isEmpty();
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
    , m_zoomScanPoints(8)
    , m_adPattern(QStringLiteral(R"(AD\s*:\s*(-?\d+))"), QRegularExpression::CaseInsensitiveOption)
    , m_ad8Pattern(QStringLiteral(R"(AD8\s*:\s*(-?\d+(?:\s*,\s*-?\d+){7}))"), QRegularExpression::CaseInsensitiveOption)
    , m_hexPattern(QStringLiteral(R"(HEX\s*:\s*0x([0-9A-Fa-f]+))"), QRegularExpression::CaseInsensitiveOption)
    , m_pgaPattern(QStringLiteral(R"(PGA\s*(?:set|changed)?\s*to\s*x?\s*(\d+))"), QRegularExpression::CaseInsensitiveOption)
{
    qRegisterMetaType<QVector<SampleEntry>>("QVector<SampleEntry>");

    m_serial->setReadBufferSize(kMaxRxBufferBytes);

    connect(m_serial, &QSerialPort::readyRead, this, &ADS1256Controller::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &ADS1256Controller::onSerialError);

    m_sampleWriterThread = new QThread(this);
    m_sampleWriter = new SampleSqlWriter();
    m_sampleWriter->moveToThread(m_sampleWriterThread);
    connect(m_sampleWriterThread, &QThread::finished, m_sampleWriter, &QObject::deleteLater);
    connect(
        m_sampleWriter,
        &SampleSqlWriter::writerWarning,
        this,
        [this](const QString &message) {
            appendLog(QStringLiteral("error"), message, false);
            CrashLogger::log(message);
        },
        Qt::QueuedConnection);
    m_sampleWriterThread->start();

    initializeDatabase();
    applyEkfParams();
    refreshPorts();
}

ADS1256Controller::~ADS1256Controller()
{
    if (m_sampleWriter && m_sampleWriterThread && m_sampleWriterThread->isRunning()) {
        bool ignored = false;
        QMetaObject::invokeMethod(
            m_sampleWriter,
            "flushPendingSync",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, ignored));
        m_sampleWriterThread->quit();
        m_sampleWriterThread->wait(2000);
    }
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

bool ADS1256Controller::axisXInHours() const
{
    return m_axisXInHours;
}

double ADS1256Controller::axisYMin() const
{
    return m_axisYMin;
}

double ADS1256Controller::axisYMax() const
{
    return m_axisYMax;
}

bool ADS1256Controller::zoomActive() const
{
    return m_zoomActive;
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
    QString mode = value.trimmed().toUpper();
    if (mode == QStringLiteral("MULTI")) {
        mode = QStringLiteral("SCAN8");
    }
    if ((mode != QStringLiteral("SINGLE") && mode != QStringLiteral("SCAN8")) || m_acqMode == mode) {
        return;
    }
    m_acqMode = mode;
    m_zoomActive = false;
    clearZoomCache();
    emit configChanged();
    emit axisRangeChanged();
    updatePlot(true);
}

void ADS1256Controller::setViewChannel(int value)
{
    const int channel = qBound(0, value, 7);
    if (m_viewChannel == channel) {
        return;
    }
    m_viewChannel = channel;

    if (m_hasLatestScanFrame) {
        const int adc = m_latestScanAdc[static_cast<size_t>(channel)];
        const double voltage = m_latestScanVoltage[static_cast<size_t>(channel)];
        m_latestAdText = QStringLiteral("CH%1:%2").arg(channel).arg(adc);
        m_latestVoltageText = QStringLiteral("%1 V").arg(voltage, 0, 'f', 6);
        m_latestHexText = QStringLiteral("0x%1").arg(adc & 0x00FFFFFF, 6, 16, QLatin1Char('0')).toUpper();
        emit metricChanged();
    }

    emit configChanged();
    updatePlot(true);
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

void ADS1256Controller::startCaptureWithDirectory(const QString &directoryPathOrUrl)
{
    if (m_acquisitionEnabled) {
        return;
    }

    const QString storageDir = normalizeDbPath(directoryPathOrUrl);
    if (storageDir.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("未选择存储目录"), false);
        return;
    }

    if (!configureStorageDirectory(storageDir)) {
        return;
    }

    startCapture();
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

    if (m_acquisitionEnabled) {
        stopCapture(true);
    }

    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_connected = false;
    m_statusText = QStringLiteral("未连接");
    emit connectedChanged();
    emit statusTextChanged();

    appendLog(QStringLiteral("info"), QStringLiteral("串口已断开"));
}

void ADS1256Controller::startCapture()
{
    if (!m_connected || !m_serial->isOpen()) {
        appendLog(QStringLiteral("error"), QStringLiteral("设备未连接，无法采样"));
        return;
    }

    if (m_acquisitionEnabled) {
        return;
    }

    if (m_storageDir.isEmpty() && !configureStorageDirectory(defaultStorageDirectory())) {
        appendLog(QStringLiteral("error"), QStringLiteral("初始化采样存储目录失败"), false);
        return;
    }

    if (!openCsvForSession()) {
        appendLog(QStringLiteral("error"), QStringLiteral("无法创建 CSV 采样文件"), false);
        return;
    }

    resetStats();
    m_acquisitionEnabled = true;
    m_statusText = (m_acqMode == QStringLiteral("SCAN8"))
        ? QStringLiteral("采样中（多通道）")
        : QStringLiteral("采样中（单通道）");
    emit acquisitionEnabledChanged();
    emit statusTextChanged();

    appendLog(QStringLiteral("info"), QStringLiteral("开始连续采样"));
    appendLog(QStringLiteral("info"), QStringLiteral("CSV 文件: %1").arg(m_csvPath), false);
    appendLog(QStringLiteral("info"), QStringLiteral("日志数据库: %1").arg(m_logDbPath), false);
    appendLog(QStringLiteral("info"), QStringLiteral("崩溃日志: %1").arg(CrashLogger::logFilePath()), false);

    const QString cfg = buildCfgCommand();
    QTimer::singleShot(0, this, [this, cfg]() { sendLine(cfg); });
    QTimer::singleShot(180, this, [this]() { sendLine(QStringLiteral("SELFCAL")); });
    QTimer::singleShot(360, this, [this]() { sendLine(QStringLiteral("START")); });
}

void ADS1256Controller::stopCapture(bool saveData)
{
    if (!m_acquisitionEnabled) {
        return;
    }

    m_acquisitionEnabled = false;
    m_statusText = QStringLiteral("已连接（已暂停采样）");
    emit acquisitionEnabledChanged();
    emit statusTextChanged();

    if (m_serial->isOpen()) {
        sendLine(QStringLiteral("STOP"));
    }

    const QString csvPath = m_csvPath;
    const int localSampleRows = m_pendingSamples.size();
    if (saveData) {
        if (persistPendingSamples()) {
            closeCsvFile();
            appendLog(
                QStringLiteral("ok"),
                QStringLiteral("已停止连续采样，CSV 已保存: %1，SQL 异步队列已落盘（本地缓冲 %2 条）")
                    .arg(csvPath)
                    .arg(localSampleRows));
        } else {
            closeCsvFile();
            appendLog(
                QStringLiteral("error"),
                QStringLiteral("已停止连续采样，CSV 已保存: %1，但 SQL 异步落盘未完成（本地缓冲 %2 条）")
                    .arg(csvPath)
                    .arg(localSampleRows));
        }
    } else {
        int droppedAsyncRows = 0;
        if (m_sampleWriter && m_sampleWriterThread && m_sampleWriterThread->isRunning()) {
            QMetaObject::invokeMethod(
                m_sampleWriter,
                "discardPendingSync",
                Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(int, droppedAsyncRows));
        }
        m_pendingSamples.clear();
        closeCsvFile();
        appendLog(
            QStringLiteral("info"),
            QStringLiteral("已停止连续采样，CSV 已保存: %1；丢弃本地 %2 条 + 异步队列 %3 条 SQL 缓冲")
                .arg(csvPath)
                .arg(localSampleRows)
                .arg(droppedAsyncRows));
    }
}

void ADS1256Controller::toggleCapture()
{
    if (!m_acquisitionEnabled) {
        startCapture();
        return;
    }

    stopCapture(true);
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

void ADS1256Controller::openLogFileDialog()
{
    const QString startDir = m_storageDir.isEmpty() ? defaultStorageDirectory() : m_storageDir;
    const QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("选择日志文件"),
        startDir,
        QStringLiteral("SQLite 数据库 (*.db *.sqlite *.sqlite3);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }
    loadLogFile(filePath);
}

void ADS1256Controller::openDataFileDialog()
{
    const QString startDir = m_storageDir.isEmpty() ? defaultStorageDirectory() : m_storageDir;
    const QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("选择数据文件"),
        startDir,
        QStringLiteral("数据文件 (*.csv *.db *.sqlite *.sqlite3);;CSV 文件 (*.csv);;SQLite 数据库 (*.db *.sqlite *.sqlite3);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }
    loadDataFile(filePath);
}

void ADS1256Controller::openCaptureDirectoryDialog()
{
    const QString startDir = m_storageDir.isEmpty() ? defaultStorageDirectory() : m_storageDir;
    const QString dirPath = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("选择采样存放目录"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        return;
    }
    startCaptureWithDirectory(dirPath);
}

void ADS1256Controller::loadLogFile(const QString &filePathOrUrl)
{
    if (m_acquisitionEnabled) {
        appendLog(QStringLiteral("error"), QStringLiteral("采样进行中，请先停止后再打开日志文件"), false);
        return;
    }

    const QString dbPath = normalizeDbPath(filePathOrUrl);
    if (dbPath.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("未选择日志文件"), false);
        return;
    }

    const QFileInfo info(dbPath);
    if (!info.exists() || !info.isFile()) {
        appendLog(QStringLiteral("error"), QStringLiteral("日志文件不存在: %1").arg(dbPath), false);
        return;
    }

    QString error;
    const QVector<LogEntry> logs = LogDatabase::readRecentLogsFromFile(dbPath, kMaxLoadLogRows, &error);
    if (!error.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("打开日志文件失败: %1").arg(error), false);
        return;
    }

    m_logModel.clearEntries();
    m_logModel.appendEntries(logs);
    appendLog(
        QStringLiteral("info"),
        QStringLiteral("已加载日志文件: %1（显示 %2 条）").arg(dbPath).arg(logs.size()),
        false);
}

void ADS1256Controller::loadDataFile(const QString &filePathOrUrl)
{
    if (m_acquisitionEnabled) {
        appendLog(QStringLiteral("error"), QStringLiteral("采样进行中，请先停止后再打开数据文件"), false);
        return;
    }

    const QString dataPath = normalizeDbPath(filePathOrUrl);
    if (dataPath.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("未选择数据文件"), false);
        return;
    }

    const QFileInfo info(dataPath);
    if (!info.exists() || !info.isFile()) {
        appendLog(QStringLiteral("error"), QStringLiteral("数据文件不存在: %1").arg(dataPath), false);
        return;
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix == QStringLiteral("csv")) {
        QString error;
        if (!rebuildPlotDataFromCsv(dataPath, &error)) {
            appendLog(
                QStringLiteral("error"),
                QStringLiteral("打开 CSV 数据文件失败: %1").arg(error.isEmpty() ? QStringLiteral("无有效采样数据") : error),
                false);
            return;
        }

        m_csvPath = dataPath;
        clearZoomCache();
        appendLog(
            QStringLiteral("info"),
            QStringLiteral("已加载 CSV 数据文件: %1（%2 条采样）").arg(dataPath).arg(m_sampleCount),
            false);
        return;
    }

    QString error;
    const QVector<SampleEntry> samples = LogDatabase::readRecentSamplesFromFile(
        dataPath,
        kMaxLoadSampleRows,
        &error);
    if (!error.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("打开数据文件失败: %1").arg(error), false);
        return;
    }

    m_csvPath.clear();
    clearZoomCache();
    rebuildPlotDataFromSamples(samples);
    appendLog(
        QStringLiteral("info"),
        QStringLiteral("已加载数据文件: %1（%2 条采样）").arg(dataPath).arg(samples.size()),
        false);
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

bool ADS1256Controller::setScanChannelSampling(int channel, bool enabled)
{
    if (channel < 0 || channel > 7) {
        return false;
    }

    const quint8 bit = static_cast<quint8>(1u << static_cast<unsigned int>(channel));
    quint8 nextMask = m_scanSampleMask;
    if (enabled) {
        nextMask = static_cast<quint8>(nextMask | bit);
    } else {
        nextMask = static_cast<quint8>(nextMask & static_cast<quint8>(~bit));
    }

    if (nextMask == 0) {
        appendLog(QStringLiteral("error"), QStringLiteral("至少保留一个采样通道"), false);
        return false;
    }

    if (nextMask == m_scanSampleMask) {
        return true;
    }

    m_scanSampleMask = nextMask;

    if ((m_scanSampleMask & static_cast<quint8>(1u << static_cast<unsigned int>(m_viewChannel))) == 0) {
        for (int i = 0; i < 8; ++i) {
            if ((m_scanSampleMask & static_cast<quint8>(1u << static_cast<unsigned int>(i))) != 0) {
                setViewChannel(i);
                break;
            }
        }
    }

    appendLog(
        QStringLiteral("info"),
        QStringLiteral("AIN%1 采样%2").arg(channel).arg(enabled ? QStringLiteral("启用") : QStringLiteral("禁用")),
        false);

    emit configChanged();
    return true;
}

void ADS1256Controller::setZoomRange(double xMin, double xMax, double yMin, double yMax)
{
    if (xMax <= xMin || yMax <= yMin) {
        return;
    }

    const double xScale = m_axisXInHours ? 3600.0 : 1.0;

    m_zoomActive = true;
    m_zoomXMin = xMin * xScale;
    m_zoomXMax = xMax * xScale;
    m_zoomYMin = yMin;
    m_zoomYMax = yMax;
    clearZoomCache();
    rebuildZoomCacheFromCsv();

    emit axisRangeChanged();
    updatePlot(true);
}

void ADS1256Controller::resetZoom()
{
    if (!m_zoomActive) {
        return;
    }

    m_zoomActive = false;
    clearZoomCache();
    emit axisRangeChanged();
    updatePlot(true);
}

void ADS1256Controller::initializeDatabase()
{
    configureStorageDirectory(defaultStorageDirectory());
}

QString ADS1256Controller::defaultStorageDirectory() const
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }
    return QDir(root).filePath(QStringLiteral("BMS_Data"));
}

bool ADS1256Controller::configureStorageDirectory(const QString &directoryPath)
{
    const QString storageDir = QDir::cleanPath(directoryPath.trimmed());
    if (storageDir.isEmpty()) {
        appendLog(QStringLiteral("error"), QStringLiteral("存储目录为空"), false);
        return false;
    }

    QDir dir(storageDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        appendLog(QStringLiteral("error"), QStringLiteral("无法创建存储目录: %1").arg(storageDir), false);
        return false;
    }

    const QString dbPath = dir.filePath(QStringLiteral("bms_logs.db"));
    if (m_logDbPath == dbPath && m_logDatabase.isReady()) {
        m_storageDir = storageDir;
        CrashLogger::setLogDirectory(storageDir);
        return true;
    }

    QString error;
    if (!m_logDatabase.initialize(dbPath, &error)) {
        appendLog(QStringLiteral("error"), QStringLiteral("初始化 SQL 日志失败: %1").arg(error), false);
        return false;
    }

    m_storageDir = storageDir;
    m_logDbPath = dbPath;

    if (m_sampleWriter && m_sampleWriterThread && m_sampleWriterThread->isRunning()) {
        bool writerReady = false;
        QMetaObject::invokeMethod(
            m_sampleWriter,
            "configureDatabaseSync",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, writerReady),
            Q_ARG(QString, dbPath));
        if (!writerReady) {
            appendLog(QStringLiteral("error"), QStringLiteral("异步采样写入器初始化失败"), false);
            return false;
        }
    }

    CrashLogger::setLogDirectory(storageDir);

    m_logModel.clearEntries();
    m_logModel.appendEntries(m_logDatabase.recentLogs(200));
    appendLog(QStringLiteral("info"), QStringLiteral("日志数据库切换为: %1").arg(dbPath), false);
    return true;
}

bool ADS1256Controller::openCsvForSession()
{
    if (m_storageDir.isEmpty()) {
        return false;
    }

    closeCsvFile();

    const QString fileName = QStringLiteral("samples_%1.csv")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_csvPath = QDir(m_storageDir).filePath(fileName);
    m_csvFile.setFileName(m_csvPath);

    if (!m_csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&m_csvFile);
    if (m_acqMode == QStringLiteral("SCAN8")) {
        out << QStringLiteral("timestamp,ch0_voltage,ch1_voltage,ch2_voltage,ch3_voltage,ch4_voltage,ch5_voltage,ch6_voltage,ch7_voltage") << Qt::endl;
    } else {
        out << QStringLiteral("timestamp,voltage") << Qt::endl;
    }
    if (!m_csvFile.flush()) {
        closeCsvFile();
        return false;
    }

    m_csvFlushCounter = 0;
    m_csvWriteHealthy = true;
    return true;
}

void ADS1256Controller::closeCsvFile()
{
    if (m_csvFile.isOpen()) {
        if (!m_csvFile.flush() && m_csvWriteHealthy) {
            appendLog(QStringLiteral("error"), QStringLiteral("关闭前刷新 CSV 失败: %1").arg(m_csvPath), false);
            CrashLogger::log(QStringLiteral("CSV final flush failed: %1").arg(m_csvPath));
            m_csvWriteHealthy = false;
        }
        m_csvFile.close();
    }
}

bool ADS1256Controller::appendSampleToCsv(const SampleEntry &sample, qint64 sampleTimestampNs)
{
    if (!m_csvFile.isOpen()) {
        return false;
    }

    const qint64 timestampMs = qMax<qint64>(0, sampleTimestampNs / 1'000'000);
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(timestampMs)
                                  .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString line = QStringLiteral("%1,%2\n")
                             .arg(timestamp)
                             .arg(static_cast<double>(sample.voltage), 0, 'f', 9);

    if (m_csvFile.write(line.toUtf8()) < 0) {
        if (m_csvWriteHealthy) {
            appendLog(QStringLiteral("error"), QStringLiteral("写入 CSV 失败: %1").arg(m_csvPath), false);
            CrashLogger::log(QStringLiteral("CSV write failed: %1").arg(m_csvPath));
        }
        m_csvWriteHealthy = false;
        return false;
    }

    ++m_csvFlushCounter;
    if (m_csvFlushCounter >= kCsvFlushInterval) {
        if (!m_csvFile.flush() && m_csvWriteHealthy) {
            appendLog(QStringLiteral("error"), QStringLiteral("刷新 CSV 失败: %1").arg(m_csvPath), false);
            CrashLogger::log(QStringLiteral("CSV flush failed: %1").arg(m_csvPath));
            m_csvWriteHealthy = false;
            return false;
        }
        m_csvFlushCounter = 0;
    }
    return true;
}

bool ADS1256Controller::appendScanFrameToCsv(const QVector<double> &voltages, qint64 sampleTimestampNs)
{
    if (!m_csvFile.isOpen() || voltages.size() < 8) {
        return false;
    }

    const qint64 timestampMs = qMax<qint64>(0, sampleTimestampNs / 1'000'000);
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(timestampMs)
                                  .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    const QString line = QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                             .arg(timestamp)
                             .arg(voltages.at(0), 0, 'f', 9)
                             .arg(voltages.at(1), 0, 'f', 9)
                             .arg(voltages.at(2), 0, 'f', 9)
                             .arg(voltages.at(3), 0, 'f', 9)
                             .arg(voltages.at(4), 0, 'f', 9)
                             .arg(voltages.at(5), 0, 'f', 9)
                             .arg(voltages.at(6), 0, 'f', 9)
                             .arg(voltages.at(7), 0, 'f', 9);

    if (m_csvFile.write(line.toUtf8()) < 0) {
        if (m_csvWriteHealthy) {
            appendLog(QStringLiteral("error"), QStringLiteral("写入 CSV 失败: %1").arg(m_csvPath), false);
            CrashLogger::log(QStringLiteral("CSV write failed: %1").arg(m_csvPath));
        }
        m_csvWriteHealthy = false;
        return false;
    }

    ++m_csvFlushCounter;
    if (m_csvFlushCounter >= kCsvFlushInterval) {
        if (!m_csvFile.flush() && m_csvWriteHealthy) {
            appendLog(QStringLiteral("error"), QStringLiteral("刷新 CSV 失败: %1").arg(m_csvPath), false);
            CrashLogger::log(QStringLiteral("CSV flush failed: %1").arg(m_csvPath));
            m_csvWriteHealthy = false;
            return false;
        }
        m_csvFlushCounter = 0;
    }
    return true;
}

void ADS1256Controller::enforcePendingSampleLimit()
{
    if (m_pendingSamples.size() <= kMaxPendingSamples) {
        return;
    }

    const int dropCount = m_pendingSamples.size() - kMaxPendingSamples;
    m_pendingSamples.remove(0, dropCount);

    if (!m_sqlOverflowWarned) {
        const QString message = QStringLiteral("SQL 写入持续失败，已丢弃最旧缓冲 %1 条以防内存膨胀").arg(dropCount);
        appendLog(QStringLiteral("error"), message, false);
        CrashLogger::log(message);
        m_sqlOverflowWarned = true;
    }
}

void ADS1256Controller::trimPlotBuffers()
{
    while (static_cast<int>(m_tSingle.size()) > m_plotBufferMax) {
        compactSingleOverview();
    }

    while (static_cast<int>(m_tScan.size()) > m_plotBufferMax) {
        compactScanOverview();
    }
}

void ADS1256Controller::appendSingleOverview(double t, double rawVoltage, double filteredVoltage)
{
    ++m_singleSampleSeq;
    const qint64 step = qMax(1, m_singleOverviewStep);
    if (!m_tSingle.empty() && (m_singleSampleSeq % step) != 0) {
        return;
    }

    m_tSingle.push_back(t);
    m_vSingleRaw.push_back(rawVoltage);
    m_vSingle.push_back(filteredVoltage);
}

void ADS1256Controller::appendScanOverview(double t, const QVector<double> &values)
{
    if (values.size() < 8) {
        return;
    }

    ++m_scanFrameSeq;
    const qint64 step = qMax(1, m_scanOverviewStep);
    if (!m_tScan.empty() && (m_scanFrameSeq % step) != 0) {
        return;
    }

    m_tScan.push_back(t);
    for (int ch = 0; ch < 8; ++ch) {
        m_vScan[ch].push_back(values.at(ch));
    }
}

void ADS1256Controller::compactSingleOverview()
{
    const int count = qMin(
        static_cast<int>(m_tSingle.size()),
        qMin(static_cast<int>(m_vSingleRaw.size()), static_cast<int>(m_vSingle.size())));
    if (count <= 1) {
        return;
    }

    std::deque<double> t;
    std::deque<double> raw;
    std::deque<double> filtered;
    for (int i = 0; i < count; i += 2) {
        t.push_back(m_tSingle[i]);
        raw.push_back(m_vSingleRaw[i]);
        filtered.push_back(m_vSingle[i]);
    }

    if (((count - 1) % 2) != 0) {
        t.push_back(m_tSingle[count - 1]);
        raw.push_back(m_vSingleRaw[count - 1]);
        filtered.push_back(m_vSingle[count - 1]);
    }

    m_tSingle.swap(t);
    m_vSingleRaw.swap(raw);
    m_vSingle.swap(filtered);
    const qint64 nextStep = static_cast<qint64>(m_singleOverviewStep) * 2;
    m_singleOverviewStep = static_cast<int>(qMin(nextStep, static_cast<qint64>(std::numeric_limits<int>::max())));
}

void ADS1256Controller::compactScanOverview()
{
    int count = static_cast<int>(m_tScan.size());
    for (int ch = 0; ch < 8; ++ch) {
        count = qMin(count, static_cast<int>(m_vScan[ch].size()));
    }
    if (count <= 1) {
        return;
    }

    std::deque<double> t;
    QVector<std::deque<double>> values(8);
    for (int i = 0; i < count; i += 2) {
        t.push_back(m_tScan[i]);
        for (int ch = 0; ch < 8; ++ch) {
            values[ch].push_back(m_vScan[ch][i]);
        }
    }

    if (((count - 1) % 2) != 0) {
        t.push_back(m_tScan[count - 1]);
        for (int ch = 0; ch < 8; ++ch) {
            values[ch].push_back(m_vScan[ch][count - 1]);
        }
    }

    m_tScan.swap(t);
    m_vScan.swap(values);
    const qint64 nextStep = static_cast<qint64>(m_scanOverviewStep) * 2;
    m_scanOverviewStep = static_cast<int>(qMin(nextStep, static_cast<qint64>(std::numeric_limits<int>::max())));
}

void ADS1256Controller::clearZoomCache()
{
    m_zoomCacheReady = false;
    m_zoomSingleRawPoints.clear();
    m_zoomSinglePoints.clear();
    for (QList<QPointF> &series : m_zoomScanPoints) {
        series.clear();
    }
}

bool ADS1256Controller::rebuildZoomCacheFromCsv()
{
    clearZoomCache();

    if (m_csvPath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(m_csvPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QFile file(m_csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    if (!in.atEnd()) {
        in.readLine();
    }

    const qint64 minNs = qMax<qint64>(0, static_cast<qint64>(std::floor(m_zoomXMin * 1'000'000'000.0)));
    const qint64 maxNs = qMax<qint64>(minNs + 1, static_cast<qint64>(std::ceil(m_zoomXMax * 1'000'000'000.0)));

    qint64 loadedRows = 0;
    qint64 firstTimestampMs = -1;
    bool reachedRange = false;

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(',');
        if (parts.size() >= 9) {
            qint64 elapsedNs = 0;
            if (!parseTimestampElapsedNs(parts.at(0), firstTimestampMs, &elapsedNs)) {
                continue;
            }

            if (elapsedNs < minNs) {
                continue;
            }
            if (elapsedNs > maxNs) {
                if (reachedRange) {
                    break;
                }
                continue;
            }

            reachedRange = true;

            const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;
            bool hasAnyVoltage = false;
            for (int ch = 0; ch < 8; ++ch) {
                bool okVoltage = false;
                const double voltage = parts.at(ch + 1).toDouble(&okVoltage);
                if (!okVoltage) {
                    continue;
                }
                m_zoomScanPoints[ch].push_back(QPointF(t, voltage));
                hasAnyVoltage = true;
            }

            if (hasAnyVoltage) {
                ++loadedRows;
            }
            if (loadedRows >= kZoomCsvMaxRows) {
                break;
            }
            continue;
        }

        CsvParsedRow row;
        if (!parseCsvRow(line, firstTimestampMs, &row)) {
            continue;
        }

        const qint64 elapsedNs = row.elapsedNs;

        if (elapsedNs < minNs) {
            continue;
        }
        if (elapsedNs > maxNs) {
            if (reachedRange) {
                break;
            }
            continue;
        }

        reachedRange = true;

        const double voltage = row.voltage;

        const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;
        if (row.mode == SampleMode::Single) {
            const QPointF point(t, voltage);
            m_zoomSingleRawPoints.push_back(point);
            m_zoomSinglePoints.push_back(point);
            ++loadedRows;
        } else if (row.mode == SampleMode::Scan8) {
            const int ch = qBound(0, row.channel, 7);
            m_zoomScanPoints[ch].push_back(QPointF(t, voltage));
            ++loadedRows;
        }

        if (loadedRows >= kZoomCsvMaxRows) {
            break;
        }
    }

    if (!m_zoomSinglePoints.isEmpty()) {
        m_zoomCacheReady = true;
        return true;
    }

    for (const QList<QPointF> &series : std::as_const(m_zoomScanPoints)) {
        if (!series.isEmpty()) {
            m_zoomCacheReady = true;
            return true;
        }
    }

    return false;
}

void ADS1256Controller::appendLog(const QString &level, const QString &message, bool persist)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_logModel.appendEntry(LogEntry { timestamp, level, message });
    if (persist) {
        m_logDatabase.insertLog(level, message);
    }
}

QString ADS1256Controller::normalizeDbPath(const QString &filePathOrUrl) const
{
    const QString text = filePathOrUrl.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    if (text.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        const QUrl url(text);
        if (url.isValid() && url.isLocalFile()) {
            return QDir::cleanPath(url.toLocalFile());
        }
    }

    return QDir::cleanPath(text);
}

bool ADS1256Controller::rebuildPlotDataFromCsv(const QString &csvPath, QString *errorMessage)
{
    resetStats();

    ElapsedTimelineNormalizer timeline;

    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream in(&file);
    if (!in.atEnd()) {
        in.readLine();
    }

    std::array<double, 8> lastScanValues {};
    std::array<bool, 8> hasLastScanValues {};
    std::array<double, 8> frameValues {};
    std::array<bool, 8> framePresent {};
    bool frameActive = false;
    qint64 frameElapsedNs = 0;

    bool hasSingle = false;
    bool hasScan = false;
    bool hasLatest = false;
    bool latestHasAdc = false;
    int latestAdc = 0;
    int latestChannel = 0;
    double latestVoltage = 0.0;
    qint64 firstTimestampMs = -1;

    auto flushScanFrame = [&]() {
        if (!frameActive) {
            return;
        }

        const double t = static_cast<double>(frameElapsedNs) / 1'000'000'000.0;
        QVector<double> frameVoltages(8, 0.0);
        for (int ch = 0; ch < 8; ++ch) {
            const bool hasCurrent = framePresent[ch];
            const bool hasFallback = hasLastScanValues[ch];
            const double value = hasCurrent
                ? frameValues[ch]
                : (hasFallback ? lastScanValues[ch] : 0.0);
            frameVoltages[ch] = value;
            if (hasCurrent) {
                lastScanValues[ch] = frameValues[ch];
                hasLastScanValues[ch] = true;
            }
        }

        appendScanOverview(t, frameVoltages);
        m_latestElapsedSeconds = qMax(m_latestElapsedSeconds, t);
        frameActive = false;
        frameElapsedNs = 0;
        framePresent.fill(false);
    };

    qint64 parsedRows = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(',');
        if (parts.size() >= 9) {
            qint64 elapsedNs = 0;
            if (!parseTimestampElapsedNs(parts.at(0), firstTimestampMs, &elapsedNs)) {
                continue;
            }

            QVector<double> frameVoltages;
            frameVoltages.reserve(8);
            bool allOk = true;
            for (int ch = 0; ch < 8; ++ch) {
                bool okVoltage = false;
                const double voltage = parts.at(ch + 1).toDouble(&okVoltage);
                if (!okVoltage) {
                    allOk = false;
                    break;
                }
                frameVoltages.push_back(voltage);
            }
            if (!allOk || frameVoltages.size() != 8) {
                continue;
            }

            const qint64 normalizedElapsedNs = timeline.normalize(elapsedNs);
            flushScanFrame();

            const double t = static_cast<double>(normalizedElapsedNs) / 1'000'000'000.0;
            appendScanOverview(t, frameVoltages);
            m_latestElapsedSeconds = qMax(m_latestElapsedSeconds, t);
            for (int ch = 0; ch < 8; ++ch) {
                m_latestScanAdc[static_cast<size_t>(ch)] = 0;
                m_latestScanVoltage[static_cast<size_t>(ch)] = frameVoltages.at(ch);
            }
            m_hasLatestScanFrame = true;

            hasScan = true;
            latestChannel = qBound(0, m_viewChannel, 7);
            latestVoltage = frameVoltages.at(latestChannel);
            latestAdc = 0;
            latestHasAdc = false;
            hasLatest = true;
            ++parsedRows;

            if ((parsedRows % 2048) == 0) {
                trimPlotBuffers();
            }
            continue;
        }

        CsvParsedRow row;
        if (!parseCsvRow(line, firstTimestampMs, &row)) {
            continue;
        }

        const qint64 normalizedElapsedNs = timeline.normalize(row.elapsedNs);

        const int channel = qBound(0, row.channel, 7);
        const double voltage = row.voltage;
        const int adc = row.adc;

        const double t = static_cast<double>(normalizedElapsedNs) / 1'000'000'000.0;
        if (row.mode == SampleMode::Scan8) {
            hasScan = true;
            if (!frameActive || normalizedElapsedNs != frameElapsedNs) {
                flushScanFrame();
                frameActive = true;
                frameElapsedNs = normalizedElapsedNs;
                framePresent.fill(false);
            }
            frameValues[channel] = voltage;
            framePresent[channel] = true;
            m_latestScanAdc[static_cast<size_t>(channel)] = adc;
            m_latestScanVoltage[static_cast<size_t>(channel)] = voltage;
            m_hasLatestScanFrame = true;
        } else {
            hasSingle = true;
            flushScanFrame();
            appendSingleOverview(t, voltage, voltage);
            m_latestElapsedSeconds = qMax(m_latestElapsedSeconds, t);
        }

        latestAdc = adc;
        latestChannel = channel;
        latestVoltage = voltage;
        latestHasAdc = row.hasAdc;
        hasLatest = true;
        ++parsedRows;

        if ((parsedRows % 2048) == 0) {
            trimPlotBuffers();
        }
    }

    flushScanFrame();
    trimPlotBuffers();

    if (parsedRows <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CSV 中没有可用采样行");
        }
        clearSeries();
        return false;
    }

    m_sampleCount = parsedRows;
    if (hasLatest) {
        if (hasScan && !hasSingle) {
            m_latestAdText = latestHasAdc
                ? QStringLiteral("CH%1:%2").arg(latestChannel).arg(latestAdc)
                : QStringLiteral("CH%1").arg(latestChannel);
        } else {
            m_latestAdText = latestHasAdc ? QString::number(latestAdc) : QStringLiteral("--");
        }
        m_latestVoltageText = QStringLiteral("%1 V").arg(latestVoltage, 0, 'f', 6);
        m_latestHexText = latestHasAdc
            ? QStringLiteral("0x%1")
                  .arg(latestAdc & 0x00FFFFFF, 6, 16, QLatin1Char('0'))
                  .toUpper()
            : QStringLiteral("--");
    }

    if (hasScan && !hasSingle && m_acqMode != QStringLiteral("SCAN8")) {
        m_acqMode = QStringLiteral("SCAN8");
        emit configChanged();
    }
    if (hasSingle && !hasScan && m_acqMode != QStringLiteral("SINGLE")) {
        m_acqMode = QStringLiteral("SINGLE");
        emit configChanged();
    }

    emit metricChanged();
    updatePlot(true);
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void ADS1256Controller::rebuildPlotDataFromSamples(const QVector<SampleEntry> &samples)
{
    resetStats();

    ElapsedTimelineNormalizer timeline;

    std::array<double, 8> lastScanValues {};
    std::array<bool, 8> hasLastScanValues {};
    std::array<double, 8> frameValues {};
    std::array<bool, 8> framePresent {};

    bool frameActive = false;
    qint64 frameElapsedNs = 0;

    auto flushScanFrame = [&]() {
        if (!frameActive) {
            return;
        }

        const double frameTimeSeconds = static_cast<double>(frameElapsedNs) / 1'000'000'000.0;
        QVector<double> frameVoltages(8, 0.0);
        for (int ch = 0; ch < 8; ++ch) {
            const bool hasCurrent = framePresent[ch];
            const bool hasFallback = hasLastScanValues[ch];
            const double value = hasCurrent
                ? frameValues[ch]
                : (hasFallback ? lastScanValues[ch] : 0.0);
            frameVoltages[ch] = value;
            if (hasCurrent) {
                lastScanValues[ch] = frameValues[ch];
                hasLastScanValues[ch] = true;
            }
        }

        appendScanOverview(frameTimeSeconds, frameVoltages);

        frameActive = false;
        frameElapsedNs = 0;
        framePresent.fill(false);
    };

    int latestAdc = 0;
    double latestVoltage = 0.0;
    int latestChannel = 0;
    bool hasLatest = false;

    for (const SampleEntry &sample : samples) {
        const qint64 normalizedElapsedNs = timeline.normalize(sample.elapsedNs);
        const int channel = qBound(0, static_cast<int>(sample.channel), 7);
        if (sample.mode == SampleMode::Scan8) {
            if (!frameActive || normalizedElapsedNs != frameElapsedNs) {
                flushScanFrame();
                frameActive = true;
                frameElapsedNs = normalizedElapsedNs;
                framePresent.fill(false);
            }

            frameValues[channel] = static_cast<double>(sample.voltage);
            framePresent[channel] = true;
            m_latestScanAdc[static_cast<size_t>(channel)] = sample.adc;
            m_latestScanVoltage[static_cast<size_t>(channel)] = static_cast<double>(sample.voltage);
            m_hasLatestScanFrame = true;
        } else {
            flushScanFrame();

            const double t = static_cast<double>(normalizedElapsedNs) / 1'000'000'000.0;
            const double v = static_cast<double>(sample.voltage);
            appendSingleOverview(t, v, v);
        }

        m_latestElapsedSeconds = qMax(
            m_latestElapsedSeconds,
            static_cast<double>(normalizedElapsedNs) / 1'000'000'000.0);

        latestAdc = sample.adc;
        latestVoltage = static_cast<double>(sample.voltage);
        latestChannel = channel;
        hasLatest = true;
    }

    flushScanFrame();

    m_sampleCount = samples.size();
    if (hasLatest) {
        if (!m_tScan.empty()) {
            m_latestAdText = QStringLiteral("CH%1:%2").arg(latestChannel).arg(latestAdc);
        } else {
            m_latestAdText = QString::number(latestAdc);
        }
        m_latestVoltageText = QStringLiteral("%1 V").arg(latestVoltage, 0, 'f', 6);
        m_latestHexText = QStringLiteral("0x%1").arg(latestAdc & 0x00FFFFFF, 6, 16, QLatin1Char('0')).toUpper();
    }

    if (!m_tScan.empty() && m_tSingle.empty() && m_acqMode != QStringLiteral("SCAN8")) {
        m_acqMode = QStringLiteral("SCAN8");
        emit configChanged();
    }
    if (!m_tSingle.empty() && m_tScan.empty() && m_acqMode != QStringLiteral("SINGLE")) {
        m_acqMode = QStringLiteral("SINGLE");
        emit configChanged();
    }

    emit metricChanged();
    trimPlotBuffers();
    updatePlot(true);
}

void ADS1256Controller::resetStats()
{
    // 在首个有效样本到达时再锁定起点，保证每次开始采样首点时间为0。
    m_startNs = 0;
    m_lastPlotUpdateNs = 0;
    m_latestElapsedSeconds = 0.0;
    m_sampleCount = 0;
    m_latestAdText = QStringLiteral("--");
    m_latestVoltageText = QStringLiteral("-- V");
    m_latestHexText = QStringLiteral("--");
    m_hasLatestScanFrame = false;
    m_latestScanAdc.fill(0);
    m_latestScanVoltage.fill(0.0);

    m_tSingle.clear();
    m_vSingle.clear();
    m_vSingleRaw.clear();
    m_tScan.clear();
    for (std::deque<double> &vec : m_vScan) {
        vec.clear();
    }

    m_zoomActive = false;
    m_singleOverviewStep = 1;
    m_scanOverviewStep = 1;
    m_singleSampleSeq = 0;
    m_scanFrameSeq = 0;
    clearZoomCache();

    m_pendingSamples.clear();
    if (m_pendingSamples.capacity() < kSqlBufferFlushCount * 2) {
        m_pendingSamples.reserve(kSqlBufferFlushCount * 2);
    }
    m_sqlFlushFailureStreak = 0;
    m_sqlOverflowWarned = false;

    resetEkfFilters(false);
    emit metricChanged();
    emit axisRangeChanged();
    updatePlot(true);
}

bool ADS1256Controller::persistPendingSamples()
{
    if (!flushSampleSqlBuffer(true)) {
        return false;
    }

    if (m_sampleWriter && m_sampleWriterThread && m_sampleWriterThread->isRunning()) {
        bool flushed = false;
        QMetaObject::invokeMethod(
            m_sampleWriter,
            "flushPendingSync",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, flushed));
        if (!flushed) {
            appendLog(QStringLiteral("error"), QStringLiteral("异步 SQL 队列未能完全落盘"), false);
            return false;
        }
    }

    return true;
}

bool ADS1256Controller::flushSampleSqlBuffer(bool logError)
{
    if (m_pendingSamples.isEmpty()) {
        m_sqlFlushFailureStreak = 0;
        m_sqlOverflowWarned = false;
        return true;
    }

    if (!m_sampleWriter || !m_sampleWriterThread || !m_sampleWriterThread->isRunning()) {
        if (!m_logDatabase.isReady()) {
            ++m_sqlFlushFailureStreak;
            enforcePendingSampleLimit();

            if (logError || m_sqlFlushFailureStreak == 1 || (m_sqlFlushFailureStreak % 20) == 0) {
                appendLog(QStringLiteral("error"), QStringLiteral("数据库未就绪，无法保存采样记录"), false);
            }
            return false;
        }

        if (!m_logDatabase.insertSamples(m_pendingSamples)) {
            ++m_sqlFlushFailureStreak;
            enforcePendingSampleLimit();

            if (logError || m_sqlFlushFailureStreak == 1 || (m_sqlFlushFailureStreak % 20) == 0) {
                appendLog(QStringLiteral("error"), QStringLiteral("写入 SQL 采样缓冲失败"), false);
            }
            return false;
        }

        m_pendingSamples.clear();
        m_sqlFlushFailureStreak = 0;
        m_sqlOverflowWarned = false;
        return true;
    }

    const QVector<SampleEntry> batch = m_pendingSamples;
    m_pendingSamples.clear();

    const bool invokeOk = QMetaObject::invokeMethod(
        m_sampleWriter,
        "enqueueSamples",
        Qt::QueuedConnection,
        Q_ARG(QVector<SampleEntry>, batch));

    if (!invokeOk) {
        ++m_sqlFlushFailureStreak;
        if (logError || m_sqlFlushFailureStreak == 1 || (m_sqlFlushFailureStreak % 20) == 0) {
            appendLog(QStringLiteral("error"), QStringLiteral("提交异步 SQL 写入队列失败"), false);
        }
        m_pendingSamples = batch;
        enforcePendingSampleLimit();
        return false;
    }

    m_sqlFlushFailureStreak = 0;
    m_sqlOverflowWarned = false;
    return true;
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
    const qint64 elapsedNs = elapsedSinceFirstSample(nowNs, m_startNs);
    const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;
    m_latestElapsedSeconds = qMax(m_latestElapsedSeconds, t);

    const double rawVoltage = adcToVoltage(adc);
    const int channel = singleChannelIndex();
    const double filtered = applyEkf(channel, rawVoltage);

    appendSingleOverview(t, rawVoltage, filtered);

    m_latestAdText = QString::number(adc);
    m_latestVoltageText = QStringLiteral("%1 V").arg(filtered, 0, 'f', 6);
    m_latestHexText = hexValue;
    emit metricChanged();

    const SampleEntry sample {
        elapsedNs,
        SampleMode::Single,
        static_cast<quint8>(qBound(0, channel, 7)),
        static_cast<float>(filtered),
        static_cast<qint32>(adc),
    };

    m_pendingSamples.push_back(sample);
    enforcePendingSampleLimit();
    appendSampleToCsv(sample, nowNs);

    if (m_pendingSamples.size() >= kSqlBufferFlushCount) {
        flushSampleSqlBuffer(false);
    }

    trimPlotBuffers();
    updatePlot(false);
}

void ADS1256Controller::recordScan8(const QVector<int> &values)
{
    ++m_sampleCount;

    const qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1'000'000;
    const qint64 elapsedNs = elapsedSinceFirstSample(nowNs, m_startNs);
    const double t = static_cast<double>(elapsedNs) / 1'000'000'000.0;
    m_latestElapsedSeconds = qMax(m_latestElapsedSeconds, t);

    QVector<double> voltages;
    voltages.reserve(8);
    for (int ch = 0; ch < 8; ++ch) {
        const double raw = adcToVoltage(values.at(ch));
        const double filtered = applyEkf(ch, raw);
        voltages.push_back(filtered);
        m_latestScanAdc[static_cast<size_t>(ch)] = values.at(ch);
        m_latestScanVoltage[static_cast<size_t>(ch)] = filtered;

        const SampleEntry sample {
            elapsedNs,
            SampleMode::Scan8,
            static_cast<quint8>(ch),
            static_cast<float>(filtered),
            static_cast<qint32>(values.at(ch)),
        };

        m_pendingSamples.push_back(sample);
    }

    m_hasLatestScanFrame = true;

    appendScanFrameToCsv(voltages, nowNs);
    appendScanOverview(t, voltages);
    enforcePendingSampleLimit();

    if (m_pendingSamples.size() >= kSqlBufferFlushCount) {
        flushSampleSqlBuffer(false);
    }

    const int channel = qBound(0, m_viewChannel, 7);
    m_latestAdText = QStringLiteral("CH%1:%2").arg(channel).arg(values.at(channel));
    m_latestVoltageText = QStringLiteral("%1 V").arg(voltages.at(channel), 0, 'f', 6);
    m_latestHexText = QStringLiteral("0x%1").arg(values.at(channel) & 0x00FFFFFF, 6, 16, QLatin1Char('0')).toUpper();
    emit metricChanged();

    trimPlotBuffers();
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
    appendLog(QStringLiteral("tx"), QStringLiteral("[TX] %1").arg(cmd), false);
}

QString ADS1256Controller::buildCfgCommand() const
{
    const QString mode = (m_acqMode == QStringLiteral("SCAN8")) ? QStringLiteral("RDATA") : QStringLiteral("RDATAC");
    return QStringLiteral("CFG PSEL=%1 NSEL=%2 PGA=%3 DRATE=0x%4 MODE=%5 ACQ=%6 CHMASK=0x%7")
        .arg(m_psel)
        .arg(m_nsel)
        .arg(m_pga)
        .arg(m_drate, 2, 16, QLatin1Char('0'))
        .arg(mode)
        .arg(m_acqMode)
        .arg(static_cast<int>(m_scanSampleMask), 2, 16, QLatin1Char('0'))
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
        bool hasZoomScanData = false;
        if (m_zoomActive && m_zoomCacheReady) {
            for (const QList<QPointF> &series : std::as_const(m_zoomScanPoints)) {
                if (!series.isEmpty()) {
                    hasZoomScanData = true;
                    break;
                }
            }
        }

        if (m_tScan.empty() && !hasZoomScanData) {
            clearSeries();
            return;
        }

        double xMin = 0.0;
        double xMax = 1.0;
        if (hasZoomScanData) {
            xMin = qMax(0.0, m_zoomXMin);
            xMax = qMax(xMin + 0.05, m_zoomXMax);
        } else {
            const auto [defaultXMin, defaultXMax] = xRange(m_tScan);
            xMin = defaultXMin;
            xMax = defaultXMax;
            if (m_zoomActive) {
                const double dataRight = qMax(defaultXMax, qMax(0.05, m_latestElapsedSeconds));
                xMin = qBound(0.0, m_zoomXMin, dataRight);
                xMax = qBound(xMin + 0.05, m_zoomXMax, qMax(dataRight, xMin + 0.05));
            }
        }

        const bool useHourAxis = shouldUseHourAxis(xMax);
        const double xScale = axisDisplayScale(useHourAxis);
        const bool axisUnitChanged = (m_axisXInHours != useHourAxis);
        m_axisXInHours = useHourAxis;
        int visibleScanChannels = 0;
        for (int ch = 0; ch < 8; ++ch) {
            if (m_scanSeries[ch] && m_scanVisible[ch]) {
                ++visibleScanChannels;
            }
        }

        const int scanTargetPoints = qBound(
            kRenderTargetPointsScanMin,
            kRenderTargetPointsScanBudget / qMax(1, visibleScanChannels),
            kRenderTargetPointsSingle);

        QVector<double> yVisible;
        if (!hasZoomScanData) {
            yVisible.reserve(scanTargetPoints * qMax(1, visibleScanChannels));
        }

        for (int ch = 0; ch < 8; ++ch) {
            if (!m_scanSeries[ch]) {
                continue;
            }

            m_scanSeries[ch]->setVisible(m_scanVisible[ch]);
            if (!m_scanVisible[ch]) {
                m_scanSeries[ch]->clear();
                continue;
            }

            QList<QPointF> points;
            if (hasZoomScanData) {
                points = scalePointsX(m_zoomScanPoints[ch], xScale);
            } else {
                points = scalePointsX(
                    pickPlotPoints(m_tScan, m_vScan[ch], xMin, xMax, m_zoomActive, scanTargetPoints),
                    xScale);
            }
            m_scanSeries[ch]->replace(points);
            if (hasVisibleDataPoints(points)) {
                const QVector<double> yPart = extractY(points);
                yVisible += yPart;
            }
        }

        if (m_singleShadowSeries) {
            m_singleShadowSeries->clear();
        }
        if (m_singleSeries) {
            m_singleSeries->clear();
        }

        const auto [yMin, yMax] = m_zoomActive
            ? std::pair<double, double> { m_zoomYMin, m_zoomYMax }
            : yBoundsFromValues(yVisible);
        updateAxisRange(xMin / xScale, xMax / xScale, yMin, yMax, axisUnitChanged);
        return;
    }

    const bool hasZoomSingleData = m_zoomActive
        && m_zoomCacheReady
        && !m_zoomSinglePoints.isEmpty();

    if (m_tSingle.empty() && !hasZoomSingleData) {
        clearSeries();
        return;
    }

    for (int ch = 0; ch < 8; ++ch) {
        if (m_scanSeries[ch]) {
            m_scanSeries[ch]->clear();
        }
    }

    double xMin = 0.0;
    double xMax = 1.0;
    if (hasZoomSingleData) {
        xMin = qMax(0.0, m_zoomXMin);
        xMax = qMax(xMin + 0.05, m_zoomXMax);
    } else {
        const auto [defaultXMin, defaultXMax] = xRange(m_tSingle);
        xMin = defaultXMin;
        xMax = defaultXMax;
        if (m_zoomActive) {
            const double dataRight = qMax(defaultXMax, qMax(0.05, m_latestElapsedSeconds));
            xMin = qBound(0.0, m_zoomXMin, dataRight);
            xMax = qBound(xMin + 0.05, m_zoomXMax, qMax(dataRight, xMin + 0.05));
        }
    }

    const bool useHourAxis = shouldUseHourAxis(xMax);
    const double xScale = axisDisplayScale(useHourAxis);
    const bool axisUnitChanged = (m_axisXInHours != useHourAxis);
    m_axisXInHours = useHourAxis;

    QList<QPointF> rawPoints;
    QList<QPointF> filtPoints;
    if (hasZoomSingleData) {
        rawPoints = scalePointsX(
            m_zoomSingleRawPoints.isEmpty() ? m_zoomSinglePoints : m_zoomSingleRawPoints,
            xScale);
        filtPoints = scalePointsX(m_zoomSinglePoints, xScale);
    } else {
        rawPoints = scalePointsX(
            pickPlotPoints(m_tSingle, m_vSingleRaw, xMin, xMax, m_zoomActive),
            xScale);
        filtPoints = scalePointsX(
            pickPlotPoints(m_tSingle, m_vSingle, xMin, xMax, m_zoomActive),
            xScale);
    }

    const QVector<double> yVisible = extractY(filtPoints);

    if (m_singleShadowSeries) {
        m_singleShadowSeries->replace(rawPoints);
    }
    if (m_singleSeries) {
        m_singleSeries->replace(filtPoints);
    }

    const auto [yMin, yMax] = m_zoomActive
        ? std::pair<double, double> { m_zoomYMin, m_zoomYMax }
        : yBoundsFromValues(yVisible);
    updateAxisRange(xMin / xScale, xMax / xScale, yMin, yMax, axisUnitChanged);
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
    const bool unitChanged = m_axisXInHours;
    m_axisXInHours = false;
    updateAxisRange(0.0, 1.0, -0.01, 0.01, unitChanged);
}

void ADS1256Controller::updateAxisRange(double xMin, double xMax, double yMin, double yMax, bool forceNotify)
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

    if (!changed && !forceNotify) {
        return;
    }

    if (changed) {
        m_axisXMin = xMin;
        m_axisXMax = xMax;
        m_axisYMin = yMin;
        m_axisYMax = yMax;
    }
    emit axisRangeChanged();
}

std::pair<double, double> ADS1256Controller::xRange(const std::deque<double> &x) const
{
    if (x.empty()) {
        return { 0.0, 1.0 };
    }

    const double latest = qMax(
        inferredLatestX(x),
        qMax(0.0, m_latestElapsedSeconds));

    if (m_xAxisMode == QStringLiteral("FULL")) {
        return { 0.0, qMax(1.0, latest / kLatestDataXAxisRatio) };
    }

    const double span = qMax(0.05, m_windowSeconds);
    // WINDOW 模式采用滑动窗口：让最新点稳定落在横轴 70% 位置。
    // 当采样初期数据不足时退化为 [0, span]。
    double xMin = latest - (span * kLatestDataXAxisRatio);
    double xMax = xMin + span;
    if (xMin < 0.0) {
        xMin = 0.0;
        xMax = span;
    }
    return { xMin, xMax };
}

void ADS1256Controller::onReadyRead()
{
    m_rxBuffer.append(m_serial->readAll());

    if (m_rxBuffer.size() > kMaxRxBufferBytes) {
        const int overflowBytes = m_rxBuffer.size() - kMaxRxBufferBytes;
        int removeCount = overflowBytes;
        const int alignNewline = m_rxBuffer.indexOf('\n', overflowBytes);
        if (alignNewline >= 0) {
            removeCount = alignNewline + 1;
        }

        if (removeCount > 0) {
            m_rxBuffer.remove(0, removeCount);
        }

        if (!m_rxOverflowWarned) {
            const QString message = QStringLiteral("串口接收缓冲超过上限，已丢弃旧数据以防内存膨胀");
            appendLog(QStringLiteral("error"), message, false);
            CrashLogger::log(message);
            m_rxOverflowWarned = true;
        }
    } else if (m_rxOverflowWarned && m_rxBuffer.size() < (kMaxRxBufferBytes / 2)) {
        m_rxOverflowWarned = false;
    }

    int newlineIndex = m_rxBuffer.indexOf('\n');
    while (newlineIndex >= 0) {
        QByteArray lineData = m_rxBuffer.left(newlineIndex);
        m_rxBuffer.remove(0, newlineIndex + 1);

        if (lineData.size() > kMaxSerialLineBytes) {
            if (!m_rxLineTooLongWarned) {
                const QString message = QStringLiteral("检测到超长串口行，已丢弃以保护解析与内存");
                appendLog(QStringLiteral("error"), message, false);
                CrashLogger::log(message);
                m_rxLineTooLongWarned = true;
            }
            newlineIndex = m_rxBuffer.indexOf('\n');
            continue;
        }

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
