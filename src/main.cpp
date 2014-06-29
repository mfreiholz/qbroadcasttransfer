#include <QtGui/QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QQmlContext>
#include "server.h"
#include "serverlogic.h"
#include "client.h"

int mainServer(QGuiApplication *app)
{
  // Start server.
  ServerLogic *serverLogic = new ServerLogic(app);
  if (!serverLogic->init()) {
    return 100;
  }

  QQuickView *view = new QQuickView();
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->rootContext()->setContextProperty("serverLogic", serverLogic);
  view->rootContext()->setContextProperty("server", serverLogic->server());
  view->rootContext()->setContextProperty("serverModel", serverLogic->serverModel());
  view->setSource(QUrl("qrc:/qml/server.qml"));
  view->showNormal();

  // TEST
  Client *client = new Client(app);
  client->init();

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
