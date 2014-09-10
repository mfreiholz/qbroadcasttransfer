#include "QFileDialog"
#include "QDir"
#include "QTcpSocket"
#include "clientcontrollerwidget.h"
#include "../client.h"

ClientControllerWidget::ClientControllerWidget(Client *client, QWidget *parent, Qt::WindowFlags f) :
  QWidget(parent, f),
  _client(client)
{
  _ui.setupUi(this);
  _ui.savePath->setObjectName(QDir::homePath());
  _ui.filesView->setModel(new ClientFilesModel(_client, this));
  connect(_ui.savePathButton, SIGNAL(clicked()), SLOT(onSavePathButtonClicked()));
  connect(_client, SIGNAL(serverConnected()), SLOT(updateGuiState()));
  connect(_client, SIGNAL(serverDisconnected()), SLOT(updateGuiState()));
}

ClientControllerWidget::~ClientControllerWidget()
{
  delete _client;
}

void ClientControllerWidget::startup()
{
  _client->listen();
}

void ClientControllerWidget::shutdown()
{
  _client->close();
}

void ClientControllerWidget::updateGuiState()
{
  QTcpSocket *socket = _client->getServerConnection().getSocket();
  switch (socket->state()) {
    case QAbstractSocket::ConnectedState:
      _ui.connectionStatus->setText(tr("Connected to %1 on port %2").arg(socket->peerAddress().toString()).arg(socket->peerPort()));
      _ui.savePath->setEnabled(false);
      _ui.savePathButton->setEnabled(false);
      break;
    case QAbstractSocket::UnconnectedState:
      _ui.connectionStatus->setText(tr("No connection."));
      _ui.savePath->setEnabled(true);
      _ui.savePathButton->setEnabled(true);
      break;
  }
}

void ClientControllerWidget::onSavePathButtonClicked()
{
  const QString savePath = QFileDialog::getExistingDirectory(this);
  if (savePath.isEmpty()) {
    return;
  }
  _ui.savePath->setText(QDir::toNativeSeparators(savePath));
}