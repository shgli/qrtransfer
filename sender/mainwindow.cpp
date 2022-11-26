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
            ui->label->setPixmap(QPixmap::fromImage(mCfgImg));
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
#ifdef Q_OS_MACOS
            pAckWaiter = std::make_unique<PollClipboardAck>(5);
#else
            pAckWaiter = std::make_unique<WatchClipboardAck>();
#endif

            int scalar = ui->mScalar->text().toInt();
            QString imgCfg = ui->mImgCfg->text();
            QList<QString> imgCfgs = imgCfg.split('x');
            int imgCnt = imgCfgs[0].toInt();
            int pixelChannelCnt = imgCfgs[1].toInt();
            mSender = std::make_unique<SendThread>(transferFileName, std::move(pAckWaiter), scalar, pixelChannelCnt, imgCnt);
            mTotalCnt = 0;
            ui->mProgressBar->setValue(0);
            mIsNormalFinish = true;
            connect(mSender.get(), &SendThread::qrReady, this, [this](QImage img, int seqId)
            {
                if(0 == mTotalCnt)
                {
                    mTotalCnt = seqId;
                    ui->mProgressBar->setRange(0, mTotalCnt-1);
                }

                ui->label->setPixmap(QPixmap::fromImage(img));
                ui->mProgressBar->setValue(mTotalCnt-seqId);
            });

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
