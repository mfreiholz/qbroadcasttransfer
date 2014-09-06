#ifndef SERVERCONTROLLERWIDGET_H
#define SERVERCONTROLLERWIDGET_H

#include "QWidget"
#include "ui_servercontrollerwidget.h"
class Server;
class ServerModel;


class ServerControllerWidget :
  public QWidget
{
  Q_OBJECT

public:
  explicit ServerControllerWidget(Server *server, QWidget *parent = 0);
  virtual ~ServerControllerWidget();

public slots:
  void searchForClients();
  void disconnectClients();

private slots:
  void onAddFilesButtonClicked();

private:
  // User interface elements.
  Ui::ServerControllerWidget _ui;

  // Basic server objects.
  Server *_server;
  ServerModel *_serverModel;
};


#endif // SERVERCONTROLLERWIDGET_H
