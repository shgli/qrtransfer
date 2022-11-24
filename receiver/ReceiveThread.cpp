#include "ReceiveThread.h"
#include <QApplication>
#include <QScreen>
#include <QDir>
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
    mOutputFile->open(QIODevice::ReadWrite);


    mCfgBuffer.resize(DecodeThread::maxBufferSize());
    mDecodeSep = std::make_unique<QSemaphore>(0);
    mDecodeThreads.push_back(std::make_unique<DecodeThread>(0, 0, mDecodeSep.get(), mCfgBuffer.data()));
}

void ReceiveThread::run()
{

    std::vector<cv::Mat> points;   // qrcode: Retangle, not RotatedBox
    int x0 = 0, y0 = 0;
    int x2 = -1, y2 = -1;
    bool grubFullScreen = true;

    auto& pCfgDecoder = *mDecodeThreads.begin();
    pCfgDecoder->start();
    while(mIsRunning && !pCfgDecoder->decodeSuccess())
    {
        QImage screen = grubScreen(x0, y0, x2, y2);
        pCfgDecoder->setImage(screen);
        mDecodeSep->acquire(1);
        msleep(5);
    }

    if(!mIsRunning)
    {
        pCfgDecoder->stop();
        return;
    }

    size_t fileSize = 0;
    sscanf(mCfgBuffer.data(), "%ld|%d|%d|%d|%d", &fileSize, &mTotalCnt, &mChannelCnt, &mRowCnt, &mColCnt);
    mOutputFile->resize(fileSize);
    emit acked(0, "0|1");

    char* pFileData = (char*)mOutputFile->map(0, fileSize);
    mDecodeSep = std::make_unique<QSemaphore>(mChannelCnt);
    pCfgDecoder->reInitialize(mTotalCnt, mDecodeSep.get(), pFileData);

    for(int channel = 1; channel < mChannelCnt; ++channel)
    {
        mDecodeThreads.push_back(std::make_unique<DecodeThread>(channel, mTotalCnt, mDecodeSep.get(), pFileData));
        mDecodeThreads[channel]->start();
    }

    QSet<QString> prevAcked;
    prevAcked.insert("0");
    int ackedCnt = 1;
    while(ackedCnt < (mTotalCnt+1) && mIsRunning)
    {
        QImage screen = grubScreen(x0, y0, x2, y2);
        for(int channel = 0; channel < mChannelCnt; ++channel)
        {
            mDecodeThreads[channel]->setImage(screen);
        }

        mDecodeSep->acquire(mChannelCnt);

        QStringList syncIds;
        std::vector<QRect> areas;

        bool hasDecodeSamething{false};
        for(int channel = 0; channel < mChannelCnt; ++channel)
        {
            auto& pDecoder = mDecodeThreads[channel];
            if(pDecoder->decodeSuccess())
            {
                hasDecodeSamething = true;
                for(auto& syncId : pDecoder->syncIds())
                {
                    if(!prevAcked.contains(syncId))
                    {
                        syncIds.push_back(syncId);
                    }
                }

                for(auto& area : pDecoder->areas())
                {
                    areas.push_back(area);
                }
            }
        }

        if(!syncIds.empty())
        {
            prevAcked.clear();
            for(auto syncId : syncIds)
            {
                prevAcked.insert(syncId);
            }

            syncIds.push_front(QString::number(ackedCnt));
            ackedCnt += syncIds.size()-1;
            QString composedSyncId = syncIds.join('|');
            qDebug() << ackedCnt << ":" << composedSyncId;
            emit acked(ackedCnt, composedSyncId);
        }

        if(hasDecodeSamething)
        {
            qDebug() << "before:" << x0 << "," << y0 << "," << x2 << "," << y2;
            grubFullScreen = !dectQRArea(x0, y0, x2, y2, areas);
            if(x2 == -1 && y2 == -1)
            {
                grubFullScreen = !dectQRArea(x0, y0, x2, y2, areas);
            }
            qDebug() << "after:" << x0 << "," << y0 << "," << x2 << "," << y2;
        }
        else
        {
            x0 = y0 = 0;
            x2 = y2 = -1;
            grubFullScreen = true;
        }

        msleep(5);
    }

    mOutputFile->close();
}

bool ReceiveThread::dectQRArea(int& x0, int& y0, int& x2, int& y2, std::vector<QRect>& areas)
{
    static const qreal pixelRatio = qApp->devicePixelRatio();
    int xx0(100000), yy0(100000), xx2(0), yy2(0);
    int single_width = 0, single_height = 0;
    for(auto& area : areas)
    {
        xx0 = std::min(xx0, area.left());
        yy0 = std::min(yy0, area.top());
        xx2 = std::max(xx2, area.right());
        yy2 = std::max(yy2, area.bottom());

        single_width = area.width();
        single_height = area.height();
    }

    int row_cnt = (yy2-yy0+1) / single_height;
    if(row_cnt < mRowCnt)
    {
        return false;
    }

    int col_cnt = (xx2-xx0+1) / single_width;
    if(col_cnt >= mColCnt)
    {
        x2 = x0 + xx2 + 1; y2 = y0 + yy2 + 1;
        x0 += xx0-1; y0 += yy0-1;
        return true;
    }

    return false;
}
