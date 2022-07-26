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
// Implementation

#include "ByteStreamDirectorySource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

////////// operate directory ////////////
#include <sys/types.h>   
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>


static videoMap getDirectoryVideosInfo(char const* DirectoryName){
    videoMap videos;
    if(DirectoryName == NULL){
        return videos;
    }

    DIR * dir;
    struct dirent * ptr;
    struct stat st;
    dir = opendir(DirectoryName);
    while((ptr = readdir(dir)) != NULL)
    {
        printf("d_name : %s\n", ptr->d_name);
        std::string subFile = std::string(DirectoryName) + "/" + std::string(ptr->d_name);
        int fd = open(subFile.c_str(), O_RDONLY);
        if(fd < 0){
            printf("open %s failed, errno:%d\n", subFile.c_str(), errno);
            continue;
        }

        int ret = fstat(fd, &st);
        if(ret < 0){
            printf("fstat failed, file name:%s errno:%d\n", subFile.c_str(), errno);
            close(fd);
            continue;
        }

        if(!S_ISREG(st.st_mode)){
            printf("file %s is not regular file, mode:0x%x\n", subFile.c_str(), st.st_mode);
        }else{
            unsigned long long startTime = std::stoull(ptr->d_name);
            videos.insert(std::pair<unsigned long long, std::string>(startTime, subFile));
        }

        close(fd);
    }

    closedir(dir);
    return videos;
}


////////// ByteStreamDirectorySource //////////

ByteStreamDirectorySource*
ByteStreamDirectorySource::createNew(UsageEnvironment& env, char const* DirectoryName,
				unsigned preferredFrameSize,
				unsigned playTimePerFrame) {
  videoMap videos = getDirectoryVideosInfo(DirectoryName);
  if (videos.empty()) return NULL;

  ByteStreamDirectorySource* newSource
    = new ByteStreamDirectorySource(env, videos, preferredFrameSize, playTimePerFrame);
  return newSource;
}

void ByteStreamDirectorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
//   SeekFile64(fFid, (int64_t)byteNumber, SEEK_SET);

//   fNumBytesToStream = numBytesToStream;
//   fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamDirectorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
//   SeekFile64(fFid, offset, SEEK_CUR);

//   fNumBytesToStream = numBytesToStream;
//   fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamDirectorySource::seekToEnd() {
//   SeekFile64(fFid, 0, SEEK_END);
}

ByteStreamDirectorySource::ByteStreamDirectorySource(UsageEnvironment& env, videoMap& videos,
					   unsigned preferredFrameSize,
					   unsigned playTimePerFrame)
  : FramedSource(env), fFileSize(0), fPreferredFrameSize(preferredFrameSize),
    fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
    fHaveStartedReading(False), fLimitNumBytesToStream(False), fNumBytesToStream(0),fVideos(videos),fFid(NULL) {

  fVideoIterator = fVideos.begin();
}

ByteStreamDirectorySource::~ByteStreamDirectorySource() {
  if (fFid == NULL) return;

  fclose(fFid);
}

void ByteStreamDirectorySource::doGetNextFrame() {
    // switch file
    if(fFid == NULL || feof(fFid)){
        if(fFid != NULL){
            printf("close file:%s\n", fVideoIterator->second.c_str());
            fclose(fFid);
            fFid = NULL;
            fVideoIterator++;
        }
        
        if(fVideoIterator == fVideos.end()){
            printf("fVideoIterator in end\n");
            handleClosure();
            return;
        }
        fPresentationTime.tv_sec = fVideoIterator->first;
        fPresentationTime.tv_usec = 1;
        fFid = fopen(fVideoIterator->second.c_str(), "rb");
        if(fFid == NULL){
            printf("open file:%s filed, errno:%d\n", fVideoIterator->second.c_str(), errno);
            handleClosure();
            return;   
        }
    }else{
        fPresentationTime.tv_usec = 0; // todo
    }

  if (ferror(fFid)) {
    handleClosure();
    return;
  }

  doReadFromDirectory();
}

void ByteStreamDirectorySource::doStopGettingFrames() {
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
}

void ByteStreamDirectorySource::doReadFromDirectory() {
  // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
  if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize) {
    fMaxSize = (unsigned)fNumBytesToStream;
  }
  if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
    fMaxSize = fPreferredFrameSize;
  }

  fFrameSize = fread(fTo, 1, fMaxSize, fFid);

//   if (fFrameSize < fMaxSize) {
//     // handleClosure();
//     printf("read to end of file, fFidIsSeekable:%d\n", fFidIsSeekable);
//     SeekFile64(fFid, 0, SEEK_SET);
//   }
  fNumBytesToStream -= fFrameSize;

  // Set the 'presentation time':
//   if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
//     if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
//       // This is the first frame, so use the current time:
//       gettimeofday(&fPresentationTime, NULL); // todo
//     } else {
//       // Increment by the play time of the previous data:
//       unsigned uSeconds	= fPresentationTime.tv_usec + fLastPlayTime;
//       fPresentationTime.tv_sec += uSeconds/1000000;
//       fPresentationTime.tv_usec = uSeconds%1000000;
//     }

//     // Remember the play time of this data:
//     fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
//     fDurationInMicroseconds = fLastPlayTime;
//   } else {
//     // We don't know a specific play time duration for this data,
//     // so just record the current time as being the 'presentation time':
//     gettimeofday(&fPresentationTime, NULL);
//   }


  // Inform the reader that he has data:
  // To avoid possible infinite recursion, we need to return to the event loop to do this:
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
				(TaskFunc*)FramedSource::afterGetting, this);

}
