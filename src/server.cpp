#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#define NOMINMAX
#include <Winsock2.h>
#endif
#include <QTcpSocket>
#include <QUdpSocket>
#include <QFile>
#include <QTimerEvent>
#include "server.h"
#include "protocol.h"

///////////////////////////////////////////////////////////////////////////////
// Server
///////////////////////////////////////////////////////////////////////////////

Server::Server(QObject *parent) :
  QObject(parent)
{
  qRegisterMetaType<ServerClientConnectionHandler*>("ServerClientConnectionHandler*");

  _tcpServer = new QTcpServer(this);
  connect(_tcpServer, SIGNAL(newConnection()), SLOT(onClientConnected()));

  _dataSocket = new QUdpSocket(this);
  connect(_dataSocket, SIGNAL(readyRead()), SLOT(onReadPendingDatagram()));

  _pool.setMaxThreadCount(1);
}

Server::~Server()
{
  shutdown();
  delete _tcpServer;
  delete _dataSocket;
}

bool Server::startup()
{
  bool error = false;
  if (!_tcpServer->listen(QHostAddress::Any, TCPPORTSERVER)) {
    error = true;
  }
  if (!_dataSocket->bind(UDPPORTSERVER, QUdpSocket::DontShareAddress)) {
    error = true;
  }
  if (error) {
    _tcpServer->close();
    _dataSocket->close();
  }
  return !error;
}

bool Server::shutdown()
{
  _tcpServer->close();
  _dataSocket->close();
  while (!_connections.isEmpty()) {
    ServerClientConnectionHandler *handler = _connections.takeLast();
    delete handler;
  }
  return true;
}

bool Server::isStarted()
{
  return _tcpServer->isListening() && _dataSocket->state() == QAbstractSocket::BoundState;
}

void Server::disconnectFromClients()
{
  while (!_connections.isEmpty()) {
    ServerClientConnectionHandler *handler = _connections.takeLast();
    handler->_socket->disconnectFromHost();
  }
}

void Server::registerFile(const QString &filePath)
{
  FileInfoPtr fileInfo(new FileInfo());
  if (!fileInfo->fromFile(filePath)) {
    return;
  }
  fileInfo->id = FileInfo::nextUniqueId();
  _files.append(fileInfo);

  foreach (auto conn, _connections) {
    conn->sendRegisterFile(*fileInfo.data());
  }
}

void Server::unregisterFile(FileInfo::fileid_t id)
{
  for (int i = 0; i < _files.size(); ++i) {
    if (_files[i]->id == id) {
      _files.removeAt(i);
      break;
    }
  }

  foreach (auto conn, _connections) {
    conn->sendUnregisterFile(id);
  }
}

void Server::broadcastHello()
{
  QByteArray datagram;
  QDataStream out(&datagram, QIODevice::WriteOnly);
  out << DGMAGICBIT;
  out << DGHELLO;
  out << _tcpServer->serverPort();
  _dataSocket->writeDatagram(datagram, QHostAddress::Broadcast, UDPPORTCLIENT);
}

void Server::broadcastFiles()
{
  //foreach (const FileInfo &info, _files) {
  //  ServerFileBroadcastTask *task = new ServerFileBroadcastTask(info, this);
  //  _pool.start(task);
  //}
}

void Server::onClientConnected()
{
  while (_tcpServer->hasPendingConnections()) {
    QTcpSocket *socket = _tcpServer->nextPendingConnection();
    const int val = 1;
    ::setsockopt(socket->socketDescriptor(), IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val));
    ServerClientConnectionHandler *handler = new ServerClientConnectionHandler(socket, this);
    connect(handler, SIGNAL(disconnected()), SLOT(onClientDisconnected()));
    _connections.append(handler);
    emit clientConnected(handler);
  }
}

void Server::onClientDisconnected()
{
  ServerClientConnectionHandler *handler = qobject_cast<ServerClientConnectionHandler*>(sender());
  _connections.removeAll(handler);
  handler->deleteLater();
  emit clientDisconnected(handler);
}

void Server::onReadPendingDatagram()
{
  while (_dataSocket->hasPendingDatagrams()) {
    QByteArray datagram;
    QHostAddress sender;
    quint16 senderPort;
    datagram.resize(_dataSocket->pendingDatagramSize());
    _dataSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
    
    qDebug() << "Datagram: " + datagram;

    // Process datagram.
    QDataStream in(datagram);

    quint8 magic = 0;
    in >> magic;
    if (magic != DGMAGICBIT)
      continue;

    quint8 type;
    in >> type;

    switch (type) {
      case DGDATAREQ:
        quint32 fileId;
        in >> fileId;
        quint64 fileDataOffset;
        in >> fileDataOffset;
        _requestedFileParts[fileId].insert(fileDataOffset);
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ServerClientConnectionHandler
///////////////////////////////////////////////////////////////////////////////

ServerClientConnectionHandler::ServerClientConnectionHandler(QTcpSocket *socket, QObject *parent) :
  QObject(parent),
  _socket(socket),
  _keepAliveTimerId(-1)
{
  onStateChanged(socket->state());
  connect(_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), SLOT(onStateChanged(QAbstractSocket::SocketState)));
  connect(_socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
}

ServerClientConnectionHandler::~ServerClientConnectionHandler()
{
  delete _socket;
  if (_keepAliveTimerId >= 0) {
    killTimer(_keepAliveTimerId);
    _keepAliveTimerId = -1;
  }
}

void ServerClientConnectionHandler::sendKeepAlive()
{
  qDebug() << QString("Send /keepalive.");

  QByteArray body;
  QDataStream out(&body, QIODevice::WriteOnly);
  out << QString("/keepalive");

  TCP::Request req;
  req.setBody(body);
  _socket->write(_protocol.serialize(req));
}

void ServerClientConnectionHandler::sendRegisterFile(const FileInfo &info)
{
  qDebug() << QString("Send /file/register => %1 %2").arg(info.id).arg(info.path);

  QByteArray body;
  QDataStream out(&body, QIODevice::WriteOnly);
  out << QString("/file/register");
  out << info;

  TCP::Request req;
  req.setBody(body);
  _socket->write(_protocol.serialize(req));
}

void ServerClientConnectionHandler::sendUnregisterFile(FileInfo::fileid_t id)
{
  qDebug() << QString("Send /file/unregister => %1").arg(id);

  QByteArray body;
  QDataStream out(&body, QIODevice::WriteOnly);
  out << QString("/file/unregister");
  out << id;

  TCP::Request req;
  req.setBody(body);
  _socket->write(_protocol.serialize(req));
}

void ServerClientConnectionHandler::timerEvent(QTimerEvent *ev)
{
  if (ev->timerId() == _keepAliveTimerId) {
    sendKeepAlive();
  }
}

void ServerClientConnectionHandler::onStateChanged(QAbstractSocket::SocketState state)
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

void ServerClientConnectionHandler::onReadyRead()
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
        processRequest(*request);
        break;
      case TCP::Request::Header::RESP:
        processResponse(*request);
        break;
    }
    delete request;
  }
}

void ServerClientConnectionHandler::processRequest(TCP::Request &request)
{
  QDataStream in(request.body);
  QString action;
  in >> action;
  if (action == "/keepalive") {
    processKeepAlive(request, in);
  }
}

void ServerClientConnectionHandler::processResponse(TCP::Request &response)
{
  Q_UNUSED(response)
}

void ServerClientConnectionHandler::processKeepAlive(TCP::Request &request, QDataStream &in)
{
  Q_UNUSED(in)
  TCP::Request response;
  response.initResponseByRequest(request);
  _socket->write(_protocol.serialize(response));
}

///////////////////////////////////////////////////////////////////////////////
// ServerFileBroadcastTask
///////////////////////////////////////////////////////////////////////////////

ServerFileBroadcastTask::ServerFileBroadcastTask(FileInfoPtr info, QObject *parent) :
  QObject(parent),
  QRunnable(),
  _info(info)
{
}

void ServerFileBroadcastTask::run()
{
  QUdpSocket socket;
  QFile f(_info->path);
  if (!f.open(QIODevice::ReadOnly)) {
    return;
  }
  quint32 index = 0;
  while (!f.atEnd()) {
    const QByteArray fdata = f.read(400);
    if (fdata.isEmpty()) {
      break;
    }
    QByteArray datagram;
    QDataStream out(&datagram, QIODevice::WriteOnly);
    out << _info->id;
    out << index;
    out << fdata;
    if (socket.writeDatagram(datagram, QHostAddress::Broadcast, UDPPORTCLIENT) > 0) {
      emit bytesWritten(_info->id, f.pos(), _info->size);
    }
    ++index;
  }
  f.close();
  emit finished();
}

///////////////////////////////////////////////////////////////////////////////
// ServerModel
///////////////////////////////////////////////////////////////////////////////

ServerModel::ServerModel(Server *server)
  : QAbstractTableModel(server)
{
  _server = server;
  connect(_server, SIGNAL(clientConnected(ServerClientConnectionHandler*)), SLOT(onServerChanged()));
  connect(_server, SIGNAL(clientDisconnected(ServerClientConnectionHandler*)), SLOT(onServerChanged()));

  _columnHeaders.insert(RemoteAddressColumn, tr("Address"));
  _columnHeaders.insert(RemoteTcpPortColumn, tr("TCP Port"));
}

int ServerModel::columnCount(const QModelIndex &parent) const
{
  Q_UNUSED(parent)
  return _columnHeaders.size();
}

QVariant ServerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  QVariant vt;
  switch (role) {
    case Qt::DisplayRole:
      switch (orientation) {
        case Qt::Horizontal:
          vt = _columnHeaders.value(section);
          break;
      }
      break;
  }
  return vt;
}

int ServerModel::rowCount(const QModelIndex &parent) const
{
  Q_UNUSED(parent);
  return _server->_connections.size();
}

QVariant ServerModel::data(const QModelIndex &index, int role) const
{
  QVariant vt;
  if (!index.isValid() || index.row() >= _server->_connections.size())
    return vt;

  ServerClientConnectionHandler *handler = _server->_connections[index.row()];
  switch (role) {
    case Qt::DisplayRole:
      switch (index.column()) {
        case RemoteAddressColumn:
          vt = handler->_socket->peerAddress().toString();
          break;
        case RemoteTcpPortColumn:
          vt = handler->_socket->peerPort();
          break;
      }
      break;
    case RemoteAddressRole:
      vt = handler->_socket->peerAddress().toString();
      break;
    case RemoteTcpPortRole:
      vt = handler->_socket->peerPort();
      break;
  }
  return vt;
}

void ServerModel::onServerChanged()
{
  beginResetModel();
  endResetModel();
}
