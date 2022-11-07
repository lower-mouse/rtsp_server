from genericpath import isdir
import os
# from tzlocal import get_localzone
from datetime import datetime, timezone, timedelta
from time import sleep

#g_src_path="/home/intellif/mmc1/2022video"
#g_src_path="/media/intellif/Data/test_video"
g_src_path="./src/"
g_tst_path="./dst/"

def convert():
    if len(g_src_path) == 0:
        print("please input src directory path")
        return

    now_time = datetime.now().strftime("%s")
    print("now_time: "+now_time+" ")

    dirlist = os.listdir(g_src_path)
    dirlist.sort()
    for file in dirlist:
        src_file = os.path.join(g_src_path, file)
        dst_file = os.path.join(g_tst_path, now_time)
        print("convert "+src_file+" to "+dst_file+" ")
        os.system("ffmpeg -i "+src_file+" -codec copy -bsf h264_mp4toannexb -f h264 "+dst_file+"")
        sleep(1)
        now_time = datetime.now().strftime("%s")


def main():
    if not os.path.exists(g_tst_path):
        os.mkdir(g_tst_path)

    convert()    

if __name__ == '__main__':
    main()
