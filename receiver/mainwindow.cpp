#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <algorithm>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include <QIcon>
#include <QScreen>
#include <QApplication>
#include <vector>
#include <QFileDialog>
#include <QMessageBox>
#include "imgconvertors.h"
#include "opencv2/wechat_qrcode.hpp"

static constexpr size_t SEQ_ID_SIZE = 8;
QImage ReceiveThread::grubScreen(int x0, int y0, int x2, int y2)
{
    int w = x2-x0;
    int h = y2-y0;
    QScreen* pScreen = QApplication::primaryScreen();

    QPixmap full = pScreen->grabWindow(0, x0, y0, w, h);
    return full.toImage();
}

ReceiveThread::ReceiveThread(const QString& fileName)
{
    mOutputFile = std::make_unique<QFile>(fileName, nullptr);
    mOutputFile->open(QIODevice::WriteOnly);
}

void ReceiveThread::run()
{
    const qreal pixelRatio = qApp->devicePixelRatio();
    const std::string modelDir = QDir::currentPath().toStdString();
    const auto screen_size = qApp->primaryScreen()->size();
    // 构造（使用异常捕获构造函数是否正常）
    cv::wechat_qrcode::WeChatQRCode detector{
        modelDir + "/detect.prototxt",
        modelDir + "/detect.caffemodel",
        modelDir + "/sr.prototxt",
        modelDir + "/sr.caffemodel"
    };
    // 临时变量

    std::vector<cv::Mat> points;   // qrcode: Retangle, not RotatedBox
    int x0 = 0, y0 = 0;
    int x2 = -1, y2 = -1;
    bool grubFullScreen = true;

    int seqId = 2;
    int prevSeq = 0;
    int failedCnt = 0;
    std::string received;
    while(seqId > 1 && mIsRunning)
    {
       {
//           qDebug() << "beg grub:" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz");
           QImage screen = grubScreen(x0, y0, x2, y2);
//           qDebug() << "end grub" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz");
           cv::Mat img = ASM::QImageToCvMat(screen);
//           qDebug() << "beg decode" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz");
           auto res = detector.detectAndDecode(img, points);
//           qDebug() << "end decode" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz");
           if(res.empty())
           {
#if 0
               failedCnt++;
               if(failedCnt > 3)
               {
                   x0 = y0 = 0;
                   x2 = y2 = -1;
                   grubFullScreen = true;
                   qDebug() << "full";
               }
#else
//               x0 -= 3; y0 -= 3;
//               x0 = std::max(x0, 0);
//               y0 = std::max(y0, 0);

//               x2 += 3; y2 += 3;
//               x2 = std::min(x2, screen_size.width()-1);
//               y2 = std::min(y2, screen_size.height()-1);
               x0 = y0 = 0;
               x2 = y2 = -1;
               grubFullScreen = true;
#endif
               continue;
           }
           else
           {
               x2 = x0 + int((points[0].at<float>(2, 0)+pixelRatio-1.0)/pixelRatio);
               y2 = y0 + int((points[0].at<float>(2, 1)+pixelRatio-1.0)/pixelRatio);

               x0 += int(points[0].at<float>(0, 0)/pixelRatio);
               y0 += int(points[0].at<float>(0, 1)/pixelRatio);

               grubFullScreen = false;
           }

           failedCnt = 0;
           received.swap(res[0]);
        }

        char* syncId = received.data();
        syncId[SEQ_ID_SIZE-1] = '\0';

        seqId = atoi(syncId);

        if(seqId != prevSeq)
        {
//           qDebug() << "right";
           const char* data = &received[SEQ_ID_SIZE];
           mOutputFile->write(data, received.size()-SEQ_ID_SIZE);
           prevSeq = seqId;
           emit acked(seqId);
//           qDebug() << "emid:" << seqId;
           yieldCurrentThread();
        }
        else
        {
//            qDebug() << "duplicate";
        }

        msleep(5);
    }

    mOutputFile->close();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->mFileName->setEnabled(false);
    ui->mProcessBar->setRange(0, 100);
    ui->mProcessBar->setValue(0);
    connect(ui->mFileBrowserBtn, &QPushButton::clicked, this, [this]()
    {
        if(nullptr != mReceiver)
        {
            mReceiver->stop();
            mReceiver.release();
        }

        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("open a file."),
            QDir::currentPath(),
            tr("All files(*)"));
        if (fileName.isEmpty())
        {
            QMessageBox::warning(this, "Warning", "Failed to open the file!");
        }
        else
        {
            ui->mFileName->setText(fileName);
            mReceiver = std::make_unique<ReceiveThread>(fileName);
            mTotalCnt = 0;
            connect(mReceiver.get(), &ReceiveThread::acked, this, [this](int seqId)
            {
                if(0 == mTotalCnt)
                {
                    mTotalCnt = seqId;
                    ui->mProcessBar->setRange(0, mTotalCnt-1);
                }
                ui->mProcessBar->setValue(mTotalCnt - seqId);
                mTotalCnt = std::max(mTotalCnt, seqId);
                QString sSeqId = QString::number(seqId);
                //activeSelf();
                QApplication::clipboard()->setText(sSeqId);
                qDebug() << "acked:" << sSeqId;
                QApplication::clipboard()->text();
            });

            connect(mReceiver.get(), &ReceiveThread::failed, this, [this](int seqId)
            {
                QString sSeqId = QString::asprintf("%dfailed", seqId);
                QApplication::clipboard()->setText(sSeqId);
                QApplication::clipboard()->text();
            });

            connect(mReceiver.get(), &ReceiveThread::finished, this, [this]()
            {
                mReceiver.release();
            });
            mReceiver->start();
        }
    });
}

MainWindow::~MainWindow()
{
    if(nullptr != mReceiver)
    {
        mReceiver->stop();
    }

    delete ui;
}

void MainWindow::activeSelf()
{
   activateWindow();
   setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
   raise();//必须加，不然X11会不起作用
   #if 0 // Q_OS_WIN32 //windows必须加这个，不然windows10 会不起作用，具体参看activateWindow 函数的文档
       HWND hForgroundWnd = GetForegroundWindow();
       DWORD dwForeID = ::GetWindowThreadProcessId(hForgroundWnd, NULL);
       DWORD dwCurID = ::GetCurrentThreadId();

       ::AttachThreadInput(dwCurID, dwForeID, TRUE);
       ::SetForegroundWindow((HWND)winId());xcsc_jnx
       ::AttachThreadInput(dwCurID, dwForeID, FALSE);
   #endif // MAC_OS
}
