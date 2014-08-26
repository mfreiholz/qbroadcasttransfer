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
#include "server.h"
#include "protocol.h"

///////////////////////////////////////////////////////////////////////////////
// Server
///////////////////////////////////////////////////////////////////////////////

Server::Server(QObject *parent) :
  QObject(parent)
{
  qRegisterMetaType<ServerConnectionHandler*>("ServerConnectionHandler*");

  _tcpServer = new QTcpServer(this);
  connect(_tcpServer, SIGNAL(newConnection()), SLOT(onNewConnection()));

  _dataSocket = new QUdpSocket(this);
  connect(_dataSocket, SIGNAL(readyRead()), SLOT(onReadPendingDatagram()));
}

bool Server::init()
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

void Server::disconnectFromClients()
{
  while (!_connections.isEmpty()) {
    ServerConnectionHandler *handler = _connections.takeLast();
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
  QByteArray data;
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

  foreach (ServerConnectionHandler *handler, _connections) {
    handler->_socket->write(data);
    handler->_socket->flush();
  }
  _files = files;
}

void Server::onNewConnection()
{
  while (_tcpServer->hasPendingConnections()) {
    QTcpSocket *socket = _tcpServer->nextPendingConnection();
    const int val = 1;
    ::setsockopt(socket->socketDescriptor(), IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val));
    ServerConnectionHandler *handler = new ServerConnectionHandler(this, socket);
    _connections.append(handler);
    emit clientConnected(handler);
  }
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
// ServerConnectionHandler
///////////////////////////////////////////////////////////////////////////////

ServerConnectionHandler::ServerConnectionHandler(Server *server, QTcpSocket *socket) :
  QObject(server),
  _server(server),
  _socket(socket)
{
  connect(_socket, SIGNAL(disconnected()), SLOT(onDisconnected()));
  connect(_socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
}

void ServerConnectionHandler::onDisconnected()
{
  deleteLater();
  _socket->close();
  _server->_connections.removeAll(this);
  emit _server->clientDisconnected(this);
}

void ServerConnectionHandler::onReadyRead()
{
  QByteArray data = _socket->readAll();
  //_buffer.append(data);
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
  : QAbstractListModel(server)
{
  _server = server;
  connect(_server, SIGNAL(clientConnected(ServerConnectionHandler*)), SLOT(onServerChanged()));
  connect(_server, SIGNAL(clientDisconnected(ServerConnectionHandler*)), SLOT(onServerChanged()));
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

  ServerConnectionHandler *handler = _server->_connections[index.row()];
  switch (role) {
    case Qt::DisplayRole:
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
