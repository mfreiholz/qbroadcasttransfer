#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
class QTcpSocket;
class QUdpSocket;

class Client : public QObject
{
  Q_OBJECT
public:
  explicit Client(QObject *parent = 0);
  bool init();

private slots:
  void connectTo(const QHostAddress &address, quint16 port);
  void onReadyRead();
  void onReadPendingDatagram();

private:
  QTcpSocket *_socket;
  QByteArray _buffer;
  qint32 _packetSize;
  QUdpSocket *_dataSocket;
};

#endif // CLIENT_H
