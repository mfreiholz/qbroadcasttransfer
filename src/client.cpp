#include <QTcpSocket>
#include <QUdpSocket>
#include <QDataStream>
#include "client.h"
#include "protocol.h"

Client::Client(QObject *parent) :
  QObject(parent)
{
  _socket = new QTcpSocket(this);
  _packetSize = -1;
  connect(_socket, SIGNAL(readyRead()), SLOT(onReadyRead()));

  _dataSocket = new QUdpSocket(this);
  connect(_dataSocket, SIGNAL(readyRead()), SLOT(onReadPendingDatagram()));
}

bool Client::init()
{
  bool error = false;
  if (!_dataSocket->bind(UDPPORTCLIENT, QUdpSocket::ShareAddress)) {
    error = true;
  }
  if (error) {
    _dataSocket->close();
  }
  return !error;
}

void Client::connectTo(const QHostAddress &address, quint16 port)
{
  if (_socket->isOpen()) {
    _socket->close();
  }
  _socket->connectToHost(address, port);
}

void Client::onReadyRead()
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
        if (_socket->state() == QAbstractSocket::UnconnectedState) {
          quint16 tcpPort;
          in >> tcpPort;
          connectTo(sender, tcpPort);
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
