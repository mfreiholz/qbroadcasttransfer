#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>
#include <QDataStream>
#include <QString>
#include <QQueue>
#include "api.h"

///////////////////////////////////////////////////////////////////////////////
// Server and Client
///////////////////////////////////////////////////////////////////////////////

// TCP

static const quint8 CPMAGICBYTE = 15;
static const quint8 CPFILELIST = 1;

// UDP

static const quint8 DGMAGICBIT = 16;
static const quint8 DGHELLO = 1;
static const quint8 DGDATA = 3;
static const quint8 DGDATAREQ = 4;

class FileInfo
{
public:
  quint32 id;
  QString path;
  quint64 size;
  quint32 partSize;
  quint32 partCount;
  FileInfo() : id(0), size(0), partSize(0), partCount(0) {}
  bool fromFile(const QString &filePath);
};

class FileData
{
public:
  quint32 id;
  quint32 index;
  QByteArray data;
};

///////////////////////////////////////////////////////////////////////////////
// Server side only
///////////////////////////////////////////////////////////////////////////////

static const quint16 UDPPORTSERVER = 15666;
static const quint16 TCPPORTSERVER = 15777;

///////////////////////////////////////////////////////////////////////////////
// Client side only
///////////////////////////////////////////////////////////////////////////////

static const quint16 UDPPORTCLIENT = 17666;

///////////////////////////////////////////////////////////////////////////////
// TCP Protocol
///////////////////////////////////////////////////////////////////////////////

namespace TCP {

static const quint32 MAGIC = 0x00001337;

struct Request
{
  struct Header
  {
    quint32 correlationId;
    quint32 size;
  };
  Header header;
  QByteArray body;
};

class ProtocolHandler
{
public:
  explicit ProtocolHandler();
  virtual ~ProtocolHandler();
  void append(const QByteArray &data);
  Request* next();

  QByteArray serialize(const Request &request);

private:
  QByteArray _buffer;
  Request *_request;
  QQueue<Request*> _requests;
};

}  // End of TCP namespace.

#endif // PROTOCOL_H
