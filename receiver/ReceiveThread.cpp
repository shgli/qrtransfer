#include "ReceiveThread.h"
#include <QApplication>
#include <QScreen>
#include <QDir>
#include <bitset>
#include "AreaDetector.h"

QImage ReceiveThread::grubScreen(QRect& area, QRect& qrCode, QPoint& offset, QPoint& idClrPos, QColor& idClr)
{
    QScreen* pScreen = QApplication::primaryScreen();

    QPixmap full = pScreen->grabWindow(0, area.left(), area.top(), area.width(), area.height());
    QImage img = full.toImage();
    AreaDetector detector(Qt::black, 354, 50);
    bool isFull = area.size()==pScreen->size();
    idClr = Qt::white;

    if(!isFull)
    {
        qrCode.moveTo(offset);
        QPoint idPos = idClrPos + qrCode.bottomLeft();
        idClr = img.pixelColor(idPos);

        if(!detector.IsIdentityColor(idClr))
        {
            qDebug() << "id color failed";
            return QImage();
        }
    }

    if((isFull && !detector.detect(img, area, qrCode, offset, idClrPos, idClr)))
    {
//        img.save("ok.png");
        return QImage();
    }

//    if(isFull)
//    {
//        qDebug() << "full";
//    }

    return img;
}

ReceiveThread::ReceiveThread(const QString& fileName)
{
    mOutputFile = std::make_unique<QFile>(fileName, nullptr);
    mOutputFile->open(QIODevice::ReadWrite);


    mCfgBuffer.resize(1024);
    mDecodeSep = std::make_unique<QSemaphore>(0);
    mDecodeThreads.push_back(std::make_unique<DecodeThread>(0, mDecodeSep.get(), mCfgBuffer.data()));
}

ReceiveThread::~ReceiveThread()
{
    stop();
}

void ReceiveThread::stop()
{
    if(!mIsRunning)
    {
        return;
    }

    mIsRunning = false;
    mDecodeSep->release(mRowCnt*mColCnt*mChannelCnt);

    for(auto& pDecoder : mDecodeThreads)
    {
        pDecoder->stop();
    }


    while (isRunning())
    {
        msleep(5);
    }

    mDecodeThreads.clear();
}


void ReceiveThread::run()
{
    qDebug() << "rcv thread:" << currentThreadId();
    static qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    std::vector<cv::Mat> points;   // qrcode: Retangle, not RotatedBox
    QRect fullScreenRect = qApp->primaryScreen()->virtualGeometry();
    QRect grubRect = fullScreenRect;
    QRect codeRect;
    QPoint offset, idOffset;
    QColor identityClr = Qt::white;
    auto &pCfgDecoder = *mDecodeThreads.begin();
    pCfgDecoder->start();
    DecodeTask initTask{0, 0, QRect(0, 0, grubRect.width()*pixelRatio, grubRect.height()*pixelRatio), QImage()};
    while(mIsRunning && !pCfgDecoder->decodeSuccess())
    {
        initTask.img = grubScreen(grubRect, codeRect, offset, idOffset, identityClr);
        if(!initTask.img.isNull())
        {
            initTask.area = codeRect;
            pCfgDecoder->pushTask(initTask);
            mDecodeSep->acquire(1);
        }
        else
        {
            grubRect = fullScreenRect;
        }

        msleep(5);
    }

//    qDebug() << "grub rect:" << grubRect;
//    qDebug() << "code rect:" << codeRect;
    if(!mIsRunning)
    {
        pCfgDecoder->stop();
        return;
    }

    sscanf(mCfgBuffer.data(), "%lld|%d|%d|%d", &mFileSize, &mChannelCnt, &mRowCnt, &mColCnt);
    mOutputFile->resize(mFileSize);
    emit acked(-1, "-1|1");

    pCfgDecoder->results().clear();
    char* pFileData = (char*)mOutputFile->map(0, mFileSize);
    mDecodeSep = std::make_unique<QSemaphore>(0);
    pCfgDecoder->reInitialize(mFileSize, mDecodeSep.get(), pFileData);

    for(int channel = 1; channel < mChannelCnt; ++channel)
    {
        mDecodeThreads.push_back(std::make_unique<DecodeThread>(mFileSize, mDecodeSep.get(), pFileData));
        mDecodeThreads[channel]->start();
    }

    QSet<uint> prevRecved;
    prevRecved.insert(-1);
    qint64 recvedLen = 0;
    bool isGrubFullScreen = false;
    int continusFailedCnt = 0;
    QColor prevIdentityClr = identityClr;
    while(recvedLen < mFileSize && mIsRunning)
    {
        QImage screen = grubScreen(grubRect, codeRect, offset, idOffset, identityClr);
        if(screen.isNull())
        {
            grubRect = fullScreenRect;
            isGrubFullScreen = true;
            msleep(5);
            continue;
        }

        int r1 = prevIdentityClr.red();
        int g1 = prevIdentityClr.green();
        int b1 = prevIdentityClr.blue();
        int r2 = identityClr.red();
        int g2 = identityClr.green();
        int b2 = identityClr.blue();
        if(AreaDetector::IsColor(prevIdentityClr, identityClr, 20))
        {
            msleep(5);
            continue;
        }

        prevIdentityClr = identityClr;
//        qDebug() << "grub rect:" << grubRect;
//        qDebug() << "code rect:" << codeRect;
        int taskCnt = 0;
        int codeWidth = codeRect.width() / mColCnt;;
        int codeHeight = codeRect.height() / mRowCnt;
        for(int r = 0; r < mRowCnt; ++r)
        {
            for(int c = 0; c < mColCnt; ++c)
            {
                for(int channel = 0; channel < mChannelCnt; ++channel, ++taskCnt)
                {
                    int x = codeRect.left() + c * codeWidth;
                    int y = codeRect.top() + r * codeHeight;
                    DecodeTask task{channel, taskCnt, QRect(x, y, codeWidth, codeHeight), screen};
                    mDecodeThreads[channel]->pushTask(task);
                }
            }
        }

        mDecodeSep->acquire(taskCnt);
        if(!mIsRunning) break;

        QSet<uint> offsets;
        QSet<uint> successIdxs;
        QList<QRect> areas;
        qint64 thisRecvLen = 0;
        std::bitset<64> decodedRows, decodedCols;
        for(int channel = 0; channel < mChannelCnt; ++channel)
        {
            auto& pDecoder = mDecodeThreads[channel];
            if(pDecoder->decodeSuccess())
            {
                for(auto& ret : pDecoder->results())
                {
                    if(!prevRecved.contains(ret.offset) && 0 != ret.len)
                    {
//                        qDebug() << "acked offset:" << ret.offset << ",len:" << ret.len << ", rcv len:" << recvedLen;
                        offsets.insert(ret.offset);
                        successIdxs.insert(ret.index);
                        thisRecvLen += ret.len;
                    }
                    else
                    {
//                        assert(false);
                    }
                    areas.push_back(ret.area);
                    int row = ret.index/mChannelCnt/mColCnt;
                    int col = ret.index/mChannelCnt%mColCnt;
                    decodedRows.set(row);
                    decodedCols.set(col);
                }
            }

            pDecoder->results().clear();
        }



        bool needNofifyFailed = areas.empty();
        if(!offsets.empty() || needNofifyFailed)
        {

            QStringList ackDetail;
            ackDetail.push_back(QString::number(recvedLen));

            std::vector<uint64_t> successDetail(taskCnt/64+((0 != (taskCnt%64)) ? 1 : 0), 0UL);
            for(auto susIdx : successIdxs)
            {
                successDetail[susIdx/64] |= 1 << (susIdx%64);
            }

            for(uint64_t detail : successDetail)
            {
//                if(0 != (detail & (detail+1)))
//                {
//                    qDebug() << "has some qrcode decode failed";
//                }
                ackDetail.push_back(QString::number(detail));
            }

            if(needNofifyFailed)
            {
                ackDetail.push_back("N"+QString::number(continusFailedCnt));
                continusFailedCnt += 1;
            }

            QString sAckDetail = ackDetail.join("|");
//            qDebug() << "acked rcv len:" << recvedLen << ":" << sAckDetail << ", identityColor:" << identityClr;
            recvedLen += thisRecvLen;
            emit acked(recvedLen, sAckDetail);
            if(!offsets.empty())
            {
                prevRecved.swap(offsets);
                continusFailedCnt = 0;
            }
        }

        if(decodedRows.count() == (size_t)mRowCnt && decodedCols.count() == (size_t)mColCnt)
        {
//            qDebug() << "before:" << grubRect;
//            bool grubFullScreen = !dectQRArea(grubRect, areas);
//            if(grubRect == fullScreenRect)
//            {
//                grubFullScreen = !dectQRArea(grubRect, areas);
//            }
//            qDebug() << "after :" << grubRect << ":" << grubFullScreen;
            isGrubFullScreen = false;
        }
        else
        {
            grubRect = fullScreenRect;
            isGrubFullScreen = true;
        }

        //msleep(5);
    }

    mOutputFile->close();
}

bool ReceiveThread::dectQRArea(QRect& grubArea, QList<QRect>& areas)
{
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
        int x2 = grubArea.left() + xx2 + 1; int y2 = grubArea.top() + yy2 + 1;
        int x0 = grubArea.left() + xx0 - 1; int y0 = grubArea.top() + yy0 - 1;
        grubArea = QRect(QPoint(x0, y0), QPoint(x2, y2));
        return true;
    }

    return false;
}
