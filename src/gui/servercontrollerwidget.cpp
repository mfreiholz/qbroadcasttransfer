#include "servercontrollerwidget.h"
#include "../server.h"
#include <QTimer>


ServerControllerWidget::ServerControllerWidget(Server *server, QWidget *parent) :
  QWidget(parent),
  _server(server),
  _serverModel(new ServerModel(server))
{
  _ui.setupUi(this);
  _ui.clientsView->setModel(_serverModel);
  connect(_ui.searchClientsButton, SIGNAL(clicked()), SLOT(searchForClients()));
  connect(_ui.disconnectClientsButton, SIGNAL(clicked()), SLOT(disconnectClients()));

  QTimer::singleShot(1000, this, SLOT(searchForClients()));
}

ServerControllerWidget::~ServerControllerWidget()
{
  delete _serverModel;
  delete _server;
}

void ServerControllerWidget::searchForClients()
{
  _server->broadcastHello();
}

void ServerControllerWidget::disconnectClients()
{
  _server->disconnectFromClients();
}