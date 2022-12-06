/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2022 Live Networks, Inc.  All rights reserved.
// A file source that is a plain byte stream (rather than frames)
// C++ header

#ifndef _MP4_DIR_SOURCE_HH
#define _MP4_DIR_SOURCE_HH

#ifndef _FRAMED_FILE_SOURCE_HH
#include "FramedFileSource.hh"
#endif
#include <string>
#include <map>
#include "Mp4ReaderInterface.h"

typedef std::map<int, std::string> videoMap;

class Mp4DirectorySource: public FramedSource {
public:
  static Mp4DirectorySource* createNew(UsageEnvironment& env,
					 char const* DirectoryName,
					 unsigned preferredFrameSize = 0,
					 unsigned playTimePerFrame = 0);
  // "preferredFrameSize" == 0 means 'no preference'
  // "playTimePerFrame" is in microseconds
      // 0 means zero-length, unbounded, or unknown

  void seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream = 0);
    // if "numBytesToStream" is >0, then we limit the stream to that number of bytes, before treating it as EOF
  void seekToByteRelative(int64_t offset, u_int64_t numBytesToStream = 0);
  void seekToEnd(); // to force EOF handling on the next read
  unsigned maxFrameSize() const;
protected:
  Mp4DirectorySource(UsageEnvironment& env,
		       videoMap& videos,
		       unsigned preferredFrameSize,
		       unsigned playTimePerFrame);
	// called only by createNew()

  virtual ~Mp4DirectorySource();

  void doReadFromDirectory();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  virtual void doStopGettingFrames();
  void saveData(const void *src, size_t __n);
  void packetSei(std::string seiInfo);

protected:
  u_int64_t fFileSize;

private:
  unsigned fPreferredFrameSize;
  unsigned fPlayTimePerFrame;
  Boolean fFidIsSeekable;
  unsigned fLastPlayTime;
  Boolean fHaveStartedReading;
  Boolean fLimitNumBytesToStream;
  unsigned int fWriteLen;
  u_int64_t fNumBytesToStream; // used iff "fLimitNumBytesToStream" is True
  videoMap fVideos;
  videoMap::iterator fVideoIterator;
  FILE* fFid;

  std::shared_ptr<Ifai::Ifmp4::Mp4ReaderInterface> fReader;
  Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info fMp4Info;
  bool need_open;
};

#endif
