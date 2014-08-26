#include <QtGui/QGuiApplication>

#include "server.h"
#include "serverlogic.h"
#include "client.h"

int mainServer(QGuiApplication *app)
{
  // Start server.
  ServerLogic *serverLogic = new ServerLogic(app);
  if (!serverLogic->init()) {
    return 1;
  }

  // Start client.
  Client *client = new Client(app);
  if (!client->init()) {
    return 2;
  }

  return 0;
}

int mainClient(QGuiApplication *app)
{
  return 0;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    int code = 0;
    if ((code = mainServer(&app)) != 0) {
      return code;
    }
    if ((code = mainClient(&app)) != 0) {
      return code;
    }
    return app.exec();
}
