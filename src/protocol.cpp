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
