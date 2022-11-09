#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qrencode.h"
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QClipboard>

static constexpr size_t MAX_BUFFER_SIZE = 2945;
static constexpr size_t SEQ_ID_SIZE = 8;
SendThread::SendThread(const QString& fileName, int waitMS)
{
    mWaitMS = waitMS;
    mBuffer.reserve(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
    mBuffer.resize(SEQ_ID_SIZE+MAX_BUFFER_SIZE);
    mNetSeqId = mBuffer.data();
    mData = &(mBuffer[SEQ_ID_SIZE]);
    mInputFile = std::make_unique<QFile>(fileName, nullptr);
    mInputFile->open(QIODevice::ReadOnly);

    if(waitMS < 0)
    {
        QClipboard* pClipboard = QApplication::clipboard();
        connect(pClipboard, &QClipboard::dataChanged, this, [this, pClipboard]()
        {
            QString text = pClipboard->text();
            bool isNumber = false;
            int ackId = text.toInt(&isNumber);
            if(isNumber)
            {
                mAckMut.lock();
                if(ackId == mSequenceId)
                {
                    mAckWait.wakeAll();
                }
                mAckMut.unlock();
            }
        });
    }
}

void SendThread::run()
{
    size_t fsize = mInputFile->size();
    int sendCnt = fsize/MAX_BUFFER_SIZE + ((0 == (fsize%MAX_BUFFER_SIZE)) ? 0 : 1);
    mSequenceId = sendCnt;

    while (mIsRunning && !mInputFile->atEnd())
    {
        snprintf(mNetSeqId, SEQ_ID_SIZE, "%07d", mSequenceId);
        mNetSeqId[SEQ_ID_SIZE-1] = '|';
        auto size = mInputFile->read(mData, MAX_BUFFER_SIZE);
        QImage img = qrEncode(mBuffer.data(), size+SEQ_ID_SIZE);

        emit qrReady(img, mSequenceId);
        yieldCurrentThread();
        if(mWaitMS > 0)
        {
            msleep(mWaitMS);
            --mSequenceId;
        }
        else if(0 == mWaitMS)
        {
            waitAck(mSequenceId);
            --mSequenceId;
        }
        else
        {
            mAckMut.lock();
            mAckWait.wait(&mAckMut);
            --mSequenceId;
            mAckMut.unlock();
        }
    }

    mInputFile->close();
}

bool SendThread::waitAck(int seqId)
{
    QClipboard* pClipboard = QApplication::clipboard();

    bool isOK = false;
    while(!isOK)
    {
        QString text = pClipboard->text();
        isOK = text.toInt(&isOK) == seqId && isOK;
        msleep(5);
    }

    return true;
}

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
            ui->mWaitMS->setEnabled(true);
            return;
        }

        auto transferFileName = QDir::currentPath()+"/x.tgz";
        if(QFile::exists(transferFileName))
        {
            ui->pushButton->setText("stop");
            ui->mWaitMS->setEnabled(false);
            QClipboard* pClipboard = QApplication::clipboard();
            pClipboard->clear();
            int waitMS = ui->mWaitMS->text().toInt();
            mSender = std::make_unique<SendThread>(transferFileName, waitMS);
            mTotalCnt = 0;
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
                ui->mWaitMS->setEnabled(true);
                QFile::remove(transferFileName);
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
/**
 * 从字符串创建一个符号。库自动解析输入字符串并在二维码符号中编码.
 * @warning 禁用pthread时线程不安全.
 * @param string NUL('\0')结尾的C字符串.
 * @param version 符号版本.越大可容纳的信息越多.0则按实际内容确定
 * @param level 纠错等级，枚举.
 * @param hint 编码模式，utf8用QR_MODE_8.
 * @param casesensitive 区分大小写(1) 不区分(0).
 * @return 返回QRcode类的实例。结果QRcode的版本可能是大于指定的版本.
 * 出现错误时，返回NULL，设置errno以指示错误.
 * @throw EINVAL invalid input object.
 * @throw ENOMEM unable to allocate memory for input objects.
 * @throw ERANGE input data is too large.
 */
//extern QRcode *QRcode_encodeString(const char *string, int version, QRecLevel level,
//                                   QRencodeMode hint, int casesensitive);
QImage SendThread::qrEncode(const char* data, size_t len)
{
    static const int32_t PIXEL_RATIO = qApp->devicePixelRatio();
    QImage ret; //放二维码图片结果
    int scale = 4/PIXEL_RATIO; //方块绘制大小
    QRcode* qr = QRcode_encodeData(len, (const unsigned char*)data, 40, QR_ECLEVEL_L);
    if (qr && qr->width > 0)
    {
        int img_width = qr->width * scale;
        ret = QImage(img_width, img_width, QImage::Format_Mono); //mono位图
        QPainter painter(&ret);
        painter.fillRect(0, 0, img_width, img_width, Qt::white);//背景填充白色
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black); //黑色方块
        for (int y = 0; y < qr->width; y++) //行
        {
            for (int x = 0; x < qr->width; x++) //列
            {
                if (qr->data[y * qr->width + x] & 1) //1表示黑块
                {
                    QRect r(x * scale, y * scale, scale, scale);
                    painter.drawRect(r);
                }
            }
        }
        QRcode_free(qr);
    }
    return ret;
}
