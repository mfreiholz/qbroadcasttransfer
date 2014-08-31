#include <math.h>
#include <QFileInfo>
#include "protocol.h"


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

  // Create new header.
  if (!_request) {
    const int headerSize = sizeof(quint32) + sizeof(quint32) + sizeof(quint32);
    if (_buffer.size() < headerSize) {
      return;
    }

    QDataStream in(_buffer);

    // Read and validate magic-number.
    quint32 magic;
    in >> magic;
    if (magic != MAGIC) {
      _buffer.clear();
      return;
    }

    // New request begin.
    _request = new Request();
    in >> _request->header.correlationId;
    in >> _request->header.size;
    _buffer.remove(0, headerSize);
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

Request* ProtocolHandler::next()
{
  return _requests.dequeue();
}

QByteArray ProtocolHandler::serialize(const Request &request)
{
  QByteArray data;
  QDataStream out(&data, QIODevice::WriteOnly);

  out << MAGIC;
  out << request.header.correlationId;
  out << request.header.size;
  out.writeRawData(request.body.constData(), request.body.size());

  return data;
}

}  // End of namespace.