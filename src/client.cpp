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

  _serverConnection = new ClientServerConnectionHandler(this, this);
  connect(_serverConnection->getSocket(), SIGNAL(connected()), SIGNAL(serverConnected()));
  connect(_serverConnection->getSocket(), SIGNAL(disconnected()), SIGNAL(serverDisconnected()));

  _dataSocket = new ClientUdpSocket(this, this);
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

///////////////////////////////////////////////////////////////////////////////
// ClientUdpSocket
///////////////////////////////////////////////////////////////////////////////

ClientUdpSocket::ClientUdpSocket(Client *client, QObject *parent) :
  QUdpSocket(parent),
  _client(client)
{
  connect(this, SIGNAL(readyRead()), SLOT(onReadPendingDatagram()));
}

void ClientUdpSocket::onReadPendingDatagram()
{
  while (hasPendingDatagrams()) {
    QByteArray datagram;
    QHostAddress sender;
    quint16 senderPort;
    datagram.resize(pendingDatagramSize());
    readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

    QDataStream in(datagram);

    quint8 magic = 0;
    in >> magic;
    if (magic != DGMAGICBIT) {
      qDebug() << QString("Datagram with invalid magic byte (size=%1)").arg(datagram.size());
      continue;
    }

    quint8 type;
    in >> type;
    switch (type) {
      case DGHELLO:
        processHello(in, sender, senderPort);
        break;
      case DGDATA:
        processFileData(in, sender, senderPort);
        break;
      default:
        qDebug() << QString("Unknown datagram type");
        break;
    }
  }
}

void ClientUdpSocket::processHello(QDataStream &in, const QHostAddress &sender, quint16 senderPort)
{
  quint16 serverPort;
  in >> serverPort;
  if (_client->_serverConnection->getSocket()->state() == QAbstractSocket::UnconnectedState) {
    _client->connectToServer(sender, serverPort);
    emit _client->serverBroadcastReceived(sender, serverPort);
  }
}

void ClientUdpSocket::processFileData(QDataStream &in, const QHostAddress &sender, quint16 senderPort)
{
  FileData fd;
  in >> fd;
  
  FileInfoPtr info = _client->_files2id.value(fd.id);
  if (!info) {
    qDebug() << QString("Received file data with unknown file-id (id=%1)").arg(fd.id);
    return;
  }

  qDebug() << QString("Received file data (id=%1; index=%2; size=%3)").arg(fd.id).arg(fd.index).arg(fd.data.size());
}

///////////////////////////////////////////////////////////////////////////////
// ClientServerConnectionHandler
///////////////////////////////////////////////////////////////////////////////

ClientServerConnectionHandler::ClientServerConnectionHandler(Client *client, QObject *parent) :
  QObject(parent),
  _client(client),
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
  qDebug() << "Send /keepalive to server.";

  QByteArray body;
  QDataStream out(&body, QIODevice::WriteOnly);
  out << QString("/keepalive");

  TCP::Request req;
  req.setBody(body);
  _socket->write(_protocol.serialize(req));
}

void ClientServerConnectionHandler::timerEvent(QTimerEvent *ev)
{
  if (ev->timerId() == _keepAliveTimerId) {
    sendKeepAlive();
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
        processRequest(*request);
        break;
      case TCP::Request::Header::RESP:
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
  if (action == "/keepalive") {
    processKeepAlive(request, in);
  } else if (action == "/file/register") {
    processFileRegister(request, in);
  }
}

void ClientServerConnectionHandler::processKeepAlive(TCP::Request &request, QDataStream &in)
{
  TCP::Request response;
  response.initResponseByRequest(request);
  _socket->write(_protocol.serialize(response));
}

void ClientServerConnectionHandler::processFileRegister(TCP::Request &request, QDataStream &in)
{
  FileInfoPtr info(new FileInfo());
  in >> *info.data();

  _client->_files.append(info);
  _client->_files2id.insert(info->id, info);
  emit _client->filesChanged();

  qDebug() << QString("Registered new file on client: %1 %2").arg(info->id).arg(info->path);

  TCP::Request response;
  response.initResponseByRequest(request);
  _socket->write(_protocol.serialize(response));
}

void ClientServerConnectionHandler::processFileUnregister(TCP::Request &request, QDataStream &in)
{
  FileInfo::fileid_t id;
  in >> id;

  for (int i = 0; i < _client->_files.size(); ++i) {
    FileInfoPtr fi = _client->_files.at(i);
    if (fi->id == id) {
      _client->_files.removeAt(i);
      _client->_files2id.remove(id);
      break;
    }
  }
  emit _client->filesChanged();

  qDebug() << QString("Unregistered file on client: %1").arg(id);

  TCP::Request response;
  response.initResponseByRequest(request);
  _socket->write(_protocol.serialize(response));
}

///////////////////////////////////////////////////////////////////////////////
// ClientFilesModel
///////////////////////////////////////////////////////////////////////////////

ClientFilesModel::ClientFilesModel(Client *client, QObject *parent) :
  QAbstractTableModel(parent),
  _client(client)
{
  _columnHeaders.insert(FileIdColumn, tr("Id"));
  _columnHeaders.insert(FileNameColumn, tr("Name"));
  _columnHeaders.insert(FileSizeColumn, tr("Size"));
  connect(_client, SIGNAL(filesChanged()), SLOT(onClientFilesChanged()));
}

int ClientFilesModel::columnCount(const QModelIndex &parent) const
{
  Q_UNUSED(parent)
  return _columnHeaders.size();
}

QVariant ClientFilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  switch (orientation) {
    case Qt::Horizontal:
      switch (role) {
        case Qt::DisplayRole:
          return _columnHeaders.value(section);
      }
      break;
  }
  return QVariant();
}

int ClientFilesModel::rowCount(const QModelIndex &parent) const
{
  Q_UNUSED(parent)
  return _client->_files.size();
}

QVariant ClientFilesModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() >= _client->_files.size()) {
    return QVariant();
  }
  switch (role) {
    case Qt::DisplayRole:
      switch (index.column()) {
        case FileIdColumn:
          return _client->_files.at(index.row())->id;
        case FileNameColumn:
          return _client->_files.at(index.row())->path;
        case FileSizeColumn:
          return _client->_files.at(index.row())->size;
      }
      break;
  }
  return QVariant();
}

void ClientFilesModel::onClientFilesChanged()
{
  beginResetModel();
  endResetModel();
}
