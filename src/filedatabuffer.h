#ifndef FILEDATABUFFER_H
#define FILEDATABUFFER_H

#include <QHash>
#include <QString>
#include <QFile>
#include "protocol.h"


class FileDataBuffer
{
public:
  FileDataBuffer(const FileInfo &fileInfo);
  void add(const FileData &fileData);

protected:
  void flush();

private:
  FileInfo _info;

  QFile _file;

  QHash<quint32, QByteArray> _parts;
  quint32 _index;
};


#endif // FILEDATABUFFER_H
