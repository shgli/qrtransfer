#ifndef ACK_H
#define ACK_H
#include <QObject>
#include <QMutex>
#include <QWaitCondition>

enum class AckMode
{
    WatchClipboard,
    PollClipboard,
    WatchKeyEvent
};

class IAck
{
public:
    ~IAck();

    QString wait(QString ackPrefix)
    {
        mAckPrefix = ackPrefix;
        return doWait();
    }

protected:
    virtual QString doWait( void ) = 0;
    QString mAckPrefix;
};

class WatchClipboardAck: public QObject, public IAck
{
public:
    WatchClipboardAck();

protected:
    QString doWait( void ) override;

    QMutex mAckMut;
    QWaitCondition mAckWait;
    QString mAckText;
};

class PollClipboardAck: public IAck
{
public:
    PollClipboardAck(int32_t sleepUS);

protected:
    QString doWait( void ) override;

    int32_t mSleepMS;
};

class WatchKeyEventAck: public IAck
{
public:
    WatchKeyEventAck();

protected:
    QString doWait( void ) override;

};

#endif // ACK_H
