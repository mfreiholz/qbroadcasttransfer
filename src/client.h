#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include "protocol.h"
class QTcpSocket;
class QUdpSocket;
class ClientServerConnectionHandler;


class Client : public QObject
{
  Q_OBJECT

public:
  explicit Client(QObject *parent = 0);
  virtual ~Client();
  bool listen();
  void close();
  const ClientServerConnectionHandler& getServerConnection() const;

public slots:
  void connectToServer(const QHostAddress &address, quint16 port);

private slots:
  void onReadPendingDatagram();

signals:
  void serverBroadcastReceived(const QHostAddress &address, quint16 port);
  void serverConnected();
  void serverDisconnected();

private:
  ClientServerConnectionHandler *_serverConnection;
  QUdpSocket *_dataSocket;
};


class ClientServerConnectionHandler : public QObject
{
  Q_OBJECT

public:
  explicit ClientServerConnectionHandler(QObject *parent = 0);
  virtual ~ClientServerConnectionHandler();
  QTcpSocket* getSocket() const { return _socket; }

public slots:
  void connectToHost(const QHostAddress &address, quint16 port);

protected:
  virtual void timerEvent(QTimerEvent *ev);

private slots:
  void onStateChanged(QAbstractSocket::SocketState state);
  void onReadyRead();

private:
  void processRequest(TCP::Request &request);
  void processFileList(TCP::Request &request, QDataStream &in);

signals:
  void disconnected();

private:
  QTcpSocket *_socket;
  TCP::ProtocolHandler _protocol;
  int _keepAliveTimerId;
};

#endif // CLIENT_H
