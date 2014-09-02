#ifndef CLIENTCONTROLLERWIDGET_HEADER
#define CLIENTCONTROLLERWIDGET_HEADER

#include "QWidget"
#include "ui_clientcontrollerwidget.h"
class Client;

class ClientControllerWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ClientControllerWidget(Client *client, QWidget *parent = 0, Qt::WindowFlags f = 0);
  virtual ~ClientControllerWidget();

public slots:
  void startup();
  void shutdown();

private slots:
  void updateGuiState();
  void onSavePathButtonClicked();

private:
  // User interface.
  Ui::ClientControllerWidget _ui;

  // Basic client objects.
  Client *_client;
};

#endif // CLIENTCONTROLLERWIDGET_HEADER