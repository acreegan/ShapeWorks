#pragma once

#include "Image.h"
#include "Mesh.h"
#include "ParticleSystem.h"
#include "Shapeworks.h"
#include <string>

namespace shapeworks {

struct SharedCommandData
{
  Image image;
  Image::Region region;
  Mesh mesh;
  ParticleSystem particleSystem;

  bool validImage() const { return image.image != nullptr; }
  bool validMesh() const { return true; }
  bool validParticleSystem() const {return particleSystem.N() >= 1 && particleSystem.D() >= 1; }
};

} // shapeworks
