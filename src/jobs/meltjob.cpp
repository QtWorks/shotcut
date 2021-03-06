/*
 * Copyright (c) 2012-2018 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "meltjob.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QIODevice>
#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QDir>
#include <Logger.h>
#include "mainwindow.h"
#include "dialogs/textviewerdialog.h"
#include "util.h"

MeltJob::MeltJob(const QString& name, const QString& xml)
    : AbstractJob(name)
    , m_isStreaming(false)
    , m_previousPercent(0)
{
    if (!xml.isEmpty()) {
        QAction* action = new QAction(tr("View XML"), this);
        action->setToolTip(tr("View the MLT XML for this job"));
        connect(action, SIGNAL(triggered()), this, SLOT(onViewXmlTriggered()));
        m_standardActions << action;
        m_xml.setFileTemplate(QDir::tempPath().append("/shotcut-XXXXXX.mlt"));
        m_xml.open();
        m_xml.write(xml.toUtf8());
        m_xml.close();
    } else {
        // Not an EncodeJob
        QAction* action = new QAction(tr("Open"), this);
        action->setToolTip(tr("Open the output file in the Shotcut player"));
        connect(action, SIGNAL(triggered()), this, SLOT(onOpenTiggered()));
        m_successActions << action;
    
        action = new QAction(tr("Show In Folder"), this);
        action->setToolTip(tr("Show In Folder"));
        connect(action, SIGNAL(triggered()), this, SLOT(onShowFolderTriggered()));
        m_successActions << action;
    }
}

void MeltJob::onOpenTiggered()
{
    MAIN.open(objectName().toUtf8().constData());
}

void MeltJob::onShowFolderTriggered()
{
    Util::showInFolder(objectName());
}

MeltJob::MeltJob(const QString& name, const QStringList& args)
    : MeltJob(name)
{
    m_args = args;
}

MeltJob::~MeltJob()
{
    LOG_DEBUG() << "begin";
}

void MeltJob::start()
{
    QString shotcutPath = qApp->applicationDirPath();
#ifdef Q_OS_WIN
    QFileInfo meltPath(shotcutPath, "qmelt.exe");
#else
    QFileInfo meltPath(shotcutPath, "qmelt");
#endif
    setReadChannel(QProcess::StandardError);
    QStringList args;
    args << "-verbose";
    args << "-progress2";
    args << "-abort";
    if (m_args.size() > 0)
        args.append(m_args);
    else
        args << xmlPath();
    LOG_DEBUG() << meltPath.absoluteFilePath() << args;
#ifdef Q_OS_WIN
    if (m_isStreaming) args << "-getc";
    QProcess::start(meltPath.absoluteFilePath(), args);
#else
    args.prepend(meltPath.absoluteFilePath());
    QProcess::start("/usr/bin/nice", args);
#endif
    AbstractJob::start();
}

QString MeltJob::xml()
{
    m_xml.open();
    QString s(m_xml.readAll());
    m_xml.close();
    return s;
}

void MeltJob::setIsStreaming(bool streaming)
{
    m_isStreaming = streaming;
}

void MeltJob::onViewXmlTriggered()
{
    TextViewerDialog dialog(&MAIN);
    dialog.setWindowTitle(tr("MLT XML"));
    dialog.setText(xml());
    dialog.exec();
}

void MeltJob::onReadyRead()
{
    QString msg = readLine();
    if (msg.contains("percentage:")) {
        int percent = msg.mid(msg.indexOf("percentage:") + 11).toInt();
        if (percent != m_previousPercent) {
            emit progressUpdated(m_item, percent);
            m_previousPercent = percent;
        }
    }
    else {
        appendToLog(msg);
    }
}
