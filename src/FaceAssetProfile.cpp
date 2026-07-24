#include "FaceAssetProfile.h"

#include <ArduinoJson.h>

#include "config.h"

namespace {
constexpr const char* kManifestPath = "/face_assets.json";
constexpr uint8_t kV2SchemaVersion = 2;
constexpr uint8_t kBaseMouthFrames = 4;
constexpr uint8_t kBaseEyeFrames = 4;
constexpr uint8_t kPetFrames = 16;
constexpr uint8_t kDizzyFrames = 15;

const char* expectedTargetName() {
#if STACKCHAN_DEVICE_STOPWATCH
  return "stopwatch";
#elif STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  return "atoms3r";
#else
  return "cores3";
#endif
}

bool imageStemExists(fs::FS& filesystem, const char* stem) {
  char path[48];
  snprintf(path, sizeof(path), "/%s.jpg", stem);
  if (filesystem.exists(path)) {
    return true;
  }
  snprintf(path, sizeof(path), "/%s.png", stem);
  return filesystem.exists(path);
}

bool v2FileExists(fs::FS& filesystem, const char* path, FaceAssetStatus& status) {
  if (filesystem.exists(path)) {
    return true;
  }
  status.error = "asset-missing";
  status.missingAsset = path;
  return false;
}

bool legacyMinimumComplete(fs::FS& filesystem) {
  static const char* kStems[] = {
    "idle",
    "listen",
    "talk_0",
    "talk_1",
    "blink",
  };
  for (const char* stem : kStems) {
    if (!imageStemExists(filesystem, stem)) {
      return false;
    }
  }
  return true;
}

bool transitionBaseComplete(fs::FS& filesystem) {
  char stem[24];
  for (uint8_t mouth = 0; mouth < kBaseMouthFrames; ++mouth) {
    for (uint8_t eye = 0; eye < kBaseEyeFrames; ++eye) {
      snprintf(stem,
               sizeof(stem),
               "voice_m%u_e%u",
               static_cast<unsigned>(mouth),
               static_cast<unsigned>(eye));
      if (!imageStemExists(filesystem, stem)) {
        return false;
      }
    }
  }
  return true;
}

bool manifestLayoutMatches(JsonDocument& document) {
  JsonObject groups = document["groups"].as<JsonObject>();
  if (groups.isNull()) {
    return false;
  }

  const char* basePattern = groups["base"]["pattern"] | "";
  const char* petPattern = groups["pet"]["pattern"] | "";
  const char* directionPattern = groups["direction"]["pattern"] | "";
  const char* dizzyPattern = groups["dizzy"]["pattern"] | "";
  if (strcmp(basePattern, "base_m{mouth}_e{eye}.jpg") != 0 ||
      (groups["base"]["mouthFrames"] | 0) != kBaseMouthFrames ||
      (groups["base"]["eyeFrames"] | 0) != kBaseEyeFrames ||
      strcmp(petPattern, "pet_anim_{index}.jpg") != 0 ||
      (groups["pet"]["startIndex"] | -1) != 0 ||
      (groups["pet"]["frameCount"] | 0) != kPetFrames ||
      strcmp(directionPattern, "dir{index}.jpg") != 0 ||
      (groups["direction"]["startIndex"] | -1) != 0 ||
      (groups["direction"]["frameCount"] | 0) != STACKCHAN_GURUGURU_FACE_COUNT ||
      (groups["direction"]["centerIndex"] | -1) != STACKCHAN_GURUGURU_FACE_CENTER_INDEX ||
      strcmp(dizzyPattern, "dizzy_{index:02d}.jpg") != 0 ||
      (groups["dizzy"]["startIndex"] | -1) != 1 ||
      (groups["dizzy"]["frameCount"] | 0) != kDizzyFrames) {
    return false;
  }

  JsonArray blinkFiles = groups["blink"]["files"].as<JsonArray>();
  if (blinkFiles.size() != 1) {
    return false;
  }
  char expectedBlink[24];
  snprintf(expectedBlink,
           sizeof(expectedBlink),
           "blink%u.jpg",
           static_cast<unsigned>(STACKCHAN_GURUGURU_FACE_CENTER_INDEX));
  const char* blinkFile = blinkFiles[0] | "";
  return strcmp(blinkFile, expectedBlink) == 0;
}

bool v2AssetsComplete(fs::FS& filesystem, FaceAssetStatus& status) {
  char path[48];
  for (uint8_t mouth = 0; mouth < kBaseMouthFrames; ++mouth) {
    for (uint8_t eye = 0; eye < kBaseEyeFrames; ++eye) {
      snprintf(path,
               sizeof(path),
               "/base_m%u_e%u.jpg",
               static_cast<unsigned>(mouth),
               static_cast<unsigned>(eye));
      if (!v2FileExists(filesystem, path, status)) {
        return false;
      }
    }
  }

  for (uint8_t frame = 0; frame < kPetFrames; ++frame) {
    snprintf(path, sizeof(path), "/pet_anim_%u.jpg", static_cast<unsigned>(frame));
    if (!v2FileExists(filesystem, path, status)) {
      return false;
    }
  }

  for (uint8_t direction = 0; direction < STACKCHAN_GURUGURU_FACE_COUNT; ++direction) {
    snprintf(path, sizeof(path), "/dir%u.jpg", static_cast<unsigned>(direction));
    if (!v2FileExists(filesystem, path, status)) {
      return false;
    }
  }

  snprintf(path,
           sizeof(path),
           "/blink%u.jpg",
           static_cast<unsigned>(STACKCHAN_GURUGURU_FACE_CENTER_INDEX));
  if (!v2FileExists(filesystem, path, status)) {
    return false;
  }

  for (uint8_t frame = 1; frame <= kDizzyFrames; ++frame) {
    snprintf(path, sizeof(path), "/dizzy_%02u.jpg", static_cast<unsigned>(frame));
    if (!v2FileExists(filesystem, path, status)) {
      return false;
    }
  }
  return true;
}

FaceAssetStatus detectManifestProfile(fs::FS& filesystem) {
  FaceAssetStatus status;
  status.mode = FaceAssetMode::Emergency;
  status.manifestPresent = true;

  File file = filesystem.open(kManifestPath, "r");
  if (!file) {
    status.error = "manifest-open";
    return status;
  }

  JsonDocument document;
  const DeserializationError jsonError = deserializeJson(document, file);
  file.close();
  if (jsonError) {
    status.error = "manifest-json";
    return status;
  }

  const int schemaVersion = document["schemaVersion"] | 0;
  status.schemaVersion = schemaVersion > 0 && schemaVersion <= 255
                           ? static_cast<uint8_t>(schemaVersion)
                           : 0;
  if (schemaVersion != kV2SchemaVersion ||
      strcmp(document["renderer"] | "", "animated") != 0) {
    status.error = "manifest-schema";
    return status;
  }
  if (strcmp(document["target"] | "", expectedTargetName()) != 0) {
    status.error = "manifest-target";
    return status;
  }
  if ((document["canvas"]["width"] | 0) != FACE_IMAGE_WIDTH ||
      (document["canvas"]["height"] | 0) != FACE_IMAGE_HEIGHT) {
    status.error = "manifest-canvas";
    return status;
  }
  const int expectedAssetCount = 48 + STACKCHAN_GURUGURU_FACE_COUNT;
  if ((document["assetCount"] | 0) != expectedAssetCount || !manifestLayoutMatches(document)) {
    status.error = "manifest-layout";
    return status;
  }
  if (!v2AssetsComplete(filesystem, status)) {
    return status;
  }

  status.mode = FaceAssetMode::AnimatedV2;
  status.manifestValid = true;
  status.error = "";
  return status;
}
}  // namespace

FaceAssetStatus detectFaceAssetStatus(fs::FS& filesystem) {
  if (filesystem.exists(kManifestPath)) {
    return detectManifestProfile(filesystem);
  }

  FaceAssetStatus status;
  if (transitionBaseComplete(filesystem)) {
    status.mode = FaceAssetMode::Transition;
  } else if (legacyMinimumComplete(filesystem)) {
    status.mode = FaceAssetMode::LegacyMinimal;
  } else {
    status.mode = FaceAssetMode::Emergency;
    status.error = "no-compatible-assets";
  }
  return status;
}

const char* faceAssetModeName(FaceAssetMode mode) {
  switch (mode) {
    case FaceAssetMode::Classic:
      return "classic";
    case FaceAssetMode::AnimatedV2:
      return "animated";
    case FaceAssetMode::Transition:
      return "transition";
    case FaceAssetMode::LegacyMinimal:
      return "legacy";
    case FaceAssetMode::Emergency:
    default:
      return "emergency";
  }
}

bool faceAssetModeUsesAnimation(FaceAssetMode mode) {
  return mode == FaceAssetMode::AnimatedV2 || mode == FaceAssetMode::Transition;
}

bool faceAssetModeUsesV2(FaceAssetMode mode) {
  return mode == FaceAssetMode::AnimatedV2;
}

bool faceAssetModeUsesLegacyFallback(FaceAssetMode mode) {
  return mode == FaceAssetMode::LegacyMinimal;
}
