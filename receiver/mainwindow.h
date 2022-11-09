#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QFile>
#include <memory>
#include <QTimer>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ReceiveThread : public QThread
{
     Q_OBJECT

public:
    ReceiveThread(const QString& fileName);

    void run() override;

    void stop() { mIsRunning = false; }

signals:
    void acked(int seqId);
    void failed(int seqId);
private:
    QImage grubScreen(int x0, int y0, int x2, int y2);

    std::unique_ptr<QFile> mOutputFile;
    std::atomic_bool mIsRunning{true};
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

    std::unique_ptr<ReceiveThread> mReceiver;
    int mTotalCnt = 0;
};
#endif // MAINWINDOW_H
