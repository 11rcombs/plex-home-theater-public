include(CheckIncludeFiles)

set(headers  
  libavcodec/avcodec
  libavfilter/avfilter
  libavformat/avformat
  libavutil/avutil
  libpostproc/postprocess
  libswscale/swscale
  
  ffmpeg/avcodec
  ffmpeg/avfilter
  ffmpeg/avformat
  ffmpeg/avutil
  postproc/postprocess
  ffmpeg/swscale
  
  libavcore/avcore
  libavcore/samplefmt
  libavutil/mem
  libavutil/samplefmt
  libavutil/opt
  libavutil/mathematics
  libswscale/rgb2rgb
  ffmpeg/rgb2rgb
  libswresample/swresample
  libavresample/avresample
)

foreach(header ${headers})
  plex_find_header(${header} ${FFMPEG_INCLUDE_DIRS})
endforeach()

include(CheckCSourceCompiles)

#### check FFMPEG member name
if(DEFINED HAVE_LIBAVFILTER_AVFILTER_H)
  set(AVFILTER_INC "#include <libavfilter/avfilter.h>")
else()
  set(AVFILTER_INC "#include <ffmpeg/avfilter.h>")
endif()
CHECK_C_SOURCE_COMPILES("
  ${AVFILTER_INC}
  int main(int argc, char *argv[])
  { 
    static AVFilterBufferRefVideoProps test;
    if(sizeof(test.sample_aspect_ratio))
      return 0;
    return 0;
  }
"
HAVE_AVFILTERBUFFERREFVIDEOPROPS_SAMPLE_ASPECT_RATIO)


