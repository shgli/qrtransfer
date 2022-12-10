#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include "SendThread.h"
#include "Ack.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QPalette qrcodeImgPalette;
    qrcodeImgPalette.setColor(QPalette::Window, QColor(255, 255, 255));
    ui->label->setAutoFillBackground(true);
    ui->label->setPalette(qrcodeImgPalette);

    connect(ui->pushButton, &QPushButton::clicked, this, [this]()
    {
        if(nullptr != mSender)
        {
            mSender->stop();
            mSender.release();
            ui->pushButton->setText("start");
            ui->mScalar->setEnabled(true);
            ui->mImgCfg->setEnabled(true);
            mIsNormalFinish = false;
            ui->label->setPixmap(QPixmap::fromImage(mCfgImg, Qt::ColorOnly|Qt::ThresholdDither|Qt::ThresholdAlphaDither|Qt::AvoidDither));
            ui->mProgressBar->setValue(0);
            return;
        }

        auto transferFileName = QDir::currentPath()+"/x.tgz";
        if(QFile::exists(transferFileName))
        {
            ui->pushButton->setText("stop");
            ui->mScalar->setEnabled(false);
            ui->mImgCfg->setEnabled(false);

            QClipboard* pClipboard = QApplication::clipboard();
            pClipboard->clear();
            std::unique_ptr<IAck> pAckWaiter;

            int scalar = ui->mScalar->text().toInt();
            QString imgCfg = ui->mImgCfg->text();
            QList<QString> imgCfgs = imgCfg.split('x');
            int rows = 1, cols = 1, pixelChannels = 3;
            if(3 == imgCfgs.size())
            {
                rows = imgCfgs[0].toInt();
                cols = imgCfgs[1].toInt();
                pixelChannels = imgCfgs[2].toInt();
            }
            else
            {
                rows = imgCfgs[0].toInt();
                cols = imgCfgs[1].toInt();
            }

            auto ackMode = ui->mAckMode->currentText();
            if(ackMode == "ClipboardEvent")
            {
                pAckWaiter = std::make_unique<WatchClipboardAck>();
            }
            else if(ackMode == "PollClipboard")
            {
                pAckWaiter = std::make_unique<PollClipboardAck>(5);
            }
            else
            {
                pAckWaiter = std::make_unique<WatchKeyEventAck>();
                rows = cols = 1;
            }

            mSender = std::make_unique<SendThread>(transferFileName, std::move(pAckWaiter), scalar, rows, cols, pixelChannels);
            mTotalCnt = 0;
            ui->mProgressBar->setValue(0);
            mIsNormalFinish = true;
            connect(mSender.get(), &SendThread::qrReady, this, [this](QImage img, int seqId)
            {
                if(0 == mTotalCnt)
                {
                    mTotalCnt = seqId;
                    ui->mProgressBar->setRange(0, mTotalCnt);
                }

                qDebug() << "DPI:" << img.dotsPerMeterX() << "x" << img.dotsPerMeterY();
                ui->label->setPixmap(QPixmap::fromImage(img, Qt::ColorOnly|Qt::ThresholdDither|Qt::ThresholdAlphaDither|Qt::AvoidDither));
                ui->mProgressBar->setValue(mTotalCnt-seqId);
            }, Qt::BlockingQueuedConnection);

            connect(mSender.get(), &SendThread::finished, this, [this,transferFileName]()
            {
                ui->pushButton->setText("start");
                ui->mScalar->setEnabled(true);
                ui->mImgCfg->setEnabled(true);

                if(mIsNormalFinish)
                {
                    QFile::remove(transferFileName);
                }
                mSender.release();
            });

            mSender->start();
        }
    });


}

MainWindow::~MainWindow()
{
    if(nullptr != mSender)
    {
        mSender->stop();
    }

    delete ui;
}

void MainWindow::activeSelf()
{
   activateWindow();
   setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
   raise();//必须加，不然X11会不起作用
   #if 0// 0 Q_OS_WIN32 //windows必须加这个，不然windows10 会不起作用，具体参看activateWindow 函数的文档
       HWND hForgroundWnd = GetForegroundWindow();
       DWORD dwForeID = ::GetWindowThreadProcessId(hForgroundWnd, NULL);
       DWORD dwCurID = ::GetCurrentThreadId();

       ::AttachThreadInput(dwCurID, dwForeID, TRUE);
       ::SetForegroundWindow((HWND)winId());xcsc_jnx
       ::AttachThreadInput(dwCurID, dwForeID, FALSE);
   #endif // MAC_OS
}
