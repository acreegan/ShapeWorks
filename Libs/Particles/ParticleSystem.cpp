#include "ParticleSystem.h"
#include "ShapeEvaluation.h"
#include "Shapeworks.h"

namespace shapeworks {

ParticleSystem::ParticleSystem(const std::vector<std::string> &_paths)
{
  if (_paths.empty()) {
    throw std::runtime_error("No filenames passed to readparticlesystem");
  }

  this->paths = _paths;
  const int N = paths.size();
  const int VDimension = 3; //TODO Don't hardcode VDimension
  assert(N > 0);

  //TODO: We're using the existing particle file reader here. This is not ideal
  // since this particle reader loads into a std::vector, which subsequently
  // is copied to Eigen. Refactor it to load directly to Eigen. (This is not a
  // huge problem for now because the particle files are quite small)
  typename itk::ParticlePositionReader<VDimension>::Pointer reader0
          = itk::ParticlePositionReader<VDimension>::New();

  // Read the first file to find dimensions
  reader0->SetFileName(paths[0]);
  reader0->Update();
  const int D = reader0->GetOutput().size() * VDimension;

  P.resize(D, N);
  P.col(0) = Eigen::Map<const Eigen::VectorXd>((double *) reader0->GetOutput().data(), D);

  for (int i = 1; i < N; i++) {
    typename itk::ParticlePositionReader<VDimension>::Pointer reader
            = itk::ParticlePositionReader<VDimension>::New();
    reader->SetFileName(paths[i]);
    reader->Update();
    P.col(i) = Eigen::Map<const Eigen::VectorXd>((double *) reader->GetOutput().data(), D);
  }
}

ParticleSystem::ParticleSystem(const Eigen::MatrixXd& matrix)
{
  this->P = matrix;
}

bool ParticleSystem::ExactCompare(const ParticleSystem &other) const
{
  if (P.rows() != other.P.rows() || P.cols() != other.P.cols()) {
    std::cerr << "Rows/Columns mismatch\n";
    return false;
  }
  bool same = true;
  for (int r=0; r<P.rows(); r++) {
    for (int c=0; c<P.cols(); c++) {
      if (!equalNSigDigits(P(r,c),other.P(r,c),4)) {
        std::cerr << "("<<r<<","<<c<<"): " << P(r,c) << " vs " << other.P(r,c) << "\n";
        same = false;
      }
    }
  }
  return same;
}

bool ParticleSystem::CompactnessCompare(const ParticleSystem& other) const
{
  auto compactness1 = ShapeEvaluation::ComputeFullCompactness(*this);
  auto compactness2 = ShapeEvaluation::ComputeFullCompactness(other);

  if (compactness1.size() != compactness2.size()) {
    return false;
  }

  if (compactness1.size() > 0 && compactness2.size() > 0) {
    std::cerr << "Comparing compactness: " << compactness1[0] << " vs " << compactness2[0] << "\n";
    if (!equalNSigDigits(compactness1[0], compactness2[0], 2)) {
      return false;
    }
  }

  return true;
}

}
