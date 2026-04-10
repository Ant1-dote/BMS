#pragma once

#include "logdatabase.h"
#include "loglistmodel.h"
#include "scalarekf.h"

#include <QFile>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <array>
#include <deque>
#include <utility>

#include <QtCharts/QXYSeries>
#include <QtSerialPort/qserialport.h>

class QThread;
class SampleSqlWriter;

class ADS1256Controller : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool acquisitionEnabled READ acquisitionEnabled NOTIFY acquisitionEnabledChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

    Q_PROPERTY(QString latestAdText READ latestAdText NOTIFY metricChanged)
    Q_PROPERTY(QString latestVoltageText READ latestVoltageText NOTIFY metricChanged)
    Q_PROPERTY(QString latestHexText READ latestHexText NOTIFY metricChanged)
    Q_PROPERTY(qlonglong sampleCount READ sampleCount NOTIFY metricChanged)

    Q_PROPERTY(double vref READ vref WRITE setVref NOTIFY configChanged)
    Q_PROPERTY(int pga READ pga WRITE setPga NOTIFY configChanged)
    Q_PROPERTY(QString psel READ psel WRITE setPsel NOTIFY configChanged)
    Q_PROPERTY(QString nsel READ nsel WRITE setNsel NOTIFY configChanged)
    Q_PROPERTY(int drate READ drate WRITE setDrate NOTIFY configChanged)
    Q_PROPERTY(QString acqMode READ acqMode WRITE setAcqMode NOTIFY configChanged)
    Q_PROPERTY(int viewChannel READ viewChannel WRITE setViewChannel NOTIFY configChanged)
    Q_PROPERTY(QString xAxisMode READ xAxisMode WRITE setXAxisMode NOTIFY configChanged)
    Q_PROPERTY(double windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY configChanged)

    Q_PROPERTY(bool ekfEnabled READ ekfEnabled WRITE setEkfEnabled NOTIFY configChanged)
    Q_PROPERTY(double ekfQ READ ekfQ WRITE setEkfQ NOTIFY configChanged)
    Q_PROPERTY(double ekfR READ ekfR WRITE setEkfR NOTIFY configChanged)
    Q_PROPERTY(double ekfP0 READ ekfP0 WRITE setEkfP0 NOTIFY configChanged)

    Q_PROPERTY(double axisXMin READ axisXMin NOTIFY axisRangeChanged)
    Q_PROPERTY(double axisXMax READ axisXMax NOTIFY axisRangeChanged)
    Q_PROPERTY(bool axisXInHours READ axisXInHours NOTIFY axisRangeChanged)
    Q_PROPERTY(double axisYMin READ axisYMin NOTIFY axisRangeChanged)
    Q_PROPERTY(double axisYMax READ axisYMax NOTIFY axisRangeChanged)
    Q_PROPERTY(bool zoomActive READ zoomActive NOTIFY axisRangeChanged)

    Q_PROPERTY(bool cycleLoopEnabled READ cycleLoopEnabled NOTIFY cycleLoopChanged)
    Q_PROPERTY(QString cyclePhaseText READ cyclePhaseText NOTIFY cycleLoopChanged)
    Q_PROPERTY(int cycleCompletedCount READ cycleCompletedCount NOTIFY cycleLoopChanged)
    Q_PROPERTY(bool cycleOverlayVisible READ cycleOverlayVisible NOTIFY cycleLoopChanged)
    Q_PROPERTY(double cycleDischargeEndVoltage READ cycleDischargeEndVoltage WRITE setCycleDischargeEndVoltage NOTIFY cycleConfigChanged)
    Q_PROPERTY(double cycleChargeEndVoltage READ cycleChargeEndVoltage WRITE setCycleChargeEndVoltage NOTIFY cycleConfigChanged)
    Q_PROPERTY(int cycleConfirmSamples READ cycleConfirmSamples WRITE setCycleConfirmSamples NOTIFY cycleConfigChanged)
    Q_PROPERTY(int cycleMaxCount READ cycleMaxCount WRITE setCycleMaxCount NOTIFY cycleConfigChanged)

    Q_PROPERTY(QAbstractListModel *logModel READ logModel CONSTANT)

public:
    explicit ADS1256Controller(QObject *parent = nullptr);
    ~ADS1256Controller() override;

    QStringList availablePorts() const;
    bool connected() const;
    bool acquisitionEnabled() const;
    QString statusText() const;

    QString latestAdText() const;
    QString latestVoltageText() const;
    QString latestHexText() const;
    qlonglong sampleCount() const;

    double vref() const;
    int pga() const;
    QString psel() const;
    QString nsel() const;
    int drate() const;
    QString acqMode() const;
    int viewChannel() const;
    QString xAxisMode() const;
    double windowSeconds() const;

    bool ekfEnabled() const;
    double ekfQ() const;
    double ekfR() const;
    double ekfP0() const;

    double axisXMin() const;
    double axisXMax() const;
    bool axisXInHours() const;
    double axisYMin() const;
    double axisYMax() const;
    bool zoomActive() const;

    bool cycleLoopEnabled() const;
    QString cyclePhaseText() const;
    int cycleCompletedCount() const;
    bool cycleOverlayVisible() const;
    double cycleDischargeEndVoltage() const;
    double cycleChargeEndVoltage() const;
    int cycleConfirmSamples() const;
    int cycleMaxCount() const;

    QAbstractListModel *logModel();

    void setVref(double value);
    void setPga(int value);
    void setPsel(const QString &value);
    void setNsel(const QString &value);
    void setDrate(int value);
    void setAcqMode(const QString &value);
    void setViewChannel(int value);
    void setXAxisMode(const QString &value);
    void setWindowSeconds(double value);

    void setEkfEnabled(bool value);
    void setEkfQ(double value);
    void setEkfR(double value);
    void setEkfP0(double value);
    void setCycleDischargeEndVoltage(double value);
    void setCycleChargeEndVoltage(double value);
    void setCycleConfirmSamples(int value);
    void setCycleMaxCount(int value);

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE QString portDeviceAt(int index) const;
    Q_INVOKABLE void toggleConnection(const QString &device, int baud);
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void startCaptureWithDirectory(const QString &directoryPathOrUrl);
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void stopCapture(bool saveData);
    Q_INVOKABLE void toggleCapture();

    Q_INVOKABLE void sendCommand(const QString &command);
    Q_INVOKABLE void sendApplyConfig();
    Q_INVOKABLE void sendCustomCommand(const QString &command);
    Q_INVOKABLE void openLogFileDialog();
    Q_INVOKABLE void openDataFileDialog();
    Q_INVOKABLE void openCaptureDirectoryDialog();
    Q_INVOKABLE void loadLogFile(const QString &filePathOrUrl);
    Q_INVOKABLE void loadDataFile(const QString &filePathOrUrl);
    Q_INVOKABLE void clearLogView();
    Q_INVOKABLE void clearLogStorage();
    Q_INVOKABLE void resetEkf();

    Q_INVOKABLE void attachSeries(
        QObject *singleShadow,
        QObject *single,
        QObject *ch0,
        QObject *ch1,
        QObject *ch2,
        QObject *ch3,
        QObject *ch4,
        QObject *ch5,
        QObject *ch6,
        QObject *ch7);
    Q_INVOKABLE void attachCycleSeries(
        QObject *cycle0,
        QObject *cycle1,
        QObject *cycle2,
        QObject *cycle3,
        QObject *cycle4,
        QObject *cycle5,
        QObject *cycle6,
        QObject *cycle7);

    Q_INVOKABLE void startCycleLoop();
    Q_INVOKABLE void stopCycleLoop();

    Q_INVOKABLE void setScanChannelVisible(int channel, bool visible);
    // Toggle whether a multi-channel lane participates in sampling (maps to CFG CHMASK).
    Q_INVOKABLE bool setScanChannelSampling(int channel, bool enabled);
    Q_INVOKABLE void setZoomRange(double xMin, double xMax, double yMin, double yMax);
    Q_INVOKABLE void resetZoom();

signals:
    void availablePortsChanged();
    void connectedChanged();
    void acquisitionEnabledChanged();
    void statusTextChanged();
    void metricChanged();
    void configChanged();
    void axisRangeChanged();
    void cycleLoopChanged();
    void cycleConfigChanged();

private:
    void initializeDatabase();
    QString defaultStorageDirectory() const;
    bool configureStorageDirectory(const QString &directoryPath);
    bool openCsvForSession();
    void closeCsvFile();
    bool appendSampleToCsv(const SampleEntry &sample, qint64 sampleTimestampNs);
    bool appendScanFrameToCsv(const QVector<double> &voltages, qint64 sampleTimestampNs);
    void enforcePendingSampleLimit();
    void trimPlotBuffers();
    void appendSingleOverview(double t, double rawVoltage, double filteredVoltage);
    void appendScanOverview(double t, const QVector<double> &values);
    void compactSingleOverview();
    void compactScanOverview();
    void clearZoomCache();
    bool rebuildZoomCacheFromCsv();
    void appendLog(const QString &level, const QString &message, bool persist = true);
    QString normalizeDbPath(const QString &filePathOrUrl) const;
    bool rebuildPlotDataFromCsv(const QString &csvPath, QString *errorMessage = nullptr);
    void rebuildPlotDataFromSamples(const QVector<SampleEntry> &samples);
    void resetStats();
    bool persistPendingSamples();
    bool flushSampleSqlBuffer(bool logError);
    void handleLine(const QString &line);
    void recordSingle(int adc, const QString &hexValue);
    void recordScan8(const QVector<int> &values);
    void sendLine(const QString &line);
    QString buildCfgCommand() const;

    double adcToVoltage(int adc) const;
    int singleChannelIndex() const;
    double applyEkf(int channel, double value);
    void applyEkfParams();
    void resetEkfFilters(bool logEvent);
    void clearCycleOverlay();
    void beginCycleTrace(qint64 nowNs);
    void appendCyclePoint(double elapsedSeconds, double voltage);
    void evaluateCycleTransition(double voltage, qint64 nowNs);
    void switchCyclePhase(bool toCharge, qint64 nowNs);
    void applyRelayState(bool chargeOn, bool dischargeOn);
    void stopCycleLoopInternal(bool powerOffRelays, bool completed, const QString &reason);

    void updatePlot(bool force = false);
    void clearSeries();
    void updateAxisRange(double xMin, double xMax, double yMin, double yMax, bool forceNotify = false);
    std::pair<double, double> xRange(const std::deque<double> &x) const;

    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    enum class CyclePhase {
        Idle,
        Discharge,
        Charge,
    };

    struct CycleTrace {
        int cycleIndex = 0;
        std::deque<double> t;
        std::deque<double> v;
    };

    QSerialPort *m_serial = nullptr;
    QByteArray m_rxBuffer;
    bool m_rxOverflowWarned = false;
    bool m_rxLineTooLongWarned = false;

    QThread *m_sampleWriterThread = nullptr;
    SampleSqlWriter *m_sampleWriter = nullptr;

    QStringList m_portLabels;
    QStringList m_portDevices;

    bool m_connected = false;
    bool m_acquisitionEnabled = false;
    QString m_statusText = QStringLiteral("未连接");

    QString m_latestAdText = QStringLiteral("--");
    QString m_latestVoltageText = QStringLiteral("-- V");
    QString m_latestHexText = QStringLiteral("--");
    qlonglong m_sampleCount = 0;

    double m_vref = 2.5;
    int m_pga = 1;
    QString m_psel = QStringLiteral("AIN0");
    QString m_nsel = QStringLiteral("AINCOM");
    int m_drate = 0x82;
    QString m_acqMode = QStringLiteral("SINGLE");
    int m_viewChannel = 0;
    QString m_xAxisMode = QStringLiteral("WINDOW");
    double m_windowSeconds = 1800.0;

    bool m_ekfEnabled = false;
    double m_ekfQ = 1e-6;
    double m_ekfR = 1e-4;
    double m_ekfP0 = 1.0;

    QVector<ScalarEkf> m_ekfFilters;

    qint64 m_startNs = 0;
    qint64 m_lastPlotUpdateNs = 0;
    double m_latestElapsedSeconds = 0.0;

    int m_plotBufferMax = 20000;
    int m_singleOverviewStep = 1;
    int m_scanOverviewStep = 1;
    qint64 m_singleSampleSeq = 0;
    qint64 m_scanFrameSeq = 0;

    std::deque<double> m_tSingle;
    std::deque<double> m_vSingle;
    std::deque<double> m_vSingleRaw;

    std::deque<double> m_tScan;
    QVector<std::deque<double>> m_vScan;
    QVector<bool> m_scanVisible;
    quint8 m_scanSampleMask = 0xFF;
    std::array<int, 8> m_latestScanAdc {};
    std::array<double, 8> m_latestScanVoltage {};
    bool m_hasLatestScanFrame = false;

    QPointer<QXYSeries> m_singleShadowSeries;
    QPointer<QXYSeries> m_singleSeries;
    QVector<QPointer<QXYSeries>> m_scanSeries;
    QVector<QPointer<QXYSeries>> m_cycleSeries;

    double m_axisXMin = 0.0;
    double m_axisXMax = 1.0;
    bool m_axisXInHours = false;
    double m_axisYMin = -0.01;
    double m_axisYMax = 0.01;

    bool m_zoomActive = false;
    double m_zoomXMin = 0.0;
    double m_zoomXMax = 1.0;
    double m_zoomYMin = -0.01;
    double m_zoomYMax = 0.01;
    bool m_zoomCacheReady = false;
    QList<QPointF> m_zoomSingleRawPoints;
    QList<QPointF> m_zoomSinglePoints;
    QVector<QList<QPointF>> m_zoomScanPoints;

    bool m_cycleLoopEnabled = false;
    CyclePhase m_cyclePhase = CyclePhase::Idle;
    QString m_cyclePhaseText = QStringLiteral("空闲");
    int m_cycleCompletedCount = 0;
    int m_cycleTraceSeed = 0;
    qint64 m_cycleStartNs = 0;
    qint64 m_cyclePhaseStartNs = 0;
    int m_cycleConsecutiveMatches = 0;
    double m_cycleDischargeEndVoltage = 3.2;
    double m_cycleChargeEndVoltage = 4.2;
    int m_cycleConfirmSamples = 8;
    int m_cycleMaxCount = 0;
    QVector<CycleTrace> m_cycleTraces;
    quint32 m_relayApplySeq = 0;

    QString m_storageDir;
    QString m_logDbPath;
    QString m_csvPath;
    QFile m_csvFile;
    int m_csvFlushCounter = 0;
    bool m_csvWriteHealthy = true;
    bool m_sqlOverflowWarned = false;
    int m_sqlFlushFailureStreak = 0;

    LogListModel m_logModel;
    LogDatabase m_logDatabase;
    QVector<SampleEntry> m_pendingSamples;

    QRegularExpression m_adPattern;
    QRegularExpression m_ad8Pattern;
    QRegularExpression m_hexPattern;
    QRegularExpression m_pgaPattern;
};
