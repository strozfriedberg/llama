#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "tskimgassembler.h"

TEST_CASE("testTskImgAssemblerAddImgVolumeSystemVolumeFS") {
  TskImgAssembler a;

  a.addImage(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "a", "I'm an Image" }
    }
  ));

  a.addVolumeSystem(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "b", "I'm a Volume System" },
      { "volumes", jsoncons::json(jsoncons::json_array_arg) }
    }
  ));

  a.addVolume(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "c", "I'm Volume 1" }
    }
  ));

  a.addFileSystem(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "d", "I'm a File System" }
    }
  ));

  a.addVolume(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "e", "I'm Volume 2" }
    }
  ));

  const jsoncons::json exp(
    jsoncons::json_object_arg,
    {
      { "a", "I'm an Image" },
      {
        "volumeSystem", jsoncons::json(
          jsoncons::json_object_arg,
          {
            { "b", "I'm a Volume System" },
            {
              "volumes", jsoncons::json(
                jsoncons::json_array_arg,
                {
                  jsoncons::json(
                    jsoncons::json_object_arg,
                    {
                      { "c", "I'm Volume 1" },
                      {
                        "fileSystem", jsoncons::json(
                          jsoncons::json_object_arg,
                          {
                            { "d", "I'm a File System" }
                          }
                        )
                      }
                    }
                  ),
                  jsoncons::json(
                    jsoncons::json_object_arg,
                    {
                      { "e", "I'm Volume 2" }
                    }
                  )
                }
              )
            }
          }
        )
      }
    }
  );

  REQUIRE(exp == a.dump());
}

TEST_CASE("testTskImgCollectorAddImgFS") {
  TskImgAssembler a;

  a.addImage(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "a", "I'm an Image" }
    }
  ));

  a.addFileSystem(jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "b", "I'm a File System" }
    }
  ));

  const jsoncons::json exp(
    jsoncons::json_object_arg,
    {
      { "a", "I'm an Image" },
      {
        "fileSystem", jsoncons::json(
          jsoncons::json_object_arg,
          {
            { "b", "I'm a File System" }
          }
        )
      }
    }
  );

  REQUIRE(exp == a.dump());
}

TEST_CASE("testTskImgCollectorIllegalTransitionInitToVol") {
  // INIT -> VOL
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionInitToVS") {
  // INIT -> VS
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionInitToFS") {
  // INIT -> { IMG_FS, VOL_FS }
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addFileSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgToImg") {
  // IMG -> IMG
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  CHECK_THROWS_AS(a.addImage(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgToVol") {
  // IMG -> VOL
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  CHECK_THROWS_AS(a.addVolume(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVSToVS") {
  // VS -> VS
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVSToImg") {
  // VS -> IMG
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addImage(jsoncons::json()), std::runtime_error);
}


TEST_CASE("testTskImgCollectorIllegalTransitionVSToFS") {
  // VS -> { IMG_FS, VOL_FS }
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addFileSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVolToImg") {
  // VOL -> IMG
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(
    jsoncons::json(
      jsoncons::json_object_arg,
      {
        { "volumes", jsoncons::json(jsoncons::json_array_arg) }
      }
    )
  );
  a.addVolume(jsoncons::json());
  CHECK_THROWS_AS(a.addImage(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVolToVS") {
  // VOL -> VS
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(
    jsoncons::json(
      jsoncons::json_object_arg,
      {
        { "volumes", jsoncons::json(jsoncons::json_array_arg) }
      }
    )
  );
  a.addVolume(jsoncons::json());
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgFSToImgFS") {
  // IMG_FS -> { VOL_FS, IMG_FS }
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addFileSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgFSToImg") {
  // IMG_FS -> IMG
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addImage(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgFSToVS") {
  // IMG_FS -> VS
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionImgFSToVol") {
  // IMG_FS -> VOL
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addVolume(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVolFSToVolFS") {
  // VOL_FS -> { VOL_FS, IMG_FS }
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(
    jsoncons::json(
      jsoncons::json_object_arg,
      {
        { "volumes", jsoncons::json(jsoncons::json_array_arg) }
      }
    )
  );
  a.addVolume(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addFileSystem(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVolFSToImg") {
  // VOL_FS -> IMG
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(
    jsoncons::json(
      jsoncons::json_object_arg,
      {
        { "volumes", jsoncons::json(jsoncons::json_array_arg) }
      }
    )
  );
  a.addVolume(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addImage(jsoncons::json()), std::runtime_error);
}

TEST_CASE("testTskImgCollectorIllegalTransitionVolFSToVS") {
  // VOL_FS -> VS
  TskImgAssembler a;
  a.addImage(jsoncons::json());
  a.addVolumeSystem(
    jsoncons::json(
      jsoncons::json_object_arg,
      {
        { "volumes", jsoncons::json(jsoncons::json_array_arg) }
      }
    )
  );
  a.addVolume(jsoncons::json());
  a.addFileSystem(jsoncons::json());
  CHECK_THROWS_AS(a.addVolumeSystem(jsoncons::json()), std::runtime_error);
}
