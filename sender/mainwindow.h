#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SendThread;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void activeSelf();

    Ui::MainWindow *ui;

    std::unique_ptr<SendThread> mSender;
    QImage mCfgImg;
    int mTotalCnt{0};
};
#endif // MAINWINDOW_H
