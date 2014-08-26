#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QList>
#include <QHash>
#include <QSet>
#include <QByteArray>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QAbstractListModel>
#include <QRunnable>
#include <QThreadPool>
#include "api.h"
#include "protocol.h"
class QTcpSocket;
class QUdpSocket;
class ServerConnectionHandler;


class Server : public QObject
{
  Q_OBJECT
  friend class ServerConnectionHandler;
  friend class ServerModel;

signals:
  void clientConnected(ServerConnectionHandler*);
  void clientDisconnected(ServerConnectionHandler*);

public:
  explicit Server(QObject *parent = 0);
  bool init();

public slots:
  void disconnectFromClients();
  void broadcastHello();
  void broadcastFiles();
  void registerFileList(const QList<FileInfo> &files);

private slots:
  void onNewConnection();
  void onReadPendingDatagram();

private:
  QTcpServer *_tcpServer;
  QUdpSocket *_dataSocket;
  QList<ServerConnectionHandler*> _connections;
  QList<FileInfo> _files;
  QThreadPool _pool;
  QHash<quint32,QSet<quint64> > _requestedFileParts;
};
Q_DECLARE_METATYPE(Server*)


class ServerConnectionHandler : public QObject
{
  Q_OBJECT
  friend class Server;
  friend class ServerModel;

public:
  explicit ServerConnectionHandler(Server *server, QTcpSocket *socket);

private slots:
  void onDisconnected();
  void onReadyRead();

private:
  Server *_server;
  QTcpSocket *_socket;
  QByteArray _buffer;
};
Q_DECLARE_METATYPE(ServerConnectionHandler*)


class ServerFileBroadcastTask : public QObject, public QRunnable
{
  Q_OBJECT

signals:
  void bytesWritten(quint32 fileId, quint64 writtenBytes, quint64 totalBytes);

public:
  ServerFileBroadcastTask(const FileInfo &info, QObject *parent = 0);
  virtual void run();

private:
  FileInfo _info;
};


class ServerModel : public QAbstractListModel
{
  Q_OBJECT

public:
  enum Roles {
    RemoteAddressRole = Qt::UserRole + 1,
    RemoteTcpPortRole
  };

  ServerModel(Server *server);
  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

protected:
  QHash<int, QByteArray> roleNames() const;

private slots:
  void onServerChanged();

private:
  Server *_server;
};
Q_DECLARE_METATYPE(ServerModel*)

#endif // SERVER_H
