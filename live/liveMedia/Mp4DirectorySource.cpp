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

#include "Mp4DirectorySource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

////////// operate directory ////////////
#include <sys/types.h>   
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

const unsigned char g_nal_head[4] = {0, 0, 0, 1};
int getMp4FileTime(std::string fileName){
  std::size_t pos_dot = fileName.find_last_of('.');
  if(pos_dot == std::string ::npos){
    return -1;
  }

  if("mp4" != fileName.substr(pos_dot+1)){
    return -1;
  }

  int hour = 0, minute = 0, second = 0;
  std::size_t pos_dash1 = fileName.find_first_of('-');
  if(pos_dash1 == std::string ::npos){
    return -1;
  }

  hour = std::atoi(fileName.substr(0, pos_dash1).c_str());
  std::size_t pos_dash2 = fileName.find_first_of('-', pos_dash1 + 1);
  if(pos_dash2 == std::string ::npos){
    return -1;
  }

  minute = std::atoi(fileName.substr(pos_dash1 + 1, pos_dash2 - pos_dash1).c_str());
  second = std::atoi(fileName.substr(pos_dash2 + 1, pos_dot - pos_dash2).c_str());
  printf("fileName:%s hour:%d minute:%d second:%d\n", fileName.c_str(), hour, minute, second);
  return (hour * 3600) + (minute * 60) + second;
}

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
        // printf("d_name : %s\n", ptr->d_name);
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
            int startTime = getMp4FileTime(std::string(ptr->d_name));
            if(startTime < 0){
                printf("file:%s get time failed\n", ptr->d_name);
            }else{
              videos.insert(std::pair<int, std::string>(startTime, subFile));
            }
        }

        close(fd);
    }

    closedir(dir);

    for(auto& it:videos){
      printf("playback file:%s\n", it.second.c_str());
    }
    return videos;
}


////////// Mp4DirectorySource //////////

Mp4DirectorySource*
Mp4DirectorySource::createNew(UsageEnvironment& env, char const* DirectoryName,
				unsigned preferredFrameSize,
				unsigned playTimePerFrame) {
  videoMap videos = getDirectoryVideosInfo(DirectoryName);
  if (videos.empty()) return NULL;

  Mp4DirectorySource* newSource
    = new Mp4DirectorySource(env, videos, preferredFrameSize, playTimePerFrame);
  return newSource;
}

void Mp4DirectorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
//   SeekFile64(fFid, (int64_t)byteNumber, SEEK_SET);

//   fNumBytesToStream = numBytesToStream;
//   fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void Mp4DirectorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
//   SeekFile64(fFid, offset, SEEK_CUR);

//   fNumBytesToStream = numBytesToStream;
//   fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void Mp4DirectorySource::seekToEnd() {
//   SeekFile64(fFid, 0, SEEK_END);
}

Mp4DirectorySource::Mp4DirectorySource(UsageEnvironment& env, videoMap& videos,
					   unsigned preferredFrameSize,
					   unsigned playTimePerFrame)
  : FramedSource(env), fFileSize(0), fPreferredFrameSize(preferredFrameSize),
    fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
    fHaveStartedReading(False), fLimitNumBytesToStream(False), fNumBytesToStream(0),fVideos(videos),fFid(NULL) {

  fVideoIterator = fVideos.begin();
  fReader = Ifai::Ifmp4::Mp4ReaderInterface::CreateNew();
  need_open = false;
}

Mp4DirectorySource::~Mp4DirectorySource() {
  if (fFid == NULL) return;

  fclose(fFid);
}

void Mp4DirectorySource::doGetNextFrame() {
  fWriteLen = 0;
  if(!need_open){
      // switch file
      if(fVideoIterator == fVideos.end()){
          printf("fVideoIterator in end\n");
          exit(0);
          // handleClosure();
          return;
      }

      printf("open video file:%s\n", fVideoIterator->second.c_str());
      Ifai::Ifmp4::Mp4ReaderInterface::ERRNO ret = fReader->Open(fVideoIterator->second);
      if(ret != Ifai::Ifmp4::Mp4ReaderInterface::kOk){
          printf("open file:%s filed, errno:%d\n", fVideoIterator->second.c_str(), errno);
          handleClosure();
          return;   
      }        

      // fFid = fopen("test.h264", "w+");
      fMp4Info = fReader->GetMp4Info();
      fVideoIterator++;
      need_open = true;
  }

  doReadFromDirectory();
}

void Mp4DirectorySource::doStopGettingFrames() {
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
}

void Mp4DirectorySource::doReadFromDirectory() {
  // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
  // if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize) {
  //   fMaxSize = (unsigned)fNumBytesToStream;
  // }
  // if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
  //   fMaxSize = fPreferredFrameSize;
  // }

  Ifai::Ifmp4::Mp4ReaderInterface::Frame frame;
  while(true){
    memset(&frame, 0, sizeof(frame));
    Ifai::Ifmp4::Mp4ReaderInterface::ERRNO ret = fReader->ReadFrame(frame);
    if(ret != Ifai::Ifmp4::Mp4ReaderInterface::kOk){
      printf("mp4 ReadFrame failed, ret:%d\n", ret);
      need_open = false;
      break;
    }

    if(frame.type == Ifai::Ifmp4::FrameType::video){
      if(frame.sync_frame){
        if(!fMp4Info->sps.empty()){
          saveData(fMp4Info->sps.data(), fMp4Info->sps.size());
        }
        
        if(!fMp4Info->pps.empty()){
          saveData(fMp4Info->pps.data(), fMp4Info->pps.size());
        }
      }
      
      saveData(g_nal_head, 4);
      saveData((char*)frame.frame_data + 4,frame.frame_data_len - 4);
      // gettimeofday(&fPresentationTime, NULL);
      break;
    }else if(frame.type == Ifai::Ifmp4::FrameType::gpsInfo){
      std::string gps_info(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
      printf("gps_info:%s\n", gps_info.c_str());
      packetSei(gps_info);
    }
  }

  fFrameSize = fWriteLen;
  // Inform the reader that he has data:
  // To avoid possible infinite recursion, we need to return to the event loop to do this:
  // nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
	// 			(TaskFunc*)FramedSource::afterGetting, this);
  FramedSource::afterGetting(this);
}

void Mp4DirectorySource::saveData(const void *src, size_t __n){
  unsigned int write_len = __n;
  if(fWriteLen + __n > fMaxSize){
    write_len = fMaxSize - fWriteLen;
  }

  fNumTruncatedBytes += (__n - write_len);
  memcpy(fTo + fWriteLen, src, write_len);
  // fwrite(src, 1, write_len, fFid);
  fWriteLen += write_len;
  // printf("fNumTruncatedByte:%u\n", fNumTruncatedBytes);
}

unsigned Mp4DirectorySource::maxFrameSize() const{
  return 400000;
}

void Mp4DirectorySource::packetSei(std::string seiInfo){
  unsigned char sei[256] = {0};
  // saveData(g_nal_head, 4);

  unsigned char* lenght = NULL;
  int index = 0;
  sei[index++] = 0x00;
  sei[index++] = 0x00;
  sei[index++] = 0x00;
  sei[index++] = 0x01;
  sei[index++] = 0x06;
  sei[index++] = 0xf0;
  lenght = &sei[index++];
  *lenght = 0;
  for(auto it : seiInfo){
    (*lenght)++;
    sei[index++] = it;
  }

  sei[index++] = 0x80;
  saveData(sei, index);
}