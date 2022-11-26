#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <vector>
#include <algorithm>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include <QIcon>
#include <QScreen>
#include <QApplication>
#include <vector>
#include <QFileDialog>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->mFileName->setEnabled(false);
    ui->mProcessBar->setRange(0, 100);
    ui->mProcessBar->setValue(0);
    ui->mAckNum->setTextInteractionFlags(Qt::TextSelectableByMouse);
    connect(ui->mFileBrowserBtn, &QPushButton::clicked, this, [this]()
    {
        if(nullptr != mReceiver)
        {
            mReceiver->stop();
            mReceiver.release();
        }

        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("open a file."),
            QDir::currentPath(),
            tr("All files(*)"));
        if (fileName.isEmpty())
        {
            QMessageBox::warning(this, "Warning", "Failed to open the file!");
        }
        else
        {
            ui->mFileName->setText(fileName);
            mReceiver = std::make_unique<ReceiveThread>(fileName);
            ui->mProcessBar->setValue(0);
            mTotalCnt = 0;
            connect(mReceiver.get(), &ReceiveThread::acked, this, [this](int ackedCnt, QString detail)
            {
                if(0 == ackedCnt)
                {
                    mTotalCnt = mReceiver->getTotalCnt();
                    ui->mProcessBar->setRange(0, mTotalCnt+1);
                }

                ui->mProcessBar->setValue(ackedCnt);

                QApplication::clipboard()->setText(detail);
                QString detailLabel=QString::asprintf("%05d[", mTotalCnt+1-ackedCnt);
                detailLabel += detail +"]";
                ui->mAckNum->setText(detailLabel);
                QApplication::clipboard()->text();
            });

            mReceiver->start();
        }
    });
}

MainWindow::~MainWindow()
{
    if(nullptr != mReceiver)
    {
        mReceiver->stop();
    }

    delete ui;
}

