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
    connect(pClipboard, &QClipboard::dataChanged, this, [this, pClipboard]()
    {
        QString text = pClipboard->text();
        if(text.startsWith(mAckPrefix))
        {
            mAckMut.lock();
            mAckText = text;
            mAckWait.wakeAll();
            mAckMut.unlock();
        }
    });
}

QString WatchClipboardAck::doWait()
{
    mAckMut.lock();
    mAckWait.wait(&mAckMut);
    mAckMut.unlock();

    return mAckText;
}

PollClipboardAck::PollClipboardAck(int32_t sleepMS)
    :mSleepMS(sleepMS)
{}

QString PollClipboardAck::doWait()
{
    QClipboard* pClipboard = QApplication::clipboard();
    QString text;
    bool isOK = false;
    while(!isOK)
    {
        text = pClipboard->text();
        isOK = text.startsWith(mAckPrefix);
        QThread::msleep(mSleepMS);
    }

    return text;
}

QString WatchKeyEventAck::doWait()
{
    return QString();
}
