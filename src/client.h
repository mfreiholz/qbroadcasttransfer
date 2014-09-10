#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QAbstractTableModel>
#include "protocol.h"
class QTcpSocket;
class QUdpSocket;
class ClientServerConnectionHandler;


class Client : public QObject
{
  Q_OBJECT
  friend class ClientServerConnectionHandler;
  friend class ClientFilesModel;

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
  void filesChanged();

private:
  ClientServerConnectionHandler *_serverConnection;
  QUdpSocket *_dataSocket;

  // Files
  QList<FileInfoPtr> _files;
};


class ClientServerConnectionHandler : public QObject
{
  Q_OBJECT

public:
  explicit ClientServerConnectionHandler(Client *client, QObject *parent = 0);
  virtual ~ClientServerConnectionHandler();
  QTcpSocket* getSocket() const { return _socket; }

public slots:
  void connectToHost(const QHostAddress &address, quint16 port);
  void sendKeepAlive();

protected:
  virtual void timerEvent(QTimerEvent *ev);

private slots:
  void onStateChanged(QAbstractSocket::SocketState state);
  void onReadyRead();

private:
  void processRequest(TCP::Request &request);
  void processKeepAlive(TCP::Request &request, QDataStream &in);
  void processFileRegister(TCP::Request &request, QDataStream &in);
  void processFileUnregister(TCP::Request &request, QDataStream &in);

signals:
  void disconnected();

private:
  Client *_client;
  QTcpSocket *_socket;
  TCP::ProtocolHandler _protocol;
  int _keepAliveTimerId;
};


class ClientFilesModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Columns {
    FileIdColumn,
    FileNameColumn,
    FileSizeColumn
  };

  explicit ClientFilesModel(Client *client, QObject *parent = 0);

  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private slots:
  void onClientFilesChanged();

private:
  Client *_client;
  QHash<int, QString> _columnHeaders;
};


#endif // CLIENT_H
