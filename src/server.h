#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QList>
#include <QHash>
#include <QSet>
#include <QByteArray>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QAbstractTableModel>
#include <QRunnable>
#include <QThreadPool>
#include "api.h"
#include "protocol.h"
class QTcpSocket;
class QUdpSocket;
class ServerClientConnectionHandler;


class Server : public QObject
{
  Q_OBJECT
  friend class ServerClientConnectionHandler;
  friend class ServerModel;

public:
  explicit Server(QObject *parent = 0);
  virtual ~Server();
  bool startup();
  bool shutdown();
  bool isStarted();

public slots:
  void disconnectFromClients();
  void broadcastHello();
  void broadcastFiles();
  void registerFileList(const QList<FileInfo> &files);

private slots:
  void onClientConnected();
  void onClientDisconnected();
  void onReadPendingDatagram();

signals:
  void clientConnected(ServerClientConnectionHandler*);
  void clientDisconnected(ServerClientConnectionHandler*);

private:
  QTcpServer *_tcpServer;
  QUdpSocket *_dataSocket;
  QList<ServerClientConnectionHandler*> _connections;
  QList<FileInfo> _files;
  QThreadPool _pool;
  QHash<quint32,QSet<quint64> > _requestedFileParts;
};
Q_DECLARE_METATYPE(Server*)


class ServerClientConnectionHandler : public QObject
{
  Q_OBJECT
  friend class Server;
  friend class ServerModel;

public:
  explicit ServerClientConnectionHandler(QTcpSocket *socket, QObject *parent = 0);
  virtual ~ServerClientConnectionHandler();

protected:
  virtual void timerEvent(QTimerEvent *ev);

private slots:
  void onStateChanged(QAbstractSocket::SocketState state);
  void onReadyRead();

private:
  void processRequest(TCP::Request &request);

signals:
  void disconnected();

private:
  QTcpSocket *_socket;
  TCP::ProtocolHandler _protocol;
  int _keepAliveTimerId;
};
Q_DECLARE_METATYPE(ServerClientConnectionHandler*)


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


class ServerModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Roles {
    RemoteAddressRole = Qt::UserRole + 1,
    RemoteTcpPortRole
  };

  enum Columns {
    RemoteAddressColumn,
    RemoteTcpPortColumn
  };

  explicit ServerModel(Server *server);

  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

protected:
  QHash<int, QByteArray> roleNames() const;

private slots:
  void onServerChanged();

private:
  Server *_server;
  QHash<int, QString> _columnHeaders;
};
Q_DECLARE_METATYPE(ServerModel*)

#endif // SERVER_H
