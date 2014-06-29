#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#include <QObject>
#include <QFileInfo>
#include <QFileInfoList>
#include <QList>
#include <QUrl>
class Server;
class ServerModel;

class ServerLogic : public QObject
{
  Q_OBJECT
  Q_PROPERTY(Server* server READ server)
  Q_PROPERTY(ServerModel* serverModel READ serverModel)
  Q_PROPERTY(QList<QUrl> files READ files WRITE setFiles NOTIFY filesChanged)

signals:
  void filesChanged();

public:
  explicit ServerLogic(QObject *parent = 0);
  bool init();
  Server *server() const;
  ServerModel *serverModel() const;
  QList<QUrl> files() const;

public slots:
  void setFiles(const QList<QUrl> &files);

private:
  Server *_server;
  ServerModel *_serverModel;
  QList<QUrl> _files;
};

#endif // SERVERLOGIC_H
