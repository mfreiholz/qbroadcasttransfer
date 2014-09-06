#include <QTcpSocket>
#include <QUdpSocket>
#include <QDataStream>
#include <QTimerEvent>
#include "client.h"
#include "protocol.h"

Client::Client(QObject *parent) :
  QObject(parent),
  _serverConnection(0)
{
  qRegisterMetaType<QHostAddress>("QHostAddress");

  _serverConnection = new ClientServerConnectionHandler(this);
  connect(_serverConnection->getSocket(), SIGNAL(connected()), SIGNAL(serverConnected()));
  connect(_serverConnection->getSocket(), SIGNAL(disconnected()), SIGNAL(serverDisconnected()));

  _dataSocket = new QUdpSocket(this);
  connect(_dataSocket, SIGNAL(readyRead()), SLOT(onReadPendingDatagram()));
}

Client::~Client()
{
  delete _serverConnection;
  delete _dataSocket;
}

bool Client::listen()
{
  if (_dataSocket->state() != QAbstractSocket::BoundState) {
    return _dataSocket->bind(UDPPORTCLIENT, QUdpSocket::ShareAddress);
  }
  return false;
}

void Client::close()
{
  _dataSocket->close();
}

const ClientServerConnectionHandler& Client::getServerConnection() const
{
  return *_serverConnection;
}

void Client::connectToServer(const QHostAddress &address, quint16 port)
{
  _serverConnection->connectToHost(address, port);
}

void Client::onReadPendingDatagram()
{
  while (_dataSocket->hasPendingDatagrams()) {
    QByteArray datagram;
    QHostAddress sender;
    quint16 senderPort;
    datagram.resize(_dataSocket->pendingDatagramSize());
    _dataSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

    // Process datagram.
    QDataStream in(datagram);

    quint8 magic = 0;
    in >> magic;
    if (magic != DGMAGICBIT)
      continue;

    quint8 type;
    in >> type;

    switch (type) {
      case DGHELLO:
        if (_serverConnection->getSocket()->state() == QAbstractSocket::UnconnectedState) {
          quint16 tcpPort;
          in >> tcpPort;
          connectToServer(sender, tcpPort);
          emit serverBroadcastReceived(sender, tcpPort);
        }
        break;
      case DGDATA:
        quint32 fileId;
        in >> fileId;
        quint64 fileDataOffset;
        in >> fileDataOffset;
        QByteArray fileData;
        in >> fileData;
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ClientServerConnectionHandler
///////////////////////////////////////////////////////////////////////////////

ClientServerConnectionHandler::ClientServerConnectionHandler(QObject *parent) :
  QObject(parent),
  _socket(new QTcpSocket(this)),
  _keepAliveTimerId(-1)
{
  connect(_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), SLOT(onStateChanged(QAbstractSocket::SocketState)));
  connect(_socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
}

ClientServerConnectionHandler::~ClientServerConnectionHandler()
{
  delete _socket;
  if (_keepAliveTimerId >= 0) {
    killTimer(_keepAliveTimerId);
  }
}

void ClientServerConnectionHandler::connectToHost(const QHostAddress &address, quint16 port)
{
  _socket->connectToHost(address, port);
}

void ClientServerConnectionHandler::sendKeepAlive()
{
  qDebug() << "Send keep-alive to server.";

  QByteArray body;
  QDataStream out(&body, QIODevice::WriteOnly);
  out << QString("keep-alive");

  TCP::Request req;
  req.setBody(body);
  _socket->write(_protocol.serialize(req));
}

void ClientServerConnectionHandler::timerEvent(QTimerEvent *ev)
{
  if (ev->timerId() == _keepAliveTimerId) {
    //sendKeepAlive();
  }
}

void ClientServerConnectionHandler::onStateChanged(QAbstractSocket::SocketState state)
{
  switch (state) {
    case QAbstractSocket::ConnectedState:
      _keepAliveTimerId = startTimer(1500);
      break;
    case QAbstractSocket::UnconnectedState:
      killTimer(_keepAliveTimerId);
      _keepAliveTimerId = -1;
      emit disconnected();
      break;
  }
}

void ClientServerConnectionHandler::onReadyRead()
{
  qint64 available = 0;
  while ((available = _socket->bytesAvailable()) > 0) {
    QByteArray data = _socket->read(available);
    _protocol.append(data);
  }

  TCP::Request *request = 0;
  while ((request = _protocol.next()) != 0) {
    switch (request->header.type) {
      case TCP::Request::Header::REQ:
        qDebug() << QString("Request from server.");
        processRequest(*request);
        break;
      case TCP::Request::Header::RESP:
        qDebug() << QString("Response from server.");
        //processRequest(*request);
        break;
    }
    delete request;
  }
}

void ClientServerConnectionHandler::processRequest(TCP::Request &request)
{
  QDataStream in(request.body);
  QString action;
  in >> action;
  if (action == "keep-alive") {
    TCP::Request resp;
    resp.initResponseByRequest(request);
    _socket->write(_protocol.serialize(resp));
  } else if (action == "/file/register") {
    FileInfo info;
    in >> info;
    qDebug() << QString("Register new file %1 %2").arg(info.id).arg(info.path);
  } else if (action == "filelist") {
    processFileList(request, in);
  }
}

void ClientServerConnectionHandler::processFileList(TCP::Request &request, QDataStream &in)
{
  // Parse file list.
  // (TODO)

  // Send response.
  TCP::Request response;
  response.initResponseByRequest(request);
  const QByteArray ser = _protocol.serialize(request);
  _socket->write(ser);
}


/*void Client::onReadyRead()
{
  while (_socket->bytesAvailable() > 0) {
    if (_packetSize == -1) {
      if (_socket->bytesAvailable() < 5) {
        return;
      }
      QDataStream in(_socket);

      quint8 magicbyte;
      in >> magicbyte;
      if (magicbyte != CPMAGICBYTE) {
        qDebug() << "Invalid magicbyte.";
        _socket->disconnectFromHost();
        return;
      }

      quint32 length;
      in >> length;
      _packetSize = length;
    }

    // Read entire packet content into buffer.
    if (_buffer.size() < _packetSize) {
      QByteArray b = _socket->read(_packetSize - _buffer.size());
      _buffer.append(b);
    }
    if (_buffer.size() != _packetSize) {
      return;
    }

    // Process packet.
    QDataStream in(_buffer);
    quint8 type;
    in >> type;
    switch (type) {
      case CPFILELIST: {
        QList<FileInfo> fileInfos;
        quint32 fileCount;
        in >> fileCount;
        for (quint32 i = 0; i < fileCount; ++i) {
          FileInfo fi;
          in >> fi.id;
          in >> fi.path;
          in >> fi.size;
          in >> fi.partSize;
          in >> fi.partCount;
          fileInfos.append(fi);
          qDebug() << fi.path;
        }
        break;
      }
    }

    // End of current packet.
    _packetSize = -1;
    _buffer.clear();
  }
}*/