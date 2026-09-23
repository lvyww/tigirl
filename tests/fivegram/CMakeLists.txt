cmake_minimum_required(VERSION 3.20)
project(TigirlFivegramTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(fivegram_probe ../sentence_fivegram_probe.cpp
  ${root}/native/SentenceFivegram.cpp ${root}/native/SentenceNgram.cpp
  ${root}/native/SentenceDecoder.cpp ${root}/native/SentenceLexicon.cpp
  ${root}/native/SentenceCharacterRanks.cpp ${root}/native/Dictionary.cpp
  ${root}/native/Text.cpp ${root}/native/LexiconSerialize.cpp ${root}/native/AddWord.cpp
  ${root}/native/SentenceSession.cpp ${root}/native/SentenceAutoCommit.cpp)
target_include_directories(fivegram_probe PRIVATE ${root}/native)
if(MSVC)
  set_property(TARGET fivegram_probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  target_compile_options(fivegram_probe PRIVATE /utf-8)
endif()
