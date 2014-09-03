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
  foreach (const FileInfo &info, _files) {
    ServerFileBroadcastTask *task = new ServerFileBroadcastTask(info, this);
    _pool.start(task);
  }
}

void Server::registerFileList(const QList<FileInfo> &files)
{
  /*QByteArray data;
  QDataStream out(&data, QIODevice::WriteOnly);
  out << CPMAGICBYTE;
  out << (quint32) 0;
  out << CPFILELIST;
  out << (quint32) files.size();

  foreach (const FileInfo &fi, files) {
    out << fi.id;
    out << fi.path;
    out << fi.size;
    out << fi.partSize;
    out << fi.partCount;
  }
  out.device()->seek(sizeof(CPMAGICBYTE));
  quint32 size = data.size();
  size -= sizeof(CPMAGICBYTE);
  size -= sizeof(quint32);
  out << size;

  foreach (ServerClientConnectionHandler *handler, _connections) {
    handler->_socket->write(data);
    handler->_socket->flush();
  }
  _files = files;*/
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

void ServerClientConnectionHandler::timerEvent(QTimerEvent *ev)
{
  if (ev->timerId() == _keepAliveTimerId) {
    QByteArray body = "keep-alive";
    TCP::Request req;
    req.header.size = body.size();
    req.body = body;
    _socket->write(_protocol.serialize(req));
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
      qDebug() << "New request from client." << request->body << request->header.correlationId;
      processRequest(*request);
      break;
    case TCP::Request::Header::RESP:
      qDebug() << "New response from client." << request->body << request->header.correlationId;
      //processRequest(*request);
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
  if (action == "") {
  }
}

///////////////////////////////////////////////////////////////////////////////
// ServerFileBroadcastTask
///////////////////////////////////////////////////////////////////////////////

ServerFileBroadcastTask::ServerFileBroadcastTask(const FileInfo &info, QObject *parent) :
  QObject(parent),
  QRunnable(),
  _info(info)
{
}

void ServerFileBroadcastTask::run()
{
  QUdpSocket socket;
  QFile f(_info.path);
  if (!f.open(QIODevice::ReadOnly)) {
    return;
  }
  quint32 index = 0;
  while (!f.atEnd()) {
    QByteArray datagram;
    QDataStream out(&datagram, QIODevice::WriteOnly);
    out << _info.id;
    out << index;
    out << f.read(400);
    if (socket.writeDatagram(datagram, QHostAddress::Broadcast, UDPPORTCLIENT) > 0)
      emit bytesWritten(_info.id, index * 400, _info.size);
  }
  f.close();
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

QHash<int, QByteArray> ServerModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[RemoteAddressRole] = "address";
  roles[RemoteTcpPortRole] = "tcpport";
  return roles;
}

void ServerModel::onServerChanged()
{
  beginResetModel();
  endResetModel();
}
