#include "serverlogic.h"
#include "server.h"

ServerLogic::ServerLogic(QObject *parent) :
  QObject(parent)
{
  qRegisterMetaType<Server*>();
  qRegisterMetaType<ServerModel*>();
  _server = new Server(this);
  _serverModel = new ServerModel(_server);
}

bool ServerLogic::init()
{
  return _server->startup();
}

Server *ServerLogic::server() const
{
  return _server;
}

ServerModel *ServerLogic::serverModel() const
{
  return _serverModel;
}

QList<QUrl> ServerLogic::files() const
{
  return _files;
}

void ServerLogic::setFiles(const QList<QUrl> &files)
{
  QList<FileInfo> fileInfos;
  foreach (QUrl url, files) {
    FileInfo info;
    info.fromFile(url.toLocalFile());
    fileInfos.append(info);
  }
  _server->registerFileList(fileInfos);

  _files = files;
  emit filesChanged();
}
