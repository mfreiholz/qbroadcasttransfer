#include "QApplication"
#include "server.h"
#include "client.h"
#include "gui/servercontrollerwidget.h"


int mainServer()
{
  Server *server = new Server(0);
  if (!server->startup()) {
    return 1;
  }

  ServerControllerWidget *serverWidget = new ServerControllerWidget(server, 0);
  serverWidget->show();

  return 0;
}

int mainClient()
{
  Client *client = new Client(0);
  if (!client->init()) {
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("File Broadcaster");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Humble Products");
    app.setOrganizationDomain("humble-products.com");
    app.setQuitOnLastWindowClosed(true);

    int code = 0;
    if ((code = mainServer()) != 0) {
      return code;
    }
    if ((code = mainClient()) != 0) {
      return code;
    }

    return app.exec();
}
