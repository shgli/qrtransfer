#include "Ack.h"
#include <unistd.h>
#include <QClipboard>
#include <QApplication>
#include <QThread>
IAck::~IAck()
{

}

WatchClipboardAck::WatchClipboardAck()
{
    QClipboard* pClipboard = QApplication::clipboard();
    auto handler = connect(pClipboard, &QClipboard::dataChanged, this, [this, pClipboard]()
    {
        QString text = pClipboard->text();
        if(text.startsWith(mAckPrefix))
        {
            mAckMut.lock();
            mAckText = text;
            mAckWait.wakeOne();
            mAckMut.unlock();
        }
    });

    connect(this, &WatchClipboardAck::stoped, this, [handler]()
    {
        disconnect(handler);
    });
}

QString WatchClipboardAck::doWait()
{
    mAckMut.lock();
    mAckWait.wait(&mAckMut);
    mAckMut.unlock();

    return mAckText;
}

void WatchClipboardAck::doStop( void )
{
    emit stoped();
    mAckMut.lock();
    mAckWait.wakeAll();
    mAckMut.unlock();
}

PollClipboardAck::PollClipboardAck(int32_t sleepMS)
    :mSleepMS(sleepMS)
{}

QString PollClipboardAck::doWait()
{
    QClipboard* pClipboard = QApplication::clipboard();
    QString text;
    bool isOK = false;
    while(mIsRunning && !isOK)
    {
        text = pClipboard->text();
        isOK = text.startsWith(mAckPrefix);
        QThread::msleep(mSleepMS);
    }

    return text;
}

void PollClipboardAck::doStop()
{

}

QString WatchKeyEventAck::doWait()
{
    return QString();
}

void WatchKeyEventAck::doStop()
{
}
