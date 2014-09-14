#include "filedatabuffer.h"

FileDataBuffer::FileDataBuffer(const FileInfo &fileInfo) :
  _index(0)
{
  _file.open(saveFilePath, QIODevice::ReadWrite);
}

void FileDataBuffer::add(const FileData &fileData)
{
  _parts.insert(fileData.index, fileData.data);
  flush();
}

void FileDataBuffer::flush()
{
  while (_parts.contains(_index)) {
    QByteArray data = _parts.take(_index++);
    _file.write(data);
  }
  if (index >= _info.partSize) {
    // done..
    _file.close();
  }
}
