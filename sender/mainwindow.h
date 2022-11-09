#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QFile>
#include <memory>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SendThread : public QThread
{
     Q_OBJECT
public:
    SendThread(const QString& fileName, int waitMS);

    void run() override;

    void stop() { mIsRunning = false; }

signals:
    void qrReady(const QImage img, int seqId);
    void timeout();

private:
    QImage qrEncode(const char* data, size_t len);
    bool waitAck(int seqId);


    std::vector<char> mBuffer;
    std::unique_ptr<QFile> mInputFile;
    std::atomic_bool mIsRunning{true};
    int mSequenceId{0};
    int mWaitMS{0};
    char* mNetSeqId{nullptr};
    char* mData{nullptr};
    QMutex mAckMut;
    QWaitCondition mAckWait;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void activeSelf();

    Ui::MainWindow *ui;

    std::unique_ptr<SendThread> mSender;
    int mTotalCnt{0};
    std::unique_ptr<QTimer> mActiveTimer;
};
#endif // MAINWINDOW_H
