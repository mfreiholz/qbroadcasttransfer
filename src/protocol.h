#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>
#include <QDataStream>
#include <QByteArray>
#include <QString>
#include <QQueue>
#include <QSharedPointer>
#include "api.h"

///////////////////////////////////////////////////////////////////////////////
// Common
///////////////////////////////////////////////////////////////////////////////

static const quint16 UDPPORTSERVER = 15666;
static const quint16 TCPPORTSERVER = 15777;
static const quint16 UDPPORTCLIENT = 17666;

static const quint8 DGMAGICBIT = 16;
static const quint8 DGHELLO = 1;
static const quint8 DGDATA = 3;
static const quint8 DGDATAREQ = 4;

///////////////////////////////////////////////////////////////////////////////
// UDP Protocol
///////////////////////////////////////////////////////////////////////////////

namespace UDP {


}

///////////////////////////////////////////////////////////////////////////////
// TCP Protocol
///////////////////////////////////////////////////////////////////////////////

namespace TCP {

static const quint32 MAGIC = 0x00001337;

struct Request
{
  struct Header
  {
    enum Type { REQ, RESP };
    Header() : type(REQ), correlationId(0), size(0) {}
    quint32 type;
    quint32 correlationId;
    quint32 size;
  };

  Request() : header(), body() {}
  
  void initResponseByRequest(const Request &request)
  {
    this->header.type = Header::RESP;
    this->header.correlationId = request.header.correlationId;
    this->header.size = 0;
  }

  void setBody(const QByteArray &bodyData)
  {
    this->body = bodyData;
    this->header.size = bodyData.size();
  }

  Header header;
  QByteArray body;

  static const int REQUEST_SIZE = sizeof(quint32) + sizeof(quint32) + sizeof(quint32) + sizeof(quint32);
};


class ProtocolHandler
{
public:
  explicit ProtocolHandler();
  virtual ~ProtocolHandler();
  void append(const QByteArray &data);
  Request* next();
  QByteArray serialize(const Request &request) const;

private:
  QByteArray _buffer;
  Request *_request;
  QQueue<Request*> _requests;
};

}  // End of TCP namespace.

///////////////////////////////////////////////////////////////////////////////
// Network serializeable objects.
///////////////////////////////////////////////////////////////////////////////

class FileInfo
{
public:
  typedef quint32 fileid_t;

  FileInfo() : id(0), size(0), partSize(0), partCount(0) {}
  bool fromFile(const QString &filePath);
  static fileid_t nextUniqueId();

  fileid_t id;
  QString path;
  quint64 size;
  quint32 partSize;
  quint32 partCount;
};
typedef QSharedPointer<FileInfo> FileInfoPtr;
QDataStream& operator<<(QDataStream &out, const FileInfo &info);
QDataStream& operator>>(QDataStream &in, FileInfo &info);


class FileData
{
public:
  FileInfo::fileid_t id;
  quint32 index;
  QByteArray data;
};
typedef QSharedPointer<FileData> FileDataPtr;
QDataStream& operator<<(QDataStream &out, const FileData &obj);
QDataStream& operator>>(QDataStream &in, FileData &obj);


#endif // PROTOCOL_H
