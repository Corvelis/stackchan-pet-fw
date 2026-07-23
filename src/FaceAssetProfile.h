#pragma once

#include <Arduino.h>
#include <FS.h>

enum class FaceAssetMode : uint8_t {
  Classic,
  AnimatedV2,
  Transition,
  LegacyMinimal,
  Emergency,
};

struct FaceAssetStatus {
  FaceAssetMode mode = FaceAssetMode::Emergency;
  uint8_t schemaVersion = 0;
  bool manifestPresent = false;
  bool manifestValid = false;
  String error;
  String missingAsset;
};

FaceAssetStatus detectFaceAssetStatus(fs::FS& filesystem);
const char* faceAssetModeName(FaceAssetMode mode);
bool faceAssetModeUsesAnimation(FaceAssetMode mode);
bool faceAssetModeUsesV2(FaceAssetMode mode);
bool faceAssetModeUsesLegacyFallback(FaceAssetMode mode);
