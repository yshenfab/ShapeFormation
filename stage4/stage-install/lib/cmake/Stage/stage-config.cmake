# - Config file for Stage
# It defines the following variables:
#    STAGE_INCLUDE_DIRS - Stage Include directories
#    STAGE_LIBRARIES    - Stage link libraries

set(STAGE_INCLUDE_DIRS "/Users/yang/repos/SSCI2027/code/stage4/stage-install/include/Stage-4.3"
  "/opt/homebrew/include;/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework"
  "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework")
list(REMOVE_DUPLICATES STAGE_INCLUDE_DIRS)
set(STAGE_LIBRARIES
  "${stage_DIR}/../../../lib/libstage.4.3.0.dylib"
  "-framework Carbon -framework Cocoa -framework ApplicationServices -lz;/opt/homebrew/lib/libfltk_images.dylib;/opt/homebrew/lib/libfltk_forms.dylib;/opt/homebrew/lib/libfltk_gl.dylib;/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework;/opt/homebrew/lib/libfltk.dylib"
  "")

