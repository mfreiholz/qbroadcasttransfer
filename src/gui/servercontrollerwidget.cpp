#include <QTimer>
#include <QFileDialog>
#include "servercontrollerwidget.h"
#include "../server.h"


ServerControllerWidget::ServerControllerWidget(Server *server, QWidget *parent) :
  QWidget(parent),
  _server(server),
  _serverModel(new ServerModel(server))
{
  _ui.setupUi(this);
  _ui.clientsView->setModel(_serverModel);
  connect(_ui.searchClientsButton, SIGNAL(clicked()), SLOT(searchForClients()));
  connect(_ui.disconnectClientsButton, SIGNAL(clicked()), SLOT(disconnectClients()));
  connect(_ui.addFilesButton, SIGNAL(clicked()), SLOT(onAddFilesButtonClicked()));
  connect(_ui.startTransferButton, SIGNAL(clicked()), SLOT(startTransfer()));

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

void ServerControllerWidget::startTransfer()
{
  _server->broadcastFiles();
}

void ServerControllerWidget::onAddFilesButtonClicked()
{
  QStringList files = QFileDialog::getOpenFileNames(this, "", QDir::homePath());
  foreach (auto f, files) {
    _server->registerFile(f);
  }
}