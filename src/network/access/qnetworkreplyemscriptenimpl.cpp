/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qnetworkreplyemscriptenimpl_p.h"

#include <QTimer>
#include "QtCore/qdatetime.h"
#include "qnetworkaccessmanager_p.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>
#include "qnetworkfile_p.h"
#include "qnetworkrequest.h"
//#include "qnetworkaccessbackend_p.h"

#include <iostream>

QT_BEGIN_NAMESPACE

static QNetworkReplyEmscriptenImplPrivate *gHandler = 0;

QNetworkReplyEmscriptenImplPrivate::QNetworkReplyEmscriptenImplPrivate()
    : QNetworkReplyPrivate(),
      managerPrivate(0),
      downloadBufferReadPosition(0),
      downloadBufferCurrentSize(0),
      totalDownloadSize(0),
      percentFinished(0),
      downloadZerocopyBuffer(0)
{
    gHandler = this;
}

QNetworkReplyEmscriptenImplPrivate::~QNetworkReplyEmscriptenImplPrivate()
{
    std::cout << "QNetworkReplyEmscriptenImplPrivate::~QNetworkReplyEmscriptenImplPrivate(): " << this << std::endl;
}

QNetworkReplyEmscriptenImpl::~QNetworkReplyEmscriptenImpl()
{
}

QNetworkReplyEmscriptenImpl::QNetworkReplyEmscriptenImpl(QObject *parent/*QNetworkAccessManager *manager, const QNetworkRequest &req, const QNetworkAccessManager::Operation op*/)
    : QNetworkReply(*new QNetworkReplyEmscriptenImplPrivate(), parent)
{
//qDebug() << req.rawHeaderList();
}


void QNetworkReplyEmscriptenImpl::connectionFinished()
{
}

QByteArray QNetworkReplyEmscriptenImpl::methodName() const
{
    switch (operation()) {
    case QNetworkAccessManager::HeadOperation:
        return "HEAD";
    case QNetworkAccessManager::GetOperation:
        return "GET";
    case QNetworkAccessManager::PutOperation:
        return "PUT";
    case QNetworkAccessManager::PostOperation:
        return "POST";
    case QNetworkAccessManager::DeleteOperation:
        return "DELETE";
//    case QNetworkAccessManager::CustomOperation:
//        return d->customVerb;
    default:
        break;
    }
    return QByteArray();
}

void QNetworkReplyEmscriptenImpl::close()
{
    qDebug() << Q_FUNC_INFO;
 //   Q_D(QNetworkReplyEmscriptenImpl);
    QNetworkReply::close();
}

void QNetworkReplyEmscriptenImpl::abort()
{
    close();
}

qint64 QNetworkReplyEmscriptenImpl::bytesAvailable() const
{
    Q_D(const QNetworkReplyEmscriptenImpl);

    qDebug() << Q_FUNC_INFO
             << d->isFinished
             << QNetworkReply::bytesAvailable()
             << d->downloadBufferCurrentSize
             << d->downloadBufferReadPosition;

    if (!d->isFinished)
        return QNetworkReply::bytesAvailable();

    return QNetworkReply::bytesAvailable() + d->downloadBufferCurrentSize - d->downloadBufferReadPosition;
}

bool QNetworkReplyEmscriptenImpl::isSequential() const
{
    return true;
}

qint64 QNetworkReplyEmscriptenImpl::size() const
{
    return QNetworkReply::size();
}

/*!
    \internal
*/
qint64 QNetworkReplyEmscriptenImpl::readData(char *data, qint64 maxlen)
{
    Q_D(QNetworkReplyEmscriptenImpl);

    qint64 howMuch = qMin(maxlen, (d->downloadBufferCurrentSize - d->downloadBufferReadPosition));

    qDebug() << Q_FUNC_INFO << "howMuch"<< howMuch
             << "d->downloadBufferReadPosition" << d->downloadBufferReadPosition;

    if (d->downloadZerocopyBuffer) {
        memcpy(data, d->downloadZerocopyBuffer + d->downloadBufferReadPosition, howMuch);
        d->downloadBufferReadPosition += howMuch ;
        return howMuch;
    }
    return 0;
}

void QNetworkReplyEmscriptenImpl::emitReplyError(QNetworkReply::NetworkError errorCode)
{
    qDebug() << Q_FUNC_INFO << this<< errorCode;

    setFinished(true);
    emit error(errorCode);
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    //qApp->processEvents();
}

void QNetworkReplyEmscriptenImplPrivate::setup(QNetworkAccessManager::Operation op, const QNetworkRequest &req,
                                     QIODevice *data)
{
    Q_Q(QNetworkReplyEmscriptenImpl);

    qDebug() << Q_FUNC_INFO;

    outgoingData = data;
    request = req;
    url = request.url();
    operation = op;

    q->QIODevice::open(QIODevice::ReadOnly);
    if (outgoingData) {
        if (!outgoingData->isSequential()) {
            // fixed size non-sequential (random-access)
            // just start the operation
            doSendRequest();
            return;
        } else {
            bool bufferingDisallowed =
                    request.attribute(QNetworkRequest::DoNotBufferUploadDataAttribute,
                                      false).toBool();

            if (bufferingDisallowed) {
                // if a valid content-length header for the request was supplied, we can disable buffering
                // if not, we will buffer anyway
                if (request.header(QNetworkRequest::ContentLengthHeader).isValid()) {
                    doSendRequest();
                    return;
                } else {
                    state = Buffering;
                    _q_bufferOutgoingData();
                    return;
                }
            } else {
                // doSendRequest will be called when the buffering has finished.
                state = Buffering;
                _q_bufferOutgoingData();
                return;
            }
        }
    }
    // No outgoing data (POST, ..)
    doSendRequest();
}

void QNetworkReplyEmscriptenImplPrivate::onLoadCallback(void *data, int readyState, int buffer, int bufferSize)
{
    std::cout << "load callback 1" << std::endl;
    QNetworkReplyEmscriptenImplPrivate *handler = reinterpret_cast<QNetworkReplyEmscriptenImplPrivate*>(data);

    std::cout << "load callback 2: " << handler << std::endl;
    // FIXME TODO do something with null termination lines ??
    qDebug() << Q_FUNC_INFO << (int)readyState << bufferSize;

    std::cout << "load callback 3" << std::endl;
    switch(readyState) {
    case 0://unsent
        break;
    case 1://opened
        break;
    case 2://headers received
        break;
    case 3://loading
        break;
    case 4://done
        std::cout << "load callback 4" << std::endl;
        handler->dataReceived((char *)buffer/*, bufferSize*/);
        break;
    };
    std::cout << "load callback 5" << std::endl;
 }

void QNetworkReplyEmscriptenImplPrivate::onProgressCallback(void* data, int done, int total, uint timestamp)
{
    QNetworkReplyEmscriptenImplPrivate *handler = reinterpret_cast<QNetworkReplyEmscriptenImplPrivate*>(data);
    std::cout << "Progress callback 1: " << handler << " " << done << " " << total << " " << timestamp << std::endl;
    handler->emitDataReadProgress(done, total);
    std::cout << "Progress callback 2" << std::endl;
}

void QNetworkReplyEmscriptenImplPrivate::onRequestErrorCallback(void* data, int state, int status)
{
    QNetworkReplyEmscriptenImplPrivate *handler = reinterpret_cast<QNetworkReplyEmscriptenImplPrivate*>(data);
    qDebug() << Q_FUNC_INFO << state << status;
    handler->emitReplyError(QNetworkReply::UnknownNetworkError);
}

void QNetworkReplyEmscriptenImplPrivate::onResponseHeadersCallback(void* data, int headers)
{
    QNetworkReplyEmscriptenImplPrivate *handler = reinterpret_cast<QNetworkReplyEmscriptenImplPrivate*>(data);
    std::cout << "onResponseHeadersCallback 1: " << (char*)headers << std::endl;
    handler->headersReceived((char *)headers);
    std::cout << "onResponseHeadersCallback 2" << std::endl;
}

void QNetworkReplyEmscriptenImplPrivate::doSendRequest()
{
    Q_Q(QNetworkReplyEmscriptenImpl);
    totalDownloadSize = 0;
    jsRequest(q->methodName(), // GET POST
              request.url().toString(),
              (void *)&onLoadCallback,
              (void *)&onProgressCallback,
              (void *)&onRequestErrorCallback,
              (void *)&onResponseHeadersCallback);
}


/* const QString &body, const QList<QPair<QByteArray, QByteArray> > &headers ,*/
void QNetworkReplyEmscriptenImplPrivate::jsRequest(const QString &verb, const QString &url, void *loadCallback, void *progressCallback, void *errorCallback, void *onResponseHeadersCallback)
{
    std::cout << "jsRequest 1" << std::endl;
    qDebug() << Q_FUNC_INFO << verb;
    QString extraData;
    if (outgoingData)
        extraData = outgoingData->readAll();

    std::cout << "jsRequest 2" << std::endl;
    if (verb == "POST" && extraData.startsWith("?"))
        extraData.remove("?");

    qDebug() << Q_FUNC_INFO << verb << url
             << outgoingDataBuffer.data()
             << extraData;

    std::cout << "jsRequest 3" << std::endl;
    // Probably a good idea to save any shared pointers as members in C++
    // so the objects they point to survive as long as you need them

    QList<QByteArray> headersData = request.rawHeaderList();
    QStringList headersList;
    for (int i = 0; i < headersData.size(); ++i) {
        qDebug()  << i
            << headersData.at(i)
            << request.rawHeader(headersData.at(i));

        headersList << QString(headersData.at(i)+":"+ request.rawHeader(headersData.at(i)));
    }

    //         qDebug() << Q_FUNC_INFO << headersData;

    std::cout << "jsRequest 4" << std::endl;
    //QTimer::singleShot(200, [verb, url, loadCallback, progressCallback, errorCallback, onResponseHeadersCallback, extraData, headersList]() {


    EM_ASM_ARGS({
        var verb = Pointer_stringify($0);
        var url = Pointer_stringify($1);
        var onLoadCallbackPointer = $2;
        var onProgressCallbackPointer = $3;
        var onErrorCallbackPointer = $4;
        var onHeadersCallback = $5;
        var handler = $8;

        Module.print("Used method: " + verb);

        var formData = new FormData();
        var extraData = Pointer_stringify($6); // request parameters
        var headersData = Pointer_stringify($7);

        if (extraData) {
            var extra = extraData.split("&");
            for (var i = 0; i < extra.length; i++) {
                console.log(extra[i].split("=")[0]);
                formData.append(extra[i].split("=")[0],extra[i].split("=")[1]);
            }
        }
        var xhr;
        xhr = new XMLHttpRequest();
        xhr.responseType = "arraybuffer";

        xhr.open(verb, url, true); //async

        if (headersData) {
            var headers = headersData.split("&");
            for (var i = 0; i < headers.length; i++) {
                 //xhr.setRequestHeader(headers[i].split(":")[0],headers[i].split(":")[1]);
                 Module.print(headers[i].split(":")[0]);
            }
        }

        xhr.onprogress = function(e) {
            Module.print("progress 1");
            switch(xhr.status) {
              case 200:
              case 206:
              case 300:
              case 301:
              case 302: {
                 Module.print("progress 2");
                 var date = xhr.getResponseHeader('Last-Modified');
                 Module.print("progress 3");
                 date = ((date != null) ? new Date(date).getTime() / 1000 : 0);
                 Module.print("progress 4");
                 //Runtime.dynCall('viii', onProgressCallbackPointer, [e.loaded, e.total, date]);
                 Module.print("progress 5");

                 if (window.progressUpdates == undefined)
                     window.progressUpdates = [];

                 var update = {};
                 update.handler = handler;
                 update.cb = onProgressCallbackPointer;
                 update.loaded = e.loaded;
                 update.total = e.total;
                 update.date = date;
              }
             break;
           }
            Module.print("progress 6");
        };

        xhr.onreadystatechange = function() {
            if (xhr.readyState == xhr.DONE) {

               var responseStr = xhr.getAllResponseHeaders();
               Module.print("response headers: " + responseStr);
               //var stk = Runtime.stackSave();
               //var ptr = allocate(intArrayFromString(responseStr), 'i8', ALLOC_DEFAULT);
               //Module.print("response headers 1");
               //var arr = intArrayFromString(responseStr);
               //Module.print("response headers 2");
               //var ptr = _malloc(arr.length);
               //Module.print("response headers 3");
               //HEAPU8.set(arr, ptr);
               //Module.print("response headers 4");
               //Runtime.dynCall('vi', onHeadersCallback, [ptr]);
               //Module.print("response headers 5");
               //_free(ptr);
               //Runtime.stackRestore();
               Module.print("response headers 6");

               var response = {};
               response.handler = handler;
               response.cb = onHeadersCallback;
               response.data = responseStr;
               //window.responseHeaders = response;

               if (window.responseHeaders == undefined)
                   window.responseHeaders = [];

               window.responseHeaders.push(response);
               Module.print("response headers 7");
              }
        };

        xhr.onload = function(e) {
            var byteArray = new Uint8Array(this.response);
            Module.print("response: " + this.response + " " + byteArray.length);
            var response = {};
            response.handler = handler;
            response.cb = onLoadCallbackPointer;
            response.data = byteArray;
            response.readyState = this.readyState;

            if (window.responses == undefined)
                window.responses = [];

            window.responses.push(response);
            //window.response = response;
            //var buffer = _malloc(byteArray.length);
            //HEAPU8.set(byteArray, buffer);
            //Module.Runtime.dynCall('viii', onLoadCallbackPointer, [this.readyState, buffer, byteArray.length]);
            //_free(buffer);
        };

        // error
        xhr.onerror = function(e) {
            Runtime.dynCall('vii', onErrorCallbackPointer, [xhr.readyState, xhr.status]);

                            var responseStr = xhr.getAllResponseHeaders();
                            var ptr = allocate(intArrayFromString(responseStr), 'i8', ALLOC_STACK);
                            Runtime.dynCall('vi', onHeadersCallback, [ptr]);
                            _free(ptr);
        };
        Module.print("Send xhr: " + verb + ": " + url);
        //TODO other operations, handle user/pass, handle binary data
       //xhr.setRequestHeader(header, value);
        xhr.send(formData);

      }, verb.toLatin1().data(),
                    url.toLatin1().data(),
                    loadCallback,
                    progressCallback,
                    errorCallback,
                    onResponseHeadersCallback,
                    extraData.toLatin1().data(),
                    headersList.join("&").toLatin1().data(),
                    this
                    );
}

void QNetworkReplyEmscriptenImplPrivate::emitReplyError(QNetworkReply::NetworkError errorCode)
{
    qDebug() << Q_FUNC_INFO << this << errorCode;

    Q_Q(QNetworkReplyEmscriptenImpl);
    emit q->error(QNetworkReply::UnknownNetworkError);

    emit q->finished();
    //qApp->processEvents();
}

void QNetworkReplyEmscriptenImplPrivate::emitDataReadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    totalDownloadSize = bytesTotal;

    percentFinished = (bytesReceived / bytesTotal) * 100;
    qDebug() << Q_FUNC_INFO << bytesReceived << bytesTotal << percentFinished << "%";
}

void QNetworkReplyEmscriptenImplPrivate::dataReceived(char *buffer)
{
    Q_Q(QNetworkReplyEmscriptenImpl);

    int bufferSize = strlen(buffer);

    if (bufferSize > 0)
        q->setReadBufferSize(bufferSize);

    bytesDownloaded = bufferSize;

    if (percentFinished != 100) {
        downloadBufferCurrentSize += bufferSize;
    } else {
        downloadBufferCurrentSize = bufferSize;
    }
    totalDownloadSize = downloadBufferCurrentSize;

    emit q->downloadProgress(bytesDownloaded, totalDownloadSize);
    //qApp->processEvents();

    qDebug() << Q_FUNC_INFO <<"current size" << downloadBufferCurrentSize;
    qDebug() << Q_FUNC_INFO <<"total size" << totalDownloadSize;// /*<< (int)state*/ << (char *)buffer;

    downloadZerocopyBuffer = buffer;
//     q->setAttribute(QNetworkRequest::DownloadBufferAttribute, downloadZerocopyBuffer);

     if (downloadBufferCurrentSize == totalDownloadSize) {
         q->setFinished(true);
         emit q->readyRead();
         emit q->finished();
     }

    emit q->metaDataChanged();

    //qApp->processEvents();
}

//taken from qnetworkrequest.cpp
static int parseHeaderName(const QByteArray &headerName)
{
    if (headerName.isEmpty())
        return -1;

    switch (tolower(headerName.at(0))) {
    case 'c':
        if (qstricmp(headerName.constData(), "content-type") == 0)
            return QNetworkRequest::ContentTypeHeader;
        else if (qstricmp(headerName.constData(), "content-length") == 0)
            return QNetworkRequest::ContentLengthHeader;
        else if (qstricmp(headerName.constData(), "cookie") == 0)
            return QNetworkRequest::CookieHeader;
        break;

    case 'l':
        if (qstricmp(headerName.constData(), "location") == 0)
            return QNetworkRequest::LocationHeader;
        else if (qstricmp(headerName.constData(), "last-modified") == 0)
            return QNetworkRequest::LastModifiedHeader;
        break;

    case 's':
        if (qstricmp(headerName.constData(), "set-cookie") == 0)
            return QNetworkRequest::SetCookieHeader;
        else if (qstricmp(headerName.constData(), "server") == 0)
            return QNetworkRequest::ServerHeader;
        break;

    case 'u':
        if (qstricmp(headerName.constData(), "user-agent") == 0)
            return QNetworkRequest::UserAgentHeader;
        break;
    }

    return -1; // nothing found
}


void QNetworkReplyEmscriptenImplPrivate::headersReceived(char *buffer)
{
    Q_Q(QNetworkReplyEmscriptenImpl);

    QStringList headers = QString(buffer).split("\r\n", QString::SkipEmptyParts);

    for (int i = 0; i < headers.size(); i++) {
        QString headerName = headers.at(i).split(": ").at(0);
        QString headersValue = headers.at(i).split(": ").at(1);
        if (headerName.isEmpty() || headersValue.isEmpty())
            continue;
qDebug() << Q_FUNC_INFO << headerName << headersValue;

        int headerIndex = parseHeaderName(headerName.toLocal8Bit());

        if (headerIndex == -1)
            q->setRawHeader(headerName.toLocal8Bit(), headersValue.toLocal8Bit());
        else
            q->setHeader(static_cast<QNetworkRequest::KnownHeaders>(headerIndex), (QVariant)headersValue);
    }
}

void QNetworkReplyEmscriptenImplPrivate::_q_bufferOutgoingDataFinished()
{
    Q_Q(QNetworkReplyEmscriptenImpl);

    qDebug() << Q_FUNC_INFO << "readChannelFinished";
    // make sure this is only called once, ever.
    //_q_bufferOutgoingData may call it or the readChannelFinished emission
    if (state != Buffering)
        return;

    // disconnect signals
    QObject::disconnect(outgoingData, SIGNAL(readyRead()), q, SLOT(_q_bufferOutgoingData()));
    QObject::disconnect(outgoingData, SIGNAL(readChannelFinished()), q, SLOT(_q_bufferOutgoingDataFinished()));

    // finally, start the request
    doSendRequest();
}

void QNetworkReplyEmscriptenImplPrivate::_q_bufferOutgoingData()
{
    Q_Q(QNetworkReplyEmscriptenImpl);

    qDebug() << Q_FUNC_INFO <<"readyRead";

    if (!outgoingDataBuffer) {
        // first call, create our buffer
        outgoingDataBuffer = QSharedPointer<QRingBuffer>::create();

        QObject::connect(outgoingData, SIGNAL(readyRead()), q, SLOT(_q_bufferOutgoingData()));
        QObject::connect(outgoingData, SIGNAL(readChannelFinished()), q, SLOT(_q_bufferOutgoingDataFinished()));
    }

    qint64 bytesBuffered = 0;
    qint64 bytesToBuffer = 0;

    // read data into our buffer
    forever {
        bytesToBuffer = outgoingData->bytesAvailable();
        // unknown? just try 2 kB, this also ensures we always try to read the EOF
        if (bytesToBuffer <= 0)
            bytesToBuffer = 2*1024;

        char *dst = outgoingDataBuffer->reserve(bytesToBuffer);
        bytesBuffered = outgoingData->read(dst, bytesToBuffer);

        qDebug() << Q_FUNC_INFO << "bytesToBuffer" << bytesToBuffer
                 << "bytesBuffered" << bytesBuffered;

        if (bytesBuffered == -1) {
            // EOF has been reached.
            outgoingDataBuffer->chop(bytesToBuffer);

            _q_bufferOutgoingDataFinished();
            break;
        } else if (bytesBuffered == 0) {
            // nothing read right now, just wait until we get called again
            outgoingDataBuffer->chop(bytesToBuffer);

            break;
        } else {
            // don't break, try to read() again
            outgoingDataBuffer->chop(bytesToBuffer - bytesBuffered);
        }
    }
}

QT_END_NAMESPACE

#include "moc_qnetworkreplyemscriptenimpl_p.cpp"

