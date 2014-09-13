#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QAbstractTableModel>
#include <QUdpSocket>
#include "protocol.h"
class QTcpSocket;


class Client : public QObject
{
  Q_OBJECT
  friend class ClientUdpSocket;
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
  QHash<FileInfo::fileid_t, FileInfoPtr> _files2id;
};


class ClientUdpSocket : public QUdpSocket
{
  Q_OBJECT

public:
  explicit ClientUdpSocket(Client *client, QObject *parent);

private slots:
  void onReadPendingDatagram();
  void processHello(QDataStream &in, const QHostAddress &sender, quint16 senderPort);
  void processFileData(QDataStream &in, const QHostAddress &sender, quint16 senderPort);

private:
  Client *_client;
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
