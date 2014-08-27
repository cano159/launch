/*
The MIT License (MIT)

Copyright (c) 2013 The ioquake Group

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include "installwizard_copy.h"
#include "ui_installwizard_copy.h"

InstallWizard_Copy::InstallWizard_Copy(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::InstallWizard_Copy),
    copyWorker(NULL),
    isCopyFinished(false)
{
    ui->setupUi(this);
}

InstallWizard_Copy::~InstallWizard_Copy()
{
    copyThread.quit();
    copyThread.wait();
    delete ui;
}

void InstallWizard_Copy::initializePage()
{
    ui->lblStatus->setText("");
    isCopyFinished = false;
    copyFilename = QString();

    // Try to create the destination directory and baseq3 subdirectory.
    const QString quake3Path(((InstallWizard *)wizard())->getQuakePath() + QString("/baseq3"));
    QDir dir;

    if (!dir.mkpath(quake3Path))
    {
        ui->lblStatus->setText(QString("Error creating directory '%1'").arg(quake3Path));
        return;
    }

    // Start copy thread.
    qRegisterMetaType<QList<FileOperation> >("QList<FileOperation>");
    copyWorker = new FileCopyWorker(((InstallWizard *)wizard())->getFileCopyOperations());
    copyWorker->moveToThread(&copyThread);
    connect(&copyThread, &QThread::finished, copyWorker, &QObject::deleteLater);
    connect(this, &InstallWizard_Copy::copy, copyWorker, &FileCopyWorker::copy);
    connect(copyWorker, &FileCopyWorker::fileChanged, this, &InstallWizard_Copy::setCopyFilename);
    connect(copyWorker, &FileCopyWorker::progressChanged, this, &InstallWizard_Copy::setCopyProgress);
    connect(copyWorker, &FileCopyWorker::errorMessage, this, &InstallWizard_Copy::setCopyErrorMessage);
    connect(copyWorker, &FileCopyWorker::copyFinished, this, &InstallWizard_Copy::finishCopy);
    copyThread.start();

    emit copy();
}

bool InstallWizard_Copy::isComplete() const
{
    return isCopyFinished;
}

int InstallWizard_Copy::nextId() const
{
    if (((InstallWizard *)wizard())->getIsQuake3PatchRequired())
    {
        return InstallWizard::Page_Patch;
    }

    return InstallWizard::Page_Finished;
}

void InstallWizard_Copy::cancel()
{
    if (!isCopyFinished)
    {
        copyWorker->cancel();
    }
}

void InstallWizard_Copy::setCopyFilename(const QString &filename)
{
    copyFilename = filename;
}

void InstallWizard_Copy::setCopyProgress(qint64 bytesWritten, qint64 bytesTotal)
{
    ui->lblStatus->setText(QString("Copying %1 (%2MB / %3MB)").arg(copyFilename).arg(bytesWritten / 1024.0 / 1024.0, 0, 'f', 2).arg(bytesTotal / 1024.0 / 1024.0, 0, 'f', 2));
    ui->pbProgress->setMaximum((int)bytesTotal);
    ui->pbProgress->setValue((int)bytesWritten);
}

void InstallWizard_Copy::setCopyErrorMessage(const QString &message)
{
    ui->lblStatus->setText(message);
}

void InstallWizard_Copy::finishCopy(QList<FileOperation> renameOperations)
{
    copyThread.quit();
    copyThread.wait();

    // Complete the transaction.
    const QString transactionError = FileUtils::completeTransaction(renameOperations);

    if (!transactionError.isEmpty())
    {
        ui->lblStatus->setText(transactionError);
        return;
    }

    isCopyFinished = true;
    emit completeChanged();
    wizard()->next();
}