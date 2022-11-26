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
    virtual ~IAck();

    QString wait(QString ackPrefix)
    {
        mAckPrefix = ackPrefix;
        return doWait();
    }

    void stop()
    {
        mIsRunning = false;
        doStop();
    }
protected:
    virtual QString doWait( void ) = 0;
    virtual void doStop( void ) = 0;
    QString mAckPrefix;
    std::atomic_bool mIsRunning{true};
};

class WatchClipboardAck: public QObject, public IAck
{
    Q_OBJECT
public:
    WatchClipboardAck();

signals:
    void stoped();

protected:
    QString doWait( void ) override;
    void doStop( void ) override;

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
    void doStop( void ) override;

    int32_t mSleepMS;
};

class WatchKeyEventAck: public IAck
{
public:
    WatchKeyEventAck();

protected:
    QString doWait( void ) override;
    void doStop( void ) override;
};

#endif // ACK_H
