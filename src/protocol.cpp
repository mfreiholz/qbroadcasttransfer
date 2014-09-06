#include <math.h>
#include <QFileInfo>
#include "protocol.h"

///////////////////////////////////////////////////////////////////////////////
// TCP Protocol Implementation
///////////////////////////////////////////////////////////////////////////////

namespace TCP {

ProtocolHandler::ProtocolHandler() :
  _request(0)
{
}

ProtocolHandler::~ProtocolHandler()
{
  delete _request;
  while (!_requests.isEmpty()) {
    Request *r = _requests.dequeue();
    delete r;
  }
}

void ProtocolHandler::append(const QByteArray &data)
{
  _buffer.append(data);
  while (true) {
    // Create new header.
    if (!_request) {
      if (_buffer.size() < Request::REQUEST_SIZE) {
        return;
      }

      QDataStream in(_buffer);

      // Read and validate magic-number.
      quint32 magic;
      in >> magic;
      if (magic != MAGIC) {
        _buffer.remove(0, sizeof(quint32));
        continue;
      }

      // New request begin.
      _request = new Request();
      in >> _request->header.type;
      in >> _request->header.correlationId;
      in >> _request->header.size;
      _buffer.remove(0, Request::REQUEST_SIZE);
    }

    // Only continue, if the buffer contains the entire request body.
    if (_buffer.size() < _request->header.size) {
      return;
    }

    _request->body = _buffer.mid(0, _request->header.size);
    _requests.enqueue(_request);
    _buffer.remove(0, _request->header.size);
    _request = 0;
  }
}

Request* ProtocolHandler::next()
{
  if (_requests.isEmpty()) {
    return 0;
  }
  return _requests.dequeue();
}

QByteArray ProtocolHandler::serialize(const Request &request) const
{
  QByteArray data;
  QDataStream out(&data, QIODevice::WriteOnly);

  out << MAGIC;
  out << request.header.type;
  out << request.header.correlationId;
  out << request.header.size;
  out.writeRawData(request.body.constData(), request.body.size());

  return data;
}

}  // End of namespace (TCP).

///////////////////////////////////////////////////////////////////////////////
// UDP Protocol Implementation
///////////////////////////////////////////////////////////////////////////////

namespace UDP {

} // End of namespace (UDP).

///////////////////////////////////////////////////////////////////////////////
// Serialization
///////////////////////////////////////////////////////////////////////////////

bool FileInfo::fromFile(const QString &filePath)
{
  QFileInfo fi(filePath);
  if (!fi.exists() || !fi.isFile()) {
    return false;
  }
  this->id = 0;
  this->path = fi.filePath();
  this->size = fi.size();
  this->partSize = 400;
  this->partCount = ceil((double)fi.size() / 400);
  return true;
}

QDataStream& operator<<(QDataStream &out, const FileInfo &info)
{
  out << info.id;
  out << info.path;
  out << info.size;
  out << info.partSize;
  out << info.partCount;
  return out;
}

QDataStream& operator>>(QDataStream &in, FileInfo &info)
{
  in >> info.id;
  in >> info.path;
  in >> info.size;
  in >> info.partSize;
  in >> info.partCount;
  return in;
}

QDataStream& operator<<(QDataStream &out, const FileData &obj)
{
  out << obj.id;
  out << obj.index;
  out << obj.data;
  return out;
}

QDataStream& operator>>(QDataStream &in, FileData &obj)
{
  in >> obj.id;
  in >> obj.index;
  in >> obj.data;
  return in;
}
